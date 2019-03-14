
#include "module.h"
#include "module_tcp.h"

#include "../protocol/socks5.h"

#include "redis/anet.h"

typedef struct server {
    module mod;
    tcpServer *ts;
} server;

static void redirInit();
static void redirRun();
static void redirExit();

static void tcpServerInit();
static void tcpServerExit();

static int tcpClientReadHandler(tcpClient *client);

static server s;
module *app = (module *)&s;

int main(int argc, char *argv[]) {
    moduleHook hook = {
        .init = redirInit,
        .run = redirRun,
        .exit = redirExit,
    };

    return moduleMain(MODULE_REDIR, hook, app, argc, argv);
}

static void redirInit() {
    getLogger()->syslog_ident = "xs-redir";

    if (app->config->mode & MODE_TCP_ONLY) tcpServerInit();

    if (app->config->mode & MODE_UDP_ONLY) {
        LOGW("Only support TCP now!");
        LOGW("UDP mode is not working!");
    }

    if (!s.ts) exit(EXIT_ERR);
}

static void redirRun() {
    char addr_info[ADDR_INFO_STR_LEN];

    if (s.ts && anetFormatSock(s.ts->fd, addr_info, ADDR_INFO_STR_LEN) > 0) LOGN("TCP server listen at: %s", addr_info);
}

static void redirExit() {
    tcpServerExit();
}

static void tcpServerInit() {
    s.ts = moduleTcpServerCreate(app->config->local_addr, app->config->local_port, tcpClientReadHandler);
}

static void tcpServerExit() {
    moduleTcpServerFree(s.ts);
}

static int tcpClientReadHandler(tcpClient *client) {
    tcpRemote *remote = client->remote;

    char err[ANET_ERR_LEN];
    int buflen = NET_IOBUF_LEN;

    int cfd = client->fd;
    int nread;

    // Read client buffer
    nread = read(cfd, client->buf.data, buflen);
    if (nread == -1) {
        if (errno == EAGAIN) return TCP_OK;

        LOGW("TCP client [%s] read error: %s", client->client_addr_info, STRERR);
        return TCP_ERR;
    } else if (nread == 0) {
        LOGD("TCP client [%s] closed connection", client->client_addr_info);
        return TCP_ERR;
    }
    client->buf.len = nread;

    if (client->stage == STAGE_INIT) {
        sockAddrEx sa;
        char ip[HOSTNAME_MAX_LEN];
        int port;

        if (netTcpGetDestSockAddr(err, cfd, app->config->ipv6_first, &sa) == NET_ERR) {
            LOGW("TCP client get dest sockaddr error: %s", err);
            return TCP_ERR;
        }

        if (netIpPresentBySockAddr(err, ip, sizeof(ip), &port, &sa) == NET_ERR) {
            LOGW("TCP client get dest addr error: %s", err);
            return TCP_ERR;
        }

        anetFormatAddr(client->remote_addr_info, ADDR_INFO_STR_LEN, ip, port);
        LOGI("TCP client (%d) [%s] request remote addr [%s]", client->fd, client->client_addr_info,
             client->remote_addr_info);

        sds addr = socks5AddrInit(NULL, ip, port);
        int addr_len = sdslen(addr);
        buffer_t addr_buf;
        bzero(&addr_buf, sizeof(addr_buf));
        balloc(&addr_buf, sdslen(addr));
        memcpy(addr_buf.data, addr, addr_len);
        addr_buf.len = addr_len;

        bprepend(&client->buf, &addr_buf, buflen);

        sdsfree(addr);
        bfree(&addr_buf);
    }

    // Do buffer encrypt
    if (app->crypto->encrypt(&client->buf, client->e_ctx, buflen)) {
        LOGW("TCP client encrypt buffer error");
        return TCP_ERR;
    }

    if (client->stage == STAGE_INIT) {
        remote = tcpRemoteCreate(app->config->remote_addr, app->config->remote_port);
        LOGD("TCP remote (%d) [%s] is connecting ...", remote->fd, client->remote_addr_info);

        remote->client = client;

        client->remote = remote;
        client->stage = STAGE_HANDSHAKE;

        DEL_EVENT(client->re);

        s.ts->remote_count++;
        LOGD("TCP remote current count: %d", s.ts->remote_count);
        return TCP_OK;
    }

    // Write to remote
    int nwrite;
    int rfd = remote->fd;

    if (client->stage == STAGE_HANDSHAKE) {
        LOGD("TCP remote (%d) [%s] connect success", rfd, client->remote_addr_info);
        client->stage = STAGE_STREAM;

        anetDisableTcpNoDelay(NULL, client->fd);
        anetDisableTcpNoDelay(NULL, remote->fd);
    }

    nwrite = write(rfd, client->buf.data, client->buf.len);
    if (nwrite == -1) {
        if (errno == EAGAIN) goto write_again;

        LOGW("TCP remote (%d) [%s] write error: %s", rfd, client->remote_addr_info, STRERR);
        return TCP_ERR;
    } else if (nwrite < (int)client->buf.len) {
        client->buf_off = nwrite;
        goto write_again;
    }

    return TCP_OK;

write_again:
    DEL_EVENT(client->re);
    DEL_EVENT(remote->re);
    ADD_EVENT(remote->we);
    return TCP_OK;
}
