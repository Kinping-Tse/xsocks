
#ifndef __PROTOCOL_TCP_SHADOWSOCKS_H
#define __PROTOCOL_TCP_SHADOWSOCKS_H

#include "tcp.h"

#include "shadowsocks-libev/crypto.h"

enum {
    SHADOWSOCKS_STATE_INIT = 0,
    SHADOWSOCKS_STATE_HANDSHAKE,
    SHADOWSOCKS_STATE_STREAM,
};

enum {
    ERROR_SHADOWSOCKS_ENCRYPT = 10000,
    ERROR_SHADOWSOCKS_DECRYPT,
    ERROR_SHADOWSOCKS_SOCKS5,
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
