
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

static void initServer();

static void initTcpServer();
static void tcpLocalServerReadHandler(event *e);
static void tcpRemoteClientReadHandler(event *e);

static void initUdpServer();
static udpLocalServer *udpLocalServerNew(int fd);
static void udpLocalServerFree(udpLocalServer *server);
static void udpLocalServerReadHandler(event *e);

static udpRemoteClient *initUdpRemote(char *host);
static udpRemoteClient *udpRemoteClientNew(int fd);
static void udpRemoteClientFree(udpRemoteClient *remote);
static void udpRemoteClientReadHandler(event *e);

int main(int argc, char *argv[]) {
    moduleHook hook = {initServer, NULL, NULL};

    moduleInit(MODULE_SERVER, hook, &server, argc, argv);
    moduleRun();
    moduleExit();

    return EXIT_SUCCESS;
}

static void initServer() {
    getLogger()->syslog_ident = "xs-server";

    if (app->config->mode & MODE_TCP_ONLY) {
    }

    if (app->config->mode & MODE_UDP_ONLY) {
        initUdpServer();
    }
}

static void initUdpServer() {
    char err[ANET_ERR_LEN];
    char *host = app->config->remote_addr;
    int port = app->config->remote_port;
    int fd;

    if (host && isIPv6Addr(host))
        fd = netUdp6Server(err, port, host);
    else
        fd = netUdpServer(err, port, host);

    if (fd == ANET_ERR) {
        FATAL("Could not create local server UDP socket %s:%d: %s",
              host ? host : "*", port, err);
    }

    udpLocalServerNew(fd);
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

static void udpLocalServerFree(udpLocalServer *server) {
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

    remote = initUdpRemote(rip);
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

udpRemoteClient *initUdpRemote(char *host) {
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
