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

#ifndef __PROTOCOL_TCP_SOCKS5_H
#define __PROTOCOL_TCP_SOCKS5_H

#include "socks5.h"
#include "tcp.h"

enum {
    SOCKS5_STATE_INIT = 0,
    SOCKS5_STATE_HANDSHAKE,
    SOCKS5_STATE_STREAM,
};

enum {
    ERROR_SOCKS5_NOT_SUPPORTED = 10000,
    ERROR_SOCKS5_AUTH,
    ERROR_SOCKS5_HANDSHAKE,
};

enum {
    SOCKS5_FLAG_SERVER = 1<<0,
    SOCKS5_FLAG_CLIENT = 1<<1,
};

typedef struct tcpSocks5Conn {
    tcpConn conn;
    int flags;
    int state;
    sds addrbuf_dest;
    char addrinfo_dest[ADDR_INFO_STR_LEN];
} tcpSocks5Conn;

tcpSocks5Conn *tcpSocks5ConnNew(tcpConn *conn);
int tcpSocks5ConnInit(tcpSocks5Conn *conn, char *host, int port);

#endif /* __PROTOCOL_TCP_SOCKS5_H */
