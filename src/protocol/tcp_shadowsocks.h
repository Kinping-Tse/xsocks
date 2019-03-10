
#ifndef __PROTOCOL_TCP_SHADOWSOCKS_H
#define __PROTOCOL_TCP_SHADOWSOCKS_H

#include "tcp.h"

#include "shadowsocks-libev/crypto.h"

enum {
    SHADOWSOCKS_STATE_INIT = 1<<0,
    SHADOWSOCKS_STATE_HANDSHAKE = 1<<1,
    SHADOWSOCKS_STATE_STREAM = 1<<2,
};

enum {
    ERROR_SHADOWSOCKS_ENCRYPT = 1000,
    ERROR_SHADOWSOCKS_DECRYPT,
};

typedef struct tcpShadowsocksConn {
    tcpConn conn;
    int state;
    buffer_t *dest_addr;
    crypto_t *crypto;
    cipher_ctx_t *e_ctx;
    cipher_ctx_t *d_ctx;
} tcpShadowsocksConn;

tcpShadowsocksConn *tcpShadowsocksConnNew(tcpConn *conn, crypto_t *crypto, char *host, int port);

#endif /* __PROTOCOL_TCP_SHADOWSOCKS_H */
