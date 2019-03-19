
#include "module.h"
#include "module_tcp.h"
#include "module_udp.h"

#include "../protocol/socks5.h"
#include "../protocol/tcp_shadowsocks.h"
#include "../protocol/udp_shadowsocks.h"

typedef struct server {
    module mod;
    tcpServer *ts;
    udpServer *us;
} server;

static void serverInit();
static void serverRun();
static void serverExit();

static void tcpServerOnAccept(void *data);
static void tcpClientOnRead(void *data);
static void tcpRemoteOnConnect(void *data, int status);

static void udpServerOnRead(void *data);

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
    if (app->config->mode & MODE_TCP_ONLY)
        s.ts = tcpServerNew(app->config->remote_addr, app->config->remote_port, tcpServerOnAccept);
    if (app->config->mode & MODE_UDP_ONLY)
        s.us = udpServerNew(app->config->remote_addr, app->config->remote_port,
                            CONN_TYPE_SHADOWSOCKS, udpServerOnRead);

    if (!s.ts && !s.us) exit(EXIT_ERR);

    if (s.ts) LOGN("TCP server listen at: %s", s.ts->ln->addrinfo);
    if (s.us) LOGN("UDP server listen at: %s", s.us->conn->addrinfo);
}

static void serverExit() {
    tcpServerFree(s.ts);
    udpServerFree(s.us);
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

        socks5AddrParse(conn_client->addrbuf_dest->data, conn_client->addrbuf_dest->len, NULL, host,
                        &host_len, &port);

        LOGD("TCP client proxy dest addr: %s:%d", host, port);

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
        LOGW("TCP remote %s connect error: %s", CONN_GET_ADDRINFO(client->conn),
             remote->conn->errstr);
        return;
    }
    LOGD("TCP remote %s connect success", CONN_GET_ADDRINFO(client->conn));

    // Write shadowsocks client handshake left buffer
    if (client->conn->rbuf_off) {
        int nwrite = TCP_WRITE(remote->conn, client->conn->rbuf, client->conn->rbuf_off);
        if (nwrite == TCP_ERR)
            return;
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

static void udpServerOnRead(void *data) {
    udpServer *server = data;
    udpShadowsocksConn *conn = (udpShadowsocksConn *)server->conn;
    udpClient *client;
    udpRemote *remote;

    char buf[NET_IOBUF_LEN];
    int buf_len = sizeof(buf);
    int nread;

    char host[HOSTNAME_MAX_LEN];
    int host_len = sizeof(host);
    int port;

    char cip[HOSTNAME_MAX_LEN];
    int cip_len = sizeof(cip);
    int cport;

    if ((client = udpClientNew(server)) == NULL) return;

    nread = UDP_READ(server->conn, buf, buf_len, &client->sa_client);
    if (nread == UDP_ERR) goto error;

    if (netIpPresentBySockAddr(NULL, cip, cip_len, &cport, &client->sa_client) == NET_OK)
        LOGD("UDP server read from %s:%d", cip, cport);

    socks5AddrParse(conn->addrbuf_dest->data, conn->addrbuf_dest->len, NULL, host, &host_len,
                    &port);

    LOGD("UDP client proxy dest addr: %s:%d", host, port);

    remote = udpRemoteNew(client, CONN_TYPE_RAW, host, port);
    if (!remote) goto error;

    UDP_WRITE(remote->conn, buf, nread, &client->sa_remote);

    return;

error:
    udpConnectionFree(client);
}
