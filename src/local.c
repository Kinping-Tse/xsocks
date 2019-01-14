
#include "common.h"
#include "config.h"
#include "event/event.h"
#include "utils.h"
#include "net.h"

#include "anet.h"
#include "sds.h"
#include "socks5.h"
#include "crypto.h"

#define _usage() usage(MODULE_LOCAL)

#define STAGE_ERROR     -1  /* Error detected                   */
#define STAGE_INIT       0  /* Initial stage                    */
#define STAGE_HANDSHAKE  1  /* Handshake with client            */
#define STAGE_SNI        3  /* Parse HTTP/SNI header            */
#define STAGE_RESOLVE    4  /* Resolve the hostname             */
#define STAGE_STREAM     5  /* Stream between client and server */

struct local {
    xsocksConfig *config;
    eventLoop *el;
    crypto_t *crypto;
} local;

struct remoteServer;

typedef struct localClient {
    int stage;
    event *re;
    sds buf;
    int buf_off;
    sds addr_buf;
    struct remoteServer *remote;
} localClient;

typedef struct remoteServer {
    event *re;
    sds buf;
    int buf_off;
    localClient *client;
} remoteServer;

typedef struct method_select_request socks5AuthReq;
typedef struct method_select_response socks5AuthResp;
typedef struct socks5_request socks5Req;
typedef struct socks5_response socks5Resp;

void acceptHandler(event *e);
void remoteServerReadHandler(event *e);
void localClientReadHandler(event *e);

static void initLogger() {
    logger *log = getLogger();
    xsocksConfig *config = local.config;

    log->file = config->logfile;
    log->level = config->loglevel;
    log->color_enabled = 1;
    log->syslog_enabled = config->use_syslog;
    log->file_line_enabled = 1;
    log->syslog_ident = "xs-local";
    // log->syslog_facility = LOG_USER;
}

static void initCrypto() {
    crypto_t *crypto = crypto_init(local.config->password, local.config->key, local.config->method);
    if (crypto == NULL) FATAL("Failed to initialize ciphers");

    local.crypto =crypto;
}

void listenForLocal(int *fd) {
    char err[256];
    char *host = local.config->local_addr;
    int port = local.config->local_port;
    int backlog = 256;
    if (host == NULL) host = "0.0.0.0";
    if (strchr(host, ':')) {
        /* Bind IPv6 address. */
        *fd = anetTcp6Server(err, port, host, backlog);
    } else {
        /* Bind IPv4 address. */
        *fd = anetTcpServer(err, port, host, backlog);
    }
    if (*fd == ANET_ERR) {
        FATAL("Could not create server TCP listening socket %s:%d: %s",
              host ? host : "*", port, err);
    }
    anetNonBlock(NULL, *fd);
}

static void initLocal(xsocksConfig *config) {
    local.config = config;
    initLogger();
    initCrypto();

    local.el = eventLoopNew();

    if (config->mode != MODE_UDP_ONLY) {
        int lfd;
        listenForLocal(&lfd);

        event* e = eventNew(lfd, EVENT_TYPE_IO, EVENT_FLAG_READ, acceptHandler, NULL);
        eventAdd(local.el, e);
    }
}

localClient *newClient(int fd) {
    localClient *client = xs_calloc(sizeof(*client));

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);

    event* re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, localClientReadHandler, client);
    eventAdd(local.el, re);

    client->stage = STAGE_INIT;
    client->buf = sdsempty();
    client->buf_off = 0;
    client->re = re;

    return client;
}

void freeClient(localClient *client) {
    sdsfree(client->buf);
    sdsfree(client->addr_buf);
    eventDel(client->re);
    eventFree(client->re);
    close(client->re->id);

    xs_free(client);
}

remoteServer *newRemote(int fd) {
    remoteServer *remote = xs_calloc(sizeof(*remote));

    anetNonBlock(NULL, fd);
    anetEnableTcpNoDelay(NULL, fd);

    event* re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, remoteServerReadHandler, remote);
    eventAdd(local.el, re);

    remote->re = re;

    return remote;
}

void freeRemote(remoteServer *remote) {
    sdsfree(remote->buf);
    eventDel(remote->re);
    eventFree(remote->re);
    close(remote->re->id);
    xs_free(remote);
}

void remoteServerReadHandler(event *e) {
    remoteServer *remote = e->data;
    localClient *client = remote->client;

    char buf[NET_IOBUF_LEN];
    int readlen = NET_IOBUF_LEN;
    int rfd = e->id;
    int nread = read(rfd, buf, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) return;

        LOG_STRERROR("Remote server read error");
        freeClient(client);
        freeRemote(remote);
        return;
    } else if (nread == 0) {
        LOGD("Remote server closed connection");
        freeClient(client);
        freeRemote(remote);
        return;
    }

}

void localClientReadHandler(event *e) {
    localClient *client = e->data;

    if (client->stage == STAGE_INIT) {
        // client->buf = sdsMakeRoomFor(client->buf, readlen);
        // sdsIncrLen(client->buf, nread);
        char buf[NET_IOBUF_LEN];
        int readlen = NET_IOBUF_LEN;
        int cfd = e->id;
        int nread = read(cfd, buf, readlen);
        if (nread == -1) {
            if (errno == EAGAIN) return;

            LOG_STRERROR("Local client read error");
            freeClient(client);
            return;
        } else if (nread == 0) {
            LOGD("Local client closed connection");
            freeClient(client);
            return;
        } else if (nread <= (int)sizeof(socks5AuthReq)) {
            LOGW("Local client socks5 authenticates error");
            freeClient(client);
            return;
        }

        socks5AuthReq *auth_req = (socks5AuthReq *)buf;
        int auth_len = sizeof(socks5AuthReq) + auth_req->nmethods;
        if (nread < auth_len || auth_req->ver != SVERSION) {
            LOGW("Local client socks5 authenticates error");
            freeClient(client);
            return;
        }

        // Must be noauth here
        socks5AuthResp auth_resp = {
            SVERSION,
            METHOD_UNACCEPTABLE
        };
        for (int i = 0; i < auth_req->nmethods; i++) {
            if (auth_req->methods[i] == METHOD_NOAUTH) {
                auth_resp.method = METHOD_NOAUTH;
                break;
            }
        }

        if (auth_resp.method == METHOD_UNACCEPTABLE) {
            freeClient(client);
            return;
        }

        write(cfd, &auth_resp, sizeof(auth_resp));

        client->stage = STAGE_HANDSHAKE;

        return;
    } else if (client->stage == STAGE_HANDSHAKE) {
        char buf[NET_IOBUF_LEN];
        int readlen = NET_IOBUF_LEN;
        int cfd = e->id;
        int nread = read(cfd, buf, readlen);
        if (nread == -1) {
            if (errno == EAGAIN) return;

            LOG_STRERROR("Local client read error");
            freeClient(client);
            return;
        } else if (nread == 0) {
            LOGD("Local client closed connection");
            freeClient(client);
            return;
        }

        socks5Req *request = (socks5Req *)buf;
        int request_len = sizeof(socks5Req);
        if (nread < request_len || request->ver != SVERSION) {
            LOGW("Local client socks5 request error");
            freeClient(client);
            return;
        }

        socks5Resp resp = {
            SVERSION,
            SOCKS5_REP_SUCCEEDED,
            0,
            SOCKS5_ATYP_IPV4
        };

        if (request->cmd != SOCKS5_CMD_CONNECT) {
            LOGW("Local client request error, unsupported cmd: %d", request->cmd);
            resp.rep = SOCKS5_REP_CMD_NOT_SUPPORTED;
            write(cfd, &resp, sizeof(resp));
            freeClient(client);
            return;
        }

        int addrType = request->atyp;

        char dip[NET_IP_MAX_STR_LEN] = {0};
        int port = 0;
        int addr_len;
        char host[HOSTNAME_MAX_LEN] = {0};

        if (addrType == SOCKS5_ATYP_IPV4) {
            addr_len = sizeof(ipV4Addr);
            if (nread < request_len + addr_len + 2) {
                LOGW("Local client request error, addrlen: %d", addr_len);
                resp.rep = SOCKS5_REP_ADDRTYPE_NOT_SUPPORTED;
                write(cfd, &resp, sizeof(resp));
                freeClient(client);
                return;
            }
            port = ntohs(*(uint16_t *)(buf+request_len+addr_len));
            inet_ntop(AF_INET, buf+request_len, dip, NET_IP_MAX_STR_LEN);
            LOGD("Local client request addr: [%s:%d]", dip, port);

            client->addr_buf = sdsnewlen(buf + request_len - 1, addr_len+2);
        } else if (addrType == SOCKS5_ATYP_DOMAIN) {
            addr_len = *(buf + request_len);
            if (nread < request_len + 1 + addr_len + 2) {
                LOGW("Local client request error, addrlen: %d", addr_len);
                resp.rep = SOCKS5_REP_ADDRTYPE_NOT_SUPPORTED;
                write(cfd, &resp, sizeof(resp));
                freeClient(client);
                return;
            }

            memcpy(host, buf+request_len+1, addr_len);
            port = ntohs(*(uint16_t *)(buf+request_len+1+addr_len));
            LOGD("Local client request addr: [%s:%d]", host, port);

            client->addr_buf = sdsnewlen(buf + request_len - 1, 1+addr_len+2);
        } else if (addrType == SOCKS5_ATYP_IPV6) {
            addr_len = sizeof(ipV6Addr);
            if (nread < request_len + addr_len + 2) {
                LOGW("Local client request error, addrlen: %d", addr_len);
                resp.rep = SOCKS5_REP_ADDRTYPE_NOT_SUPPORTED;
                write(cfd, &resp, sizeof(resp));
                freeClient(client);
                return;
            }
            port = ntohs(*(uint16_t *)(buf+request_len+addr_len));
            inet_ntop(AF_INET6, buf+request_len, dip, NET_IP_MAX_STR_LEN);
            LOGD("Local client request addr: [%s:%d]", dip, port);

            client->addr_buf = sdsnewlen(buf + request_len - 1, addr_len+2);
        } else {
            LOGW("Local client request error, unsupported addrtype: %d", addrType);
            resp.rep = SOCKS5_REP_ADDRTYPE_NOT_SUPPORTED;
            write(cfd, &resp, sizeof(resp));
            freeClient(client);
            return;
        }

        sockAddrIpV4 sock_addr;
        bzero(&sock_addr, sizeof(sock_addr));

        char resp_buf[NET_IOBUF_LEN];
        memcpy(resp_buf, &resp, sizeof(resp));
        memcpy(resp_buf + sizeof(resp), &sock_addr.sin_addr, sizeof(sock_addr.sin_addr));
        memcpy(resp_buf + sizeof(resp) + sizeof(sock_addr.sin_addr),
            &sock_addr.sin_port, sizeof(sock_addr.sin_port));

        int reply_size = sizeof(resp) + sizeof(sock_addr.sin_addr) + sizeof(sock_addr.sin_port);

        int nwrite = write(cfd, &resp_buf, reply_size);
        if (nwrite != reply_size) {
            LOGW("Local client replay error");
            freeClient(client);
            return;
        }
        client->stage = STAGE_STREAM;

        char err[ANET_ERR_LEN];
        int rfd  = anetTcpConnect(err, local.config->remote_addr, local.config->remote_port);
        if (rfd == ANET_ERR) {
            LOGW("Remote server connenct error: %s", err);
            freeClient(client);
            return;
        }

        remoteServer *remote = newRemote(rfd);
        remote->client = client;
        client->remote = remote;

        return;
    } else if (client->stage == STAGE_STREAM) {
        char buf[NET_IOBUF_LEN] = {0};
        int readlen = NET_IOBUF_LEN;
        int cfd = e->id;
        int nread = read(cfd, buf, readlen);
        if (nread == -1) {
            if (errno == EAGAIN) return;

            LOG_STRERROR("Local client read error");
            freeClient(client);
            return;
        } else if (nread == 0) {
            LOGW("Local client closed connection");
            freeClient(client);
            return;
        }

        LOGDR("%s\n", buf);

        remoteServer *remote = client->remote;
        int rfd = remote->re->id;

        /*
         * Append Shadowsocks TCP Relay Header:
         *
         *    +------+----------+----------+
         *    | ATYP | DST.ADDR | DST.PORT |
         *    +------+----------+----------+
         *    |  1   | Variable |    2     |
         *    +------+----------+----------+
         */
        if (client->addr_buf) {

            sdsfree(client->addr_buf);
            client->addr_buf = NULL;
        }

        int nwrite = write(rfd, &buf, nread);
        if (nwrite != nread) {
            LOG_STRERROR("Remote server wrete error");
            freeClient(client);
            freeRemote(remote);
            return;
        }

        return;
    }

    LOGE("Local client read error");
    freeClient(client);
}

void acceptHandler(event* e) {
    int cfd, cport;
    char cip[NET_IP_MAX_STR_LEN];
    char err[ANET_ERR_LEN];

    cfd = anetTcpAccept(err, e->id, cip, sizeof(cip), &cport);
    if (cfd == ANET_ERR) {
        if (errno != EWOULDBLOCK)
            LOGW("Accepting local client connection: %s", err);
        return;
    }
    LOGD("Accepted %s:%d fd: %d", cip, cport, cfd);

    newClient(cfd);
}

int main(int argc, char *argv[]) {
    setLogger(loggerNew());

    xsocksConfig *config = configNew();

    if (configParse(config, argc, argv) == CONFIG_ERR) FATAL("Parse config error");

    if (config->help) {
        _usage();
        return EXIT_SUCCESS;
    }

    initLocal(config);

    LOGI("Initializing ciphers... %s", config->method);
    LOGI("Start password: %s", config->password);
    LOGI("Start key: %s", config->key);

    if (config->mtu) LOGI("set MTU to %d", config->mtu);
    if (config->no_delay) LOGI("enable TCP no-delay");
    LOGN("Start local: %s:%d", config->local_addr, config->local_port);
    LOGI("Start remote: %s:%d", config->remote_addr, config->remote_port);

    LOGN("Start event loop with: %s", eventGetApiName());

    eventLoopRun(local.el);

    eventLoopFree(local.el);
    loggerFree(getLogger());

    return EXIT_SUCCESS;
}
