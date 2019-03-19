
#ifndef __PROTOCOL_TCP_H
#define __PROTOCOL_TCP_H

#include "proxy.h"

enum {
    TCP_OK = 0,
    TCP_ERR = -1,

    TCP_FLAG_INIT = 1<<0,
    TCP_FLAG_CONNECTING = 1<<1,
    TCP_FLAG_CONNECTED = 1<<2,
    TCP_FLAG_LISTEN = 1<<3,
    TCP_FLAG_PIPE = 1<<4,
    TCP_FLAG_CLOSED = 1<<5,

    TCP_ERROR_READ = 10000,
    TCP_ERROR_WRITE = 10001,
    TCP_ERROR_TIMEOUT = 10002,
    TCP_ERROR_CLOSED = 10003,
    TCP_ERROR_CONNECT = 10004,
};

struct tcpConn;

typedef void (*tcpEventHandler)(void *data);
typedef int (*tcpIoHandler)(struct tcpConn *conn, char *buf, int buf_len);
typedef void (*tcpConnectHandler)(void *data, int status);

typedef struct tcpListener {
    int fd;
    int flags;
    eventLoop *el;
    event *re;
    tcpEventHandler onAccept;
    void (*close)(struct tcpListener *c);
    char addrinfo[ADDR_INFO_STR_LEN];
    void *data;
} tcpListener;

typedef struct tcpConn {
    int fd;
    int flags;
    int timeout;
    eventLoop *el;
    event *re;
    event *we;
    event *te;
    tcpEventHandler onRead;
    tcpEventHandler onWrite;
    tcpEventHandler onTimeout;
    tcpEventHandler onClose;
    tcpEventHandler onError;
    tcpConnectHandler onConnect;
    tcpIoHandler read;
    tcpIoHandler write;
    void (*close)(struct tcpConn *c);
    char *(*getAddrinfo)(struct tcpConn *c);
    void *data;
    char addrinfo[ADDR_INFO_STR_LEN];
    char addrinfo_peer[ADDR_INFO_STR_LEN];
    sockAddrEx rsa; // For connect check
    char *rbuf;
    int rbuf_len;
    int rbuf_off;
    char *wbuf;
    int wbuf_len;
    int err;
    char errstr[XS_ERR_LEN];
    struct tcpConn *pipe;
} tcpConn;

tcpListener *tcpListen(char *err, eventLoop *el, char *host, int port, void *data,
                       tcpEventHandler onAccept);

tcpConn *tcpAccept(char *err, eventLoop *el, int fd, int timeout, void *data);
tcpConn *tcpConnect(char *err, eventLoop *el, char *host, int port, int timeout, void *data);
int tcpSetTimeout(tcpConn *c, int timeout);
int tcpIsConnected(tcpConn *c);
int tcpPipe(tcpConn *src, tcpConn *dst);

int tcpInit(tcpConn *c);
void tcpClose(tcpConn *c);
int tcpRead(tcpConn *c, char *buf, int buf_len);
int tcpWrite(tcpConn *c, char *buf, int buf_len);
char *tcpGetAddrinfo(tcpConn *c);

#endif /* __PROTOCOL_TCP_H */
