
#include "../protocol/tcp_raw.h"

#include <getopt.h>

typedef struct server {
    char *host;
    int port;
    eventLoop *el;
    int timeout;
} server;

static server s;
static server *app = &s;

typedef struct tcpServer {
    tcpListener *ln;
} tcpServer;

typedef struct tcpClient {
    tcpRawConn *conn;
} tcpClient;

static void initServer();
static void parseOptions(int argc, char *argv[]);
static void usage();

static tcpServer *tcpServerNew();
static void tcpServerFree(tcpServer *server);
static void tcpOnAccept(void *data);

static tcpClient *tcpClientNew(int fd);
static void tcpClientFree(tcpClient *client);
static void tcpOnRead(void *data);
static void tcpOnWrite(void *data);
static void tcpOnTime(void *data);

int main(int argc, char *argv[]) {
    initServer();
    parseOptions(argc, argv);

    // Listen
    tcpServer *server = tcpServerNew();
    if (server == NULL) {
        exit(EXIT_ERR);
    }

    LOGD("Use event type: %s", eventGetApiName());
    LOGI("Use server addr: %s:%d", app->host, app->port);
    LOGI("Use timeout: %ds", app->timeout);

    // Run the loop
    eventLoopRun(app->el);

    tcpServerFree(server);

    return EXIT_OK;
}

static void initServer() {
    app->host = "127.0.0.1";
    app->timeout = 60;
    app->port = 19999;

    setupIgnoreHandlers();
    setupSigsegvHandlers();

    logger *log = getLogger();
    log->level = LOGLEVEL_DEBUG;
    log->color_enabled = 1;
    log->file_line_enabled = 0;

    app->el = eventLoopNew();
}

static void parseOptions(int argc, char *argv[]) {
    int help = 0;
    int opt;

    struct option long_options[] = {
        { "help",  no_argument,  NULL, 'h' },
        { NULL,    0,            NULL, 0   },
    };

    while ((opt = getopt_long(argc, argv, "s:p:t:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 's': app->host = optarg; break;
            case 'p': app->port = atoi(optarg); break;
            case 't': app->timeout = atoi(optarg); break;
            case 'h': help = 1;  break;
            case '?':
            default:
                help = 1;
                break;
        }
    }

    if (help) {
        usage();
        exit(EXIT_ERR);
    }
}

static void usage() {
    printf(
"Usage: xs-benchmark-server\n\n"
" [-s <hostname>]      Server hostname (default 127.0.0.1)\n"
" [-p <port>]          Server port (default 19999)\n"
" [-t <timeout>]       Socket timeout (default 60)\n"
" [-h, --help]         Print this help\n"
    );
}

tcpServer *tcpServerNew() {
    tcpServer *server = xs_calloc(sizeof(*server));
    if (!server) {
        LOGW("TCP server is NULL, please check the memory");
        return NULL;
    }

    char err[XS_ERR_LEN];
    tcpListener *ln = tcpListen(err, app->el, app->host, app->port, server, tcpOnAccept);
    if (!ln) {
        LOGE(err);
        tcpServerFree(server);
        return NULL;
    }
    server->ln = ln;

    LOGN("TCP server listen at: %s", ln->addrinfo);

    return server;
}

static void tcpServerFree(tcpServer *server) {
    if (!server) return;
    TCP_L_CLOSE(server->ln);
    xs_free(server);
}

static void tcpOnAccept(void *data) {
    tcpServer *server = data;
    tcpClient *client = tcpClientNew(server->ln->fd);
    if (client) {
        tcpConn *conn = (tcpConn *)client->conn;
        LOGD("TCP server accepted client %s", conn->addrinfo_peer);
    }
}

static tcpClient *tcpClientNew(int fd) {
    tcpClient *client = xs_calloc(sizeof(*client));
    if (!client) {
        LOGW("TCP client is NULL, please check the memory");
        return NULL;
    }

    char err[XS_ERR_LEN];
    tcpConn *conn = tcpAccept(err, app->el, fd, app->timeout, client);
    if (!conn) {
        LOGW(err);
        tcpClientFree(client);
        return NULL;
    }
    client->conn = tcpRawConnNew(conn);

    TCP_ON_READ(conn, tcpOnRead);
    TCP_ON_WRITE(conn, tcpOnWrite);
    TCP_ON_TIME(conn, tcpOnTime);

    ADD_EVENT_READ(conn);
    DEL_EVENT_WRITE(conn);
    ADD_EVENT_TIME(conn);

    return client;
}

static void tcpClientFree(tcpClient *client) {
    if (!client) return;
    TCP_CLOSE(client->conn);
    xs_free(client);
}

static void tcpOnRead(void *data) {
    tcpClient *client = data;
    tcpConn *conn = (tcpConn *)client->conn;

    DEL_EVENT_TIME(conn);

    int nread;
    char *addrinfo = tcpGetAddrinfo(conn);

    nread = tcpPipe(conn, conn);
    if (nread == TCP_ERR) {
        LOGW("TCP client %s pipe error: %s", addrinfo, conn->errstr);
        goto error;
    } else if (nread == 0) {
        LOGD("TCP client %s closed connection", addrinfo);
        goto error;
    }

    ADD_EVENT_TIME(conn);
    return;

error:
    tcpClientFree(client);
}

static void tcpOnWrite(void *data) {
    tcpClient *client = data;
    tcpConn *conn = (tcpConn *)client->conn;

    char *wbuf = conn->wbuf;
    int wbuf_len = conn->wbuf_len;
    int nwrite;

    // Write done
    if (wbuf_len == 0) {
        conn->rbuf_off = 0;
        conn->wbuf = NULL;
        conn->wbuf_len = 0;
        ADD_EVENT_READ(conn);
        DEL_EVENT_WRITE(conn);
        return;
    }

    nwrite = TCP_WRITE(conn, wbuf, wbuf_len);
    if (nwrite == TCP_ERR) {
        tcpClientFree(client);
        return;
    }

    conn->wbuf += nwrite;
    conn->wbuf_len -= nwrite;

    ADD_EVENT_WRITE(conn);
}

static void tcpOnTime(void *data) {
    tcpClient *client = data;
    tcpConn *conn = (tcpConn *)client->conn;

    LOGI("TCP client %s read timeout", conn->addrinfo_peer);

    tcpClientFree(client);
}
