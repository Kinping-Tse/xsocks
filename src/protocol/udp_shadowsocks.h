
#ifndef __PROTOCOL_UDP_SHADOWSOCKS_H
#define __PROTOCOL_UDP_SHADOWSOCKS_H

#include "udp.h"

#include "shadowsocks-libev/crypto.h"

enum {
    ERROR_SHADOWSOCKS_ENCRYPT = 10000,
    ERROR_SHADOWSOCKS_DECRYPT,
};

typedef struct udpShadowsocksConn {
    udpConn conn;
    // int state;
    // buffer_t *tmp_buf;
    // int tmp_buf_off;
    buffer_t *addrbuf_dest;
    // char addrinfo_dest[ADDR_INFO_STR_LEN];
    crypto_t *crypto;
    // cipher_ctx_t *e_ctx;
    // cipher_ctx_t *d_ctx;
} udpShadowsocksConn;

udpShadowsocksConn *udpShadowsocksConnNew(udpConn *conn, crypto_t *crypto);
// int udpShadowsocksConnInit(tcpShadowsocksConn *conn, char *host, int port);

#endif /* __PROTOCOL_TCP_SHADOWSOCKS_H */
