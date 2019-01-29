
#include "module.h"

#include "redis/anet.h"
#include "redis/sds.h"

#define STAGE_ERROR     -1  /* Error detected                   */
#define STAGE_INIT       0  /* Initial stage                    */
#define STAGE_HANDSHAKE  1  /* Handshake with client            */
#define STAGE_SNI        3  /* Parse HTTP/SNI header            */
#define STAGE_RESOLVE    4  /* Resolve the hostname             */
#define STAGE_STREAM     5  /* Stream between client and server */

/*
c: client lo: local r: remote
lc: localClient rs: remoteServer
amrq: auth method req
amrp: auth method resp
hrq: handshake req (addr)
hrp: handshake resp (addr)
ss: shadowsocks
client                      local                     remote
1. socks5:      (amrq)-----> lc
2. socks5:      <-----(amrp) lc
3. socks5:      (hrq)------> lc
4. socks5:      <-----(hrp)  lc
5. ss req:                       rs enc(addr) --------->
6. ss stream: (raw)------> lc enc(raw) -> rs  (enc_buf)-------->
7. ss stream: <-----(raw) lc <- dec(enc_buf) rs <--------------(enc_buf)
8. (6.7 loop).....

*/

static module local;

struct remoteServer;

typedef struct localClient {
    int fd;
    int stage;
    event *re;
    sds buf;
    // int buf_off;
    sds addr_buf;
    struct remoteServer *remote;
    cipher_ctx_t *e_ctx;
    char dest_addr_info[ADDR_INFO_STR_LEN];
} localClient;

typedef struct remoteServer {
    int fd;
    event *re;
    // sds buf;
    // int buf_off;
    localClient *client;
    cipher_ctx_t *d_ctx;
} remoteServer;

void acceptHandler(event *e);
void remoteServerReadHandler(event *e);
void localClientReadHandler(event *e);

void listenForLocal(int *fd) {
    char err[ANET_ERR_LEN];
    char *host = local.config->local_addr;
    int port = local.config->local_port;
    int backlog = 256;

    if (host && isIPv6Addr(host)) {
        *fd = anetTcp6Server(err, port, host, backlog);
    } else {
        *fd = anetTcpServer(err, port, host, backlog);
    }
    if (*fd == ANET_ERR) {
        FATAL("Could not create server TCP listening socket %s:%d: %s",
              host ? host : "*", port, err);
    }

    anetNonBlock(NULL, *fd);
}

static void initLocal() {
    getLogger()->syslog_ident = "xs-local";

    if (local.config->mode & MODE_TCP_ONLY) {
        int lfd;
        listenForLocal(&lfd);

        event* e = eventNew(lfd, EVENT_TYPE_IO, EVENT_FLAG_READ, acceptHandler, NULL);
        eventAdd(local.el, e);
    }
}

localClient *newClient(int fd) {
    localClient *client = xs_calloc(sizeof(*client));

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);

    event* re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, localClientReadHandler, client);
    eventAdd(local.el, re);

    client->fd = fd;
    client->re = re;
    client->stage = STAGE_INIT;
    client->addr_buf = NULL;
    client->buf = sdsempty();
    // client->buf_off = 0;

    client->e_ctx = xs_calloc(sizeof(*client->e_ctx));
    local.crypto->ctx_init(local.crypto->cipher, client->e_ctx, 1);

    return client;
}

void freeClient(localClient *client) {
    sdsfree(client->buf);
    sdsfree(client->addr_buf);
    eventDel(client->re);
    eventFree(client->re);
    close(client->fd);

    local.crypto->ctx_release(client->e_ctx);
    xs_free(client->e_ctx);

    xs_free(client);
}

remoteServer *newRemote(int fd) {
    remoteServer *remote = xs_calloc(sizeof(*remote));

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);

    event* re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, remoteServerReadHandler, remote);
    eventAdd(local.el, re);

    // remote->buf = sdsempty();
    // remote->buf_off = 0;
    remote->re = re;
    remote->fd = fd;

    remote->d_ctx = xs_calloc(sizeof(*remote->d_ctx));
    local.crypto->ctx_init(local.crypto->cipher, remote->d_ctx, 0);

    return remote;
}

void freeRemote(remoteServer *remote) {
    // sdsfree(remote->buf);
    eventDel(remote->re);
    eventFree(remote->re);
    close(remote->fd);

    local.crypto->ctx_release(remote->d_ctx);
    xs_free(remote->d_ctx);

    xs_free(remote);
}

void remoteServerReadHandler(event *e) {
    remoteServer *remote = e->data;
    localClient *client = remote->client;

    buffer_t tmp_buf = {0,0,0, NULL};
    balloc(&tmp_buf, NET_IOBUF_LEN);

    int readlen = NET_IOBUF_LEN;
    int rfd = remote->fd;
    int nread = read(rfd, tmp_buf.data, readlen);
    tmp_buf.len = nread;

    if (nread == -1) {
        if (errno == EAGAIN) goto end;

        LOGW("Remote server [%s] read error: %s", client->dest_addr_info, strerror(errno));
        goto error;
    } else if (nread == 0) {
        LOGD("Remote server [%s] closed connection", client->dest_addr_info);
        goto error;
    }
    if (local.crypto->decrypt(&tmp_buf, remote->d_ctx, NET_IOBUF_LEN)) {
        LOGW("Remote server [%s] decrypt stream buffer error", client->dest_addr_info);
        goto error;
    }

    int cfd = client->fd;

    int nwrite = write(cfd, tmp_buf.data, tmp_buf.len);
    if (nwrite != (int)tmp_buf.len) {
        LOGW("Local client [%s] write error: %s", client->dest_addr_info, strerror(errno));
        goto error;
    }

    goto end;

error:
    freeClient(client);
    freeRemote(remote);
end:
    bfree(&tmp_buf);
}

void handleSocks5Auth(localClient* client) {
    char buf[NET_IOBUF_LEN];
    int readlen = NET_IOBUF_LEN;
    int cfd = client->re->id;
    int nread;

    nread = read(cfd, buf, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) return;

        LOG_STRERROR("Local client auth read error");
        goto error;
    } else if (nread == 0) {
        LOGD("Local client auth closed connection");
        goto error;
    } else if (nread <= (int)sizeof(socks5AuthReq)) {
        LOGW("Local client socks5 authenticates error");
        goto error;
    }

    socks5AuthReq *auth_req = (socks5AuthReq *)buf;
    int auth_len = sizeof(socks5AuthReq) + auth_req->nmethods;
    if (nread < auth_len || auth_req->ver != SVERSION) {
        LOGW("Local client socks5 authenticates error");
        goto error;
    }

    // Must be noauth here
    socks5AuthResp auth_resp = {
        SVERSION,
        METHOD_UNACCEPTABLE
    };
    for (int i = 0; i < auth_req->nmethods; i++) {
        if (auth_req->methods[i] == METHOD_NOAUTH) {
            auth_resp.method = METHOD_NOAUTH;
            break;
        }
    }

    if (auth_resp.method == METHOD_UNACCEPTABLE) goto error;

    if (write(cfd, &auth_resp, sizeof(auth_resp)) != sizeof(auth_resp))
        goto error;

    client->stage = STAGE_HANDSHAKE;

    return;

error:
    freeClient(client);
}

/*
 * Just Send Shadowsocks TCP Relay Header:
 *
 *    +------+----------+----------+
 *    | ATYP | DST.ADDR | DST.PORT |
 *    +------+----------+----------+
 *    |  1   | Variable |    2     |
 *    +------+----------+----------+
 */
int shadowSocksHandshake(localClient *client) {
    assert(client->addr_buf);

    int ok = MODULE_OK;
    buffer_t tmp_buf = {0,0,0,NULL};

    balloc(&tmp_buf, NET_IOBUF_LEN);
    memcpy(tmp_buf.data, client->addr_buf, sdslen(client->addr_buf));
    tmp_buf.len = sdslen(client->addr_buf);

    if (local.crypto->encrypt(&tmp_buf, client->e_ctx, NET_IOBUF_LEN))
        LOGW("Local client encrypt stream buffer error");

    int nwrite = write(client->remote->fd, tmp_buf.data, tmp_buf.len);
    if (nwrite != (int)tmp_buf.len) {
        LOGW("Local client [%s] write to remote server error", client->dest_addr_info);
        ok = MODULE_ERR;
    }

    bfree(&tmp_buf);
    sdsfree(client->addr_buf);
    client->addr_buf = NULL;

    return ok;
}

void handleSocks5Handshake(localClient* client) {
    char buf[NET_IOBUF_LEN];
    int readlen = NET_IOBUF_LEN;
    int cfd = client->re->id;
    int nread;

    nread = read(cfd, buf, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) return;

        LOG_STRERROR("Local client handshake read error");
        goto error;
    } else if (nread == 0) {
        LOGD("Local client handshake closed connection");
        goto error;
    }

    socks5Req *request = (socks5Req *)buf;
    int request_len = sizeof(socks5Req);
    if (nread < request_len || request->ver != SVERSION) {
        LOGW("Local client socks5 request error");
        goto error;
    }

    socks5Resp resp = {
        SVERSION,
        SOCKS5_REP_SUCCEEDED,
        0,
        SOCKS5_ATYP_IPV4
    };

    if (request->cmd != SOCKS5_CMD_CONNECT) {
        LOGW("Local client request error, unsupported cmd: %d", request->cmd);
        resp.rep = SOCKS5_REP_CMD_NOT_SUPPORTED;
        write(cfd, &resp, sizeof(resp));

        goto error;
    }

    int port;
    int addr_type;
    int addr_len = HOSTNAME_MAX_LEN;
    char addr[HOSTNAME_MAX_LEN] = {0};

    char *addr_buf = buf+request_len-1;
    int buf_len = nread-request_len+1;
    if (socks5AddrParse(addr_buf, buf_len, &addr_type, addr, &addr_len, &port) == SOCKS5_ERR) {
        LOGW("Local client request error, long addrlen: %d or addrtype: %d", addr_len, addr_type);
        resp.rep = SOCKS5_REP_ADDRTYPE_NOT_SUPPORTED;
        write(cfd, &resp, sizeof(resp));
        goto error;
    }

    snprintf(client->dest_addr_info, ADDR_INFO_STR_LEN, "%s:%d", addr, port);
    LOGI("Local client request addr: [%s]", client->dest_addr_info);

    sockAddrIpV4 sock_addr;
    bzero(&sock_addr, sizeof(sock_addr));

    char resp_buf[NET_IOBUF_LEN];
    memcpy(resp_buf, &resp, sizeof(resp));
    memcpy(resp_buf + sizeof(resp), &sock_addr.sin_addr, sizeof(sock_addr.sin_addr));
    memcpy(resp_buf + sizeof(resp) + sizeof(sock_addr.sin_addr),
        &sock_addr.sin_port, sizeof(sock_addr.sin_port));

    int reply_size = sizeof(resp) + sizeof(sock_addr.sin_addr) + sizeof(sock_addr.sin_port);

    int nwrite = write(cfd, &resp_buf, reply_size);
    if (nwrite != reply_size) {
        LOGW("Local client replay error");
        goto error;
    }

    // Connect to remote server
    char err[ANET_ERR_LEN];
    // bzero(err, ANET_ERR_LEN);
    char *remote_addr = local.config->remote_addr;
    int remote_port = local.config->remote_port;
    int rfd  = anetTcpConnect(err, remote_addr, remote_port);
    if (rfd == ANET_ERR) {
        LOGE("Remote server [%s:%d] connenct error: %s", remote_addr, remote_port, err);
        goto error;
    }
    LOGD("Connect to remote server suceess, fd:%d", rfd);

    // Prepare stream
    remoteServer *remote = newRemote(rfd);
    remote->client = client;

    client->remote = remote;
    client->stage = STAGE_STREAM;
    client->buf = sdsMakeRoomFor(client->buf, NET_IOBUF_LEN);
    client->addr_buf = sdsnewlen(addr_buf, buf_len);

    if (shadowSocksHandshake(client) == MODULE_ERR) {
        freeRemote(remote);
        goto error;
    }

    anetDisableTcpNoDelay(err, client->fd);
    anetDisableTcpNoDelay(err, remote->fd);
    return;

error:
    freeClient(client);
}

void handleSocks5Stream(localClient *client) {
    remoteServer *remote = client->remote;
    int readlen = NET_IOBUF_LEN;
    int cfd = client->fd;
    int nread;
    buffer_t tmp_buf = {0,0,0,NULL};

    // Read local client buffer
    sdssetlen(client->buf, 0);
    nread = read(cfd, client->buf, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) goto end;

        LOGW("Local client [%s] stream read error: %s", client->dest_addr_info, strerror(errno));
        goto error;
    } else if (nread == 0) {
        LOGD("Local client [%s] stream closed connection", client->dest_addr_info);
        goto error;
    }
    sdsIncrLen(client->buf, nread);

    // Do buffer encrypt
    balloc(&tmp_buf, NET_IOBUF_LEN);
    memcpy(tmp_buf.data, client->buf, sdslen(client->buf));
    tmp_buf.len = sdslen(client->buf);

    if (local.crypto->encrypt(&tmp_buf, client->e_ctx, NET_IOBUF_LEN))
        LOGW("Local client encrypt stream buffer error");

    // Write to remote server
    int rfd = remote->re->id;
    int nwrite;

    nwrite = write(rfd, tmp_buf.data, tmp_buf.len);
    if (nwrite != (int)tmp_buf.len) {
        LOGW("Local client [%s] write to remote server error", client->dest_addr_info);
        goto error;
    }

    goto end;

error:
    freeClient(client);
    freeRemote(remote);
end:
    bfree(&tmp_buf);
}

void localClientReadHandler(event *e) {
    localClient *client = e->data;

    if (client->stage == STAGE_INIT) {
        handleSocks5Auth(client);
        return;
    } else if (client->stage == STAGE_HANDSHAKE) {
        handleSocks5Handshake(client);
        return;
    }

    assert(client->stage == STAGE_STREAM);
    handleSocks5Stream(client);
}

void acceptHandler(event* e) {
    int cfd, cport;
    char cip[NET_IP_MAX_STR_LEN];
    char err[ANET_ERR_LEN];

    cfd = anetTcpAccept(err, e->id, cip, sizeof(cip), &cport);
    if (cfd == ANET_ERR) {
        if (errno != EWOULDBLOCK)
            LOGW("Accepting local client connection: %s", err);
        return;
    }
    LOGD("Accepted local client %s:%d fd:%d", cip, cport, cfd);

    newClient(cfd);
}

int main(int argc, char *argv[]) {
    moduleHook hook = {initLocal, NULL, NULL};

    moduleInit(MODULE_LOCAL, hook, &local, argc, argv);
    moduleRun();
    moduleExit();

    return EXIT_SUCCESS;
}
