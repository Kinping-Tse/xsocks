
#include "tcp.h"
#include "../core/utils.h"

static tcpListener *tcpListenNew(int fd, eventLoop *el, void *data);
static void tcpListenFree(void *data);
static void tcpListenReadHandler(event *e);

static tcpConn *tcpConnNew(int fd, int timeout, eventLoop *el, void *data);
static int tcpCheckConnectDone(tcpConn *c);
static void tcpConnInit(tcpConn * c);

static int handleTcpConnection(tcpConn *c);
static void tcpConnReadHandler(event *e);
static void tcpConnWriteHandler(event *e);
static void tcpConnReadTimeHandler(event *e);

tcpListener *tcpListen(char *err, eventLoop *el, char *host, int port,
                       void *data, tcpEventHandler onAccept) {
    int backlog = 256;
    int fd;

    if ((host && isIPv6Addr(host)))
        fd = anetTcp6Server(err, port, host, backlog);
    else
        fd = anetTcpServer(err, port, host, backlog);

    if (fd == ANET_ERR) return NULL;

    tcpListener *ln = tcpListenNew(fd, el, data);
    if (!ln) {
        close(fd);
        xs_error(err, "TCP Listener is NULL, please check the memory");
        return NULL;
    }

    TCP_ON_ACCEPT(ln, onAccept);
    ADD_EVENT_READ(ln);

    return ln;
}

static tcpListener *tcpListenNew(int fd, eventLoop *el, void *data) {
    tcpListener *ln = xs_calloc(sizeof(*ln));
    if (!ln) return NULL;

    ln->fd = fd;
    ln->el = el;
    ln->data = data;
    ln->re = NEW_EVENT_READ(fd, tcpListenReadHandler, ln);
    ln->close = tcpListenFree;

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);
    netNoSigPipe(NULL, fd);

    anetFormatSock(fd, ln->addrinfo, sizeof(ln->addrinfo));

    return ln;
}

static void tcpListenFree(void *data) {
    tcpListener *ln = data;

    xs_free(ln);
}

static void tcpListenReadHandler(event *e) {
    tcpListener *ln = e->data;

    if (ln->onAccept) ln->onAccept(ln->data);
}

tcpConn *tcpAccept(char *err, eventLoop *el, int fd, int timeout, void *data) {
    int cfd;

    cfd = anetTcpAccept(err, fd, NULL, 0, NULL);
    if (cfd == ANET_ERR) return NULL;

    tcpConn *c = tcpConnNew(cfd, timeout, el, data);
    if (!c) {
        close(cfd);
        xs_error(err, "TCP conn is NULL, please check the memory");
        return NULL;
    }

    tcpConnInit(c);
    c->flags |= TCP_FLAG_CONNECTED;

    return c;
}

tcpConn *tcpConnect(char *err, eventLoop *el, char *host, int port, int timeout, void *data) {
    int fd;
    tcpConn *c;
    sockAddrEx sa;

    fd = netTcpNonBlockConnect(err, host, port, &sa);
    if (fd == ANET_ERR) return NULL;

    c = tcpConnNew(fd, timeout, el, data);
    if (!c) {
        close(fd);
        xs_error(err, "TCP conn is NULL, please check the memory");
        return NULL;
    }

    memcpy(&c->rsa, &sa, sizeof(sa));

    ADD_EVENT_WRITE(c);

    return c;
}

void tcpClose(tcpConn *c) {
    if (!c) return;

    CLR_EVENT_READ(c);
    CLR_EVENT_WRITE(c);
    CLR_EVENT_TIME(c);
    close(c->fd);

    xs_free(c->rbuf);

    xs_free(c);
}

static tcpConn *tcpConnNew(int fd, int timeout, eventLoop *el, void *data) {
    tcpConn *c = xs_calloc(sizeof(*c));
    if (!c) return NULL;

    c->fd = fd;
    c->flags = 0;
    c->timeout = timeout;

    c->el = el;
    c->data = data;
    c->re = NEW_EVENT_READ(fd, tcpConnReadHandler, c);
    c->we = NEW_EVENT_WRITE(fd, tcpConnWriteHandler, c);
    if (timeout > 0) c->te = NEW_EVENT_ONCE(timeout * MILLISECOND_UNIT, tcpConnReadTimeHandler, c);
    c->read = (tcpReadHandler)tcpRead;
    c->write = (tcpWriteHandler)tcpWrite;
    c->close = (tcpCloseHandler)tcpClose;

    c->rbuf = xs_calloc(NET_IOBUF_LEN);
    c->rbuf_len = NET_IOBUF_LEN;
    c->rbuf_off = 0;
    c->wbuf = NULL;
    c->wbuf_len = 0;

    return c;
}

static void tcpConnInit(tcpConn * c) {
    int fd = c->fd;

    anetFormatSock(fd, c->addrinfo, sizeof(c->addrinfo));
    anetFormatPeer(fd, c->addrinfo_peer, sizeof(c->addrinfo_peer));

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);
    netNoSigPipe(NULL, fd);
}

static int tcpCheckConnectDone(tcpConn *c) {
    int rc = connect(c->fd, (sockAddr *)&c->rsa.sa, c->rsa.sa_len);
    if (rc == 0) return TCP_OK;

    switch (errno) {
        case EISCONN:
            return TCP_OK;
        case EALREADY:
        case EINPROGRESS:
        case EWOULDBLOCK:
            return TCP_AGAIN;
        default:
            c->err = errno;
            xs_error(c->errstr, STRERR);
            return TCP_ERR;
    }
}

int tcpIsConnected(tcpConn *c) {
    return c->flags & TCP_FLAG_CONNECTED;
}

int tcpRead(tcpConn *c, char *buf, int buf_len) {
    int nread;
    int closed;

    nread = netTcpRead(c->errstr, c->fd, buf, buf_len, &closed);
    if (nread == NET_ERR) {
        c->err = errno;
        return TCP_ERR;
    } else if (nread == 0 && closed == 0)
        return TCP_AGAIN;

    return nread;
}

int tcpWrite(tcpConn *c, char *buf, int buf_len) {
    int nwrite;

    nwrite = netTcpWrite(c->errstr, c->fd, buf, buf_len);
    if (nwrite == NET_ERR) {
        c->err = errno;
        return TCP_ERR;
    }
    return nwrite;
}

int tcpPipe(tcpConn *dst, tcpConn *src) {
    char *rbuf = src->rbuf + src->rbuf_off;
    int rbuf_len = src->rbuf_len - src->rbuf_off;
    int nread;

    nread = TCP_READ(src, rbuf, rbuf_len);
    if (nread <= 0) return nread;

    src->rbuf_off += nread;

    if (!dst->wbuf) dst->wbuf = src->rbuf;
    dst->wbuf_len = src->rbuf_off;

    int nwrite;

    nwrite = TCP_WRITE(dst, dst->wbuf, dst->wbuf_len);
    if (nwrite == TCP_ERR) return TCP_ERR;

    dst->wbuf += nwrite;
    dst->wbuf_len -= nwrite;

    if (dst->wbuf_len > 0) {
        ADD_EVENT_WRITE(dst);
        DEL_EVENT_READ(src);
    } else {
        // Write done
        src->rbuf_off = 0;
        dst->wbuf = NULL;
        dst->wbuf_len = 0;

        DEL_EVENT_WRITE(dst);
        ADD_EVENT_READ(src);
    }

    return nread;
}

char *tcpGetAddrinfo(tcpConn *c) {
    return tcpIsConnected(c) ? c->addrinfo_peer : c->addrinfo;
}

static int handleTcpConnection(tcpConn *c) {
    if (!tcpIsConnected(c)) {
        int status;

        status = tcpCheckConnectDone(c);
        if (status == TCP_OK) {
            c->flags |= TCP_FLAG_CONNECTED;
            DEL_EVENT_WRITE(c);

            tcpConnInit(c);
        }
        if (c->onConnect) c->onConnect(c->data, status);

        return status;
    }

    return TCP_OK;
}

static void tcpConnReadHandler(event *e) {
    tcpConn *c = e->data;

    if (handleTcpConnection(c) == TCP_ERR) return;
    if (c->onRead) c->onRead(c->data);
}

static void tcpConnWriteHandler(event *e) {
    tcpConn *c = e->data;

    if (handleTcpConnection(c) == TCP_ERR) return;
    if (c->onWrite) c->onWrite(c->data);
}

static void tcpConnReadTimeHandler(event *e) {
    tcpConn *c = e->data;

    if (c->onTimeout) c->onTimeout(c->data);
}
