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

#ifndef __MODULE_UDP_H
#define __MODULE_UDP_H

#include "../protocol/udp.h"

typedef struct udpServer {
    udpConn *conn;
    int remote_count;
} udpServer;

typedef struct udpClient {
    udpServer *server;
    struct udpRemote *remote;
    sockAddrEx sa_client;
    sockAddrEx sa_remote;
} udpClient;

typedef struct udpRemote {
    udpConn *conn;
    udpClient *client;
} udpRemote;

udpServer *udpServerNew(char *host, int port, int type, udpEventHandler onRead);
void udpServerFree(udpServer *server);

udpClient *udpClientNew(udpServer *server);
udpRemote *udpRemoteNew(udpClient *client, int type, char *host, int port);
void udpConnectionFree(udpClient *client);

#endif /* __MODULE_UDP_H */
