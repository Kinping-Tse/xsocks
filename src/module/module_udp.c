
#include "module_udp.h"
#include "module.h"

#include "../protocol/udp_shadowsocks.h"
#include "../protocol/raw.h"

static udpConn *udpConnNew(int type, udpConn *conn);

static void udpClientFree(udpClient *client);
static void udpRemoteFree(udpRemote *remote);

static void udpRemoteOnRead(void *data);
static void udpRemoteOnClose(void *data);
static void udpRemoteOnError(void *data);
static void udpRemoteOnTimeout(void *data);

udpServer *udpServerNew(char *host, int port, int type, udpEventHandler onRead) {
    udpServer *server;
    udpConn *conn;
    char err[XS_ERR_LEN];

    if (CALLOC_P(server) == NULL) {
        LOGW("UDP server is NULL, please check the memory");
        return NULL;
    }

    if ((conn = udpCreate(err, app->el, host, port, app->config->ipv6_first,
                          app->config->timeout, server)) == NULL) {
        LOGW("UDP server create error: %s", err);
        udpServerFree(server);
        return NULL;
    }
    server->conn = udpConnNew(type, conn);

    CONN_ON_READ(server->conn, onRead);

    return server;
}

void udpServerFree(udpServer *server) {
    if (!server) return;

    CONN_CLOSE(server->conn);
    xs_free(server);
}

static udpConn *udpConnNew(int type, udpConn *conn) {
    switch (type) {
        case CONN_TYPE_SHADOWSOCKS: return (udpConn *)udpShadowsocksConnNew(conn, app->crypto);
        case CONN_TYPE_RAW: return (udpConn *)udpRawConnNew(conn);
        default: return conn;
    }
}

udpClient *udpClientNew(udpServer *server) {
    udpClient *client;

    if (CALLOC_P(client) == NULL) {
        LOGW("UDP client is NULL, please check the memory");
        return NULL;
    }

    client->server = server;
    netSockAddrExInit(&client->sa_client);
    netSockAddrExInit(&client->sa_remote);

    return client;
}

udpRemote *udpRemoteNew(udpClient *client, int type, char *host, int port) {
    udpRemote *remote;
    udpConn *conn;
    char err[XS_ERR_LEN];

    if (CALLOC_P(remote) == NULL) {
        LOGW("UDP remote is NULL, please check the memory");
        return NULL;
    }

    conn = udpCreate(err, app->el, NULL, 0, app->config->ipv6_first, app->config->timeout, remote);
    if (!conn) {
        LOGW("UDP remote create error: %s", err);
        udpRemoteFree(remote);
        return NULL;
    }
    remote->client = client;
    remote->conn = udpConnNew(type, conn);

    CONN_ON_READ(remote->conn, udpRemoteOnRead);
    CONN_ON_CLOSE(remote->conn, udpRemoteOnClose);
    CONN_ON_ERROR(remote->conn, udpRemoteOnError);
    CONN_ON_TIMEOUT(remote->conn, udpRemoteOnTimeout);

    client->remote = remote;
    if (netUdpGetSockAddrEx(err, host, port, app->config->ipv6_first, &client->sa_remote) == NET_ERR) {
        LOGW("Get UDP remote sockaddr error: %s", err);
        udpRemoteFree(remote);
        return NULL;
    }

    LOGD("UDP remote current count: %d", ++client->server->remote_count);

    return remote;
}

void udpConnectionFree(udpClient *client) {
    if (!client) return;

    udpServer *server = client->server;

    if (client->remote) server->remote_count--;
    LOGD("UDP remote current count: %d", server->remote_count);

    udpRemoteFree(client->remote);
    udpClientFree(client);
}

static void udpClientFree(udpClient *client) {
    if (!client) return;

    FREE_P(client);
}

static void udpRemoteFree(udpRemote *remote) {
    if (!remote) return;

    CONN_CLOSE(remote->conn);
    FREE_P(remote);
}

static void udpRemoteOnRead(void *data) {
    udpRemote *remote = data;
    udpClient *client = remote->client;

    char buf[NET_IOBUF_LEN];
    int buf_len = sizeof(buf);
    int nread;

    char rip[HOSTNAME_MAX_LEN];
    int rip_len = sizeof(rip);
    int rport;

    nread = UDP_READ(remote->conn, buf, buf_len, NULL);
    if (nread == UDP_ERR) return;

    if (netIpPresentBySockAddr(NULL, rip, rip_len, &rport, &client->sa_remote) == NET_OK)
        LOGD("UDP remote read from %s:%d", rip, rport);

    UDP_WRITE(client->server->conn, buf, nread, &client->sa_client);

    udpConnectionFree(client);
}

static void udpRemoteOnClose(void *data) {
    udpRemote *remote = data;

    LOGD("UDP remote %s closed connection", CONN_GET_ADDRINFO(remote->conn));

    udpConnectionFree(remote->client);
}

static void udpRemoteOnError(void *data) {
    udpRemote *remote = data;
    udpConn *conn = remote->conn;

    LOGW("UDP remote %s %s error: %s", CONN_GET_ADDRINFO(conn),
         conn->err == UDP_ERROR_READ ? "read" : "write", conn->errstr);
}

static void udpRemoteOnTimeout(void *data) {
    udpRemote *remote = data;

    LOGI("UDP remote %s read timeout", CONN_GET_ADDRINFO(remote->conn));
}
