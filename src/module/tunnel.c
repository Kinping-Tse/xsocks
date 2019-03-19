
#include "module.h"
#include "module_udp.h"

#include "../protocol/udp_shadowsocks.h"

typedef struct server {
    module mod;
    udpServer *us;
} server;

static void tunnelInit();
static void tunnelRun();
static void tunnelExit();

static void udpServerOnRead(void *data);

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

    if (app->config->mode & MODE_UDP_ONLY)
        s.us = udpServerNew(app->config->local_addr, app->config->local_port,
                            CONN_TYPE_RAW, udpServerOnRead);

    if (!s.us) exit(EXIT_ERR);
}

static void tunnelRun() {
    LOGI("Use tunnel addr: %s:%d", app->config->tunnel_addr, app->config->tunnel_port);

    if (s.us) LOGN("UDP server listen at: %s", s.us->conn->addrinfo);
}

static void tunnelExit() {
    udpServerFree(s.us);
}

static void udpServerOnRead(void *data) {
    udpServer *server = data;
    udpClient *client;
    udpRemote *remote;

    char buf[NET_IOBUF_LEN];
    int buf_len = sizeof(buf);
    int nread;

    char cip[HOSTNAME_MAX_LEN];
    int cip_len = sizeof(cip);
    int cport;

    if ((client = udpClientNew(server)) == NULL) return;

    nread = UDP_READ(server->conn, buf, buf_len, &client->sa_client);
    if (nread == UDP_ERR) goto error;

    if (netIpPresentBySockAddr(NULL, cip, cip_len, &cport, &client->sa_client) == NET_OK)
        LOGD("UDP server read from %s:%d", cip, cport);

    remote = udpRemoteNew(client, CONN_TYPE_SHADOWSOCKS, app->config->remote_addr,
                          app->config->remote_port);
    if (!remote) goto error;

    udpShadowsocksConnInit((udpShadowsocksConn *)remote->conn, app->config->tunnel_addr,
                           app->config->tunnel_port);

    LOGD("UDP client proxy dest addr: %s:%d", app->config->tunnel_addr, app->config->tunnel_port);

    UDP_WRITE(remote->conn, buf, nread, &client->sa_remote);

    return;

error:
    udpConnectionFree(client);
}
