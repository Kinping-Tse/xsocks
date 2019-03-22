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

#ifndef __PROTOCOL_TCP_SHADOWSOCKS_H
#define __PROTOCOL_TCP_SHADOWSOCKS_H

#include "shadowsocks.h"
#include "tcp.h"

enum {
    SHADOWSOCKS_STATE_INIT = 0,
    SHADOWSOCKS_STATE_HANDSHAKE,
    SHADOWSOCKS_STATE_STREAM,
};

typedef struct tcpShadowsocksConn {
    tcpConn conn;
    int state;
    buffer_t *tmp_buf;
    int tmp_buf_off;
    buffer_t *addrbuf_dest;
    char addrinfo_dest[ADDR_INFO_STR_LEN];
    crypto_t *crypto;
    cipher_ctx_t *e_ctx;
    cipher_ctx_t *d_ctx;
} tcpShadowsocksConn;

tcpShadowsocksConn *tcpShadowsocksConnNew(tcpConn *conn, crypto_t *crypto);
int tcpShadowsocksConnInit(tcpShadowsocksConn *conn, char *host, int port);

#endif /* __PROTOCOL_TCP_SHADOWSOCKS_H */
