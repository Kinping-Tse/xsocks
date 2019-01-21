
#include "module.h"

static module *mod;

static void initCrypto() {
    crypto_t *crypto = crypto_init(mod->config->password, mod->config->key, mod->config->method);
    if (crypto == NULL) FATAL("Failed to initialize ciphers");

    mod->crypto =crypto;
}

static void initLogger() {
    logger *log = getLogger();
    xsocksConfig *config = mod->config;

    log->file = config->logfile;
    log->level = config->loglevel;
    log->color_enabled = 1;
    log->syslog_enabled = config->use_syslog;
    log->file_line_enabled = 1;
    // log->syslog_facility = LOG_USER;
}

void moduleInit(int type, moduleHook hook, module *m, int argc, char *argv[]) {
    mod = m;
    mod->type = type;
    mod->hook = hook;

    setLogger(loggerNew());

    xsocksConfig *config = configNew();
    if (configParse(config, argc, argv) == CONFIG_ERR) FATAL("Parse config error");
    if (config->help) {
        xs_usage(type);
        exit(EXIT_SUCCESS);
    }

    mod->config = config;

    initCrypto();
    initLogger();
    mod->el = eventLoopNew();

    if (mod->hook.init) mod->hook.init();
}

void moduleRun() {
    xsocksConfig* config = mod->config;

    LOGI("Initializing ciphers... %s", config->method);
    LOGI("Start password: %s", config->password);
    LOGI("Start key: %s", config->key);

    if (config->mtu) LOGI("set MTU to %d", config->mtu);
    if (config->no_delay) LOGI("enable TCP no-delay");
    LOGN("Start local: %s:%d", config->local_addr, config->local_port);
    LOGI("Start remote: %s:%d", config->remote_addr, config->remote_port);
    LOGI("Start tunnel: %s:%d", config->tunnel_addr, config->tunnel_port);
    LOGN("Start event loop with: %s", eventGetApiName());

    eventLoopRun(mod->el);
}

void moduleExit() {
    eventLoopFree(mod->el);
    loggerFree(getLogger());
}
