
#include "module.h"
#include "module_tcp.h"
#include "module_udp.h"
#include "../core/socks5.h"

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

static int tcpClientReadHandler(tcpClient *client);
static int tcpShadowSocksHandshake(tcpClient *client);

static void udpServerInit();
static void udpServerExit();

static int udpServerHookProcess(void *data);
static int udpRemoteHookProcess(void *data);

static server s;
module *app = (module *)&s;

int main(int argc, char *argv[]) {
    moduleHook hook = { .init = serverInit, .run = serverRun, .exit = serverExit, };

    return moduleMain(MODULE_SERVER, hook, app, argc, argv);
}

static void serverInit() {
    getLogger()->syslog_ident = "xs-server";

    if (app->config->mode & MODE_TCP_ONLY) tcpServerInit();
    if (app->config->mode & MODE_UDP_ONLY) udpServerInit();

    if (!s.ts && !s.us) exit(EXIT_ERR);
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
    s.ts = tcpServerCreate(app->config->remote_addr, app->config->remote_port, tcpClientReadHandler);
}

static void tcpServerExit() {
    tcpServerFree(s.ts);
}

static int tcpClientReadHandler(tcpClient *client) {
    tcpRemote *remote = client->remote;

    int readlen = NET_IOBUF_LEN;
    int cfd = client->fd;
    int nread;

    // Read client buffer
    nread = read(cfd, client->buf.data, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) return TCP_OK;

        LOGW("TCP client [%s] read error: %s", client->client_addr_info, STRERR);
        return TCP_ERR;
    } else if (nread == 0) {
        LOGD("TCP client [%s] closed connection", client->client_addr_info);
        return TCP_ERR;
    }
    client->buf.len = nread;

    // Decrypt client buffer
    if (app->crypto->decrypt(&client->buf, client->d_ctx, NET_IOBUF_LEN)) {
        LOGW("TCP client [%s] decrypt buffer error", client->client_addr_info);
        return TCP_ERR;
    }

    int raddr_len;
    if (client->stage == STAGE_INIT) {
        if ((raddr_len = tcpShadowSocksHandshake(client)) == -1) return TCP_ERR;

        return TCP_OK;
    }

    // Write to remote
    int nwrite;
    int rfd = remote->fd;

    nwrite = anetWrite(rfd, client->buf.data, client->buf.len);
    if (nwrite != (int)client->buf.len) {
        LOGW("TCP remote (%d) [%s] write error: %s", rfd, client->remote_addr_info, STRERR);
        return TCP_ERR;
    }

    if (client->stage == STAGE_HANDSHAKE) {
        LOGD("TCP remote (%d) [%s] connect success", rfd, client->remote_addr_info);
        client->stage = STAGE_STREAM;

        anetDisableTcpNoDelay(NULL, client->fd);
        anetDisableTcpNoDelay(NULL, remote->fd);
    }

    return TCP_OK;
}

int tcpShadowSocksHandshake(tcpClient *client) {
    int raddr_len = 0;

    // Validate the buffer
    char rhost[HOSTNAME_MAX_LEN];
    int rhost_len = HOSTNAME_MAX_LEN;
    int rport;

    if ((raddr_len= socks5AddrParse(client->buf.data, client->buf.len, NULL, rhost,
                                    &rhost_len, &rport)) == SOCKS5_ERR) {
        LOGW("TCP client [%s] parse socks5 addr error", client->client_addr_info);
        return -1;
    }

    anetFormatAddr(client->remote_addr_info, ADDR_INFO_STR_LEN, rhost, rport);
    LOGI("TCP client (%d) [%s] request remote addr [%s]", client->fd, client->client_addr_info,
         client->remote_addr_info);

    // Connect to remote server
    char err[ANET_ERR_LEN];
    int rfd;

    if ((rfd = anetTcpNonBlockConnect(err, rhost, rport)) == ANET_ERR) {
        LOGW("TCP remote (%d) [%s] connect error: %s", rfd, client->remote_addr_info, err);
        return -1;
    }
    LOGD("TCP remote (%d) [%s] is connecting ...", rfd, client->remote_addr_info);

    // Prepare stream
    tcpRemote *remote = tcpRemoteNew(rfd);
    if (!remote) {
        LOGW("TCP remote is NULL, please check the memory");
        return -1;
    }

    remote->client = client;

    client->remote = remote;
    client->stage = STAGE_HANDSHAKE;

    // Wait for remote connect succees to write left buffer
    client->buf_off = raddr_len;
    DEL_EVENT(client->re);

    s.ts->remote_count++;
    LOGD("TCP remote current count: %d", s.ts->remote_count);

    return raddr_len;
}

static void udpServerInit() {
    udpHook hook = {.init = NULL, .process = udpServerHookProcess, .free = NULL};
    s.us = udpServerCreate(app->config->remote_addr, app->config->remote_port, hook, NULL);
}

static void udpServerExit() {
    udpServerFree(s.us);
}

static int udpServerHookProcess(void *data) {
    udpClient *client = data;
    udpRemote *remote;

    char err[ANET_ERR_LEN];
    int buflen = NET_IOBUF_LEN;

    // Decrypt client buffer
    if (app->crypto->decrypt_all(&client->buf, app->crypto->cipher, buflen)) {
        LOGW("UDP server decrypt buffer error");
        return UDP_ERR;
    }

    // Get remote addr from the buffer
    char rip[HOSTNAME_MAX_LEN];
    int rip_len = sizeof(rip);
    int rport;
    int raddr_len;

    if ((raddr_len= socks5AddrParse(client->buf.data, client->buf.len, NULL, rip, &rip_len, &rport)) == -1) {
        LOGW("UDP server parse buffer error");
        return UDP_ERR;
    }

    if (netUdpGetSockAddrEx(err, rip, rport, app->config->ipv6_first, &client->sa_remote) == NET_ERR) {
        LOGW("Get UDP remote sockaddr error: %s", err);
        return UDP_ERR;
    }

    // Log the remote info
    char raddr_info[ADDR_INFO_STR_LEN];
    anetFormatAddr(raddr_info, ADDR_INFO_STR_LEN, rip, rport);
    LOGI("UDP client request addr: %s", raddr_info);

    // Prepare remote
    udpHook hook = {.init = NULL, .process = udpRemoteHookProcess, .free = NULL};
    if ((remote = udpRemoteCreate(&hook, NULL)) == NULL) return UDP_ERR;

    remote->client = client;

    client->remote = remote;
    client->buf_off = raddr_len;

    return UDP_OK;
}

static int udpRemoteHookProcess(void *data) {
    udpRemote *remote = data;
    udpClient *client = remote->client;

    int buflen = NET_IOBUF_LEN;

    // Append address buffer
    char rip[HOSTNAME_MAX_LEN];
    int rport;
    buffer_t addr_buf;
    sds sbuf;
    int addr_len;

    netIpPresentBySockAddr(NULL, rip, sizeof(rip), &rport, &client->sa_remote);
    sbuf = socks5AddrInit(NULL, rip, rport);
    addr_len = sdslen(sbuf);

    bzero(&addr_buf, sizeof(addr_buf));
    balloc(&addr_buf, addr_len);
    memcpy(addr_buf.data, sbuf, addr_len);
    addr_buf.len = addr_len;

    bprepend(&remote->buf, &addr_buf, buflen);

    sdsfree(sbuf);
    bfree(&addr_buf);

    // Encrypt remote buffer
    if (app->crypto->encrypt_all(&remote->buf, app->crypto->cipher, buflen)) {
        LOGW("UDP remote encrypt buffer error");
        return UDP_ERR;
    }

    return UDP_OK;
}
