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

#include "lib/protocol/raw.h"

#include <getopt.h>

typedef struct server {
    char *host;
    int port;
    eventLoop *el;
    int timeout;
} server;

static server s;
static server *app = &s;

typedef struct tcpServer {
    tcpListener *ln;
    int client_count;
} tcpServer;

typedef struct tcpClient {
    tcpConn *conn;
    tcpServer *server;
} tcpClient;

static void initServer();
static void parseOptions(int argc, char *argv[]);
static void usage();

static tcpServer *tcpServerNew();
static void tcpServerFree(tcpServer *server);
static void tcpServerOnAccept(void *data);

static tcpClient *tcpClientNew(tcpServer *server);
static void tcpClientFree(tcpClient *client);
static void tcpClientOnRead(void *data);
static void tcpClientOnClose(void *data);
static void tcpClientOnError(void *data);
static void tcpClientOnTimeout(void *data);

int main(int argc, char *argv[]) {
    initServer();
    parseOptions(argc, argv);

    tcpServer *server = tcpServerNew();
    if (server == NULL) exit(EXIT_ERR);

    LOGI("Use event type: %s", eventGetApiName());
    LOGI("Use server addr: %s:%d", app->host, app->port);
    LOGI("Use timeout: %ds", app->timeout);

    eventLoopRun(app->el);

    tcpServerFree(server);

    return EXIT_OK;
}

static void initServer() {
    app->host = "127.0.0.1";
    app->timeout = 60;
    app->port = 19999;

    setupIgnoreHandlers();
    setupSigsegvHandlers();

    logger *log = getLogger();
    log->level = LOGLEVEL_DEBUG;
    log->color_enabled = 1;
    log->file_line_enabled = 0;
    log->syslog_ident = "xs-benchmark-server";

    app->el = eventLoopNew(1024*10);
}

static void parseOptions(int argc, char *argv[]) {
    int help = 0;
    int opt;

    struct option long_options[] = {
        { "help",  no_argument,  NULL, 'h' },
        { NULL,    0,            NULL, 0   },
    };

    while ((opt = getopt_long(argc, argv, "s:p:t:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 's': app->host = optarg; break;
            case 'p': app->port = atoi(optarg); break;
            case 't': app->timeout = atoi(optarg); break;
            case 'h': help = 1; break;
            case '?':
            default: help = 1; break;
        }
    }

    if (help) {
        usage();
        exit(EXIT_ERR);
    }
}

static void usage() {
    printf("Usage: xs-benchmark-server [options]\n\n"
           "Options:\n"
           " [-s <hostname>]      Server hostname (default 127.0.0.1)\n"
           " [-p <port>]          Server port (default 19999)\n"
           " [-t <timeout>]       Socket timeout (default 60)\n"
           " [-h, --help]         Print this help\n");
}

tcpServer *tcpServerNew() {
    tcpServer *server = xs_calloc(sizeof(*server));
    if (!server) {
        LOGW("TCP server is NULL, please check the memory");
        return NULL;
    }

    char err[XS_ERR_LEN];
    tcpListener *ln = tcpListen(err, app->el, app->host, app->port, server, tcpServerOnAccept);
    if (!ln) {
        LOGE(err);
        tcpServerFree(server);
        return NULL;
    }
    server->ln = ln;

    LOGN("TCP server listen at: %s", ln->addrinfo);

    return server;
}

static void tcpServerFree(tcpServer *server) {
    if (!server) return;
    CONN_CLOSE(server->ln);
    xs_free(server);
}

static void tcpServerOnAccept(void *data) {
    tcpServer *server = data;
    tcpClient *client = tcpClientNew(server);
    if (client) {
        tcpConn *conn = client->conn;
        LOGD("TCP server accepted client %s", conn->addrinfo_peer);
        LOGI("TCP client current count: %d", ++server->client_count);
    }
}

static tcpClient *tcpClientNew(tcpServer *server) {
    tcpClient *client = xs_calloc(sizeof(*client));
    if (!client) {
        LOGW("TCP client is NULL, please check the memory");
        return NULL;
    }

    char err[XS_ERR_LEN];
    tcpConn *conn = tcpAccept(err, app->el, server->ln->fd, app->timeout, client);
    if (!conn) {
        LOGW(err);
        tcpClientFree(client);
        return NULL;
    }
    client->conn = (tcpConn *)tcpRawConnNew(conn);
    client->server = server;

    CONN_ON_READ(client->conn, tcpClientOnRead);
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

static void tcpClientOnRead(void *data) {
    tcpClient *client = data;
    tcpConn *conn = client->conn;

    tcpPipe(conn, conn);
}

static void tcpClientOnClose(void *data) {
    tcpClient *client = data;

    LOGD("TCP client %s closed connection", CONN_GET_ADDRINFO(client->conn));
    LOGD("TCP client current count: %d", --client->server->client_count);

    tcpClientFree(client);
}

static void tcpClientOnError(void *data) {
    tcpClient *client = data;
    tcpConn *conn = client->conn;

    LOGW("TCP client %s pipe (%s) error: %s", CONN_GET_ADDRINFO(conn),
         conn->err == TCP_ERROR_READ ? "read" : "write", conn->errstr);
}

static void tcpClientOnTimeout(void *data) {
    tcpClient *client = data;
    tcpConn *conn = client->conn;

    LOGI("TCP client %s read timeout", conn->addrinfo_peer);
}
