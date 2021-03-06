/*
 * This file is part of xsocks, a lightweight proxy tool for science online.
 *
 * Copyright (C) 2019 XJP09_HK <jianping_xie@aliyun.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "module_tcp.h"
#include "module.h"

#include "lib/protocol/raw.h"
#include "lib/protocol/tcp_shadowsocks.h"
#include "lib/protocol/tcp_socks5.h"

static tcpConn *tcpConnNew(int type, tcpConn *conn);

static void tcpClientFree(tcpClient *client);
static void tcpRemoteFree(tcpRemote *remote);

static void tcpClientOnClose(void *data);
static void tcpClientOnError(void *data);
static void tcpClientOnTimeout(void *data);

static void tcpRemoteOnRead(void *data);
static void tcpRemoteOnClose(void *data);
static void tcpRemoteOnError(void *data);
static void tcpRemoteOnTimeout(void *data);

tcpServer *tcpServerNew(char *host, int port, tcpEventHandler onAccept) {
    tcpServer *server;

    server = xs_calloc(sizeof(*server));
    if (!server) {
        LOGW("TCP server is NULL, please check the memory");
        return NULL;
    }

    char err[XS_ERR_LEN];
    tcpListener *ln = tcpListen(err, app->el, host, port, server, onAccept);
    if (!ln) {
        LOGE(err);
        tcpServerFree(server);
        return NULL;
    }
    server->ln = ln;

    return server;
}

void tcpServerFree(tcpServer *server) {
    if (!server) return;

    CONN_CLOSE(server->ln);
    xs_free(server);
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

static tcpConn *tcpConnNew(int type, tcpConn *conn) {
    switch (type) {
        case CONN_TYPE_SHADOWSOCKS: return (tcpConn *)tcpShadowsocksConnNew(conn, app->crypto);
        case CONN_TYPE_RAW: return (tcpConn *)tcpRawConnNew(conn);
        case CONN_TYPE_SOCKS5: return (tcpConn *)tcpSocks5ConnNew(conn);
        default: return conn;
    }
}

tcpClient *tcpClientNew(tcpServer *server, int type, tcpEventHandler onRead) {
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
    client->conn = tcpConnNew(type, conn);
    client->server = server;

    CONN_ON_READ(client->conn, onRead);
    CONN_ON_CLOSE(client->conn, tcpClientOnClose);
    CONN_ON_ERROR(client->conn, tcpClientOnError);
    CONN_ON_TIMEOUT(client->conn, tcpClientOnTimeout);

    ADD_EVENT_READ(client->conn);

    return client;
}

static void tcpClientFree(tcpClient *client) {
    if (!client) return;

    CONN_CLOSE(client->conn);
    xs_free(client);
}

static void tcpClientOnClose(void *data) {
    tcpClient *client = data;

    LOGD("TCP client %s closed connection", CONN_GET_ADDRINFO(client->conn));

    tcpConnectionFree(client);
}

static void tcpClientOnError(void *data) {
    tcpClient *client = data;
    tcpRemote *remote = client->remote;

    LOGW("TCP client %s pipe error: %s", CONN_GET_ADDRINFO(client->conn),
         client->conn->err != 0 ? client->conn->errstr : remote->conn->errstr);
}

static void tcpClientOnTimeout(void *data) {
    tcpClient *client = data;

    LOGI("TCP client %s read timeout", CONN_GET_ADDRINFO(client->conn));
}

tcpRemote *tcpRemoteNew(tcpClient *client, int type, char *host, int port,
                        tcpConnectHandler onConnect) {
    tcpRemote *remote;
    tcpConn *conn;
    char err[XS_ERR_LEN];

    remote = xs_calloc(sizeof(*remote));
    if (!remote) {
        LOGE("TCP remote is NULL, please check the memory");
        return NULL;
    }

    conn = tcpConnect(err, app->el, host, port, app->config->timeout, remote);
    if (!conn) {
        LOGW("TCP remote %s connect error: %s", CONN_GET_ADDRINFO(client->conn), err);
        tcpRemoteFree(remote);
        return NULL;
    }
    remote->client = client;
    remote->conn = tcpConnNew(type, conn);

    CONN_ON_CONNECT(remote->conn, onConnect);
    CONN_ON_READ(remote->conn, tcpRemoteOnRead);
    CONN_ON_CLOSE(remote->conn, tcpRemoteOnClose);
    CONN_ON_ERROR(remote->conn, tcpRemoteOnError);
    CONN_ON_TIMEOUT(remote->conn, tcpRemoteOnTimeout);

    LOGD("TCP remote %s is connecting ...", CONN_GET_ADDRINFO(client->conn));
    LOGD("TCP remote current count: %d", ++client->server->remote_count);

    // Prepare remote connect
    client->remote = remote;
    tcpSetTimeout(client->conn, -1);
    DEL_EVENT_READ(client->conn);

    return remote;
}

static void tcpRemoteFree(tcpRemote *remote) {
    if (!remote) return;

    CONN_CLOSE(remote->conn);
    xs_free(remote);
}

static void tcpRemoteOnRead(void *data) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;

    tcpPipe(remote->conn, client->conn);
}

static void tcpRemoteOnClose(void *data) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;

    LOGD("TCP remote %s closed connection", CONN_GET_ADDRINFO(client->conn));

    tcpConnectionFree(client);
}

static void tcpRemoteOnError(void *data) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;

    LOGW("TCP remote %s pipe error: %s", CONN_GET_ADDRINFO(client->conn),
         client->conn->err != 0 ? client->conn->errstr : remote->conn->errstr);
}

static void tcpRemoteOnTimeout(void *data) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;
    char *addr_info = CONN_GET_ADDRINFO(client->conn);

    if (tcpIsConnected(remote->conn))
        LOGI("TCP remote %s read timeout", addr_info);
    else
        LOGW("TCP remote %s connect timeout", addr_info);
}
