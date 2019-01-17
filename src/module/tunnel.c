
#include "module.h"

#include "sds.h"
#include "anet.h"

/*
c: client lo: local r: remote
lc: localClient rs: remoteServer
ss: shadowsocks
client                      local                    remote
1. get addr by config
2. ss req:                       rs enc(addr) --------->
3. ss stream: (raw)------> lc enc(raw) -> rs  (enc_buf)-------->
4. ss stream: <-----(raw) lc <- dec(enc_buf) rs <--------------(enc_buf)
5. (3.4 loop).....
*/

static module tunnel;
static module *app = &tunnel;

typedef struct localClient {
    int fd;
    int stage;
    event *re;
    sds buf;
    sds addr_buf;
    struct remoteServer *remote;
    cipher_ctx_t *e_ctx;
    char dest_addr_info[ADDR_INFO_STR_LEN];
} localClient;

typedef struct remoteServer {
    int fd;
    event *re;
    localClient *client;
    cipher_ctx_t *d_ctx;
} remoteServer;

void remoteServerReadHandler(event *e);
void localClientReadHandler(event *e);

localClient *newClient(int fd) {
    localClient *client = xs_calloc(sizeof(*client));

    anetNonBlock(NULL, fd);

    event* re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, localClientReadHandler, client);
    eventAdd(app->el, re);

    client->fd = fd;
    client->re = re;
    // client->stage = STAGE_INIT;
    client->addr_buf = NULL;
    client->buf = sdsempty();

    client->e_ctx = xs_calloc(sizeof(*client->e_ctx));
    app->crypto->ctx_init(app->crypto->cipher, client->e_ctx, 1);

    return client;
}

void freeClient(localClient *client) {
    sdsfree(client->buf);
    sdsfree(client->addr_buf);
    eventDel(client->re);
    eventFree(client->re);
    close(client->fd);

    app->crypto->ctx_release(client->e_ctx);
    xs_free(client->e_ctx);

    xs_free(client);
}

remoteServer *newRemote(int fd) {
    remoteServer *remote = xs_calloc(sizeof(*remote));

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);

    event *re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, remoteServerReadHandler, remote);
    eventAdd(app->el, re);

    remote->re = re;
    remote->fd = fd;

    remote->d_ctx = xs_calloc(sizeof(*remote->d_ctx));
    app->crypto->ctx_init(app->crypto->cipher, remote->d_ctx, 0);

    return remote;
}

void freeRemote(remoteServer *remote) {
    eventDel(remote->re);
    eventFree(remote->re);
    close(remote->fd);

    app->crypto->ctx_release(remote->d_ctx);
    xs_free(remote->d_ctx);

    xs_free(remote);
}

static void initTunnel() {
    getLogger()->syslog_ident = "xs-tunnel";

    if (app->config->mode & MODE_TCP_ONLY) {
        LOGE("Only support UDP now!");
        exit(EXIT_ERR);
    }

    if (app->config->mode & MODE_UDP_ONLY) {
        char err[ANET_ERR_LEN];
        char *host = app->config->local_addr;
        int port = app->config->local_port;
        int fd;

        if (host && strchr(host, ':'))
            fd = netUdp6Server(err, port, host);
        else
            fd = netUdpServer(err, port, host);

        if (fd == ANET_ERR) {
            FATAL("Could not create UDP socket %s:%d: %s",
                  host ? host : "*", port, err);
        }

        localClient *client = newClient(fd);
        remoteServer *remote = newRemote(-1);
        client->remote = remote;
        remote->client = client;
    }
}

void remoteServerReadHandler(event *e) {
    remoteServer *remote = e->data;
    UNUSED(remote);
}

void localClientReadHandler(event *e) {
    localClient *client = e->data;

    char buf[NET_IOBUF_LEN] = {0};
    int nread = recvfrom(client->fd, buf, NET_IOBUF_LEN, 0, NULL, NULL);

    LOGD("%d", nread);

    DUMP(buf, nread);
}

int main(int argc, char *argv[]) {
    moduleHook hook = {initTunnel, NULL, NULL};

    moduleInit(MODULE_TUNNEL, hook, &tunnel, argc, argv);
    moduleRun();
    moduleExit();

    return EXIT_OK;
}
