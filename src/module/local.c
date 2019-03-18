
#include "module.h"
#include "module_tcp.h"

#include "../protocol/tcp_socks5.h"
#include "../protocol/tcp_shadowsocks.h"

typedef struct server {
    module mod;
    tcpServer *ts;
} server;

static void localInit();
static void localRun();
static void localExit();

static void tcpServerInit();
static void tcpServerExit();

static void tcpServerOnAccept(void *data);
static void tcpClientOnRead(void *data);
static void tcpRemoteOnConnect(void *data, int status);

static server s;
module *app = (module *)&s;

int main(int argc, char *argv[]) {
    moduleHook hook = {
        .init = localInit,
        .run = localRun,
        .exit = localExit,
    };

    return moduleMain(MODULE_LOCAL, hook, app, argc, argv);
}

static void localInit() {
    getLogger()->syslog_ident = "xs-local";
}

static void localRun() {
    if (app->config->mode & MODE_TCP_ONLY) tcpServerInit();

    if (app->config->mode & MODE_UDP_ONLY) {
        LOGW("Only support TCP now!");
        LOGW("UDP mode is not working!");
    }

    if (!s.ts) exit(EXIT_ERR);

    if (s.ts) LOGN("TCP server listen at: %s", s.ts->ln->addrinfo);
}

static void localExit() {
    tcpServerExit();
}

static void tcpServerInit() {
    s.ts = tcpServerNew(app->config->local_addr, app->config->local_port, tcpServerOnAccept);
}

static void tcpServerExit() {
    tcpServerFree(s.ts);
}

static void tcpServerOnAccept(void *data) {
    tcpServer *server = data;
    tcpClient *client = tcpClientNew(server, CONN_TYPE_SOCKS5, tcpClientOnRead);
    if (client) {
        LOGD("TCP server accepted client %s", TCP_GET_ADDRINFO(client->conn));
        LOGD("TCP client current count: %d", ++server->client_count);
    }
}

static void tcpClientOnRead(void *data) {
    tcpClient *client = data;
    tcpRemote *remote = client->remote;
    tcpSocks5Conn *conn_client = (tcpSocks5Conn *)client->conn;

    if (conn_client->state != SOCKS5_STATE_STREAM) {
        int nread = TCP_READ(client->conn, client->conn->rbuf, client->conn->rbuf_len);
        if (nread > 0) TCP_WRITE(client->conn, client->conn->rbuf, nread);
        return;
    }

    if (!remote) {
        remote = tcpRemoteNew(client, CONN_TYPE_SHADOWSOCKS, app->config->remote_addr,
                              app->config->remote_port, tcpRemoteOnConnect);
        if (!remote) {
            tcpConnectionFree(client);
            return;
        }

        char host[HOSTNAME_MAX_LEN];
        int host_len = sizeof(host);
        int port;

        socks5AddrParse(conn_client->addrbuf_dest, sdslen(conn_client->addrbuf_dest),
                        NULL, host, &host_len, &port);
        tcpShadowsocksConnInit((tcpShadowsocksConn *)remote->conn, host, port);
    } else {
        tcpPipe(client->conn, remote->conn);
    }
}

static void tcpRemoteOnConnect(void *data, int status) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;
    char *addrinfo = TCP_GET_ADDRINFO(client->conn);

    if (status == TCP_ERR) {
        LOGW("TCP remote %s connect error: %s", addrinfo, remote->conn->errstr);
        return;
    }
    LOGD("TCP remote %s connect success", addrinfo);

    // Prepare pipe
    ADD_EVENT_READ(remote->conn);
    ADD_EVENT_READ(client->conn);
}
