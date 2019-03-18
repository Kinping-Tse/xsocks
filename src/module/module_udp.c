
#include "module_udp.h"
#include "module.h"

#include "redis/anet.h"

#define ADD_EVENT(e) eventAdd(app->el, e);

static udpServer *udpServerNew(int fd);
static void udpServerReadHandler(event *e);
static void udpConnectionFree(udpClient *client);

static udpClient *udpClientNew();
static void udpClientFree(udpClient *client);

static udpRemote *udpRemoteNew(int fd);
static void udpRemoteReadHandler(event *e);
static void udpRemoteReadTimeHandler(event *e);

udpServer *moduleUdpServerCreate(char *host, int port, udpHook hook, void *data) {
    char err[ANET_ERR_LEN];
    int fd;

    if ((host && isIPv6Addr(host)))
        fd = netUdp6Server(err, port, host);
    else
        fd = netUdpServer(err, port, host);

    if (fd == ANET_ERR) {
        LOGE("UDP server create error %s:%d: %s", host ? host : "*", port, err);
        return NULL;
    }

    udpServer *server = udpServerNew(fd);
    if (server) {
        server->hook = hook;
        server->data = data;

        if (server->hook.init) server->hook.init(server);
    }
    return server;
}

static udpServer *udpServerNew(int fd) {
    udpServer *server = xs_calloc(sizeof(*server));

    if (server) {
        server->fd = fd;
        server->re = NEW_EVENT_READ(fd, udpServerReadHandler, server);

        anetNonBlock(NULL, server->fd);
        ADD_EVENT(server->re);
    }

    return server;
}

void moduleUdpServerFree(udpServer *server) {
    if (!server) return;

    if (server->hook.free) server->hook.free(server);

    CLR_EVENT(server->re);
    close(server->fd);

    xs_free(server);
}

static void udpServerReadHandler(event *e) {
    udpServer *server = e->data;
    udpClient *client = udpClientNew();

    if (!client) {
        LOGW("UDP client is NULL, please check the memory");
        goto error;
    }

    client->server = server;
    char err[ANET_ERR_LEN];

    // Read from client
    int buflen = NET_IOBUF_LEN;
    int sfd = server->fd;
    int nread;

    nread = netUdpRead(err, sfd, client->buf.data, buflen, &client->sa_client);
    if (nread == -1) {
        LOGW("UDP server read error: %s", err);
        goto error;
    }
    client->buf.len = nread;

    // Log client info
    char cip[HOSTNAME_MAX_LEN];
    int cport;
    netIpPresentBySockAddr(NULL, cip, sizeof(cip), &cport, &client->sa_client);
    LOGD("UDP server read from [%s:%d]", cip, cport);

    // Process buffer
    if (server->hook.process && server->hook.process(client) == UDP_ERR) goto error;
    assert(client->remote);

    // Write to remote
    int rfd = client->remote->fd;
    int nwrite;

    nwrite = netUdpWrite(err, rfd, client->buf.data + client->buf_off, client->buf.len - client->buf_off,
                         &client->sa_remote);
    if (nwrite == -1) {
        LOGW("UDP server write error: %s", err);
        goto error;
    }

    server->remote_count++;
    LOGD("UDP remote current count: %d", server->remote_count);

    return;

error:
    udpConnectionFree(client);
}

static void udpConnectionFree(udpClient *client) {
    if (!client) return;

    udpServer *server = client->server;

    if (client->remote) server->remote_count--;
    LOGD("UDP remote current count: %d", server->remote_count);

    udpRemoteFree(client->remote);
    udpClientFree(client);
}

static udpClient *udpClientNew() {
    udpClient *client = xs_calloc(sizeof(*client));

    if (client) {
        bzero(&client->buf, sizeof(client->buf));
        balloc(&client->buf, NET_IOBUF_LEN);

        netSockAddrExInit(&client->sa_client);
        netSockAddrExInit(&client->sa_remote);
    }

    return client;
}

static void udpClientFree(udpClient *client) {
    if (!client) return;

    bfree(&client->buf);

    xs_free(client);
}

udpRemote *udpRemoteCreate(udpHook *hook, void *data) {
    char err[ANET_ERR_LEN];
    int fd = ANET_ERR;

    // Todo: order by app->config->ipv6_first
    fd = netUdpServer(err, 0, NULL);
    if (fd == ANET_ERR) fd = netUdp6Server(err, 0, NULL);

    if (fd == ANET_ERR) {
        LOGW("UDP remote create error: %s", err);
        return NULL;
    }

    udpRemote *remote = udpRemoteNew(fd);
    if (!remote) {
        LOGW("UDP remote is NULL, please check the memory");
        return NULL;
    }

    if (hook) memcpy(&remote->hook, hook, sizeof(*hook));
    remote->data = data;

    if (remote->hook.init) remote->hook.init(remote);

    return remote;
}

static udpRemote *udpRemoteNew(int fd) {
    udpRemote *remote = xs_calloc(sizeof(*remote));

    if (remote) {
        remote->fd = fd;
        remote->re = NEW_EVENT_READ(fd, udpRemoteReadHandler, remote);
        remote->te = NEW_EVENT_ONCE(app->config->timeout * MILLISECOND_UNIT_F, udpRemoteReadTimeHandler, remote);

        bzero(&remote->buf, sizeof(remote->buf));
        balloc(&remote->buf, NET_IOBUF_LEN);

        anetNonBlock(NULL, remote->fd);
        ADD_EVENT(remote->re);
        ADD_EVENT(remote->te);
    }

    return remote;
}

void udpRemoteFree(udpRemote *remote) {
    if (!remote) return;

    if (remote->hook.free) remote->hook.free(remote);

    bfree(&remote->buf);

    CLR_EVENT(remote->re);
    CLR_EVENT(remote->te);
    close(remote->fd);

    xs_free(remote);
}

static void udpRemoteReadHandler(event *e) {
    udpRemote *remote = e->data;
    udpClient *client = remote->client;

    char err[ANET_ERR_LEN];

    int readlen = NET_IOBUF_LEN;
    int rfd = remote->fd;
    int nread;
    sockAddrEx remote_addr;

    netSockAddrExInit(&remote_addr);

    // Read from remote
    nread = netUdpRead(err, rfd, remote->buf.data, readlen, NULL);
    if (nread == -1) {
        LOGW("UDP remote read error: %s", err);
        goto error;
    }
    remote->buf.len = nread;

    // Log remote info
    char rip[HOSTNAME_MAX_LEN];
    int rport;
    netIpPresentBySockAddr(NULL, rip, sizeof(rip), &rport, &client->sa_remote);
    LOGD("UDP remote read from [%s:%d] ", rip, rport);

    // Process buffer
    if (remote->hook.process && remote->hook.process(remote) == UDP_ERR) goto error;

    // Write to client
    int cfd = client->server->fd;
    int nwrite;

    nwrite = netUdpWrite(err, cfd, remote->buf.data + remote->buf_off, remote->buf.len - remote->buf_off,
                         &client->sa_client);
    if (nwrite == -1) {
        LOGW("UDP remote write error: %s", err);
        goto error;
    }

error:
    udpConnectionFree(client);
}

static void udpRemoteReadTimeHandler(event *e) {
    udpRemote *remote = e->data;

    LOGN("UDP remote read timeout");

    udpConnectionFree(remote->client);
}
