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

#include "module/module.h"
#include "module/module_tcp.h"

#include "lib/protocol/tcp_shadowsocks.h"
#include "lib/protocol/tcp_socks5.h"

typedef struct server {
    module mod;
    tcpServer *ts;
} server;

static void localInit();
static void localRun();
static void localExit();

static void tcpServerOnAccept(void *data);
static void tcpClientOnRead(void *data);
static void tcpRemoteOnConnect(void *data, int status);

static int isBypass(char *ip);

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
    if (app->config->mode & MODE_TCP_ONLY)
        s.ts = tcpServerNew(app->config->local_addr, app->config->local_port, tcpServerOnAccept);

    if (app->config->mode & MODE_UDP_ONLY) {
        LOGW("Only support TCP now!");
        LOGW("UDP mode is not working!");
    }

    if (!s.ts) exit(EXIT_ERR);
    if (s.ts) LOGN("TCP server listen at: %s", s.ts->ln->addrinfo);
}

static void localExit() {
    tcpServerFree(s.ts);
}

static void tcpServerOnAccept(void *data) {
    tcpServer *server = data;
    tcpClient *client = tcpClientNew(server, CONN_TYPE_SOCKS5, tcpClientOnRead);
    if (client) {
        LOGD("TCP server accepted client %s", CONN_GET_ADDRINFO(client->conn));
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
        int bypass = 0;
        int host_match = 0;

        char host[HOSTNAME_MAX_LEN];
        int host_len = sizeof(host);
        char ip[NET_IP_MAX_STR_LEN];
        int ip_len = sizeof(ip);
        int port;
        int atyp;

        socks5AddrParse(conn_client->addrbuf_dest, sdslen(conn_client->addrbuf_dest), &atyp, host,
                        &host_len, &port);

        if (app->config->acl) {
            if (atyp == SOCKS5_ATYP_DOMAIN) host_match = acl_match_host(host);

            if (host_match > 0) bypass = 1;
            else if (host_match < 0) bypass = 0;
            else {
                int resolved = 0;

                if (atyp != SOCKS5_ATYP_DOMAIN) {
                    memcpy(ip, host, ip_len);
                    resolved = 1;
                } else if (anetResolve(NULL, host, ip, ip_len) == ANET_OK)
                    resolved = 1;

                if (resolved) bypass = isBypass(ip);
            }
        }

        if (bypass) {
            remote = tcpRemoteNew(client, CONN_TYPE_RAW, host, port, tcpRemoteOnConnect);

            if (remote) LOGD("TCP client bypass dest addr: %s:%d", host, port);
        } else {
            remote = tcpRemoteNew(client, CONN_TYPE_SHADOWSOCKS, app->config->remote_addr,
                                  app->config->remote_port, tcpRemoteOnConnect);

            if (remote) {
                tcpShadowsocksConnInit((tcpShadowsocksConn *)remote->conn, host, port);
                LOGD("TCP client proxy dest addr: %s:%d", host, port);
            }
        }

        if (!remote) {
            tcpConnectionFree(client);
            return;
        }
    } else {
        tcpPipe(client->conn, remote->conn);
    }
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

static int isBypass(char *ip) {
    int bypass = 0;
    int ip_match = acl_match_host(ip);

    switch (get_acl_mode()) {
        case BLACK_LIST:
            if (ip_match > 0) bypass = 1;
            break;
        case WHITE_LIST:
            bypass = 1;
            if (ip_match < 0) bypass = 0;
            break;
    }

    return bypass;
}
