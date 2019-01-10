
#include "common.h"
#include "utils.h"
#include "config.h"

#include <syslog.h>

#define _usage() usage(MODULE_LOCAL)

struct client {
    xsocksConfig* config;
} client;

static void initLogger() {
    logger *log = getLogger();
    xsocksConfig* config = client.config;

    log->file = config->logfile;
    log->level = config->loglevel;
    log->color_enabled = 1;
    log->syslog_enabled = config->use_syslog;
    log->file_line_enabled = 1;
    log->syslog_ident = "xs-client";
    log->syslog_facility = LOG_USER;
}

static void initClient(xsocksConfig* config) {
    client.config = config;
    initLogger();
}

int main(int argc, char *argv[]) {
    setLogger(loggerNew());

    xsocksConfig* config = configNew();

    if (configParse(config, argc, argv) == CONFIG_ERR)
        FATAL("Parse config error");

    if (config->help) {
        _usage();
        return EXIT_SUCCESS;
    }

    initClient(config);

    if (config->mtu) LOGD("set MTU to %d", config->mtu);
    if (config->no_delay) LOGI("enable TCP no-delay");
    LOGI("Start remote: %s:%d", config->remote_addr, config->remote_port);
    LOGN("Start local: %s:%d", config->local_addr, config->local_port);
    LOGI("Start password: %s", config->password);
    LOGI("Start key: %s", config->key);

    if (config->mode != MODE_UDP_ONLY) {
        // int listenfd;
        // listenfd = create_and_bind(local_addr, local_port_str);
        // if (listenfd == -1) {
        //     ERROR("bind()");
        //     return -1;
        // }
        // if (listen(listenfd, SOMAXCONN) == -1) {
        //     ERROR("listen()");
        //     return -1;
        // }
        // setnonblocking(listenfd);

        // listen_ctx.fd = listenfd;

        // ev_io_init(&listen_ctx.io, accept_cb, listenfd, EV_READ);
        // ev_io_start(loop, &listen_ctx.io);
    }

    loggerFree(getLogger());

    return EXIT_SUCCESS;
}
