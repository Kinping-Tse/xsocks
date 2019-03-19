
#ifndef __PROTOCOL_UDP_SHADOWSOCKS_H
#define __PROTOCOL_UDP_SHADOWSOCKS_H

#include "shadowsocks.h"
#include "udp.h"

typedef struct udpShadowsocksConn {
    udpConn conn;
    buffer_t *addrbuf_dest;
    crypto_t *crypto;
} udpShadowsocksConn;

udpShadowsocksConn *udpShadowsocksConnNew(udpConn *conn, crypto_t *crypto);
int udpShadowsocksConnInit(udpShadowsocksConn *conn, char *host, int port);

#endif /* __PROTOCOL_UDP_SHADOWSOCKS_H */
