
#include "../protocol/raw.h"
#include "../protocol/tcp_shadowsocks.h"

#include <getopt.h>

enum {
    GETOPT_VAL_KEY = 300,
    GETOPT_VAL_TYPE,
    GETOPT_VAL_KEEPALIVE,
};

#define KB_UNIT (1024)
#define MB_UNIT (1024*1024)
#define GB_UNIT (1024*1024*1024)

typedef struct client {
    char *password;
    char *method;
    char *key;
    char *remote_addr;
    int remote_port;
    char *tunnel_address;
    char tunnel_addr[HOSTNAME_MAX_LEN];
    int tunnel_port;
    eventLoop *el;
    event *te;
    crypto_t *crypto;
    int buf_size;
    uint64_t start_time;
    double duration;
    double *duration_list;
    int numclients;
    int liveclients;
    int keepalive;
    int requests;
    int requests_issued;
    int requests_finished;
    int timeout;
    int type;
    char *type_str;
    char *title;
} client;

typedef struct tcpClient {
    tcpConn *conn;
    uint64_t start_time;
    double duration;
} tcpClient;

static client c;
static client *app = &c;

static void initClient();
static void parseOptions(int argc, char *argv[]);
static void usage();
static void benchmark();
static void showReport();
static void showThroughput(event *e);

static tcpClient *tcpClientCreate();
static void createMissingClients();
static void tcpClientFree(tcpClient *client);
static void tcpClientDone(tcpClient *client);
static void tcpClientReset(tcpClient *client);

static void tcpClientOnRead(void *data);
static void tcpClientOnWrite(void *data);
static void tcpClientOnConnect(void *data, int status);
static void tcpClientOnClose(void *data);
static void tcpClientOnError(void *data);
static void tcpClientOnTimeout(void *data);

int main(int argc, char *argv[]) {
    initClient();
    parseOptions(argc, argv);

    netHostPortParse(app->tunnel_address, app->tunnel_addr, &app->tunnel_port);
    app->crypto = crypto_init(app->password, app->key, app->method);
    app->duration_list = xs_calloc(sizeof(double) * app->requests);

    LOGI("Use client type: %s", app->type_str);
    LOGI("Use event type: %s", eventGetApiName());
    LOGI("Use remote addr: %s:%d", app->remote_addr, app->remote_port);
    LOGI("Use timeout: %ds", app->timeout);
    LOGI("Use requests: %d", app->requests);
    LOGI("Use clients: %d", app->numclients);
    LOGI("Use buffer size: %d", app->buf_size);
    LOGI("Use keepalive: %d", app->keepalive);

    if (app->type == CONN_TYPE_SHADOWSOCKS) {
        LOGI("Use tunnel addr: %s:%d", app->tunnel_addr, app->tunnel_port);
        LOGI("Use crypto method: %s", app->method);
        LOGI("Use crypto password: %s", app->password);
        LOGI("Use crypto key: %s", app->key);
    }

    benchmark();

    return EXIT_OK;
}

static void initClient() {
    app->title = "XSOCKS";
    app->remote_addr = "127.0.0.1";
    app->remote_port = 8388;
    app->password = "foobar";
    app->method = "aes-256-cfb";
    app->key = NULL;
    app->tunnel_address = "127.0.0.1:19999";
    app->timeout = 60;
    app->buf_size = 1024*500;
    app->type = CONN_TYPE_SHADOWSOCKS;
    app->type_str = "shadowsocks";
    app->requests = 10000;
    app->numclients = 20;
    app->requests_issued = 0;
    app->requests_finished = 0;
    app->start_time = 0;
    app->duration = 0;
    app->keepalive = 1;

    setupIgnoreHandlers();
    setupSigsegvHandlers();

    logger *log = getLogger();
    log->level = LOGLEVEL_DEBUG;
    // log->level = LOGLEVEL_INFO;
    log->color_enabled = 1;
    // log->file_line_enabled = 1;
    log->syslog_ident = "xs-benchmark-client";

    app->el = eventLoopNew(1024*10);
    app->te = NEW_EVENT_REPEAT(250, showThroughput, NULL);
    ADD_EVENT_TIME(app);
}

static void parseOptions(int argc, char *argv[]) {
    int help = 0;
    int opt;

    struct option long_options[] = {
        { "help",       no_argument,        NULL, 'h'                  },
        { "key",        required_argument,  NULL, GETOPT_VAL_KEY       },
        { "type",       required_argument,  NULL, GETOPT_VAL_TYPE      },
        { "keepalive",  required_argument,  NULL, GETOPT_VAL_KEEPALIVE },
        { NULL,         0,                  NULL, 0                    },
    };

    while ((opt = getopt_long(argc, argv, "c:n:s:p:k:m:L:d:t:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c': app->numclients = atoi(optarg); break;
            case 'n': app->requests = atoi(optarg); break;
            case 's': app->remote_addr = optarg; break;
            case 'p': app->remote_port = atoi(optarg); break;
            case 't': app->timeout = atoi(optarg); break;
            case 'k': app->password = optarg; break;
            case 'm': app->method = optarg; break;
            case 'h': help = 1; break;
            case GETOPT_VAL_KEY: app->key = optarg; break;
            case GETOPT_VAL_TYPE:
                if (strcmp(optarg, "raw") == 0) {
                    app->type_str = optarg;
                    app->type = CONN_TYPE_RAW;
                } else if (strcmp(optarg, "shadowsocks") == 0) {
                    app->type_str = optarg;
                    app->type = CONN_TYPE_SHADOWSOCKS;
                } else {
                    LOGW("ignore unknown type: %s, use %s as fallback", optarg, app->type_str);
                }
                break;
            case 'L': app->tunnel_address = optarg; break;
            case 'd': app->buf_size = atoi(optarg); break;
            case GETOPT_VAL_KEEPALIVE: app->keepalive = atoi(optarg); break;
            case '?':
            default: help = 1; break;
        }
    }

    if (help) {
        usage();
        exit(EXIT_ERR);
    }
}

static void usage() {
    printf("Usage: xs-benchmark-client [options]\n\n"
           "Options:\n"
           " [-s <hostname>]          Server hostname (default 127.0.0.1)\n"
           " [-p <port>]              Server port (default 18388)\n"
           " [-k <password>]          Server password (default foobar)\n"
           " [-m <encrypt_method>]    Server encrypt method (default aes-256-cfb)\n"
           " [--key <key_in_base64>]  Server key (default null)\n"
           " [-c <clients>]           Number of parallel connections (default 20)\n"
           " [-n <requests>]          Total number of requests (default 10000)\n"
           " [-L <addr>:<port>]       Tunnel address and port (default 127.0.0.1:19999)\n"
           " [--keepalive <boolean>]  1=keep alive 0=reconnect (default 1)\n"
           " [-t <timeout>]           Socket timeout (default 60 s)\n"
           " [-d <size>]              Data size (bytes) of one client request (default 500KB)\n"
           " [--type <client_type>]   Client connection type (default shadowsocks),\n"
           "                          support type: [shadowsocks, raw]\n"
           " [-h, --help]             Print this help\n");
}

void freeAllClients() {}

static void benchmark() {
    tcpClientCreate();
    createMissingClients();

    app->start_time = timerStart();
    eventLoopRun(app->el);
    app->duration = timerStop(app->start_time, SECOND_UNIT, NULL);

    showReport();
    freeAllClients();
}

static int compareLatency(const void *a, const void *b) {
    return (*(double *)a)-(*(double *)b);
}

static void showReport() {
    // int i, curlat = 0;
    double perc, reqpersec, bytesec;

    reqpersec = app->requests_finished / app->duration;
    bytesec = (uint64_t)app->requests_finished * app->buf_size / app->duration;

    LOGIR("\n====== %s ======\n", app->title);
    LOGIR("  %d requests completed in %.5f seconds\n", app->requests_finished, app->duration);
    LOGIR("  %d parallel clients\n", app->numclients);
    LOGIR("  %d bytes payload\n", app->buf_size);
    LOGIR("  keep alive: %d\n", app->keepalive);
    LOGIR("\n");

    UNUSED(perc);
    qsort(app->duration_list, app->requests, sizeof(double), compareLatency);
    // for (i = 0; i < app->requests; i++) {
    //     if (app->duration_list[i] != curlat || i == (app->requests-1)) {
    //         curlat = app->duration_list[i];
    //         perc = ((float)(i+1)*100)/app->requests;
    //         printf("%.2f%% <= %d milliseconds\n", perc, curlat);
    //     }
    // }
    LOGIR("%.2f requests per second\n\n", reqpersec);
    LOGIR("%.2f MB per second\n\n", bytesec/MB_UNIT);
}

static void showThroughput(event *e) {
    UNUSED(e);

    if (app->liveclients == 0 && app->requests_finished != app->requests) {
        LOGE("All clients disconnected... aborting");
        exit(EXIT_ERR);
    }

    double dt = timerStop(app->start_time, SECOND_UNIT, NULL);
    double rps = app->requests_finished / dt;
    double dps = (uint64_t)app->requests_finished * app->buf_size / dt;

    LOGIR("%s: %.2f rps %.2f mbps \r", app->title, rps, dps/MB_UNIT);
}

static tcpClient *tcpClientCreate() {
    char err[XS_ERR_LEN];
    tcpClient *client;
    tcpConn *conn;

    if ((client = xs_calloc(sizeof(*client))) == NULL) {
        LOGW("TCP client is NULL, please check the memory");
        return NULL;
    }

    conn = tcpConnect(err, app->el, app->remote_addr, app->remote_port, app->timeout, client);
    if (!conn) {
        LOGW("TCP client connect error: %s", err);
        exit(EXIT_ERR);
    }
    if (app->type == CONN_TYPE_RAW)
        client->conn = (tcpConn *)tcpRawConnNew(conn);
    else {
        client->conn = (tcpConn *)tcpShadowsocksConnNew(conn, app->crypto);
        tcpShadowsocksConnInit((tcpShadowsocksConn *)client->conn, app->tunnel_addr, app->tunnel_port);
    }

    CONN_ON_READ(client->conn, tcpClientOnRead);
    CONN_ON_WRITE(client->conn, tcpClientOnWrite);
    CONN_ON_CONNECT(client->conn, tcpClientOnConnect);
    CONN_ON_CLOSE(client->conn, tcpClientOnClose);
    CONN_ON_ERROR(client->conn, tcpClientOnError);
    CONN_ON_TIMEOUT(client->conn, tcpClientOnTimeout);

    app->liveclients++;
    LOGI("TCP client current count: %d", app->liveclients);

    return client;
}

static void tcpClientFree(tcpClient *client) {
    if (!client) return;

    CONN_CLOSE(client->conn);

    xs_free(client);

    app->liveclients--;
    LOGI("TCP client current count: %d", app->liveclients);
}

static void tcpClientDone(tcpClient *client) {
    if (app->requests_finished == app->requests) {
        LOGD("All done");
        eventLoopStop(app->el);
        return;
    }

    if (app->keepalive) {
        tcpClientReset(client);
    } else {
        tcpClientFree(client);
        createMissingClients();
    }
}

static void tcpClientReset(tcpClient *client) {
    tcpConn *conn = client->conn;

    DEL_EVENT_READ(conn);
    ADD_EVENT_WRITE(conn);
    conn->wbuf_len = 0;
    conn->rbuf_off = 0;
}

static void createMissingClients() {
    int n = 0;

    while (app->liveclients < app->numclients) {
        tcpClientCreate();

        /* Listen backlog is quite limited on most systems */
        if (++n > 64) {
            usleep(50000);
            n = 0;
        }
    }
}

static void tcpClientOnConnect(void *data, int status) {
    tcpClient *client = data;
    tcpConn *conn = client->conn;

    if (status == TCP_ERR) {
        LOGE("TCP client connect error: %s", conn->errstr);
        exit(EXIT_ERR);
    }

    LOGD("TCP client %s connect success", conn->addrinfo);
    LOGI("TCP client current count: %d", app->liveclients);

    tcpClientReset(client);
}

static void tcpClientOnClose(void *data) {
    tcpClient *client = data;
    tcpConn *conn = client->conn;

    LOGD("TCP client %s closed connection", CONN_GET_ADDRINFO(conn));

    tcpClientFree(client);
    createMissingClients();
}

static void tcpClientOnError(void *data) {
    tcpClient *client = data;
    tcpConn *conn = client->conn;

    LOGW("TCP client %s %s error: %s", conn->addrinfo,
         conn->err == TCP_ERROR_READ ? "read" : "write", conn->errstr);
}

static void tcpClientOnTimeout(void *data) {
    tcpClient *client = data;
    tcpConn *conn = client->conn;

    if (tcpIsConnected(conn))
        LOGI("TCP client %s read timeout", conn->addrinfo);
    else
        LOGE("TCP client %s connect timeout", conn->addrinfo);
}

static void tcpClientOnRead(void *data) {
    tcpClient *client = data;
    tcpConn *conn = client->conn;

    char *buf = conn->rbuf;
    int buflen = conn->rbuf_len;
    int nread;

    bzero(buf, buflen);
    nread = TCP_READ(conn, buf, buflen);
    if (nread <= 0) return;

    conn->rbuf_off += nread;

    if (buf[0] != 'x') {
        LOGE("Buffer error");
        exit(EXIT_ERR);
    }

    if (conn->rbuf_off >= app->buf_size) {
        if (conn->rbuf_off > app->buf_size) LOGW("The server write more buffer");

        client->duration = timerStop(client->start_time, SECOND_UNIT, NULL);
        app->duration_list[app->requests_finished++] = client->duration;
        tcpClientDone(client);
        return;
    }

    if (conn->rbuf_off == conn->wbuf_len) {
        ADD_EVENT_WRITE(conn);
        DEL_EVENT_READ(conn);
    }
}

static void tcpClientOnWrite(void *data) {
    tcpClient *client = data;
    tcpConn *conn = client->conn;

    if (conn->wbuf_len == 0) {
        if (app->requests_issued++ >= app->requests) {
            tcpClientFree(client);
            return;
        }
        client->start_time = timerStart();
    }

    int nwrite;
    char buf[NET_IOBUF_LEN];
    int buflen = MIN((int)sizeof(buf), app->buf_size - conn->wbuf_len);

    if (buflen > 0) {
        memset(buf, 'x', buflen);

        nwrite = TCP_WRITE(conn, buf, buflen);
        if (nwrite <= 0) return;

        conn->wbuf_len += nwrite;
    }

    ADD_EVENT_READ(conn);
    DEL_EVENT_WRITE(conn);
}
