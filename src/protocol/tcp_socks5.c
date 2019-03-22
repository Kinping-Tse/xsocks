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

#include "tcp_socks5.h"

static void tcpSocks5ConnFree(tcpConn *conn);
static int tcpSocks5ConnRead(tcpConn *conn, char *buf, int buf_len);
static int tcpSocks5ConnWrite(tcpConn *conn, char *buf, int buf_len);
static char *tcpSocks5GetAddrinfo(tcpConn *conn);

tcpSocks5Conn *tcpSocks5ConnNew(tcpConn *conn) {
    tcpSocks5Conn *c;

    c = xs_realloc(conn, sizeof(*c));
    if (!c) return c;

    conn = &c->conn;

    c->state = SOCKS5_STATE_INIT;
    c->flags = SOCKS5_FLAG_SERVER;

    conn->read = tcpSocks5ConnRead;
    conn->write = tcpSocks5ConnWrite;
    conn->close = tcpSocks5ConnFree;
    conn->getAddrinfo = tcpSocks5GetAddrinfo;

    tcpInit(conn);

    return c;
}

int tcpSocks5ConnInit(tcpSocks5Conn *conn, char *host, int port) {
    conn->addrbuf_dest = socks5AddrInit(NULL, host, port);
    anetFormatAddr(conn->addrinfo_dest, ADDR_INFO_STR_LEN, host, port);

    return TCP_OK;
}

static char *tcpSocks5GetAddrinfo(tcpConn *conn) {
    tcpSocks5Conn *c = (tcpSocks5Conn *)conn;
    return c->state == SOCKS5_STATE_STREAM ? c->addrinfo_dest : tcpGetAddrinfo(conn);
}

static void tcpSocks5ConnFree(tcpConn *conn) {
    tcpSocks5Conn *c = (tcpSocks5Conn *)conn;
    if (!c) return;

    sdsfree(c->addrbuf_dest);

    tcpClose(conn);
}

static int tcpSocks5ConnRead(tcpConn *conn, char *buf, int buf_len) {
    tcpSocks5Conn *c = (tcpSocks5Conn *)conn;
    int nread;

    if (c->flags & SOCKS5_FLAG_CLIENT) {
        conn->err = ERROR_SOCKS5_NOT_SUPPORTED;
        xs_error(conn->errstr, "Socks5 client is not supported yet");
        goto error;
    }

    nread = tcpRead(conn, buf, buf_len);
    if (nread <= 0) return nread;

    if (c->state == SOCKS5_STATE_INIT) {
        socks5AuthReq *auth_req = (socks5AuthReq *)buf;
        int auth_len = sizeof(*auth_req) + auth_req->nmethods;
        int method = METHOD_UNACCEPTABLE;

        if (nread < auth_len || auth_req->ver != SVERSION) {
            conn->err = ERROR_SOCKS5_AUTH;
            xs_error(conn->errstr, "Socks5 authenticates buffer or version error");
            goto error;
        }

        // Only support noauth method now!!!
        for (int i = 0; i < auth_req->nmethods; i++) {
            if (auth_req->methods[i] == METHOD_NOAUTH) {
                method = METHOD_NOAUTH;
                break;
            }
        }
        if (method == METHOD_UNACCEPTABLE) {
            conn->err = ERROR_SOCKS5_AUTH;
            xs_error(conn->errstr, "Socks5 authenticates not support method");
            goto error;
        }
    } else if (c->state == SOCKS5_STATE_HANDSHAKE) {
        socks5Req *request = (socks5Req *)buf;
        int request_len = sizeof(*request);

        char host[HOSTNAME_MAX_LEN];
        int host_len = HOSTNAME_MAX_LEN;
        int port;

        if (nread < request_len || request->ver != SVERSION) {
            conn->err = ERROR_SOCKS5_HANDSHAKE;
            xs_error(conn->errstr, "Socks5 request buffer or version error");
            goto error;
        }

        if (request->cmd != SOCKS5_CMD_CONNECT) {
            conn->err = ERROR_SOCKS5_HANDSHAKE;
            xs_error(conn->errstr, "Socks5 request unsupported cmd: %d", request->cmd);
            goto error;
        }

        if (socks5AddrParse(buf + request_len - 1, nread - request_len + 1,
                            NULL, host, &host_len, &port) == SOCKS5_ERR) {
            conn->err = ERROR_SOCKS5_HANDSHAKE;
            xs_error(conn->errstr, "Socks5 request addr buffer error");
            goto error;
        }

        tcpSocks5ConnInit(c, host, port);
    }

    return nread;

error:
    FIRE_ERROR(conn);
    FIRE_CLOSE(conn);
    return TCP_ERR;
}

static int tcpSocks5ConnWrite(tcpConn *conn, char *buf, int buf_len) {
    tcpSocks5Conn *c = (tcpSocks5Conn *)conn;
    int nwrite;

    if (c->flags & SOCKS5_FLAG_CLIENT) {
        conn->err = ERROR_SOCKS5_NOT_SUPPORTED;
        xs_error(conn->errstr, "Socks5 client is not supported yet");
        goto error;
    }

    if (c->state == SOCKS5_STATE_INIT) {
        socks5AuthResp auth_resp = {
            SVERSION,
            METHOD_NOAUTH,
        };
        nwrite = tcpWrite(conn, (char *)&auth_resp, sizeof(auth_resp));
        if (nwrite < 0) return nwrite;
        if (nwrite != sizeof(auth_resp)) {
            xs_error(conn->errstr, "Socks5 write auth resp error");
            goto error;
        }

        c->state = SOCKS5_STATE_HANDSHAKE;
        return nwrite;
    } else if (c->state == SOCKS5_STATE_HANDSHAKE) {
        socks5Resp resp = {
            SVERSION,
            SOCKS5_REP_SUCCEEDED,
            0,
            SOCKS5_ATYP_IPV4,
        };

        sockAddrIpV4 sock_addr;
        bzero(&sock_addr, sizeof(sock_addr));

        char resp_buf[NET_IOBUF_LEN];
        memcpy(resp_buf, &resp, sizeof(resp));
        memcpy(resp_buf + sizeof(resp), &sock_addr.sin_addr, sizeof(sock_addr.sin_addr));
        memcpy(resp_buf + sizeof(resp) + sizeof(sock_addr.sin_addr),
               &sock_addr.sin_port,
               sizeof(sock_addr.sin_port));

        int reply_size = sizeof(resp) + sizeof(sock_addr.sin_addr) + sizeof(sock_addr.sin_port);

        nwrite = tcpWrite(conn, resp_buf, reply_size);
        if (nwrite < 0) return nwrite;
        if (nwrite != reply_size) {
            xs_error(conn->errstr, "Socks5 write resp error");
            goto error;
        }

        c->state = SOCKS5_STATE_STREAM;

        anetDisableTcpNoDelay(NULL, conn->fd);

        return nwrite;
    }

    return tcpWrite(conn, buf, buf_len);

error:
    FIRE_ERROR(conn);
    FIRE_CLOSE(conn);
    return TCP_ERR;
}
