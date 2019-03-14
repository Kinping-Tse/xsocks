
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

/**
 * Socks5 addr buffer
 *
 *    +------+----------+----------+
 *    | ATYP | DST.ADDR | DST.PORT |
 *    +------+----------+----------+
 *    |  1   | Variable |    2     |
 *    +------+----------+----------+
 */

int socks5AddrCreate(char *err, char *host, int port, char *addr_buf, int *buf_len);
int socks5AddrParse(char *addr_buf, int buf_len, int *atyp, char *host, int *host_len, int *port);
sds socks5AddrInit(char *err, char *host, int port);

#endif /* __PROTOCOL_SOCKS5_H */
