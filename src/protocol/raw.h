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

#ifndef __PROTOCOL_RAW_H
#define __PROTOCOL_RAW_H

#include "tcp.h"
#include "udp.h"

typedef struct tcpRawConn {
    tcpConn conn;
} tcpRawConn;

typedef struct udpRawConn {
    udpConn conn;
} udpRawConn;

tcpRawConn *tcpRawConnNew(tcpConn *conn);
udpRawConn *udpRawConnNew(udpConn *conn);

#endif /* __PROTOCOL_RAW_H */
