
#include "module.h"
#include "module_tcp.h"
#include "module_udp.h"

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

typedef struct server {
    module mod;
    tcpServer *ts;
    udpServer *us;
} server;

static void serverInit();
static void serverRun();
static void serverExit();

static void tcpServerInit();
static void tcpServerExit();

static void _tcpClientReadHandler(event *e);
static int tcpShadowSocksHandshake(tcpClient *client);

static void udpServerInit();
static void udpServerExit();

static server s;
module *app = (module *)&s;
eventHandler tcpClientReadHandler = _tcpClientReadHandler;

int main(int argc, char *argv[]) {
    moduleHook hook = { .init = serverInit, .run = serverRun, .exit = serverExit, };

    moduleInit(MODULE_SERVER, hook, app, argc, argv);
    moduleRun();
    moduleExit();

    return EXIT_SUCCESS;
}

static void serverInit() {
    getLogger()->syslog_ident = "xs-server";

    if (app->config->mode & MODE_TCP_ONLY) tcpServerInit();
    if (app->config->mode & MODE_UDP_ONLY) udpServerInit();
}

static void serverRun() {
    char addr_info[ADDR_INFO_STR_LEN];

    if (s.ts && anetFormatSock(s.ts->fd, addr_info, ADDR_INFO_STR_LEN) > 0)
        LOGN("TCP server listen at: %s", addr_info);
    if (s.us && anetFormatSock(s.us->fd, addr_info, ADDR_INFO_STR_LEN) > 0)
        LOGN("UDP server read at: %s", addr_info);
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

    s.ts = tcpServerNew(fd);
}

static void tcpServerExit() {
    tcpServerFree(s.ts);
}

static void _tcpClientReadHandler(event *e) {
    tcpClient *client = e->data;
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

    // Decrypt local client buffer
    if (app->crypto->decrypt(&client->buf, client->d_ctx, NET_IOBUF_LEN)) {
        LOGW("Tcp client [%s] decrypt buffer error", client->client_addr_info);
        goto error;
    }

    int raddr_len;
    if (client->stage == STAGE_INIT) {
        if ((raddr_len = tcpShadowSocksHandshake(client)) == -1)
            goto error;

        return;
    }

    // Write to remote server
    int nwrite;
    int rfd = remote->fd;

    nwrite = anetWrite(rfd, client->buf.data, client->buf.len);
    if (nwrite != (int)client->buf.len) {
        LOGW("Tcp remote (%d) [%s] write error: %s", rfd, client->remote_addr_info, STRERR);
        goto error;
    }

    if (client->stage == STAGE_HANDSHAKE){
        LOGD("Tcp remote (%d) [%s] connect success", rfd, client->remote_addr_info);
        client->stage = STAGE_STREAM;

        anetDisableTcpNoDelay(NULL, client->fd);
        anetDisableTcpNoDelay(NULL, remote->fd);
    }

    return;

error:
    tcpConnectionFree(client);
}

int tcpShadowSocksHandshake(tcpClient *client) {
    int raddr_len = 0;

    // Validate the buffer
    char rhost[HOSTNAME_MAX_LEN];
    int rhost_len = HOSTNAME_MAX_LEN;
    int rport;

    if ((raddr_len= socks5AddrParse(client->buf.data, client->buf.len, NULL, rhost,
                                    &rhost_len, &rport)) == SOCKS5_ERR) {
        LOGW("Tcp client [%s] parse socks5 addr error", client->client_addr_info);
        return -1;
    }

    anetFormatAddr(client->remote_addr_info, ADDR_INFO_STR_LEN, rhost, rport);
    LOGI("Tcp client (%d) [%s] request remote addr [%s]", client->fd, client->client_addr_info,
         client->remote_addr_info);

    // Connect to remote server
    char err[ANET_ERR_LEN];
    int rfd;

    if ((rfd = anetTcpNonBlockConnect(err, rhost, rport)) == ANET_ERR) {
        LOGW("Tcp remote (%d) [%s] connect error: %s", rfd, client->remote_addr_info, err);
        return -1;
    }
    LOGD("Tcp remote (%d) [%s] is connecting ...", rfd, client->remote_addr_info);

    // Prepare stream
    tcpRemote *remote = tcpRemoteNew(rfd);
    if (!remote) {
        LOGW("Tcp remote is NULL, please check the memory");
        return -1;
    }

    remote->client = client;

    client->remote = remote;
    client->stage = STAGE_HANDSHAKE;

    // Wait for remote connect succees to write left buffer
    client->buf_off = raddr_len;
    eventDel(client->re);

    s.ts->remote_count++;
    LOGD("Tcp remote current count: %d", s.ts->remote_count);

    return raddr_len;
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

    s.us = udpServerNew(fd);
}

static void udpServerExit() {
    udpServerFree(s.us);
}
