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

#ifndef __MODULE_TCP_H
#define __MODULE_TCP_H

#include "lib/protocol/tcp.h"

typedef struct tcpServer {
    tcpListener *ln;
    int client_count;
    int remote_count;
} tcpServer;

typedef struct tcpClient {
    int type;
    tcpConn *conn;
    tcpServer *server;
    struct tcpRemote *remote;
} tcpClient;

typedef struct tcpRemote {
    int type;
    tcpConn *conn;
    tcpClient *client;
} tcpRemote;

tcpServer *tcpServerNew(char *host, int port, tcpEventHandler onAccept);
void tcpServerFree(tcpServer *server);

tcpClient *tcpClientNew(tcpServer *server, int type, tcpEventHandler onRead);
tcpRemote *tcpRemoteNew(tcpClient *client, int type, char *host, int port,
                        tcpConnectHandler onConnect);
void tcpConnectionFree(tcpClient *client);

#endif /* __MODULE_TCP_H */
