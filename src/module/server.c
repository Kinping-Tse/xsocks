
#include "module.h"

#include "redis/anet.h"

/*
c: client lo: local r: remote
ls: localServer rs: remoteServer
ss: shadowsocks

    local                       server                            remote

tcp:
1. ss req:  enc(addr+raw)---> ls dec(addr+raw) -> rs (raw)----------->
2. ss stream: <-----(enc_buf) ls <- enc(buf) rs <--------------(raw)
3. ss stream: enc_buf ------> lc dec(raw) -> rs  (raw)-------->
4. ss stream: <-----(enc_buf) ls <- enc(buf) rs <--------------(raw)
5. (3.4 loop).....

udp:
1. udp: enc(addr+raw) ----> ls dec(addr+raw)->rs (raw) --------->
2. udp: <---------(enc_buf) ls <- enc(addr+raw) rs <--------------(raw)
3. (1,2 loop)...

*/

static module server;
static module *app = &server;

#define STAGE_ERROR     -1  /* Error detected                   */
#define STAGE_INIT       0  /* Initial stage                    */
#define STAGE_STREAM     5  /* Stream between client and server */

typedef struct tcpClient {
    int fd;
    int stage;
    event *re;
    struct tcpRemote *remote;
    cipher_ctx_t *e_ctx;
    cipher_ctx_t *d_ctx;
    char client_addr_info[ADDR_INFO_STR_LEN];
    char remote_addr_info[ADDR_INFO_STR_LEN];
} tcpClient;

typedef struct tcpRemote {
    int fd;
    event *re;
    tcpClient *client;
} tcpRemote;

typedef struct udpLocalServer {
    int fd;
    event *re;
} udpLocalServer;

typedef struct udpRemoteClient {
    int fd;
    event *re;
    sockAddrEx local_client_sa;
    udpLocalServer *local_server;
} udpRemoteClient;

static void serverInit();
static void serverExit();

static void tcpServerInit();
static void tcpServerExit();
static void tcpAcceptHandler(event *e);

static tcpClient *tcpClientNew(int fd);
static void tcpClientFree(tcpClient *client);
static void tcpClientReadHandler(event *e);
static int tcpShadowSocksHandshake(tcpClient *client, buffer_t *buffer, tcpRemote **remote);

static tcpRemote *tcpRemoteNew(int fd);
static void tcpRemoteFree(tcpRemote *remote);
static void tcpRemoteReadHandler(event *e);

static void udpServerInit();
static void udpServerExit();
static udpLocalServer *udpLocalServerNew(int fd);
static void udpClientFree(udpLocalServer *server);
static void udpLocalServerReadHandler(event *e);

static udpRemoteClient *udpRemoteCreate(char *host);
static udpRemoteClient *udpRemoteClientNew(int fd);
static void udpRemoteClientFree(udpRemoteClient *remote);
static void udpRemoteClientReadHandler(event *e);

int main(int argc, char *argv[]) {
    moduleHook hook = {serverInit, NULL, serverExit};

    moduleInit(MODULE_SERVER, hook, &server, argc, argv);
    moduleRun();
    moduleExit();

    return EXIT_SUCCESS;
}

static void serverInit() {
    getLogger()->syslog_ident = "xs-server";

    if (app->config->mode & MODE_TCP_ONLY) {
        tcpServerInit();
    }

    if (app->config->mode & MODE_UDP_ONLY) {
        udpServerInit();
    }
}

static void serverExit() {
    tcpServerExit();
    udpServerExit();
}

static void tcpServerInit() {
    char err[ANET_ERR_LEN];
    char *host = app->config->remote_addr;
    int port = app->config->remote_port;
    int backlog = 256;
    int fd;

    if (app->config->ipv6_first || (host && isIPv6Addr(host)))
        fd = anetTcp6Server(err, port, host, backlog);
    else
        fd = anetTcpServer(err, port, host, backlog);

    if (fd == ANET_ERR)
        FATAL("Could not create server TCP listening socket %s:%d: %s",
              host ? host : "*", port, err);

    anetNonBlock(NULL, fd);

    event* e = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, tcpAcceptHandler, NULL);
    eventAdd(app->el, e);
}

static void tcpServerExit() {

}

static void tcpAcceptHandler(event* e) {
    int cfd, cport;
    char cip[NET_IP_MAX_STR_LEN];
    char err[ANET_ERR_LEN];

    cfd = anetTcpAccept(err, e->id, cip, sizeof(cip), &cport);
    if (cfd == ANET_ERR) {
        if (errno != EWOULDBLOCK)
            LOGW("Accepting local client connection: %s", err);

        return;
    }

    tcpClient *client = tcpClientNew(cfd);
    snprintf(client->client_addr_info, ADDR_INFO_STR_LEN, "%s:%d", cip, cport);

    LOGD("Accepted local client %s fd:%d", client->client_addr_info, cfd);
}

tcpClient *tcpClientNew(int fd) {
    tcpClient *client = xs_calloc(sizeof(*client));

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);

    event* re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, tcpClientReadHandler, client);
    eventAdd(app->el, re);

    client->fd = fd;
    client->re = re;
    client->remote = NULL;
    client->stage = STAGE_INIT;

    client->e_ctx = xs_calloc(sizeof(*client->e_ctx));
    client->d_ctx = xs_calloc(sizeof(*client->d_ctx));

    app->crypto->ctx_init(app->crypto->cipher, client->e_ctx, 1);
    app->crypto->ctx_init(app->crypto->cipher, client->d_ctx, 0);

    return client;
}

static void tcpClientFree(tcpClient *client) {
    if (!client) return;

    eventDel(client->re);
    eventFree(client->re);
    close(client->fd);

    app->crypto->ctx_release(client->e_ctx);
    app->crypto->ctx_release(client->d_ctx);
    xs_free(client->e_ctx);
    xs_free(client->d_ctx);

    xs_free(client);
}

static void tcpClientReadHandler(event *e) {
    tcpClient *client = e->data;
    tcpRemote *remote = client->remote;

    int readlen = NET_IOBUF_LEN;
    int cfd = client->fd;
    int nread;
    buffer_t tmp_buf = {0,0,0,NULL};

    balloc(&tmp_buf, NET_IOBUF_LEN);

    // Read local client buffer
    nread = read(cfd, tmp_buf.data, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) goto end;

        LOGW("Tcp client [%s] read error: %s", client->client_addr_info, strerror(errno));
        goto error;
    } else if (nread == 0) {
        LOGD("Tcp client [%s] closed connection", client->client_addr_info);
        goto error;
    }
    tmp_buf.len = nread;

    // Decrypt local client buffer
    if (app->crypto->decrypt(&tmp_buf, client->d_ctx, NET_IOBUF_LEN)) {
        LOGW("Tcp client [%s] decrypt buffer error", client->client_addr_info);
        goto error;
    }

    int raddr_len;
    if ((raddr_len = tcpShadowSocksHandshake(client, &tmp_buf, &remote)) == -1)
        goto error;
    if ((tmp_buf.len - raddr_len) == 0)
        goto end;

    // Write to remote server
    int nwrite;
    int rfd = remote->fd;

    nwrite = write(rfd, tmp_buf.data+raddr_len, tmp_buf.len-raddr_len);
    if (nwrite != (int)tmp_buf.len-raddr_len) {
        LOGW("Tcp remote [%s] write error: %s", client->remote_addr_info, STRERR);
        goto error;
    }

    goto end;

error:
    tcpClientFree(client);
    tcpRemoteFree(remote);

end:
    bfree(&tmp_buf);
}

int tcpShadowSocksHandshake(tcpClient *client, buffer_t *buffer, tcpRemote **ppremote) {
    int raddr_len = 0;

    if (client->stage == STAGE_INIT) {
        // Validate the buffer
        char rhost[HOSTNAME_MAX_LEN];
        int rhost_len = HOSTNAME_MAX_LEN;
        int rport;

        if ((raddr_len= socks5AddrParse(buffer->data, buffer->len, NULL, rhost,
                                        &rhost_len, &rport)) == SOCKS5_ERR) {
            LOGW("Tcp client [%s] parse socks5 addr error", client->client_addr_info);
            return -1;
        }

        anetFormatAddr(client->remote_addr_info, ADDR_INFO_STR_LEN, rhost, rport);
        LOGI("Tcp client [%s] request remote addr [%s]", client->client_addr_info,
             client->remote_addr_info);

        // Connect to remote server
        char err[ANET_ERR_LEN];
        int rfd;

        if ((rfd = anetTcpConnect(err, rhost, rport)) == ANET_ERR) {
            LOGW("Tcp Remote [%s] connenct error: %s", client->remote_addr_info, err);
            return -1;
        }
        LOGD("Tcp remote [%s] connect suceess, fd:%d", client->remote_addr_info, rfd);

        // Prepare stream
        tcpRemote *remote = tcpRemoteNew(rfd);
        remote->client = client;

        client->remote = remote;
        client->stage = STAGE_STREAM;

        anetDisableTcpNoDelay(err, client->fd);
        anetDisableTcpNoDelay(err, remote->fd);

        if (ppremote) *ppremote = remote;
    }

    return raddr_len;
}

static tcpRemote *tcpRemoteNew(int fd) {
    tcpRemote *remote = xs_calloc(sizeof(*remote));

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);

    event* re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, tcpRemoteReadHandler, remote);
    eventAdd(app->el, re);

    remote->re = re;
    remote->fd = fd;

    return remote;
}

static void tcpRemoteFree(tcpRemote *remote) {
    if (!remote) return;

    eventDel(remote->re);
    eventFree(remote->re);
    close(remote->fd);

    xs_free(remote);
}

static void tcpRemoteReadHandler(event *e) {
    tcpRemote *remote = e->data;
    tcpClient *client = remote->client;

    buffer_t tmp_buf = {0,0,0,NULL};
    balloc(&tmp_buf, NET_IOBUF_LEN);

    int readlen = NET_IOBUF_LEN;
    int rfd = remote->fd;
    int cfd = client->fd;
    int nread, nwrite;

    nread = read(rfd, tmp_buf.data, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) goto end;

        LOGW("Tcp remote [%s] read error: %s", client->remote_addr_info, STRERR);
        goto error;
    } else if (nread == 0) {
        LOGD("Tcp remote [%s] closed connection", client->remote_addr_info);
        goto error;
    }
    tmp_buf.len = nread;

    if (app->crypto->encrypt(&tmp_buf, client->e_ctx, NET_IOBUF_LEN)) {
        LOGW("Tcp remote [%s] encrypt buffer error", client->remote_addr_info);
        goto error;
    }

    if ((nwrite = write(cfd, tmp_buf.data, tmp_buf.len)) != (int)tmp_buf.len) {
        LOGW("Local client [%s] write error: %s", client->client_addr_info, STRERR);
        goto error;
    }

    goto end;

error:
    tcpClientFree(client);
    tcpRemoteFree(remote);
end:
    bfree(&tmp_buf);
}

static void udpServerInit() {
    char err[ANET_ERR_LEN];
    char *host = app->config->remote_addr;
    int port = app->config->remote_port;
    int fd;

    if (app->config->ipv6_first || (host && isIPv6Addr(host)))
        fd = netUdp6Server(err, port, host);
    else
        fd = netUdpServer(err, port, host);

    if (fd == ANET_ERR) {
        FATAL("Could not create local server UDP socket %s:%d: %s",
              host ? host : "*", port, err);
    }

    udpLocalServerNew(fd);
}

static void udpServerExit() {
    udpClientFree(NULL);
}

static udpLocalServer *udpLocalServerNew(int fd) {
    udpLocalServer *server = xs_calloc(sizeof(*server));

    anetNonBlock(NULL, fd);

    event* re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, udpLocalServerReadHandler, server);
    eventAdd(app->el, re);

    server->fd = fd;
    server->re = re;

    return server;
}

static void udpClientFree(udpLocalServer *server) {
    if (!server) return;

    eventDel(server->re);
    eventFree(server->re);
    close(server->fd);

    xs_free(server);
}

static void udpLocalServerReadHandler(event *e) {
    udpLocalServer *local = e->data;
    udpRemoteClient *remote = NULL;

    int readlen = NET_IOBUF_LEN;
    int lfd = local->fd;
    int nread;

    sockAddrEx caddr;
    buffer_t tmp_buf = {0,0,0,NULL};

    balloc(&tmp_buf, NET_IOBUF_LEN);
    netSockAddrExInit(&caddr);

    // Read local client buffer
    nread = recvfrom(lfd, tmp_buf.data, readlen, 0,
                    (sockAddr *)&caddr.sa, &caddr.sa_len);
    if (nread == -1) {
        LOGW("Local server read UDP error: %s", strerror(errno));
        goto error;
    }
    tmp_buf.len = nread;

    char cip[HOSTNAME_MAX_LEN];
    int cport;

    netIpPresentBySockAddr(NULL, cip, HOSTNAME_MAX_LEN, &cport, &caddr);
    LOGD("Local server read UDP from [%s:%d]", cip, cport);

    // Decrypt local client buffer
    if (app->crypto->decrypt_all(&tmp_buf, app->crypto->cipher, NET_IOBUF_LEN)) {
        LOGW("Local server decrypt UDP buffer error");
        goto error;
    }

    // Validate the buffer
    char rip[HOSTNAME_MAX_LEN];
    int rip_len = HOSTNAME_MAX_LEN;
    int rport;
    int raddr_len;

    if ((raddr_len= socks5AddrParse(tmp_buf.data, tmp_buf.len, NULL, rip, &rip_len, &rport)) == -1) {
        LOGW("Local server parse UDP buffer error");
        goto error;
    }

    char raddr_info[ADDR_INFO_STR_LEN];
    anetFormatAddr(raddr_info, ADDR_INFO_STR_LEN, rip, rport);
    LOGI("Local client request UDP addr: %s", raddr_info);

    // Write to remote server
    sockAddrEx raddr;
    char err[ANET_ERR_LEN];
    if (netGetUdpSockAddr(err, rip, rport, &raddr, 0) == NET_ERR) {
        LOGW("Remote server get sockaddr error: %s", err);
        goto error;
    }

    remote = udpRemoteCreate(rip);
    remote->local_server = local;
    memcpy(&remote->local_client_sa, &caddr, sizeof(caddr));

    int rfd = remote->fd;
    if (sendto(rfd, tmp_buf.data+raddr_len, tmp_buf.len-raddr_len, 0,
               (sockAddr *)&raddr.sa, raddr.sa_len) == -1) {
        LOGW("Remote client send UDP buffer error: %s", strerror(errno));
        goto error;
    }

    goto end;

error:
    udpRemoteClientFree(remote);

end:
    bfree(&tmp_buf);
}

udpRemoteClient *udpRemoteCreate(char *host) {
    char err[ANET_ERR_LEN];
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

    return udpRemoteClientNew(fd);
}

static udpRemoteClient *udpRemoteClientNew(int fd) {
    udpRemoteClient *remote = xs_calloc(sizeof(*remote));

    anetNonBlock(NULL, fd);

    event *re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, udpRemoteClientReadHandler, remote);
    eventAdd(app->el, re);

    remote->re = re;
    remote->fd = fd;

    netSockAddrExInit(&remote->local_client_sa);

    return remote;
}

static void udpRemoteClientFree(udpRemoteClient *remote) {
    if (!remote) return;

    eventDel(remote->re);
    eventFree(remote->re);
    close(remote->fd);

    xs_free(remote);
}

static void udpRemoteClientReadHandler(event *e) {
    udpRemoteClient *remote = e->data;
    udpLocalServer *local = remote->local_server;

    int readlen = NET_IOBUF_LEN;
    int rfd = remote->fd;
    int nread;
    char buf[NET_IOBUF_LEN];
    sds sbuf = NULL;
    buffer_t tmp_buf = {0,0,0,NULL};
    sockAddrEx remote_addr;

    balloc(&tmp_buf, NET_IOBUF_LEN);
    netSockAddrExInit(&remote_addr);

    // Read remote server buffer
    nread = recvfrom(rfd, buf, readlen, 0,
                    (sockAddr *)&remote_addr.sa, &remote_addr.sa_len);
    if (nread == -1) {
        LOGW("Remote client UDP read error: %s", strerror(errno));
        goto error;
    }

    // Log remote server info
    char rip[HOSTNAME_MAX_LEN];
    int rport;
    netIpPresentBySockAddr(NULL, rip, HOSTNAME_MAX_LEN, &rport, &remote_addr);
    LOGD("Remote client read UDP from [%s:%d] ", rip, rport);

    // Append address buffer
    sbuf = socks5AddrInit(NULL, rip, rport);
    sbuf = sdscatlen(sbuf, buf, nread);
    memcpy(tmp_buf.data, sbuf, sdslen(sbuf));
    tmp_buf.len = sdslen(sbuf);

    // Encrypt remote buffer
    if (app->crypto->encrypt_all(&tmp_buf, app->crypto->cipher, NET_IOBUF_LEN)) {
        LOGW("Remote client encrypt UDP buffer error");
        goto error;
    }

    // Write to local client
    int lfd = local->fd;
    int nwrite = sendto(lfd, tmp_buf.data, tmp_buf.len, 0,
                        (sockAddr *)&remote->local_client_sa.sa, remote->local_client_sa.sa_len);
    if (nwrite == -1) {
        LOGW("Local server UDP write error: %s", strerror(errno));
        goto error;
    }

    goto end;

error:
end:
    udpRemoteClientFree(remote);
    sdsfree(sbuf);
    bfree(&tmp_buf);
}
