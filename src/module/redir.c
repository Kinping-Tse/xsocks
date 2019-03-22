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

#include "module.h"
#include "module_tcp.h"

#include "../protocol/tcp_shadowsocks.h"
#include "../protocol/tcp_socks5.h"

typedef struct server {
    module mod;
    tcpServer *ts;
} server;

static void redirInit();
static void redirRun();
static void redirExit();

static void tcpServerOnAccept(void *data);
static void tcpClientOnRead(void *data);
static void tcpRemoteOnConnect(void *data, int status);

static server s;
module *app = (module *)&s;

int main(int argc, char *argv[]) {
    moduleHook hook = {
        .init = redirInit,
        .run = redirRun,
        .exit = redirExit,
    };

    return moduleMain(MODULE_REDIR, hook, app, argc, argv);
}

static void redirInit() {
    getLogger()->syslog_ident = "xs-redir";
}

static void redirRun() {
    if (app->config->mode & MODE_TCP_ONLY)
        s.ts = tcpServerNew(app->config->local_addr, app->config->local_port, tcpServerOnAccept);

    if (app->config->mode & MODE_UDP_ONLY) {
        LOGW("Only support TCP now!");
        LOGW("UDP mode is not working!");
    }

    if (!s.ts) exit(EXIT_ERR);
    if (s.ts) LOGN("TCP server listen at: %s", s.ts->ln->addrinfo);
}

static void redirExit() {
    tcpServerFree(s.ts);
}

static void tcpServerOnAccept(void *data) {
    tcpServer *server = data;
    tcpClient *client;
    tcpRemote *remote;

    if ((client = tcpClientNew(server, CONN_TYPE_RAW, tcpClientOnRead)) == NULL) return;

    LOGD("TCP server accepted client %s", CONN_GET_ADDRINFO(client->conn));
    LOGD("TCP client current count: %d", ++server->client_count);

    remote = tcpRemoteNew(client, CONN_TYPE_SHADOWSOCKS, app->config->remote_addr,
                          app->config->remote_port, tcpRemoteOnConnect);
    if (!remote) goto error;

    char host[HOSTNAME_MAX_LEN];
    int host_len = sizeof(host);
    int port;
    char err[NET_ERR_LEN];
    sockAddrEx sa;

    if (netTcpGetDestSockAddr(err, client->conn->fd, app->config->ipv6_first, &sa) == NET_ERR) {
        LOGW("TCP client get dest sockaddr error: %s", err);
        goto error;
    }
    if (netIpPresentBySockAddr(err, host, host_len, &port, &sa) == NET_ERR) {
        LOGW("TCP client get dest addr error: %s", err);
        goto error;
    }
    LOGD("TCP client proxy dest addr: %s:%d", host, port);

    tcpShadowsocksConnInit((tcpShadowsocksConn *)remote->conn, host, port);

    return;

error:
    tcpConnectionFree(client);
}

static void tcpClientOnRead(void *data) {
    tcpClient *client = data;
    tcpRemote *remote = client->remote;

    tcpPipe(client->conn, remote->conn);
}

static void tcpRemoteOnConnect(void *data, int status) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;
    char *addrinfo = CONN_GET_ADDRINFO(client->conn);

    if (status == TCP_ERR) {
        LOGW("TCP remote %s connect error: %s", addrinfo, remote->conn->errstr);
        return;
    }
    LOGD("TCP remote %s connect success", addrinfo);

    // Prepare pipe
    ADD_EVENT_READ(remote->conn);
    ADD_EVENT_READ(client->conn);
}
