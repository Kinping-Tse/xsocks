
#include "udp.h"

static udpConn *udpConnNew(int fd, int timeout, eventLoop *el, void *data);

static void udpConnReadHandler(event *e);
static void udpConnTimeoutHandler(event *e);

udpConn *udpCreate(char *err, eventLoop *el, char *host, int port, int timeout, void *data) {
    int fd;
    udpConn *conn;

    if (host) {
        if (isIPv6Addr(host))
            fd = netUdp6Server(err, port, host);
        else
            fd = netUdpServer(err, port, host);
    } else {
        // Todo: order by app->config->ipv6_first
        fd = netUdpServer(err, 0, NULL);
        if (fd == ANET_ERR) fd = netUdp6Server(err, 0, NULL);
    }

    if (fd == ANET_ERR) return NULL;

    conn = udpConnNew(fd, timeout, el, data);
    if (!conn) {
        close(fd);
        xs_error(err, "UDP conn is NULL, please check the memory");
        return NULL;
    }

    return conn;
}

int udpInit(udpConn *c) {
    c->re = NEW_EVENT_READ(c->fd, udpConnReadHandler, c);
    udpSetTimeout(c, c->timeout);

    return UDP_OK;
}

static udpConn *udpConnNew(int fd, int timeout, eventLoop *el, void *data) {
    udpConn *c;

    c = xs_calloc(sizeof(*c));
    if (!c) return NULL;

    c->fd = fd;
    c->timeout = timeout;
    c->el = el;
    c->data = data;

    c->read = udpRead;
    c->write = udpRead;
    c->close = udpClose;

    anetNonBlock(NULL, c->fd);

    return c;
}

int udpSetTimeout(udpConn *c, int timeout) {
    if (timeout <= 0 || timeout != c->timeout) CLR_EVENT_TIME(c);
    if (timeout > 0) {
        c->te = NEW_EVENT_ONCE(c->timeout * MILLISECOND_UNIT, udpConnTimeoutHandler, c);
        ADD_EVENT_TIME(c);
    }
    c->timeout = timeout;

    return UDP_OK;
}

void udpClose(udpConn *c) {
    if (!c) return;

    CLR_EVENT_READ(c);
    CLR_EVENT_TIME(c);
    close(c->fd);

    xs_free(c);
}

int udpRead(udpConn *c, char *buf, int buf_len, sockAddrEx *sa) {
    int nread;

    DEL_EVENT_TIME(c);

    nread = netUdpRead(c->errstr, c->fd, buf, buf_len, sa);
    if (nread == NET_ERR) {
        c->err = UDP_ERROR_READ;
        FIRE_ERROR(c);
        FIRE_CLOSE(c);
        return UDP_ERR;
    } else if (nread == 0) {
        c->err = UDP_ERROR_CLOSED;
        FIRE_CLOSE(c);
        return UDP_ERR;
    }

    ADD_EVENT_TIME(c);

    return nread;
}

int udpWrite(udpConn *c, char *buf, int buf_len, sockAddrEx *sa) {
    int nwrite;

    nwrite = netUdpWrite(c->errstr, c->fd, buf, buf_len, sa);
    if (nwrite != buf_len) {
        c->err = UDP_ERROR_WRITE;
        FIRE_ERROR(c);
        FIRE_CLOSE(c);
        return UDP_ERR;
    }

    return nwrite;
}

static void udpConnReadHandler(event *e) {
    udpConn *c = e->data;

    FIRE_READ(c);
}

static void udpConnTimeoutHandler(event *e) {
    udpConn *c = e->data;

    c->err = UDP_ERROR_TIMEOUT;
    xs_error(c->errstr, "UDP conn timeout");

    FIRE_TIMEOUT(c);
    FIRE_CLOSE(c);
}
