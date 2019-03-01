
#include "module.h"
#include "module_tcp.h"

#include "redis/anet.h"

static void tcpServerReadHandler(event *e);

static void tcpClientReadHandler(event *e);
static void tcpClientWriteHandler(event *e);
static void tcpClientReadTimeHandler(event *e);

static void tcpRemoteReadHandler(event *e);
static void tcpRemoteWriteHandler(event *e);
static void tcpRemoteConnectTimeHandler(event *e);

tcpServer *tcpServerCreate(char *host, int port, clientReadHandler handler) {
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

void tcpServerFree(tcpServer *server) {
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
            LOGW("Tcp server accept: %s", err);

        return;
    }

    tcpClient *client = tcpClientNew(cfd);
    if (!client) {
        LOGW("Tcp client is NULL, please check the memory");
        return;
    }

    client->server = server;
    snprintf(client->client_addr_info, ADDR_INFO_STR_LEN, "%s:%d", cip, cport);

    server->client_count++;

    LOGD("Tcp server accepted client (%d) [%s]", cfd, client->client_addr_info);
    LOGD("Tcp client current count: %d", server->client_count);
}

void tcpConnectionFree(tcpClient *client) {
    if (!client) return;

    tcpServer *server = client->server;

    server->client_count--;
    if (client->remote) server->remote_count--;

    LOGD("Tcp client current count: %d", server->client_count);
    LOGD("Tcp remote current count: %d", server->remote_count);

    tcpRemoteFree(client->remote);
    tcpClientFree(client);
}

tcpClient *tcpClientNew(int fd) {
    tcpClient *client = xs_calloc(sizeof(*client));
    if (!client) return NULL;

    client->fd = fd;
    client->re = NEW_EVENT_READ(fd, tcpClientReadHandler, client);
    client->we = NEW_EVENT_WRITE(fd, tcpClientWriteHandler, client);
    client->te = NEW_EVENT_ONCE(app->config->timeout, tcpClientReadTimeHandler, client);
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

    if (client->server->crHandler && client->server->crHandler(client) == TCP_ERR) goto error;

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

            LOGW("Tcp client [%s] write error: %s", client->remote_addr_info, STRERR);
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

    if (client->stage != STAGE_STREAM) {
        LOGN("Tcp client (%d) [%s] read timeout", client->fd, client->client_addr_info);

        tcpConnectionFree(client);
    }
}

tcpRemote *tcpRemoteNew(int fd) {
    tcpRemote *remote = xs_calloc(sizeof(*remote));
    if (!remote) return NULL;

    remote->fd = fd;
    remote->re = NEW_EVENT_READ(fd, tcpRemoteReadHandler, remote);
    remote->we = NEW_EVENT_WRITE(fd, tcpRemoteWriteHandler, remote);
    remote->te = NEW_EVENT_ONCE(app->config->timeout, tcpRemoteConnectTimeHandler, remote);
    remote->buf_off = 0;

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);
    netNoSigPipe(NULL, fd);

    ADD_EVENT(remote->we);
    ADD_EVENT(remote->te);

    bzero(&remote->buf, sizeof(remote->buf));
    balloc(&remote->buf, NET_IOBUF_LEN);

    return remote;
}

void tcpRemoteFree(tcpRemote *remote) {
    if (!remote) return;

    bfree(&remote->buf);

    CLR_EVENT(remote->re);
    CLR_EVENT(remote->we);
    CLR_EVENT(remote->te);
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

        LOGW("Tcp remote [%s] read error: %s", client->remote_addr_info, STRERR);
        goto error;
    } else if (nread == 0) {
        LOGD("Tcp remote [%s] closed connection", client->remote_addr_info);
        goto error;
    }
    remote->buf.len = nread;

    if (app->type == MODULE_REMOTE) {
        if (app->crypto->encrypt(&remote->buf, client->e_ctx, NET_IOBUF_LEN)) {
            LOGW("Tcp remote [%s] encrypt buffer error", client->remote_addr_info);
            goto error;
        }
    } else if (app->type == MODULE_LOCAL) {
        if (app->crypto->decrypt(&remote->buf, client->d_ctx, NET_IOBUF_LEN)) {
            LOGW("Tcp remote [%s] decrypt buffer error", client->remote_addr_info);
            goto error;
        }
    }

    // Write buffer to client
    nwrite = write(cfd, remote->buf.data, remote->buf.len);
    if (nwrite == -1) {
        if (errno == EAGAIN) goto write_again;

        LOGW("Tcp client [%s] write error: %s", client->client_addr_info, STRERR);
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

    DEL_EVENT(remote->we);
    ADD_EVENT(remote->re);
    ADD_EVENT(client->re);

    if (client->buf.len - client->buf_off == 0) return;

    // Write to remote server
    int nwrite;
    int rfd = remote->fd;

    nwrite = anetWrite(rfd, client->buf.data+client->buf_off, client->buf.len-client->buf_off);
    if (nwrite != (int)client->buf.len-client->buf_off) {
        LOGW("Tcp remote [%s] write error: %s", client->remote_addr_info, STRERR);
        goto error;
    }

    return;

error:
    tcpConnectionFree(client);
}

static void tcpRemoteConnectTimeHandler(event *e) {
    tcpRemote *remote = e->data;
    tcpClient *client = remote->client;

    if (client->stage != STAGE_STREAM) {
        LOGN("Tcp remote [%s] connect timeout", client->remote_addr_info);

        tcpConnectionFree(client);
    }
}
