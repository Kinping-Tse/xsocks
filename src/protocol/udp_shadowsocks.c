
#include "udp_shadowsocks.h"

#include "socks5.h"

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

int udpShadowsocksConnInit(udpShadowsocksConn *conn, char *host, int port) {
    char addr[SOCKS5_ADDR_MAX_LEN];
    int addr_len;

    socks5AddrCreate(NULL, host, port, addr, &addr_len);

    memcpy(conn->addrbuf_dest->data, addr, addr_len);
    conn->addrbuf_dest->len = addr_len;

    return UDP_OK;
}

static void udpShadowsocksConnFree(udpConn *conn) {
    udpShadowsocksConn *c = (udpShadowsocksConn *)conn;
    if (!c) return;

    bfree(c->addrbuf_dest);
    xs_free(c->addrbuf_dest);

    udpClose(&c->conn);
}

static int udpShadowsocksConnRead(udpConn *conn, char *buf, int buf_len, sockAddrEx *sa) {
    udpShadowsocksConn *c = (udpShadowsocksConn *)conn;
    int nread;

    nread = udpRead(conn, buf, buf_len, sa);
    if (nread == UDP_ERR) return UDP_ERR;

    buffer_t tmp_buf = {.idx = 0, .len = nread, .capacity = buf_len, .data = buf};
    if (c->crypto->decrypt_all(&tmp_buf, c->crypto->cipher, tmp_buf.capacity) != CRYPTO_OK) {
        conn->err = ERROR_SHADOWSOCKS_DECRYPT;
        xs_error(conn->errstr, "Decrypt UDP shadowsocks buffer error");
        goto error;
    }

    char host[HOSTNAME_MAX_LEN];
    int host_len = sizeof(host);
    int port;
    int addr_len;

    if ((addr_len = socks5AddrParse(buf, nread, NULL, host, &host_len, &port)) == SOCKS5_ERR) {
        conn->err = ERROR_SHADOWSOCKS_SOCKS5;
        xs_error(conn->errstr, "Parse shadowsocks socks5 addr error");
        goto error;
    }

    memmove(buf, buf + addr_len, nread - addr_len);
    udpShadowsocksConnInit(c, host, port);

    return nread - addr_len;

error:
    FIRE_ERROR(conn);
    FIRE_CLOSE(conn);
    return UDP_ERR;
}

static int udpShadowsocksConnWrite(udpConn *conn, char *buf, int buf_len, sockAddrEx *sa) {
    udpShadowsocksConn *c = (udpShadowsocksConn *)conn;
    buffer_t tmp_buf;
    int nwrite;

    bzero(&tmp_buf, sizeof(tmp_buf));
    balloc(&tmp_buf, NET_IOBUF_LEN);
    memcpy(tmp_buf.data, buf, buf_len);
    tmp_buf.len = buf_len;

    bprepend(&tmp_buf, c->addrbuf_dest, tmp_buf.capacity);

    if (c->crypto->encrypt_all(&tmp_buf, c->crypto->cipher, tmp_buf.capacity)) {
        conn->err = ERROR_SHADOWSOCKS_ENCRYPT;
        xs_error(conn->errstr, "Encrypt UDP shadowsocks buffer error");
        goto error;
    }

    nwrite = udpWrite(conn, tmp_buf.data, tmp_buf.len, sa);
    if (nwrite == UDP_ERR) return UDP_ERR;

    bfree(&tmp_buf);

    return nwrite - c->addrbuf_dest->len;

error:
    bfree(&tmp_buf);
    FIRE_ERROR(conn);
    FIRE_CLOSE(conn);
    return UDP_ERR;
}
