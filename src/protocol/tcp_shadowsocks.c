
#include "tcp_shadowsocks.h"

#include "socks5.h"

static void tcpShadowsocksConnFree(void *data);
static int tcpShadowsocksConnRead(tcpConn *conn, char *buf, int buf_len);
static int tcpShadowsocksConnWrite(tcpConn *conn, char *buf, int buf_len);
static char *tcpShadowsocksGetAddrinfo(tcpConn *conn);

tcpShadowsocksConn *tcpShadowsocksConnNew(tcpConn *conn, crypto_t *crypto) {
    tcpShadowsocksConn *c;

    c = xs_realloc(conn, sizeof(*c));
    if (!c) return c;

    conn = &c->conn;

    conn->read = tcpShadowsocksConnRead;
    conn->write = tcpShadowsocksConnWrite;
    conn->close = tcpShadowsocksConnFree;
    conn->getAddrinfo = tcpShadowsocksGetAddrinfo;

    c->crypto = crypto;
    c->state = SHADOWSOCKS_STATE_INIT;

    c->e_ctx = xs_calloc(sizeof(*c->e_ctx));
    c->d_ctx = xs_calloc(sizeof(*c->d_ctx));

    c->crypto->ctx_init(c->crypto->cipher, c->e_ctx, 1);
    c->crypto->ctx_init(c->crypto->cipher, c->d_ctx, 0);

    c->addrbuf_dest = xs_calloc(sizeof(*c->addrbuf_dest));
    balloc(c->addrbuf_dest, IOBUF_MIN_LEN);

    c->tmp_buf = xs_calloc(sizeof(*c->tmp_buf));
    balloc(c->tmp_buf, NET_IOBUF_LEN);
    c->tmp_buf->len = 0;
    c->tmp_buf_off = 0;

    tcpInit(conn);

    return c;
}

int tcpShadowsocksConnInit(tcpShadowsocksConn *conn, char *host, int port) {
    char addr[SOCKS5_ADDR_MAX_LEN];
    int addr_len;

    socks5AddrCreate(NULL, host, port, addr, &addr_len);

    memcpy(conn->addrbuf_dest->data, addr, addr_len);
    conn->addrbuf_dest->len = addr_len;

    anetFormatAddr(conn->addrinfo_dest, ADDR_INFO_STR_LEN, host, port);

    return TCP_OK;
}

static char *tcpShadowsocksGetAddrinfo(tcpConn *conn) {
    tcpShadowsocksConn *c = (tcpShadowsocksConn *)conn;
    return c->state == SHADOWSOCKS_STATE_STREAM ? c->addrinfo_dest : tcpGetAddrinfo(conn);
}

static void tcpShadowsocksConnFree(void *data) {
    tcpShadowsocksConn *c = data;
    if (!c) return;

    c->crypto->ctx_release(c->e_ctx);
    c->crypto->ctx_release(c->d_ctx);
    xs_free(c->e_ctx);
    xs_free(c->d_ctx);

    bfree(c->tmp_buf);
    bfree(c->addrbuf_dest);
    xs_free(c->tmp_buf);
    xs_free(c->addrbuf_dest);

    tcpClose(&c->conn);
}

static int tcpShadowsocksConnRead(tcpConn *conn, char *buf, int buf_len) {
    tcpShadowsocksConn *c = (tcpShadowsocksConn *)conn;
    int nread;

    if (c->state == SHADOWSOCKS_STATE_HANDSHAKE) c->state = SHADOWSOCKS_STATE_STREAM;

    nread = tcpRead(conn, buf, buf_len);
    if (nread <= 0) return nread;

    buffer_t tmp_buf = {.idx = 0, .len = nread, .capacity = buf_len, .data = buf};
    if (c->crypto->decrypt(&tmp_buf, c->d_ctx, buf_len)) {
        conn->err = ERROR_SHADOWSOCKS_DECRYPT;
        xs_error(conn->errstr, "Decrypt shadowsocks stream buffer error");
        goto error;
    }
    nread = tmp_buf.len;

    if (c->state == SHADOWSOCKS_STATE_INIT) {
        char host[HOSTNAME_MAX_LEN];
        int host_len = sizeof(host);
        int port;
        int raddr_len;

        if ((raddr_len = socks5AddrParse(buf, nread, NULL, host, &host_len, &port)) == SOCKS5_ERR) {
            conn->err = ERROR_SHADOWSOCKS_SOCKS5;
            xs_error(conn->errstr, "Parse shadowsocks socks5 addr error");
            goto error;
        }

        tcpShadowsocksConnInit(c, host, port);
        c->state = SHADOWSOCKS_STATE_HANDSHAKE;
    }

    return nread;

error:
    FIRE_ERROR(conn);
    FIRE_CLOSE(conn);
    return TCP_ERR;
}

static int tcpShadowsocksConnWrite(tcpConn *conn, char *buf, int buf_len) {
    tcpShadowsocksConn *c = (tcpShadowsocksConn *)conn;
    int nwrite;

    if (c->state == SHADOWSOCKS_STATE_INIT) {
        if (c->crypto->encrypt(c->addrbuf_dest, c->e_ctx, c->addrbuf_dest->capacity)) {
            conn->err = ERROR_SHADOWSOCKS_ENCRYPT;
            xs_error(conn->errstr, "Encrypt shadowsocks handshake buffer error");
            goto error;
        }
        nwrite = tcpWrite(conn, c->addrbuf_dest->data, c->addrbuf_dest->len);
        if (nwrite < 0) return nwrite;
        else if (nwrite != (int)c->addrbuf_dest->len) {
            xs_error(conn->errstr, "Write shadowsocks handshake buffer error");
            goto error;
        }

        c->state = SHADOWSOCKS_STATE_HANDSHAKE;
    } else if (c->state == SHADOWSOCKS_STATE_HANDSHAKE)
        c->state = SHADOWSOCKS_STATE_STREAM;

    if (c->tmp_buf->len == 0) {
        memcpy(c->tmp_buf->data, buf, buf_len);
        c->tmp_buf->len = buf_len;

        if (c->crypto->encrypt(c->tmp_buf, c->e_ctx, c->tmp_buf->capacity)) {
            conn->err = ERROR_SHADOWSOCKS_ENCRYPT;
            xs_error(conn->errstr, "Encrypt shadowsocks stream buffer error");
            goto error;
        }
    }

    char *wbuf = c->tmp_buf->data + c->tmp_buf_off;
    int wbuf_len = c->tmp_buf->len - c->tmp_buf_off;

    nwrite = tcpWrite(conn, wbuf, wbuf_len);
    if (nwrite == wbuf_len) {
        c->tmp_buf->len = 0;
        c->tmp_buf_off = 0;
    } else if (nwrite > 0 && nwrite < wbuf_len) {
        c->tmp_buf_off += nwrite;
    }
    return nwrite;

error:
    FIRE_ERROR(conn);
    FIRE_CLOSE(conn);
    return TCP_ERR;
}
