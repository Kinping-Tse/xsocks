
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

typedef void (*udpEventHandler)(struct udpConn *c);
typedef int (*udpIoHandler)(struct udpConn *c, char *buf, int buf_len, sockAddrEx *sa);
typedef void (*udpCloseHandler)(struct udpConn *c);

typedef struct udpConn {
    int fd;
    // int flags;
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
    udpEventHandler close;
    void *data;
    char addrinfo[ADDR_INFO_STR_LEN];
    int err;
    char errstr[XS_ERR_LEN];
    // struct udpConn *pipe;
} udpConn;

udpConn *udpCreate(char *err, eventLoop *el, char *host, int port, int timeout, void *data);
int udpInit(udpConn *c);
int udpSetTimeout(udpConn *c, int timeout);
void udpClose(udpConn *c);

int udpRead(udpConn *c, char *buf, int buf_len, sockAddrEx *sa);
int udpWrite(udpConn *c, char *buf, int buf_len, sockAddrEx *sa);

#endif /* __PROTOCOL_UDP_H */
