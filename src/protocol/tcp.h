
#ifndef __PROTOCOL_TCP_H
#define __PROTOCOL_TCP_H

#include "proxy.h"

enum {
    TCP_OK = 0,
    TCP_ERR = -1,
    TCP_AGAIN = -2,
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

typedef void (*tcpEventHandler)(void *data);
typedef int (*tcpReadHandler)(void *data, char *buf, int buf_len);
typedef int (*tcpWriteHandler)(void *data, char *buf, int buf_len);
typedef void (*tcpCloseHandler)(void *data);

typedef struct tcpListener {
    int fd;
    int flags;
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
    tcpEventHandler onClose;
    tcpEventHandler onError;
    void (*onConnect)(void *data, int status);
    tcpReadHandler read;
    tcpWriteHandler write;
    tcpCloseHandler close;
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

#define TCP_ON_ACCEPT(ln, h) ((tcpListener *)ln)->onAccept = (h)
#define TCP_L_CLOSE(ln) do { if (ln) ((tcpListener *)ln)->close(ln); } while (0)

#define TCP_ON_READ(c, h) ((tcpConn *)c)->onRead = (h)
#define TCP_ON_WRITE(c, h) ((tcpConn *)c)->onWrite = (h)
#define TCP_ON_CONNECT(c, h) ((tcpConn *)c)->onConnect = (h)
#define TCP_ON_TIMEOUT(c, h) ((tcpConn *)c)->onTimeout = (h)
#define TCP_ON_CLOSE(c, h) ((tcpConn *)c)->onClose = (h)
#define TCP_ON_ERROR(c, h) ((tcpConn *)c)->onError = (h)

#define FIRE_CLOSE(c) do { if (c->onClose) c->onClose(c->data); } while (0)
#define FIRE_ERROR(c) do { if (c->onError) c->onError(c->data); } while (0)

#define TCP_READ(c, buf, len) ((tcpConn *)c)->read(c, buf, len)
#define TCP_WRITE(c, buf, len) ((tcpConn *)c)->write(c, buf, len)
#define TCP_CLOSE(c) do { if (c) ((tcpConn *)c)->close(c); } while (0)

tcpListener *tcpListen(char *err, eventLoop *el, char *host, int port,
                       void *data, tcpEventHandler onAccept);

tcpConn *tcpAccept(char *err, eventLoop *el, int fd, int timeout, void *data);
tcpConn *tcpConnect(char *err, eventLoop *el, char *host, int port, int timeout, void *data);
int tcpInit(tcpConn *c);
int tcpSetTimeout(tcpConn *c, int timeout);
void tcpClose(tcpConn *c);

int tcpIsConnected(tcpConn *c);
int tcpRead(tcpConn *c, char *buf, int buf_len);
int tcpWrite(tcpConn *c, char *buf, int buf_len);
int tcpPipe(tcpConn *src, tcpConn *dst);

char *tcpGetAddrinfo(tcpConn *c);

#endif /* __PROTOCOL_TCP_H */
