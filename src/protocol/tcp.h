
#ifndef __PROTOCOL_TCP_H
#define __PROTOCOL_TCP_H

#include "proxy.h"

enum {
    TCP_OK = 0,
    TCP_ERR = -1,
    TCP_AGAIN = -2,
    TCP_FLAG_CONNECTED = 1<<0,
    TCP_FLAG_LISTEN = 1<<1,
};

typedef void (*tcpEventHandler)(void *data);
typedef int (*tcpReadHandler)(void *data, char *buf, int buf_len);
typedef int (*tcpWriteHandler)(void *data, char *buf, int buf_len);
typedef void (*tcpCloseHandler)(void *data);

typedef struct tcpListener {
    int fd;
    eventLoop *el;
    event *re;
    tcpEventHandler onAccept;
    tcpCloseHandler close;
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
    void (*onConnect)(void *data, int status);
    tcpReadHandler read;
    tcpWriteHandler write;
    tcpCloseHandler close;
    char addrinfo[ADDR_INFO_STR_LEN];
    char addrinfo_peer[ADDR_INFO_STR_LEN];
    sockAddrEx rsa;
    void *data;
    char *rbuf;
    int rbuf_len;
    int rbuf_off;
    char *wbuf;
    int wbuf_len;
    int err;
    char errstr[XS_ERR_LEN];
} tcpConn;

#define TCP_ON_ACCEPT(ln, h) ((tcpListener *)ln)->onAccept = (h)
#define TCP_L_CLOSE(ln) do { if (ln) ((tcpListener *)ln)->close(ln); } while (0)

#define TCP_ON_READ(c, h) ((tcpConn *)c)->onRead = (h)
#define TCP_ON_WRITE(c, h) ((tcpConn *)c)->onWrite = (h)
#define TCP_ON_CONNECT(c, h) ((tcpConn *)c)->onConnect = (h)
#define TCP_ON_CONN TCP_ON_CONNECT
#define TCP_ON_TIMEOUT(c, h) ((tcpConn *)c)->onTimeout = (h)
#define TCP_ON_TIME TCP_ON_TIMEOUT
#define TCP_READ(c, buf, len) ((tcpConn *)c)->read(c, buf, len)
#define TCP_WRITE(c, buf, len) ((tcpConn *)c)->write(c, buf, len)
#define TCP_CLOSE(c) do { if (c) ((tcpConn *)c)->close(c); } while (0)

tcpListener *tcpListen(char *err, eventLoop *el, char *host, int port,
                       void *data, tcpEventHandler onAccept);

tcpConn *tcpAccept(char *err, eventLoop *el, int fd, int timeout, void *data);
tcpConn *tcpConnect(char *err, eventLoop *el, char *host, int port, int timeout, void *data);
void tcpClose(tcpConn *c);

int tcpIsConnected(tcpConn *c);
int tcpRead(tcpConn *c, char *buf, int buf_len);
int tcpWrite(tcpConn *c, char *buf, int buf_len);
int tcpPipe(tcpConn *dst, tcpConn *src);

char *tcpGetAddrinfo(tcpConn *c);

#endif /* __PROTOCOL_TCP_H */
