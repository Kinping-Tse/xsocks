
#include "module.h"
#include "module_tcp.h"

#include "redis/anet.h"

static void tcpServerReadHandler(event *e);

static void tcpClientReadHandler(event *e);
static void tcpClientWriteHandler(event *e);
static void tcpClientReadTimeHandler(event *e);

static void tcpRemoteReadHandler(event *e);
static void tcpRemoteWriteHandler(event *e);

tcpServer *moduleTcpServerCreate(char *host, int port, clientReadHandler handler) {
    char err[ANET_ERR_LEN];
    int backlog = 256;
    int fd;

    if ((host && isIPv6Addr(host)))
        fd = anetTcp6Server(err, port, host, backlog);
    else
        fd = anetTcpServer(err, port, host, backlog);

    if (fd == ANET_ERR) {
        LOGE("TCP server create error %s:%d: %s", host ? host : "*", port, err);
        return NULL;
    }

    tcpServer *server = tcpServerNew(fd);
    if (server) {
        server->crHandler = handler;
    }
    return server;
}

tcpServer *tcpServerNew(int fd) {
    tcpServer *server = xs_calloc(sizeof(*server));

    if (server) {
        server->fd = fd;
        server->re = NEW_EVENT_READ(fd, tcpServerReadHandler, server);

        anetNonBlock(NULL, server->fd);
        ADD_EVENT(server->re);
    }

    return server;
}

void moduleTcpServerFree(tcpServer *server) {
    if (!server) return;

    CLR_EVENT(server->re);
    close(server->fd);

    xs_free(server);
}

static void tcpServerReadHandler(event* e) {
    tcpServer *server = e->data;

    int cfd, cport;
    char cip[NET_IP_MAX_STR_LEN];
    char err[ANET_ERR_LEN];

    cfd = anetTcpAccept(err, e->id, cip, sizeof(cip), &cport);
    if (cfd == ANET_ERR) {
        if (errno != EWOULDBLOCK)
            LOGW("TCP server accept: %s", err);

        return;
    }

    tcpClient *client = tcpClientNew(cfd);
    if (!client) {
        LOGW("TCP client is NULL, please check the memory");
        return;
    }

    client->server = server;
    anetFormatAddr(client->client_addr_info, ADDR_INFO_STR_LEN, cip, cport);

    server->client_count++;

    LOGD("TCP server accepted client (%d) [%s]", cfd, client->client_addr_info);
    LOGD("TCP client current count: %d", server->client_count);
}

void tcpConnectionFree(tcpClient *client) {
    if (!client) return;

    tcpServer *server = client->server;

    server->client_count--;
    if (client->remote) server->remote_count--;

    LOGD("TCP client current count: %d", server->client_count);
    LOGD("TCP remote current count: %d", server->remote_count);

    tcpRemoteFree(client->remote);
    tcpClientFree(client);
}

tcpClient *tcpClientNew(int fd) {
    tcpClient *client = xs_calloc(sizeof(*client));
    if (!client) return NULL;

    client->fd = fd;
    client->re = NEW_EVENT_READ(fd, tcpClientReadHandler, client);
    client->we = NEW_EVENT_WRITE(fd, tcpClientWriteHandler, client);
    client->te = NEW_EVENT_ONCE(app->config->timeout * MILLISECOND_UNIT,
                                tcpClientReadTimeHandler, client);
    client->server = NULL;
    client->remote = NULL;
    client->stage = STAGE_INIT;
    client->buf_off = 0;
    client->addr_buf = NULL;
    client->e_ctx = xs_calloc(sizeof(*client->e_ctx));
    client->d_ctx = xs_calloc(sizeof(*client->d_ctx));

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);

    ADD_EVENT(client->re);
    ADD_EVENT(client->te);

    app->crypto->ctx_init(app->crypto->cipher, client->e_ctx, 1);
    app->crypto->ctx_init(app->crypto->cipher, client->d_ctx, 0);

    bzero(&client->buf, sizeof(client->buf));
    balloc(&client->buf, NET_IOBUF_LEN);

    return client;
}

void tcpClientFree(tcpClient *client) {
    if (!client) return;

    bfree(&client->buf);
    sdsfree(client->addr_buf);

    CLR_EVENT(client->re);
    CLR_EVENT(client->we);
    CLR_EVENT(client->te);
    close(client->fd);

    app->crypto->ctx_release(client->e_ctx);
    app->crypto->ctx_release(client->d_ctx);
    xs_free(client->e_ctx);
    xs_free(client->d_ctx);

    xs_free(client);
}

static void tcpClientReadHandler(event *e) {

    tcpClient *client = e->data;

    DEL_EVENT(client->te);

    if (client->server->crHandler && client->server->crHandler(client) == TCP_ERR) goto error;

    ADD_EVENT(client->te);

    return;

error:
    tcpConnectionFree(client);
}

static void tcpClientWriteHandler(event *e) {
    tcpClient *client = e->data;
    tcpRemote *remote = client->remote;

    int nwrite;
    int cfd = client->fd;
    int write_len = remote->buf.len - remote->buf_off;
    char *write_buf = remote->buf.data + remote->buf_off;

    if (write_len == 0) {
        remote->buf_off = 0;
        DEL_EVENT(client->we);
        ADD_EVENT(client->re);
        ADD_EVENT(remote->re);
        return;
    }

    nwrite = write(cfd, write_buf, write_len);
    if (nwrite <= write_len) {
        if (nwrite == -1) {
            if (errno == EAGAIN) return;

            LOGW("TCP client [%s] write error: %s", client->remote_addr_info, STRERR);
            goto error;
        }

        remote->buf_off += nwrite;
    }

    return;

error:
    tcpConnectionFree(client);
}

static void tcpClientReadTimeHandler(event *e) {
    tcpClient *client = e->data;

    LOGN("TCP client (%d) [%s] read timeout", client->fd,
         client->stage == STAGE_STREAM ? client->remote_addr_info : client->client_addr_info);

    tcpConnectionFree(client);
}

tcpRemote *tcpRemoteCreate(char *host, int port) {
    char err[ANET_ERR_LEN];
    int fd;

    if ((fd = anetTcpNonBlockConnect(err, host, port)) == ANET_ERR) {
        LOGW("TCP remote connect error: %s", err);
        return NULL;
    }

    tcpRemote *remote = tcpRemoteNew(fd);
    if (!remote) {
        LOGW("TCP remote is NULL, please check the memory");
        close(fd);
        return NULL;
    }
    return remote;
}

tcpRemote *tcpRemoteNew(int fd) {
    tcpRemote *remote = xs_calloc(sizeof(*remote));
    if (!remote) return NULL;

    remote->fd = fd;
    remote->re = NEW_EVENT_READ(fd, tcpRemoteReadHandler, remote);
    remote->we = NEW_EVENT_WRITE(fd, tcpRemoteWriteHandler, remote);
    remote->buf_off = 0;

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);
    netNoSigPipe(NULL, fd);

    ADD_EVENT(remote->we);

    bzero(&remote->buf, sizeof(remote->buf));
    balloc(&remote->buf, NET_IOBUF_LEN);

    return remote;
}

void tcpRemoteFree(tcpRemote *remote) {
    if (!remote) return;

    bfree(&remote->buf);

    CLR_EVENT(remote->re);
    CLR_EVENT(remote->we);
    close(remote->fd);

    xs_free(remote);
}

static void tcpRemoteReadHandler(event *e) {
    tcpRemote *remote = e->data;
    tcpClient *client = remote->client;

    int readlen = NET_IOBUF_LEN;
    int rfd = remote->fd;
    int cfd = client->fd;
    int nread, nwrite;

    // Read remote buffer
    nread = read(rfd, remote->buf.data, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) return;

        LOGW("TCP remote [%s] read error: %s", client->remote_addr_info, STRERR);
        goto error;
    } else if (nread == 0) {
        LOGD("TCP remote [%s] closed connection", client->remote_addr_info);
        goto error;
    }
    remote->buf.len = nread;

    if (app->type == MODULE_REMOTE) {
        if (app->crypto->encrypt(&remote->buf, client->e_ctx, NET_IOBUF_LEN)) {
            LOGW("TCP remote [%s] encrypt buffer error", client->remote_addr_info);
            goto error;
        }
    } else if (app->type == MODULE_LOCAL || app->type == MODULE_REDIR) {
        if (app->crypto->decrypt(&remote->buf, client->d_ctx, NET_IOBUF_LEN)) {
            LOGW("TCP remote [%s] decrypt buffer error", client->remote_addr_info);
            goto error;
        }
    }

    // Write buffer to client
    nwrite = write(cfd, remote->buf.data, remote->buf.len);
    if (nwrite == -1) {
        if (errno == EAGAIN) goto write_again;

        LOGW("TCP client [%s] write error: %s", client->client_addr_info, STRERR);
        goto error;
    } else if (nwrite < (int)remote->buf.len) {
        remote->buf_off = nwrite;
        goto write_again;
    }

    return;

write_again:
    DEL_EVENT(remote->re);
    DEL_EVENT(client->re);
    ADD_EVENT(client->we);
    return;

error:
    tcpConnectionFree(client);
}

static void tcpRemoteWriteHandler(event *e) {
    tcpRemote *remote = e->data;
    tcpClient *client = remote->client;

    int nwrite;
    int rfd = remote->fd;
    int write_len = client->buf.len - client->buf_off;
    char *write_buf = client->buf.data + client->buf_off;

    if (write_len == 0) {
        client->buf_off = 0;
        DEL_EVENT(remote->we);
        ADD_EVENT(remote->re);
        ADD_EVENT(client->re);
        return;
    }

    nwrite = write(rfd, write_buf, write_len);
    if (nwrite <= write_len) {
        if (nwrite == -1) {
            if (errno == EAGAIN) return;

            LOGW("TCP remote [%s] write error: %s", client->remote_addr_info, STRERR);
            goto error;
        }

        client->buf_off += nwrite;
    }

    return;

error:
    tcpConnectionFree(client);
}
