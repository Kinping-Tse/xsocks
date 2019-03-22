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

#ifndef __PROTOCOL_SOCKS5_H
#define __PROTOCOL_SOCKS5_H

#include "redis/sds.h"
#include "shadowsocks-libev/socks5.h"

enum {
    SOCKS5_OK = 0,
    SOCKS5_ERR = -1,
    SOCKS5_ADDR_MAX_LEN = 259, // (1+1+255+2)
};

typedef struct method_select_request socks5AuthReq;
typedef struct method_select_response socks5AuthResp;
typedef struct socks5_request socks5Req;
typedef struct socks5_response socks5Resp;

int socks5AddrCreate(char *err, char *host, int port, char *addr_buf, int *buf_len);
sds socks5AddrInit(char *err, char *host, int port);

int socks5AddrParse(char *addr_buf, int buf_len, int *atyp, char *host, int *host_len, int *port);

/**
 *
 * Socks5 auth req
 *
 *    +-----+----------+-----------------+
 *    | VER | NMETHODS |      METHODS    |
 *    +-----+----------+-----------------+
 *    |  1  |    1     | Variable(1-255) |
 *    +-----+----------+-----------------+
 *
 * Socks5 auth resp
 *
 *    +-----+--------+
 *    | VER | METHOD |
 *    +-----+--------+
 *    |  1  |   1    |
 *    +-----+--------+
 *
 * Socks5 request
 *
 *    +-----+-----+---------+--------------------+
 *    | VER | CMD |   RSV   | Socks5 addr buffer |
 *    +-----+---------------+--------------------+
 *    |  1  |  1  | 1(0x00) |      Variable      |
 *    +-----+-----+---------+--------------------+
 *
 * Socks5 response
 *
 *    +-----+-----+---------+--------------------+
 *    | VER | REP |   RSV   | Socks5 addr buffer |
 *    +-----+---------------+--------------------+
 *    |  1  |  1  | 1(0x00) |      Variable      |
 *    +-----+-----+---------+--------------------+
 *
 *
 * Socks5 addr buffer
 *
 *    +------+----------+----------+
 *    | ATYP | DST.ADDR | DST.PORT |
 *    +------+----------+----------+
 *    |  1   | Variable |    2     |
 *    +------+----------+----------+
 */

#endif /* __PROTOCOL_SOCKS5_H */
