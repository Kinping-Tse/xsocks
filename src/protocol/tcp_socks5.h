
#ifndef __PROTOCOL_TCP_SOCKS5_H
#define __PROTOCOL_TCP_SOCKS5_H

#include "tcp.h"
#include "socks5.h"

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
