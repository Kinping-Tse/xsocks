
#include "tcp.h"
#include "../core/utils.h"

static tcpListener *tcpListenNew(int fd, eventLoop *el, void *data);
static void tcpListenFree(tcpListener *ln);
static void tcpListenReadHandler(event *e);

static tcpConn *tcpConnNew(int fd, int timeout, eventLoop *el, void *data);
static void tcpConnInit(tcpConn *c);
static int tcpCheckConnectDone(tcpConn *c, int *done);

static int tcpPipeWrite(tcpConn *c);

static int handleTcpConnection(tcpConn *c);
static void tcpConnReadHandler(event *e);
static void tcpConnWriteHandler(event *e);
static void tcpConnTimeoutHandler(event *e);

tcpListener *tcpListen(char *err, eventLoop *el, char *host, int port, void *data, tcpEventHandler onAccept) {
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

    ln->flags = TCP_FLAG_LISTEN;
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
    ln->flags = TCP_FLAG_INIT;

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);
    netNoSigPipe(NULL, fd);

    anetFormatSock(fd, ln->addrinfo, sizeof(ln->addrinfo));

    return ln;
}

static void tcpListenFree(tcpListener *ln) {
    if (!ln) return;

    CLR_EVENT_READ(ln);
    close(ln->fd);

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

    c->flags |= TCP_FLAG_CONNECTING;
    memcpy(&c->rsa, &sa, sizeof(sa));

    return c;
}

int tcpInit(tcpConn *c) {
    c->re = NEW_EVENT_READ(c->fd, tcpConnReadHandler, c);
    c->we = NEW_EVENT_WRITE(c->fd, tcpConnWriteHandler, c);
    tcpSetTimeout(c, c->timeout);

    if (c->flags & TCP_FLAG_CONNECTING) ADD_EVENT_WRITE(c);

    return TCP_OK;
}

int tcpSetTimeout(tcpConn *c, int timeout) {
    if (timeout <= 0 || timeout != c->timeout) CLR_EVENT_TIME(c);
    if (timeout > 0) {
        c->te = NEW_EVENT_ONCE(c->timeout * MILLISECOND_UNIT, tcpConnTimeoutHandler, c);
        ADD_EVENT_TIME(c);
    }
    c->timeout = timeout;

    return TCP_OK;
}

void tcpClose(tcpConn *c) {
    if (!c) return;

    c->flags |= TCP_FLAG_CLOSED;
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
    c->flags = TCP_FLAG_INIT;
    c->timeout = timeout;

    c->el = el;
    c->data = data;

    c->read = tcpRead;
    c->write = tcpWrite;
    c->close = tcpClose;
    c->getAddrinfo = tcpGetAddrinfo;

    c->rbuf = xs_calloc(NET_IOBUF_LEN);
    c->rbuf_len = NET_IOBUF_LEN;
    c->rbuf_off = 0;
    c->wbuf = NULL;
    c->wbuf_len = 0;

    anetNonBlock(NULL, fd);
    anetFormatSock(fd, c->addrinfo, sizeof(c->addrinfo));

    return c;
}

/*
 Init connnection when it is done
 */
static void tcpConnInit(tcpConn *c) {
    int fd = c->fd;

    anetFormatPeer(fd, c->addrinfo_peer, sizeof(c->addrinfo_peer));

    anetDisableTcpNoDelay(NULL, fd);
    netNoSigPipe(NULL, fd);

    c->flags |= TCP_FLAG_CONNECTED;
}

static int tcpCheckConnectDone(tcpConn *c, int *done) {
    int rc = connect(c->fd, (sockAddr *)&c->rsa.sa, c->rsa.sa_len);
    if (rc == 0) {
        if (done) *done = 1;
        return TCP_OK;
    }

    if (done) *done = 0;
    switch (errno) {
        case EISCONN: if (done) *done = 1; return TCP_OK;
        case EALREADY:
        case EINPROGRESS:
        case EWOULDBLOCK: return TCP_OK;
        default:
            c->err = TCP_ERROR_CONNECT;
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

    DEL_EVENT_TIME(c);

    nread = netTcpRead(c->errstr, c->fd, buf, buf_len, &closed);
    if (nread == NET_ERR) {
        c->err = TCP_ERROR_READ;
        FIRE_ERROR(c);
        FIRE_CLOSE(c);
        return TCP_ERR;
    } else if (nread == 0) {
        if (closed == 1) {
            c->err = TCP_ERROR_CLOSED;
            FIRE_CLOSE(c);
            return TCP_ERR;
        }
    }

    ADD_EVENT_TIME(c);

    return nread;
}

int tcpWrite(tcpConn *c, char *buf, int buf_len) {
    int nwrite;

    nwrite = netTcpWrite(c->errstr, c->fd, buf, buf_len);
    if (nwrite == NET_ERR) {
        c->err = TCP_ERROR_WRITE;
        FIRE_ERROR(c);
        FIRE_CLOSE(c);
        return TCP_ERR;
    }

    return nwrite;
}

int tcpPipe(tcpConn *src, tcpConn *dst) {
    char *rbuf = src->rbuf + src->rbuf_off;
    int rbuf_len = src->rbuf_len - src->rbuf_off;
    int nread;
    int nwrite;

    dst->pipe = src;
    dst->flags |= TCP_FLAG_PIPE;

    nread = TCP_READ(src, rbuf, rbuf_len);
    if (nread <= 0) return nread;

    src->rbuf_off += nread;

    if (!dst->wbuf) dst->wbuf = src->rbuf;
    dst->wbuf_len = src->rbuf_off;

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

static int tcpPipeWrite(tcpConn *c) {
    char *wbuf = c->wbuf;
    int wbuf_len = c->wbuf_len;
    int nwrite;

    // Write done
    if (wbuf_len == 0) {
        c->pipe->rbuf_off = 0;
        c->wbuf = NULL;
        c->wbuf_len = 0;
        ADD_EVENT_READ(c->pipe);
        DEL_EVENT_WRITE(c);
        return TCP_OK;
    }

    nwrite = TCP_WRITE(c, wbuf, wbuf_len);
    if (nwrite == TCP_ERR) return nwrite;

    if (nwrite > 0) {
        c->wbuf += nwrite;
        c->wbuf_len -= nwrite;
    }
    ADD_EVENT_WRITE(c);

    return nwrite;
}

char *tcpGetAddrinfo(tcpConn *c) {
    return tcpIsConnected(c) ? c->addrinfo_peer : c->addrinfo;
}

static int handleTcpConnection(tcpConn *c) {
    if (!tcpIsConnected(c)) {
        int status;
        int done;

        status = tcpCheckConnectDone(c, &done);
        if (status == TCP_OK) {
            if (done == 0) return TCP_ERR;

            tcpConnInit(c);
            DEL_EVENT_WRITE(c);
        }

        FIRE_CONNECT(c, status);
        if (status == TCP_ERR) FIRE_CLOSE(c);

        return status;
    }

    return TCP_OK;
}

static void tcpConnReadHandler(event *e) {
    tcpConn *c = e->data;
    int status;

    status = handleTcpConnection(c);
    if (status != TCP_OK) return;

    FIRE_READ(c);
}

static void tcpConnWriteHandler(event *e) {
    tcpConn *c = e->data;
    int status;

    status = handleTcpConnection(c);
    if (status != TCP_OK) return;

    if (c->flags & TCP_FLAG_PIPE) {
        tcpPipeWrite(c);
        return;
    }

    FIRE_WRITE(c);
}

static void tcpConnTimeoutHandler(event *e) {
    tcpConn *c = e->data;

    c->err = TCP_ERROR_TIMEOUT;
    xs_error(c->errstr, "TCP conn timeout");

    FIRE_TIMEOUT(c);
    FIRE_CLOSE(c);
}
