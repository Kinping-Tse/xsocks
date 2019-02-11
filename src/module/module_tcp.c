
#include "module.h"
#include "module_tcp.h"

#include "redis/anet.h"

static void tcpServerReadHandler(event *e);

static void tcpClientWriteHandler(event *e);
static void tcpClientReadTimeHandler(event *e);

static void tcpRemoteReadHandler(event *e);
static void tcpRemoteWriteHandler(event *e);
static void tcpRemoteConnectTimeHandler(event *e);

tcpServer *tcpServerNew(int fd) {
    tcpServer *server = xs_calloc(sizeof(*server));

    server->fd = fd;
    server->re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, tcpServerReadHandler, server);

    anetNonBlock(NULL, server->fd);
    eventAdd(app->el, server->re);

    return server;
}

void tcpServerFree(tcpServer *server) {
    if (!server) return;

    eventDel(server->re);
    eventFree(server->re);
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

    client->server = server;
    snprintf(client->client_addr_info, ADDR_INFO_STR_LEN, "%s:%d", cip, cport);

    server->client_count++;

    LOGD("Tcp server accepted client (%d) [%s]", cfd, client->client_addr_info);
    LOGD("Tcp client current count: %d", server->client_count);
}

void tcpConnectionFree(tcpClient *client) {
    tcpServer *server = client->server;

    server->client_count--;
    if (client->remote) server->remote_count--;

    LOGD("Tcp client current count: %d", server->client_count);
    LOGD("Tcp remote current count: %d", server->remote_count);

    tcpClientFree(client);
    tcpRemoteFree(client->remote);
}

tcpClient *tcpClientNew(int fd) {
    tcpClient *client = xs_calloc(sizeof(*client));

    client->fd = fd;
    client->re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, tcpClientReadHandler, client);
    client->we = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_WRITE, tcpClientWriteHandler, client);
    client->te = eventNew(app->config->timeout, EVENT_TYPE_TIME, EVENT_FLAG_TIME_ONCE,
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

    eventAdd(app->el, client->re);
    eventAdd(app->el, client->te);

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

    eventDel(client->re);
    eventDel(client->we);
    eventDel(client->te);
    eventFree(client->re);
    eventFree(client->we);
    eventFree(client->te);
    close(client->fd);

    app->crypto->ctx_release(client->e_ctx);
    app->crypto->ctx_release(client->d_ctx);
    xs_free(client->e_ctx);
    xs_free(client->d_ctx);

    xs_free(client);
}

static void tcpClientWriteHandler(event *e) {
    tcpClient *client = e->data;
    tcpRemote *remote = client->remote;
    int nwrite;
    int cfd = client->fd;

    if (remote->buf.len - remote->buf_off == 0) {
        remote->buf_off = 0;
        eventDel(client->we);
        eventAdd(app->el, client->re);
        eventAdd(app->el, remote->re);
        return;
    }

    nwrite = write(cfd, remote->buf.data, remote->buf.len);
    if (nwrite <= (int)remote->buf.len-remote->buf_off) {
        if (nwrite == -1) {
            if (errno == EAGAIN) return;

            LOGW("Tcp client [%s] write error: %s", client->client_addr_info, STRERR);
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
        LOGD("Tcp client (%d) [%s] read timeout", client->fd, client->client_addr_info);

        tcpConnectionFree(client);
    }
}

tcpRemote *tcpRemoteNew(int fd) {
    tcpRemote *remote = xs_calloc(sizeof(*remote));

    remote->fd = fd;
    remote->re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, tcpRemoteReadHandler, remote);
    remote->we = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_WRITE, tcpRemoteWriteHandler, remote);
    remote->te = eventNew(app->config->timeout, EVENT_TYPE_TIME, EVENT_FLAG_TIME_ONCE,
                          tcpRemoteConnectTimeHandler, remote);
    remote->buf_off = 0;

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);
    netNoSigPipe(NULL, fd);

    eventAdd(app->el, remote->we);
    eventAdd(app->el, remote->te);

    bzero(&remote->buf, sizeof(remote->buf));
    balloc(&remote->buf, NET_IOBUF_LEN);

    return remote;
}

void tcpRemoteFree(tcpRemote *remote) {
    if (!remote) return;

    bfree(&remote->buf);

    eventDel(remote->re);
    eventDel(remote->we);
    eventDel(remote->te);
    eventFree(remote->re);
    eventFree(remote->we);
    eventFree(remote->te);
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
    if (nwrite != (int)remote->buf.len) {
        if (nwrite == -1) {
            if (errno == EAGAIN) {
                eventDel(remote->re);
                eventDel(client->re);
                eventAdd(app->el, client->we);

                return;
            }

            LOGW("Tcp client [%s] write error: %s", client->client_addr_info, STRERR);
            goto error;
        }

        remote->buf_off = nwrite;
        eventDel(remote->re);
        eventDel(client->re);
        eventAdd(app->el, client->we);
    }

    return;

error:
    tcpConnectionFree(client);
}

static void tcpRemoteWriteHandler(event *e) {
    tcpRemote *remote = e->data;
    tcpClient *client = remote->client;

    eventDel(remote->we);
    eventAdd(app->el, remote->re);
    eventAdd(app->el, client->re);

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
        LOGD("Tcp remote [%s] connect timeout", client->remote_addr_info);

        tcpConnectionFree(client);
    }
}
