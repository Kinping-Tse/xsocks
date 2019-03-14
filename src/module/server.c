
#include "module.h"
#include "module_udp.h"

#undef ADD_EVENT
#include "../protocol/socks5.h"
#include "../protocol/tcp_raw.h"
#include "../protocol/tcp_shadowsocks.h"

#include "redis/anet.h"

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
    udpServer *us;
} server;

static void serverInit();
static void serverRun();
static void serverExit();

static void tcpServerInit();
static void tcpServerExit();

static tcpServer *tcpServerNew();
static void tcpServerFree(tcpServer *server);
static void tcpOnAccept(void *data);

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

static void udpServerInit();
static void udpServerExit();

static int udpServerHookProcess(void *data);
static int udpRemoteHookProcess(void *data);

static server s;
module *app = (module *)&s;

int main(int argc, char *argv[]) {
    moduleHook hook = {
        .init = serverInit,
        .run = serverRun,
        .exit = serverExit,
    };

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

    if (s.ts) LOGN("TCP server listen at: %s", s.ts->ln->addrinfo);

    if (s.us && anetFormatSock(s.us->fd, addr_info, ADDR_INFO_STR_LEN) > 0) LOGN("UDP server read at: %s", addr_info);
}

static void serverExit() {
    tcpServerExit();
    udpServerExit();
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
    tcpListener *ln = tcpListen(err, app->el, app->config->remote_addr, app->config->remote_port, server, tcpOnAccept);
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

static void tcpOnAccept(void *data) {
    tcpServer *server = data;
    tcpClient *client = tcpClientNew(server);
    if (client) {
        tcpConn *conn = client->conn;
        LOGD("TCP server accepted client %s", conn->addrinfo_peer);
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
    client->conn = (tcpConn *)tcpShadowsocksConnNew(conn, app->crypto);
    client->server = server;

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
    tcpShadowsocksConn *conn_client = (tcpShadowsocksConn *)client->conn;

    if (!client->remote) {
        int nread = TCP_READ(client->conn, client->conn->rbuf, client->conn->rbuf_len);
        if (nread <= 0) return;

        client->remote = tcpRemoteNew(client);
        if (!client->remote) {
            tcpConnectionFree(client);
            return;
        }

        // Rewind rbuffer
        int rbuf_off = conn_client->addrbuf_dest->len;
        int rbuf_len = nread - rbuf_off;

        if (rbuf_len > 0) {
            memmove(client->conn->rbuf, client->conn->rbuf + rbuf_off, rbuf_len);
            client->conn->rbuf_off += rbuf_len;
        }

        tcpSetTimeout(client->conn, -1);
        DEL_EVENT_READ(client->conn);
        DEL_EVENT_WRITE(client->conn);
    } else {
        tcpPipe(client->conn, remote->conn);
    }
}

static void tcpClientOnClose(void *data) {
    tcpClient *client = data;
    tcpShadowsocksConn *conn = (tcpShadowsocksConn *)client->conn;

    LOGD("TCP client %s closed connection", conn->addrinfo_dest);

    tcpConnectionFree(client);
}

static void tcpClientOnError(void *data) {
    tcpClient *client = data;
    tcpRemote *remote = client->remote;
    tcpShadowsocksConn *conn = (tcpShadowsocksConn *)client->conn;

    LOGW("TCP client %s pipe error: %s",
         conn->state == SHADOWSOCKS_STATE_STREAM ? conn->addrinfo_dest : client->conn->addrinfo_peer,
         client->conn->err != 0 ? client->conn->errstr : remote->conn->errstr);
}

static void tcpClientOnTimeout(void *data) {
    tcpClient *client = data;
    tcpConn *conn = client->conn;

    LOGI("TCP client %s read timeout", conn->addrinfo_peer);
}

static tcpRemote *tcpRemoteNew(tcpClient *client) {
    tcpRemote *remote = xs_calloc(sizeof(*remote));
    if (!remote) {
        LOGE("TCP remote is NULL, please check the memory");
        return NULL;
    }

    tcpShadowsocksConn *conn_client = (tcpShadowsocksConn *)client->conn;
    tcpConn *conn_remote;

    char err[XS_ERR_LEN];
    char host[HOSTNAME_MAX_LEN];
    int host_len = sizeof(host);
    int port;

    socks5AddrParse(conn_client->addrbuf_dest->data, conn_client->addrbuf_dest->len,
                    NULL, host, &host_len, &port);
    conn_remote = tcpConnect(err, app->el, host, port, app->config->timeout, remote);
    if (!conn_remote) {
        LOGW("TCP remote %s connect error: %s", err, conn_client->addrinfo_dest);
        tcpRemoteFree(remote);
        return NULL;
    }
    remote->client = client;
    remote->conn = (tcpConn *)tcpRawConnNew(conn_remote);

    TCP_ON_READ(remote->conn, tcpRemoteOnRead);
    TCP_ON_CONNECT(remote->conn, tcpRemoteOnConnect);
    TCP_ON_CLOSE(remote->conn, tcpRemoteOnClose);
    TCP_ON_ERROR(remote->conn, tcpRemoteOnError);
    TCP_ON_TIMEOUT(remote->conn, tcpRemoteOnTimeout);

    LOGD("TCP remote %s is connecting ...", conn_client->addrinfo_dest);
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
    tcpShadowsocksConn *conn_client = (tcpShadowsocksConn *)client->conn;
    char *addrinfo = conn_client->addrinfo_dest;

    if (status == TCP_ERR) {
        LOGW("TCP remote %s connect error: %s", addrinfo, remote->conn->errstr);
        return;
    }
    LOGD("TCP remote %s connect success", addrinfo);

    // Write shadowsocks client handshake left buffer
    if (client->conn->rbuf_off) {
        int nwrite = TCP_WRITE(remote->conn, client->conn->rbuf, client->conn->rbuf_off);
        if (nwrite < client->conn->rbuf_off) {
            tcpConnectionFree(client);
            return;
        }
        client->conn->rbuf_off = 0;
    }

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
    tcpShadowsocksConn *conn_client = (tcpShadowsocksConn *)client->conn;

    LOGD("TCP remote %s closed connection", conn_client->addrinfo_dest);

    tcpConnectionFree(client);
}

static void tcpRemoteOnError(void *data) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;
    tcpShadowsocksConn *conn_client = (tcpShadowsocksConn *)client->conn;

    LOGW("TCP remote %s pipe error: %s", conn_client->addrinfo_dest,
         client->conn->err != 0 ? client->conn->errstr : remote->conn->errstr);
}

static void tcpRemoteOnTimeout(void *data) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;
    tcpShadowsocksConn *conn_client = (tcpShadowsocksConn *)client->conn;

    char *addr_info = conn_client->addrinfo_dest;

    if (tcpIsConnected(remote->conn))
        LOGI("TCP remote %s read timeout", addr_info);
    else
        LOGW("TCP remote %s connect timeout", addr_info);
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

    if ((raddr_len = socks5AddrParse(client->buf.data, client->buf.len, NULL, rip, &rip_len, &rport)) == -1) {
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
