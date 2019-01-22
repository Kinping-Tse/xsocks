
#include "module.h"

#include "anet.h"

/*
c: client lo: local r: remote
lc: localClient rs: remoteServer
ss: shadowsocks
client                      local                    remote

tcp:
1. get addr by config
2. ss req:                       rs enc(addr) --------->
3. ss stream: (raw)------> lc enc(raw) -> rs  (enc_buf)-------->
4. ss stream: <-----(raw) lc <- dec(enc_buf) rs <--------------(enc_buf)
5. (3.4 loop).....

udp:
1. get addr by config
2. udp: (raw) ---------> lc enc(addr+raw)->rc (enc_buf) --------->
3. udp: <-------------(raw) lc <- dec(addr+raw) rc <-------------------(enc_buf)
4. (2,3 loop)...

*/

static module tunnel;
static module *app = &tunnel;

typedef struct localClient {
    int fd;
    int stage;
    event *re;
    sds buf;
    sds addr_buf;
    sockAddrStorage remote_server_sa;
} localClient;

static localClient *localTunnelServer;

typedef struct remoteServer {
    int fd;
    event *re;
    sockAddrStorage local_client_sa;
    localClient *client;
} remoteServer;

void remoteServerReadHandler(event *e);
void localClientReadHandler(event *e);

localClient *newClient(int fd) {
    localClient *client = xs_calloc(sizeof(*client));

    anetNonBlock(NULL, fd);

    event* re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, localClientReadHandler, client);
    eventAdd(app->el, re);

    client->fd = fd;
    client->re = re;
    // client->stage = STAGE_INIT;
    client->addr_buf = socks5AddrInit(NULL, app->config->tunnel_addr,
                                      app->config->tunnel_port);
    client->buf = sdsempty();
    client->buf = sdsMakeRoomFor(client->buf, NET_IOBUF_LEN);

    return client;
}

void freeClient(localClient *client) {
    sdsfree(client->buf);
    sdsfree(client->addr_buf);
    eventDel(client->re);
    eventFree(client->re);
    close(client->fd);

    xs_free(client);
}

remoteServer *newRemote(int fd) {
    remoteServer *remote = xs_calloc(sizeof(*remote));

    anetNonBlock(NULL, fd);

    event *re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, remoteServerReadHandler, remote);
    eventAdd(app->el, re);

    remote->re = re;
    remote->fd = fd;

    return remote;
}

void freeRemote(remoteServer *remote) {
    if (!remote) return;

    eventDel(remote->re);
    eventFree(remote->re);
    close(remote->fd);

    xs_free(remote);
}

localClient *initUdpClient() {
    char err[ANET_ERR_LEN];
    char *host = app->config->local_addr;
    int port = app->config->local_port;
    int fd;

    if (host && isIPv6Addr(host))
        fd = netUdp6Server(err, port, host);
    else
        fd = netUdpServer(err, port, host);

    if (fd == ANET_ERR) {
        FATAL("Could not create local client UDP socket %s:%d: %s",
              host ? host : "*", port, err);
    }

    localClient *client = newClient(fd);

    if (netGetUdpSockAddr(err, app->config->remote_addr, app->config->remote_port,
                         &client->remote_server_sa, 0) == NET_ERR) {
        FATAL(err);
    }

    return client;
}

remoteServer *initUdpRemote() {
    char err[ANET_ERR_LEN];
    char *host = app->config->remote_addr;
    int port = 0;
    int fd;

    if (host && isIPv6Addr(host))
        fd = netUdp6Server(err, 0, NULL);
    else
        fd = netUdpServer(err, 0, NULL);

    if (fd == ANET_ERR) {
        FATAL("Could not create remote server UDP socket %s:%d: %s",
              host, port, err);
    }

    return newRemote(fd);
}

static void initTunnel() {
    getLogger()->syslog_ident = "xs-tunnel";


    if (app->config->mode & MODE_TCP_ONLY) {
        LOGE("Only support UDP now!");
        exit(EXIT_ERR);
    }

    if (app->config->tunnel_addr == NULL) {
        LOGE("Error tunnel address!");
        exit(EXIT_ERR);
    }

    if (app->config->mode & MODE_UDP_ONLY) {
        localTunnelServer = initUdpClient();
    }
}

void remoteServerReadHandler(event *e) {
    remoteServer *remote = e->data;
    localClient *client = remote->client;

    buffer_t tmp_buf = {0,0,0,NULL};
    int readlen = NET_IOBUF_LEN;
    int rfd = remote->fd;
    sockAddrStorage remote_addr = {0};
    socklen_t remote_addr_len = sizeof(remote_addr);
    int nread;

    balloc(&tmp_buf, NET_IOBUF_LEN);

    // Read remote server buffer
    nread = recvfrom(rfd, tmp_buf.data, readlen, 0,
                    (sockAddr*)&remote_addr, &remote_addr_len);
    if (nread == -1) {
        LOGW("Remote Server UDP read error: %s", strerror(errno));
        goto error;
    }
    tmp_buf.len = nread;

    // Log remote server info
    char ip[HOSTNAME_MAX_LEN];
    int port;
    netIpPresentBySockAddr(NULL, ip, HOSTNAME_MAX_LEN, &port, &remote_addr);
    LOGD("Remote Server [%s:%d] UDP read", ip, port);

    // Decrypt remote buffer
    if (app->crypto->decrypt_all(&tmp_buf, app->crypto->cipher, NET_IOBUF_LEN)) {
        LOGW("Remote server decrypt UDP buffer error");
        goto error;
    }

    // Validate the buffer
    int addr_len = socks5AddrParse(tmp_buf.data, tmp_buf.len, NULL, NULL, NULL, NULL);
    if (addr_len == -1) {
        LOGW("Remote server parse UDP buffer error");
        goto error;
    }

    // Write to local client
    int cfd = client->fd;
    int nwrite = sendto(cfd, tmp_buf.data+addr_len, tmp_buf.len-addr_len, 0,
                        (sockAddr *)&remote->local_client_sa, remote->local_client_sa.ss_len);
    if (nwrite == -1) {
        LOGW("Local client UDP write error: %s", strerror(errno));
        goto error;
    }

    goto end;

error:
end:
    freeRemote(remote);
    bfree(&tmp_buf);
}

void localClientReadHandler(event *e) {
    localClient *client = e->data;
    remoteServer *remote = NULL;

    int readlen = NET_IOBUF_LEN;
    int cfd = client->fd;
    int nread;

    sds buf = sdsempty();
    sockAddrStorage src_addr = {0};
    socklen_t src_addr_len = sizeof(src_addr);
    buffer_t tmp_buf = {0,0,0,NULL};

    sdssetlen(client->buf, 0);

    // Read local client buffer
    nread = recvfrom(cfd, client->buf, readlen, 0,
                    (sockAddr *)&src_addr, &src_addr_len);
    if (nread == -1) {
        LOGW("Local client UDP read error: %s", strerror(errno));
        goto error;
    }
    sdsIncrLen(client->buf, nread);

    char ip[HOSTNAME_MAX_LEN];
    int port;
    netIpPresentBySockAddr(NULL, ip, HOSTNAME_MAX_LEN, &port, &src_addr);
    LOGD("Local client [%s:%d] UDP read", ip, port);

    // Append address buffer
    buf = sdsdup(client->addr_buf);
    buf = sdscatsds(buf, client->buf);

    balloc(&tmp_buf, NET_IOBUF_LEN);
    memcpy(tmp_buf.data, buf, sdslen(buf));
    tmp_buf.len = sdslen(buf);

    // Do buffer encrypt
    if (app->crypto->encrypt_all(&tmp_buf, app->crypto->cipher, NET_IOBUF_LEN)) {
        LOGW("Local client UDP encrypt buffer error");
        goto error;
    }

    // Write to remote server
    remote = initUdpRemote();
    remote->client = client;
    memcpy(&remote->local_client_sa, &src_addr, sizeof(src_addr));

    int rfd = remote->fd;
    if (sendto(rfd, tmp_buf.data, tmp_buf.len, 0, (sockAddr *)&client->remote_server_sa,
               client->remote_server_sa.ss_len) == -1) {
        LOGW("Remote server UDP send buffer error: %s", strerror(errno));
        goto error;
    }

    goto end;

error:
    freeRemote(remote);

end:
    sdsfree(buf);
    bfree(&tmp_buf);
}

int main(int argc, char *argv[]) {
    moduleHook hook = {initTunnel, NULL, NULL};

    moduleInit(MODULE_TUNNEL, hook, &tunnel, argc, argv);
    moduleRun();
    moduleExit();

    return EXIT_OK;
}
