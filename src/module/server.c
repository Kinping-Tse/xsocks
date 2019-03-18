
#include "module.h"
#include "module_tcp.h"
#include "module_udp.h"

#include "../protocol/socks5.h"
#include "../protocol/tcp_shadowsocks.h"

typedef struct server {
    module mod;
    tcpServer *ts;
    udpServer *us;
} server;

static void serverInit();
static void serverRun();
static void serverExit();

static void tcpServerInit();
static void tcpServerExit();

static void tcpServerOnAccept(void *data);
static void tcpClientOnRead(void *data);
static void tcpRemoteOnConnect(void *data, int status);

static void udpServerInit();
static void udpServerExit();

static int udpServerHookProcess(void *data);
static int udpRemoteHookProcess(void *data);

static server s;
module *app = (module *)&s;

int main(int argc, char *argv[]) {
    moduleHook hook = {
        .init = serverInit,
        .run = serverRun,
        .exit = serverExit,
    };

    return moduleMain(MODULE_SERVER, hook, app, argc, argv);
}

static void serverInit() {
    getLogger()->syslog_ident = "xs-server";
}

static void serverRun() {
    if (app->config->mode & MODE_TCP_ONLY) tcpServerInit();
    if (app->config->mode & MODE_UDP_ONLY) udpServerInit();

    if (!s.ts && !s.us) exit(EXIT_ERR);

    char addr_info[ADDR_INFO_STR_LEN];

    if (s.ts) LOGN("TCP server listen at: %s", s.ts->ln->addrinfo);

    if (s.us && anetFormatSock(s.us->fd, addr_info, ADDR_INFO_STR_LEN) > 0) LOGN("UDP server read at: %s", addr_info);
}

static void serverExit() {
    tcpServerExit();
    udpServerExit();
}

static void tcpServerInit() {
    s.ts = tcpServerNew(app->config->remote_addr, app->config->remote_port, tcpServerOnAccept);
}

static void tcpServerExit() {
    tcpServerFree(s.ts);
}

static void tcpServerOnAccept(void *data) {
    tcpServer *server = data;
    tcpClient *client = tcpClientNew(server, CONN_TYPE_SHADOWSOCKS, tcpClientOnRead);
    if (client) {
        tcpConn *conn = client->conn;
        LOGD("TCP server accepted client %s", conn->addrinfo_peer);
        LOGD("TCP client current count: %d", ++server->client_count);
    }
}

static void tcpClientOnRead(void *data) {
    tcpClient *client = data;
    tcpRemote *remote = client->remote;
    tcpShadowsocksConn *conn_client = (tcpShadowsocksConn *)client->conn;

    if (!client->remote) {
        int nread = TCP_READ(client->conn, client->conn->rbuf, client->conn->rbuf_len);
        if (nread <= 0) return;

        char host[HOSTNAME_MAX_LEN];
        int host_len = sizeof(host);
        int port;

        socks5AddrParse(conn_client->addrbuf_dest->data, conn_client->addrbuf_dest->len,
                        NULL, host, &host_len, &port);
        client->remote = tcpRemoteNew(client, CONN_TYPE_RAW, host, port, tcpRemoteOnConnect);
        if (!client->remote) {
            tcpConnectionFree(client);
            return;
        }

        // Rewind rbuffer
        int rbuf_off = conn_client->addrbuf_dest->len;
        int rbuf_len = nread - rbuf_off;

        if (rbuf_len > 0) {
            memmove(client->conn->rbuf, client->conn->rbuf + rbuf_off, rbuf_len);
            client->conn->rbuf_off += rbuf_len;
        }
    } else {
        tcpPipe(client->conn, remote->conn);
    }
}

static void tcpRemoteOnConnect(void *data, int status) {
    tcpRemote *remote = data;
    tcpClient *client = remote->client;

    if (status == TCP_ERR) {
        LOGW("TCP remote %s connect error: %s", TCP_GET_ADDRINFO(client->conn), remote->conn->errstr);
        return;
    }
    LOGD("TCP remote %s connect success", TCP_GET_ADDRINFO(client->conn));

    // Write shadowsocks client handshake left buffer
    if (client->conn->rbuf_off) {
        int nwrite = TCP_WRITE(remote->conn, client->conn->rbuf, client->conn->rbuf_off);
        if (nwrite == TCP_ERR) return;
        else if (nwrite < client->conn->rbuf_off) {
            tcpConnectionFree(client);
            return;
        }
        client->conn->rbuf_off = 0;
    }

    // Prepare pipe
    ADD_EVENT_READ(remote->conn);
    ADD_EVENT_READ(client->conn);
}

static void udpServerInit() {
    udpHook hook = {.init = NULL, .process = udpServerHookProcess, .free = NULL};
    s.us = moduleUdpServerCreate(app->config->remote_addr, app->config->remote_port, hook, NULL);
}

static void udpServerExit() {
    moduleUdpServerFree(s.us);
}

static int udpServerHookProcess(void *data) {
    udpClient *client = data;
    udpRemote *remote;

    char err[ANET_ERR_LEN];
    int buflen = NET_IOBUF_LEN;

    // Decrypt client buffer
    if (app->crypto->decrypt_all(&client->buf, app->crypto->cipher, buflen)) {
        LOGW("UDP server decrypt buffer error");
        return UDP_ERR;
    }

    // Get remote addr from the buffer
    char rip[HOSTNAME_MAX_LEN];
    int rip_len = sizeof(rip);
    int rport;
    int raddr_len;

    if ((raddr_len = socks5AddrParse(client->buf.data, client->buf.len, NULL, rip, &rip_len, &rport)) == -1) {
        LOGW("UDP server parse buffer error");
        return UDP_ERR;
    }

    if (netUdpGetSockAddrEx(err, rip, rport, app->config->ipv6_first, &client->sa_remote) == NET_ERR) {
        LOGW("Get UDP remote sockaddr error: %s", err);
        return UDP_ERR;
    }

    // Log the remote info
    char raddr_info[ADDR_INFO_STR_LEN];
    anetFormatAddr(raddr_info, ADDR_INFO_STR_LEN, rip, rport);
    LOGI("UDP client request addr: %s", raddr_info);

    // Prepare remote
    udpHook hook = {.init = NULL, .process = udpRemoteHookProcess, .free = NULL};
    if ((remote = udpRemoteCreate(&hook, NULL)) == NULL) return UDP_ERR;

    remote->client = client;

    client->remote = remote;
    client->buf_off = raddr_len;

    return UDP_OK;
}

static int udpRemoteHookProcess(void *data) {
    udpRemote *remote = data;
    udpClient *client = remote->client;

    int buflen = NET_IOBUF_LEN;

    // Append address buffer
    char rip[HOSTNAME_MAX_LEN];
    int rport;
    buffer_t addr_buf;
    sds sbuf;
    int addr_len;

    netIpPresentBySockAddr(NULL, rip, sizeof(rip), &rport, &client->sa_remote);
    sbuf = socks5AddrInit(NULL, rip, rport);
    addr_len = sdslen(sbuf);

    bzero(&addr_buf, sizeof(addr_buf));
    balloc(&addr_buf, addr_len);
    memcpy(addr_buf.data, sbuf, addr_len);
    addr_buf.len = addr_len;

    bprepend(&remote->buf, &addr_buf, buflen);

    sdsfree(sbuf);
    bfree(&addr_buf);

    // Encrypt remote buffer
    if (app->crypto->encrypt_all(&remote->buf, app->crypto->cipher, buflen)) {
        LOGW("UDP remote encrypt buffer error");
        return UDP_ERR;
    }

    return UDP_OK;
}
