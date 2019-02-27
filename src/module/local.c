
#include "module.h"
#include "module_tcp.h"

#include "redis/anet.h"
#include "redis/sds.h"

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

typedef struct server {
    module mod;
    tcpServer *ts;
} server;

static void localInit();
static void localRun();
static void localExit();

static void tcpServerInit();
static void tcpServerExit();

static void _tcpClientReadHandler(event *e);
static void handleSocks5Auth(tcpClient* client);
static void handleSocks5Handshake(tcpClient* client);
static void handleSocks5Stream(tcpClient *client);
static int shadowSocksHandshake(tcpClient *client);

static server s;
module *app = (module *)&s;
eventHandler tcpClientReadHandler = _tcpClientReadHandler;

int main(int argc, char *argv[]) {
    moduleHook hook = {localInit, localRun, localExit};

    return moduleMain(MODULE_LOCAL, hook, app, argc, argv);
}

static void localInit() {
    getLogger()->syslog_ident = "xs-local";

    if (app->config->mode & MODE_TCP_ONLY) tcpServerInit();

    if (app->config->mode & MODE_UDP_ONLY) {
        LOGW("Only support TCP now!");
        LOGW("UDP mode is not working!");
    }
}

static void localRun() {
    char addr_info[ADDR_INFO_STR_LEN];

    if (s.ts && anetFormatSock(s.ts->fd, addr_info, ADDR_INFO_STR_LEN) > 0)
        LOGN("TCP server listen at: %s", addr_info);
}

static void localExit() {
    tcpServerExit();
}

static void tcpServerInit() {
    char err[ANET_ERR_LEN];
    char *host = app->config->local_addr;
    int port = app->config->local_port;
    int backlog = 256;
    int fd;

    if ((host && isIPv6Addr(host)))
        fd = anetTcp6Server(err, port, host, backlog);
    else
        fd = anetTcpServer(err, port, host, backlog);

    if (fd == ANET_ERR)
        FATAL("Could not create server TCP listening socket %s:%d: %s",
              host ? host : "*", port, err);

    s.ts = tcpServerNew(fd);
}

static void tcpServerExit() {
    tcpServerFree(s.ts);
}

static void _tcpClientReadHandler(event *e) {
    tcpClient *client = e->data;

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

static void handleSocks5Auth(tcpClient* client) {
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
    tcpConnectionFree(client);
}

static void handleSocks5Handshake(tcpClient* client) {
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
        if (write(cfd, &resp, sizeof(resp)) == -1) {
            // Do nothing, just avoid warning
        }
        goto error;
    }

    int port;
    int addr_type;
    int addr_len = HOSTNAME_MAX_LEN;
    char addr[HOSTNAME_MAX_LEN];

    char *addr_buf = buf+request_len-1;
    int buf_len = nread-request_len+1;
    if (socks5AddrParse(addr_buf, buf_len, &addr_type, addr, &addr_len, &port) == SOCKS5_ERR) {
        LOGW("Local client request error, long addrlen: %d or addrtype: %d", addr_len, addr_type);
        resp.rep = SOCKS5_REP_ADDRTYPE_NOT_SUPPORTED;
        if (write(cfd, &resp, sizeof(resp)) == -1) {
            // Do nothing, just avoid warning
        }
        goto error;
    }

    snprintf(client->remote_addr_info, ADDR_INFO_STR_LEN, "%s:%d", addr, port);
    LOGI("Tcp client request addr: [%s]", client->remote_addr_info);

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
    char *remote_addr = app->config->remote_addr;
    int remote_port = app->config->remote_port;
    int rfd  = anetTcpConnect(err, remote_addr, remote_port);
    if (rfd == ANET_ERR) {
        LOGE("Remote server [%s:%d] connenct error: %s", remote_addr, remote_port, err);
        goto error;
    }
    LOGD("Connect to remote server suceess, fd:%d", rfd);

    // Prepare stream
    tcpRemote *remote = tcpRemoteNew(rfd);
    if (!remote) {
        LOGW("Tcp remote is NULL, please check the memory");
        goto error;
    }

    remote->client = client;

    // Because of block connect
    DEL_EVENT(remote->we);
    ADD_EVENT(remote->re);

    client->remote = remote;
    client->stage = STAGE_STREAM;
    client->addr_buf = sdsnewlen(addr_buf, buf_len);

    s.ts->remote_count++;
    LOGD("Tcp remote current count: %d", s.ts->remote_count);

    if (shadowSocksHandshake(client) == MODULE_ERR) {
        goto error;
    }

    anetDisableTcpNoDelay(err, client->fd);
    anetDisableTcpNoDelay(err, remote->fd);
    return;

error:
    tcpConnectionFree(client);
}

static void handleSocks5Stream(tcpClient *client) {
    tcpRemote *remote = client->remote;
    int readlen = NET_IOBUF_LEN;
    int cfd = client->fd;
    int nread;

    // Read local client buffer
    nread = read(cfd, client->buf.data, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) return;

        LOGW("Tcp client [%s] read error: %s", client->client_addr_info, STRERR);
        goto error;
    } else if (nread == 0) {
        LOGD("Tcp client [%s] closed connection", client->client_addr_info);
        goto error;
    }
    client->buf.len = nread;

    // Do buffer encrypt
    if (app->crypto->encrypt(&client->buf, client->e_ctx, NET_IOBUF_LEN)) {
        LOGW("Tcp client encrypt buffer error");
        goto error;
    }

    // Write to remote server
    int rfd = remote->re->id;
    int nwrite;

    nwrite = anetWrite(rfd, client->buf.data, client->buf.len);
    if (nwrite != (int)client->buf.len) {
        LOGW("Tcp remote [%s] write error: %s", client->remote_addr_info, STRERR);
        goto error;
    }

    return;

error:
    tcpConnectionFree(client);
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
static int shadowSocksHandshake(tcpClient *client) {
    assert(client->addr_buf);

    int ok = MODULE_OK;
    buffer_t tmp_buf = {0,0,0,NULL};

    balloc(&tmp_buf, NET_IOBUF_LEN);
    memcpy(tmp_buf.data, client->addr_buf, sdslen(client->addr_buf));
    tmp_buf.len = sdslen(client->addr_buf);

    if (app->crypto->encrypt(&tmp_buf, client->e_ctx, NET_IOBUF_LEN))
        LOGW("Tcp client encrypt buffer error");

    int nwrite = write(client->remote->fd, tmp_buf.data, tmp_buf.len);
    if (nwrite != (int)tmp_buf.len) {
        LOGW("Tcp remote [%s] write error", client->remote_addr_info);
        ok = MODULE_ERR;
    }

    bfree(&tmp_buf);
    sdsfree(client->addr_buf);
    client->addr_buf = NULL;

    return ok;
}
