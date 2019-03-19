
#ifndef __PROTOCOL_UDP_H
#define __PROTOCOL_UDP_H

#include "proxy.h"

enum {
    UDP_OK = 0,
    UDP_ERR = -1,

    UDP_ERROR_READ = 10000,
    UDP_ERROR_WRITE = 10001,
    UDP_ERROR_TIMEOUT = 10002,
    UDP_ERROR_CLOSED = 10003,
};

struct udpConn;

typedef void (*udpEventHandler)(void *data);
typedef int (*udpIoHandler)(struct udpConn *conn, char *buf, int buf_len, sockAddrEx *sa);

typedef struct udpConn {
    int fd;
    int timeout;
    eventLoop *el;
    event *re;
    event *we;
    event *te;
    udpEventHandler onRead;
    udpEventHandler onTimeout;
    udpEventHandler onClose;
    udpEventHandler onError;
    udpIoHandler read;
    udpIoHandler write;
    void (*close)(struct udpConn *c);
    char *(*getAddrinfo)(struct udpConn *c);
    void *data;
    char addrinfo[ADDR_INFO_STR_LEN];
    int err;
    char errstr[XS_ERR_LEN];
} udpConn;

udpConn *udpCreate(char *err, eventLoop *el, char *host, int port, int ipv6_first, int timeout, void *data);
int udpSetTimeout(udpConn *c, int timeout);

int udpInit(udpConn *c);
void udpClose(udpConn *c);
int udpRead(udpConn *c, char *buf, int buf_len, sockAddrEx *sa);
int udpWrite(udpConn *c, char *buf, int buf_len, sockAddrEx *sa);
char *udpGetAddrinfo(udpConn *c);

#endif /* __PROTOCOL_UDP_H */
