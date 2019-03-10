
#include "tcp_shadowsocks.h"
#include "../core/socks5.h"

#include <sys/uio.h>

static void tcpShadowsocksConnFree(void *data);
static int tcpShadowsocksConnRead(void *data, char *buf, int buf_len);
static int tcpShadowsocksConnWrite(void *data, char *buf, int buf_len);

tcpShadowsocksConn *tcpShadowsocksConnNew(tcpConn *conn, crypto_t *crypto, char *host, int port) {
    tcpShadowsocksConn *c;

    c = xs_realloc(conn, sizeof(*c));
    if (!c) return c;

    conn = &c->conn;

    conn->read = tcpShadowsocksConnRead;
    conn->write = tcpShadowsocksConnWrite;
    conn->close = tcpShadowsocksConnFree;

    c->crypto = crypto;
    c->state = SHADOWSOCKS_STATE_INIT;

    c->e_ctx = xs_calloc(sizeof(*c->e_ctx));
    c->d_ctx = xs_calloc(sizeof(*c->d_ctx));

    c->crypto->ctx_init(c->crypto->cipher, c->e_ctx, 1);
    c->crypto->ctx_init(c->crypto->cipher, c->d_ctx, 0);

    c->dest_addr = xs_calloc(sizeof(*c->dest_addr));
    balloc(c->dest_addr, IOBUF_MIN_LEN);

    sds addr = socks5AddrInit(NULL, host, port);
    int addr_len = sdslen(addr);

    memcpy(c->dest_addr->data, addr, addr_len);
    c->dest_addr->len = addr_len;

    sdsfree(addr);

    return c;
}

static void tcpShadowsocksConnFree(void *data) {
    tcpShadowsocksConn *c = data;
    if (!c) return;

    c->crypto->ctx_release(c->e_ctx);
    c->crypto->ctx_release(c->d_ctx);
    xs_free(c->e_ctx);
    xs_free(c->d_ctx);

    bfree(c->dest_addr);
    xs_free(c->dest_addr);

    tcpClose(&c->conn);
}

static int tcpShadowsocksConnRead(void *data, char *buf, int buf_len) {
    tcpShadowsocksConn *c = data;
    tcpConn *conn = &c->conn;

    if (c->state == SHADOWSOCKS_STATE_HANDSHAKE) {
        c->state = SHADOWSOCKS_STATE_STREAM;
        anetDisableTcpNoDelay(conn->errstr, conn->fd);
    }

    int nread = tcpRead(conn, buf, buf_len);
    if (nread > 0) {
        buffer_t tmp_buf = {.idx = 0, .len = nread, .capacity = buf_len, .data = buf};
        if (c->crypto->decrypt(&tmp_buf, c->d_ctx, buf_len)) {
            xs_error(conn->errstr, "Decrypt shadowsocks stream buffer error");
            return TCP_ERR;
        }
        nread = tmp_buf.len;
    }
    return nread;
}

static int tcpShadowsocksConnWrite(void *data, char *buf, int buf_len) {
    tcpShadowsocksConn *c = data;
    tcpConn *conn = &c->conn;

    if (c->state == SHADOWSOCKS_STATE_INIT) {
        c->state = SHADOWSOCKS_STATE_HANDSHAKE;
        anetEnableTcpNoDelay(conn->errstr, conn->fd);

        if (c->crypto->encrypt(c->dest_addr, c->e_ctx, c->dest_addr->capacity)) {
            conn->err = ERROR_SHADOWSOCKS_ENCRYPT;
            xs_error(conn->errstr, "Encrypt shadowsocks handshake buffer error");
            return TCP_ERR;
        }
        if (tcpWrite(conn, c->dest_addr->data, c->dest_addr->len) != (int)c->dest_addr->len) {
            xs_error(conn->errstr, "Write shadowsocks handshake buffer error: %s", conn->errstr);
            return TCP_ERR;
        }
    }

    buffer_t tmp_buf = {.data = buf, .len = buf_len, .capacity = buf_len, .idx = 0};
    if (c->crypto->encrypt(&tmp_buf, c->e_ctx, tmp_buf.capacity)) {
        conn->err = ERROR_SHADOWSOCKS_ENCRYPT;
        xs_error(conn->errstr, "Encrypt shadowsocks stream buffer error");
        return TCP_ERR;
    }

    return tcpWrite(conn, tmp_buf.data, tmp_buf.len);
}
