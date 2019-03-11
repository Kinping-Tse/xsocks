
#include "../protocol/tcp_raw.h"
#include "../protocol/tcp_shadowsocks.h"

#include <getopt.h>

enum {
    GETOPT_VAL_KEY = 300,
    GETOPT_VAL_TYPE,
    GETOPT_VAL_KEEPALIVE,
};

enum {
    CLIENT_TYPE_SHADOWSOCKS = 1,
    CLIENT_TYPE_RAW,
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
static void tcpClientOnTimeout(void *data);

int main(int argc, char *argv[]) {
    initClient();
    parseOptions(argc, argv);

    netHostPortParse(app->tunnel_address, app->tunnel_addr, &app->tunnel_port);
    app->crypto = crypto_init(app->password, app->key, app->method);
    app->duration_list = xs_calloc(sizeof(double) * app->requests);

    LOGI("Use client type: %s", app->type_str);
    LOGD("Use event type: %s", eventGetApiName());
    LOGI("Use remote addr: %s:%d", app->remote_addr, app->remote_port);
    LOGI("Use timeout: %ds", app->timeout);
    LOGI("Use requests: %d", app->requests);
    LOGI("Use clients: %d", app->numclients);
    LOGI("Use buffer size: %d", app->buf_size);
    LOGI("Use keepalive: %d", app->keepalive);

    if (app->type == CLIENT_TYPE_SHADOWSOCKS) {
        LOGD("Use tunnel addr: %s:%d", app->tunnel_addr, app->tunnel_port);
        LOGD("Use crypto method: %s", app->method);
        LOGD("Use crypto password: %s", app->password);
        LOGD("Use crypto key: %s", app->key);
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
    app->type = CLIENT_TYPE_SHADOWSOCKS;
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
    log->color_enabled = 1;

    app->el = eventLoopNew();
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
                    app->type = CLIENT_TYPE_RAW;
                } else if (strcmp(optarg, "shadowsocks") == 0) {
                    app->type_str = optarg;
                    app->type = CLIENT_TYPE_SHADOWSOCKS;
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
    printf("Usage: xs-benchmark-client\n\n"
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
    int i, curlat = 0;
    double perc, reqpersec, bytesec;

    reqpersec = app->requests_finished / app->duration;
    bytesec = (uint64_t)app->requests_finished * app->buf_size / app->duration;

    LOGIR("\n====== %s ======\n", app->title);
    LOGIR("  %d requests completed in %.5f seconds\n", app->requests_finished, app->duration);
    LOGIR("  %d parallel clients\n", app->numclients);
    LOGIR("  %d bytes payload\n", app->buf_size);
    LOGIR("  keep alive: %d\n", app->keepalive);
    LOGIR("\n");

    qsort(app->duration_list, app->requests, sizeof(double), compareLatency);
    for (i = 0; i < app->requests; i++) {
        if (app->duration_list[i] != curlat || i == (app->requests-1)) {
            curlat = app->duration_list[i];
            perc = ((float)(i+1)*100)/app->requests;
            // printf("%.2f%% <= %d milliseconds\n", perc, curlat);
        }
    }
    LOGIR("%.2f requests per second\n\n", reqpersec);
    LOGIR("%.2f MB per second\n\n", bytesec/MB_UNIT);
}

static void showThroughput(event *e) {
    UNUSED(e);

    //////
    // if (app->liveclients == 0 && app->requests_finished != app->requests) {
    //     fprintf(stderr,"All clients disconnected... aborting.\n");
    //     exit(1);
    // }

    double dt = timerStop(app->start_time, SECOND_UNIT, NULL);
    double rps = app->requests_finished / dt;
    double dps = (uint64_t)app->requests_finished * app->buf_size / dt;

    LOGIR("%s: %.2f rps %.2f mbps \r", app->title, rps, dps/MB_UNIT);
}

static tcpClient *tcpClientCreate() {
    tcpClient *client = xs_calloc(sizeof(*client));
    if (!client) {
        LOGW("TCP client is NULL, please check the memory");
        return NULL;
    }

    char err[XS_ERR_LEN];
    tcpConn *conn = tcpConnect(err, app->el, app->remote_addr, app->remote_port, app->timeout, client);
    if (!conn) {
        LOGW("TCP client connect error: %s", err);
        exit(EXIT_ERR);
    }
    client->conn = conn;

    TCP_ON_READ(conn, tcpClientOnRead);
    TCP_ON_WRITE(conn, tcpClientOnWrite);
    TCP_ON_CONN(conn, tcpClientOnConnect);
    TCP_ON_TIME(conn, tcpClientOnTimeout);

    ADD_EVENT_TIME(conn);

    app->liveclients++;

    return client;
}

static void tcpClientFree(tcpClient *client) {
    if (!client) return;

    TCP_CLOSE(client->conn);

    xs_free(client);

    app->liveclients--;
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
    DEL_EVENT_TIME(conn);
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
        tcpClientFree(client);
        exit(EXIT_ERR);
    }

    if (app->type == CLIENT_TYPE_RAW)
        client->conn = (tcpConn *)tcpRawConnNew(conn);
    else
        client->conn = (tcpConn *)tcpShadowsocksConnNew(conn, app->crypto, app->tunnel_addr, app->tunnel_port);

    tcpClientReset(client);
}

static void tcpClientOnTimeout(void *data) {
    tcpClient *client = data;
    tcpConn *conn = client->conn;

    if (tcpIsConnected(conn))
        LOGI("TCP client %s read timeout", conn->addrinfo);
    else
        LOGE("TCP client %s connect timeout", conn->addrinfo);

    tcpClientFree(client);
    exit(EXIT_ERR);
}

static void tcpClientOnRead(void *data) {
    tcpClient *client = data;
    tcpConn *conn = client->conn;

    DEL_EVENT_TIME(conn);

    char *buf = conn->rbuf;
    int buflen = conn->rbuf_len;
    int nread;
    char *addrinfo = conn->addrinfo;

    nread = TCP_READ(conn, buf, buflen);
    if (nread == TCP_AGAIN)
        goto end;
    else if (nread == TCP_ERR) {
        LOGW("TCP client %s read error: %s", addrinfo, conn->errstr);
        goto error;
    } else if (nread == 0) {
        LOGD("TCP client %s closed connection", addrinfo);
        goto error;
    }
    conn->rbuf_off += nread;

    if (buf[0] != 'x') {
        LOGE("Buffer error");
        exit(EXIT_ERR);
    }

    if (conn->rbuf_off == app->buf_size) {
        client->duration = timerStop(client->start_time, SECOND_UNIT, NULL);
        app->duration_list[app->requests_finished++] = client->duration;
        tcpClientDone(client);
        return;
    }

    if (conn->rbuf_off == conn->wbuf_len) {
        ADD_EVENT_WRITE(conn);
        DEL_EVENT_READ(conn);
    }

end:
    ADD_EVENT_TIME(conn);
    return;

error:
    tcpClientFree(client);
    exit(EXIT_ERR);
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
        if (nwrite == TCP_ERR) {
            LOGW("TCP client %s write error: %s", conn->addrinfo, conn->errstr);
            goto error;
        }
        conn->wbuf_len += nwrite;
    }

    ADD_EVENT_READ(conn);
    DEL_EVENT_WRITE(conn);

    return;

error:
    tcpClientFree(client);
    exit(EXIT_ERR);
}
