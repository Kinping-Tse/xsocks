
#include "module.h"

#undef ADD_EVENT
#include "../protocol/tcp_socks5.h"
#include "../protocol/tcp_shadowsocks.h"

typedef struct tcpServer {
    tcpListener *ln;
    int client_count;
    int remote_count;
} tcpServer;

typedef struct tcpClient {
    tcpConn *conn;
    tcpServer *server;
    struct tcpRemote *remote;
} tcpClient;

typedef struct tcpRemote {
    tcpConn *conn;
    tcpClient *client;
} tcpRemote;

typedef struct server {
    module mod;
    tcpServer *ts;
} server;

static void localInit();
static void localRun();
static void localExit();

static void tcpServerInit();
static void tcpServerExit();

static tcpServer *tcpServerNew();
static void tcpServerFree(tcpServer *server);
static void tcpServerOnAccept(void *data);

static void tcpConnectionFree(tcpClient *client);

static tcpClient *tcpClientNew(tcpServer *server);
static void tcpClientFree(tcpClient *client);

static tcpRemote *tcpRemoteNew(tcpClient *client);
static void tcpRemoteFree(tcpRemote *remote);

static void tcpClientOnRead(void *data);
static void tcpClientOnClose(void *data);
static void tcpClientOnError(void *data);
static void tcpClientOnTimeout(void *data);

static void tcpRemoteOnConnect(void *data, int status);
static void tcpRemoteOnRead(void *data);
static void tcpRemoteOnClose(void *data);
static void tcpRemoteOnError(void *data);
static void tcpRemoteOnTimeout(void *data);

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
    s.ts = tcpServerNew();
}

static void tcpServerExit() {
    tcpServerFree(s.ts);
}

static tcpServer *tcpServerNew() {
    tcpServer *server = xs_calloc(sizeof(*server));
    if (!server) {
        LOGW("TCP server is NULL, please check the memory");
        return NULL;
    }

    char err[XS_ERR_LEN];
    tcpListener *ln = tcpListen(err, app->el, app->config->local_addr, app->config->local_port, server, tcpServerOnAccept);
    if (!ln) {
        LOGE(err);
        tcpServerFree(server);
        return NULL;
    }
    server->ln = ln;

    return server;
}

static void tcpServerFree(tcpServer *server) {
    if (!server) return;
    TCP_L_CLOSE(server->ln);
    xs_free(server);
}

static void tcpServerOnAccept(void *data) {
    tcpServer *server = data;
    tcpClient *client = tcpClientNew(server);
    if (client) {
        LOGD("TCP server accepted client %s", TCP_GET_ADDRINFO(client->conn));
        LOGD("TCP client current count: %d", ++server->client_count);
    }
}

void tcpConnectionFree(tcpClient *client) {
    if (!client) return;

    tcpServer *server = client->server;

    server->client_count--;
    if (client->remote) server->remote_count--;

    LOGD("TCP client current count: %d", server->client_count);
    LOGD("TCP remote current count: %d", server->remote_count);

    tcpRemoteFree(client->remote);
    tcpClientFree(client);
}

static tcpClient *tcpClientNew(tcpServer *server) {
    tcpClient *client;
    tcpConn *conn;
    char err[XS_ERR_LEN];

    if ((client = xs_calloc(sizeof(*client))) == NULL) {
        LOGE("TCP client is NULL, please check the memory");
        return NULL;
    }

    if ((conn = tcpAccept(err, app->el, server->ln->fd, app->config->timeout, client)) == NULL) {
        LOGW(err);
        tcpClientFree(client);
        return NULL;
    }
    client->conn = (tcpConn *)tcpSocks5ConnNew(conn);
    client->server = server;

    anetEnableTcpNoDelay(err, conn->fd);

    TCP_ON_READ(client->conn, tcpClientOnRead);
    TCP_ON_CLOSE(client->conn, tcpClientOnClose);
    TCP_ON_ERROR(client->conn, tcpClientOnError);
    TCP_ON_TIMEOUT(client->conn, tcpClientOnTimeout);

    ADD_EVENT_READ(client->conn);

    return client;
}

static void tcpClientFree(tcpClient *client) {
    if (!client) return;

    TCP_CLOSE(client->conn);
    xs_free(client);
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

    if (!client->remote) {
        client->remote = tcpRemoteNew(client);
        if (!client->remote) {
            tcpConnectionFree(client);
            return;
        }

        tcpSetTimeout(client->conn, -1);
        DEL_EVENT_READ(client->conn);
    } else {
        tcpPipe(client->conn, remote->conn);
    }
}

static void tcpClientOnClose(void *data) {
    tcpClient *client = data;

    LOGD("TCP client %s closed connection", TCP_GET_ADDRINFO(client->conn));

    tcpConnectionFree(client);
}

static void tcpClientOnError(void *data) {
    tcpClient *client = data;
    tcpRemote *remote = client->remote;

    LOGW("TCP client %s pipe error: %s", TCP_GET_ADDRINFO(client->conn),
         client->conn->err != 0 ? client->conn->errstr : remote->conn->errstr);
}

static void tcpClientOnTimeout(void *data) {
    tcpClient *client = data;

    LOGI("TCP client %s read timeout", TCP_GET_ADDRINFO(client->conn));
}

static tcpRemote *tcpRemoteNew(tcpClient *client) {
    tcpRemote *remote = xs_calloc(sizeof(*remote));
    if (!remote) {
        LOGE("TCP remote is NULL, please check the memory");
        return NULL;
    }

    tcpSocks5Conn *conn_client = (tcpSocks5Conn *)client->conn;
    tcpConn *conn_remote;

    char err[XS_ERR_LEN];
    char host[HOSTNAME_MAX_LEN];
    int host_len = sizeof(host);
    int port;

    socks5AddrParse(conn_client->addrbuf_dest, sdslen(conn_client->addrbuf_dest),
                    NULL, host, &host_len, &port);
    conn_remote = tcpConnect(err, app->el, app->config->remote_addr, app->config->remote_port,
                             app->config->timeout, remote);
    if (!conn_remote) {
        LOGW("TCP remote %s connect error: %s", err, TCP_GET_ADDRINFO(conn_remote));
        tcpRemoteFree(remote);
        return NULL;
    }
    remote->client = client;
    remote->conn = (tcpConn *)tcpShadowsocksConnNew(conn_remote, app->crypto);
    tcpShadowsocksConnInit((tcpShadowsocksConn *)remote->conn, host, port);

    TCP_ON_READ(remote->conn, tcpRemoteOnRead);
    TCP_ON_CONNECT(remote->conn, tcpRemoteOnConnect);
    TCP_ON_CLOSE(remote->conn, tcpRemoteOnClose);
    TCP_ON_ERROR(remote->conn, tcpRemoteOnError);
    TCP_ON_TIMEOUT(remote->conn, tcpRemoteOnTimeout);

    LOGD("TCP remote %s is connecting ...", TCP_GET_ADDRINFO(client->conn));
    LOGD("TCP remote current count: %d", ++s.ts->remote_count);

    return remote;
}

static void tcpRemoteFree(tcpRemote *remote) {
    if (!remote) return;
    TCP_CLOSE(remote->conn);
    xs_free(remote);
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

static void tcpRemoteOnRead(void *data) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;

    tcpPipe(remote->conn, client->conn);
}

static void tcpRemoteOnClose(void *data) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;

    LOGD("TCP remote %s closed connection", TCP_GET_ADDRINFO(client->conn));

    tcpConnectionFree(client);
}

static void tcpRemoteOnError(void *data) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;

    LOGW("TCP remote %s pipe error: %s", TCP_GET_ADDRINFO(client->conn),
         client->conn->err != 0 ? client->conn->errstr : remote->conn->errstr);
}

static void tcpRemoteOnTimeout(void *data) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;
    char *addr_info = TCP_GET_ADDRINFO(client->conn);

    if (tcpIsConnected(remote->conn))
        LOGI("TCP remote %s read timeout", addr_info);
    else
        LOGW("TCP remote %s connect timeout", addr_info);
}
