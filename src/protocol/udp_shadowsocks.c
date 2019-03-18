
#include "udp_shadowsocks.h"

static void udpShadowsocksConnFree(udpConn *conn);
static int udpShadowsocksConnRead(udpConn *conn, char *buf, int buf_len, sockAddrEx *sa);
static int udpShadowsocksConnWrite(udpConn *conn, char *buf, int buf_len, sockAddrEx *sa);

udpShadowsocksConn *udpShadowsocksConnNew(udpConn *conn, crypto_t *crypto) {
    udpShadowsocksConn *c;

    c = xs_realloc(conn, sizeof(*c));
    if (!c) return c;

    conn = &c->conn;

    conn->read = udpShadowsocksConnRead;
    conn->write = udpShadowsocksConnWrite;
    conn->close = udpShadowsocksConnFree;

    c->crypto = crypto;

    c->addrbuf_dest = xs_calloc(sizeof(*c->addrbuf_dest));
    balloc(c->addrbuf_dest, IOBUF_MIN_LEN);

    udpInit(conn);

    return c;
}

static void udpShadowsocksConnFree(udpConn *conn) {
    udpShadowsocksConn *c = (udpShadowsocksConn *)conn;
    if (!c) return;

    bfree(c->addrbuf_dest);
    xs_free(c->addrbuf_dest);

    udpClose(&c->conn);
}

static int udpShadowsocksConnRead(udpConn *conn, char *buf, int buf_len, sockAddrEx *sa) {
    UNUSED(conn);
    UNUSED(buf);
    UNUSED(buf_len);
    UNUSED(sa);
    return UDP_ERR;
}

static int udpShadowsocksConnWrite(udpConn *conn, char *buf, int buf_len, sockAddrEx *sa) {
    UNUSED(conn);
    UNUSED(buf);
    UNUSED(buf_len);
    UNUSED(sa);
    return UDP_ERR;
}
