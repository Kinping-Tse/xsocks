
#include "module.h"
#include "module_udp.h"

#include "../protocol/socks5.h"

#include "redis/anet.h"

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

typedef struct server {
    module mod;
    udpServer *us;
    buffer_t tunnel_addr;
} server;

static void tunnelInit();
static void tunnelRun();
static void tunnelExit();

static void udpServerInit();
static void udpServerExit();

static int udpServerHookProcess(void *data);
static int udpRemoteHookProcess(void *data);

static server s;
module *app = (module *)&s;

int main(int argc, char *argv[]) {
    moduleHook hook = {tunnelInit, tunnelRun, tunnelExit};

    return moduleMain(MODULE_TUNNEL, hook, app, argc, argv);
}

static void tunnelInit() {
    getLogger()->syslog_ident = "xs-tunnel";

    if (app->config->mode & MODE_TCP_ONLY) {
        LOGW("Only support UDP now!");
        LOGW("Tcp mode is not working!");
    }

    if (app->config->tunnel_addr == NULL) FATAL("Error tunnel address!");

    if (app->config->mode & MODE_UDP_ONLY) udpServerInit();

    if (!s.us) exit(EXIT_ERR);
}

static void tunnelRun() {
    LOGI("Use tunnel addr: %s:%d", app->config->tunnel_addr, app->config->tunnel_port);

    char addr_info[ADDR_INFO_STR_LEN];
    if (s.us && anetFormatSock(s.us->fd, addr_info, ADDR_INFO_STR_LEN) > 0) LOGN("UDP server read at: %s", addr_info);
}

static void tunnelExit() {
    udpServerExit();
}

static void udpServerInit() {
    udpHook hook = {.init = NULL, .process = udpServerHookProcess, .free = NULL};

    sds addr = socks5AddrInit(NULL, app->config->tunnel_addr, app->config->tunnel_port);
    int addr_len = sdslen(addr);

    bzero(&s.tunnel_addr, sizeof(s.tunnel_addr));
    balloc(&s.tunnel_addr, addr_len);
    memcpy(s.tunnel_addr.data, addr, addr_len);
    s.tunnel_addr.len = addr_len;

    s.us = udpServerCreate(app->config->local_addr, app->config->local_port, hook, &s);
    sdsfree(addr);
}

static void udpServerExit() {
    bfree(&s.tunnel_addr);

    udpServerFree(s.us);
}

static int udpServerHookProcess(void *data) {
    udpClient *client = data;
    udpRemote *remote;
    server *s = client->server->data;

    char err[ANET_ERR_LEN];
    int buflen = NET_IOBUF_LEN;

    // Append address buffer
    bprepend(&client->buf, &s->tunnel_addr, buflen);

    // Encrypt client buffer
    if (app->crypto->encrypt_all(&client->buf, app->crypto->cipher, buflen)) {
        LOGW("UDP server decrypt buffer error");
        return UDP_ERR;
    }

    // Get remote addr from config
    if (netUdpGetSockAddrEx(err, app->config->remote_addr, app->config->remote_port, app->config->ipv6_first,
                            &client->sa_remote) == NET_ERR) {
        LOGW("Get UDP remote sockaddr error: %s", err);
        return UDP_ERR;
    }

    // Prepare remote
    udpHook hook = {.init = NULL, .process = udpRemoteHookProcess, .free = NULL};
    if ((remote = udpRemoteCreate(&hook, NULL)) == NULL) return UDP_ERR;

    remote->client = client;
    client->remote = remote;

    return UDP_OK;
}

static int udpRemoteHookProcess(void *data) {
    udpRemote *remote = data;

    int buflen = NET_IOBUF_LEN;

    // Decrypt remote buffer
    if (app->crypto->decrypt_all(&remote->buf, app->crypto->cipher, buflen)) {
        LOGW("UDP remote decrypt buffer error");
        return UDP_ERR;
    }

    // Validate the buffer
    int addr_len = socks5AddrParse(remote->buf.data, remote->buf.len, NULL, NULL, NULL, NULL);
    if (addr_len == -1) {
        LOGW("UDP remote parse buffer error");
        return UDP_ERR;
    }
    remote->buf_off = addr_len;

    return UDP_OK;
}
