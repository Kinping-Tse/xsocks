
#include "module.h"
#include "../core/version.h"
#include "../protocol/proxy.h"

#include "shadowsocks-libev/ppbloom.h"

#include <signal.h>

static module *mod;

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

static void moduleInit();
static void moduleRun();
static void moduleExit();

static void moduleUsage();
static void initLogger();
static void freeCrypto(crypto_t *crypto);
static void createPidFile();
static void setupSignalHandlers();
static void signalExitHandler(event *e);
static void signalEventFreeHandler(void *e);

int moduleMain(int type, moduleHook hook, module *m, int argc, char *argv[]) {
    moduleInit(type, hook, m, argc, argv);
    moduleRun();
    moduleExit();

    return EXIT_OK;
}

static void moduleInit(int type, moduleHook hook, module *m, int argc, char *argv[]) {
    mod = m;
    mod->type = type;
    mod->hook = hook;

    setLogger(loggerNew());

    xsocksConfig *config = configNew();
    if (configParse(config, argc, argv) == CONFIG_ERR) {
        moduleUsage();
        exit(EXIT_OK);
    }

    mod->config = config;
    initLogger();

    if (!config->password) {
        LOGE("Invalid password");
        moduleUsage();
        exit(EXIT_ERR);
    }

    if (config->daemonize) xs_daemonize();
    createPidFile();

    mod->el = eventLoopNew(1024);
    setupSignalHandlers();

    mod->crypto = crypto_init(mod->config->password, mod->config->key, mod->config->method);
    if (!mod->crypto) FATAL("Failed to initialize ciphers");

    if (mod->hook.init) mod->hook.init();
}

static void moduleRun() {
    xsocksConfig *config = mod->config;

    LOGN("Use crypto method: %s", config->method);
    LOGN("Use crypto password: %s", config->password);
    LOGN("Use crypto key: %s", config->key);
    if (config->mode & MODE_TCP_ONLY) LOGI("Enable TCP mode");
    if (config->mode & MODE_UDP_ONLY) LOGI("Enable UDP mode");
    if (config->mtu) LOGI("Set MTU to %d", config->mtu);
    if (config->no_delay) LOGI("Enable TCP no-delay");
    if (config->ipv6_first) LOGI("Use IPv6 address first");
    // if (config->ipv6_only) LOGI("Use IPv6 address only");
    if (config->timeout) LOGI("Use timeout: %ds", config->timeout);
    LOGI("Use local addr: %s:%d", config->local_addr, config->local_port);
    LOGI("Use remote addr: %s:%d", config->remote_addr, config->remote_port);
    LOGI("Start event loop with: %s", eventGetApiName());
    if (config->pidfile) LOGI("Process id save in file: %s", config->pidfile);
    if (config->daemonize) LOGI("Enable daemonize");
    if (config->use_syslog) LOGI("Enable syslog");

    if (mod->hook.run) mod->hook.run();

    eventLoopRun(mod->el);
}

static void moduleExit() {
    if (mod->hook.exit) mod->hook.exit();

    if (mod->config->pidfile) unlink(mod->config->pidfile);
    freeCrypto(mod->crypto);
    listRelease(mod->sigexit_events);
    eventLoopFree(mod->el);
    configFree(mod->config);
    loggerFree(getLogger());
}

static void moduleUsage() {
    int module = mod->type;

    eprintf("Usage:");

    switch (module) {
        case MODULE_LOCAL: eprintf(" xs-local"); break;
        case MODULE_REMOTE: eprintf(" xs-server"); break;
        case MODULE_TUNNEL: eprintf(" xs-tunnel"); break;
        case MODULE_REDIR: eprintf(" xs-redir"); break;
        default: FATAL("Unknown module type!");
    }
    eprintf(" [options]\n\n");
    eprintf("Options:\n");
    eprintf("  -s <server_host>           Host name or IP address of your remote server (default 127.0.0.1)\n");
    eprintf("  -p <server_port>           Port number of your remote server (default 8388)\n");
    eprintf("  -l <local_port>            Port number of your local server (default 1080)\n");
    eprintf("  -k <password>              Password of your remote server (default foobar)\n");
    eprintf("  -m <encrypt_method>        Encrypt method: rc4-md5,\n");
    eprintf("                             aes-128-gcm, aes-192-gcm, aes-256-gcm,\n");
    eprintf("                             aes-128-cfb, aes-192-cfb, aes-256-cfb,\n");
    eprintf("                             aes-128-ctr, aes-192-ctr, aes-256-ctr,\n");
    eprintf("                             camellia-128-cfb, camellia-192-cfb, camellia-256-cfb,\n");
    eprintf("                             bf-cfb, chacha20-ietf-poly1305,\n");
#ifdef FS_HAVE_XCHACHA20IETF
    eprintf("                             xchacha20-ietf-poly1305,\n");
#endif
    eprintf("                             salsa20, chacha20 and chacha20-ietf.\n");
    eprintf("                             (default aes-256-cfb)\n");
    if (module == MODULE_TUNNEL) {
        eprintf("  -L <addr>:<port>           Destination server address and port\n"
                "                             for local port forwarding. (default 8.8.8.8:53)\n");
    }
    // eprintf("       [-a <user>]                Run as another user.\n");
    eprintf("  [-f <pid_file>]            The file path to store pid.\n");
    eprintf("  [-t <timeout>]             Socket timeout in seconds (default 60)\n");
    eprintf("  [-c <config_file>]         The path to config file.\n");
    // eprintf("       [-n <number>]              Max number of open files.\n");
#ifndef MODULE_REDIR
    // eprintf("       [-i <interface>]           Network interface to bind.\n");
#endif
    eprintf("  [-b <local_address>]       Local address to bind.\n");
    eprintf("  [-u]                       Enable UDP relay.\n");
    if (module == MODULE_REDIR)
        eprintf("                                  TPROXY is required in redir mode.\n");
    eprintf("  [-U]                       Enable UDP relay and disable TCP relay.\n");
    eprintf("  [-6]                       Use IPv6 address first.\n");
    // if (module == MODULE_REMOTE)
    // eprintf(
    // "       [-d <addr>]                Name servers for internal DNS resolver.\n");

    // eprintf("       [--reuse-port]             Enable port reuse.\n");
#if defined(MODULE_REMOTE) || defined(MODULE_LOCAL) || defined(MODULE_REDIR)
    // eprintf("       [--fast-open]              Enable TCP fast open.\n");
    // eprintf("                                  with Linux kernel > 3.7.0.\n");
#endif
#if defined(MODULE_REMOTE) || defined(MODULE_LOCAL)
    // eprintf("       [--acl <acl_file>]         Path to ACL (Access Control List).\n");
#endif
    // eprintf("  [--mtu <MTU>]              MTU of your network interface.\n");
#ifdef __linux__
    // eprintf("       [--mptcp]                  Enable Multipath TCP on MPTCP Kernel.\n");
#endif
    // eprintf("       [--no-delay]               Enable TCP_NODELAY.\n");
    eprintf("  [--key <key_in_base64>]    Key of your remote server.\n");
    eprintf("  [--logfile <file>]         Log file.\n");
    eprintf("  [--loglevel <level>]       Log level (default info)\n");
    eprintf("  [-v]                       Verbose mode.\n");
    eprintf("  [-V, --version]            Print version info.\n");
    eprintf("  [-h, --help]               Print this message.\n");
}

static void initLogger() {
    logger *log = getLogger();
    xsocksConfig *config = mod->config;

    log->file = config->logfile;
    log->level = config->loglevel;
    log->color_enabled = 1;
    log->syslog_enabled = config->use_syslog;
    log->file_line_enabled = config->logfile_line;
    // log->syslog_facility = LOG_USER;
}

static void freeCrypto(crypto_t *crypto) {
    ppbloom_free();
    free(crypto->cipher);
    free(crypto);
}

static void createPidFile() {
    char *pidfile = mod->config->pidfile;

    if (pidfile && pidfile[0] != '\0') {
        FILE *fp;

        if (!(fp = fopen(pidfile, "w"))) FATAL("Invalid pidfile: %s", STRERR);

        fprintf(fp, "%d\n", getpid());
        fclose(fp);
    }
}

static void setupSignalHandlers() {
    setupIgnoreHandlers();
    setupSigsegvHandlers();

    mod->sigexit_events = listCreate();
    listSetFreeMethod(mod->sigexit_events, signalEventFreeHandler);

    event *ev_sigint = NEW_EVENT_SIGNAL(SIGINT, signalExitHandler, NULL);
    event *ev_sigterm = NEW_EVENT_SIGNAL(SIGTERM, signalExitHandler, NULL);
    event *ev_sigquit = NEW_EVENT_SIGNAL(SIGQUIT, signalExitHandler, NULL);
    ADD_EVENT(mod, ev_sigint);
    ADD_EVENT(mod, ev_sigterm);
    ADD_EVENT(mod, ev_sigquit);

    listAddNodeTail(mod->sigexit_events, ev_sigint);
    listAddNodeTail(mod->sigexit_events, ev_sigterm);
    listAddNodeTail(mod->sigexit_events, ev_sigquit);
}

static void signalExitHandler(event *e) {
    int signal = e->id;
    char *msg;
    switch (signal) {
        case SIGINT: msg = "Received SIGINT scheduling shutdown..."; break;
        case SIGTERM: msg = "Received SIGTERM scheduling shutdown..."; break;
        case SIGQUIT: msg = "Received SIGQUIT scheduling shutdown..."; break;
        default: msg = "Received shutdown signal, scheduling shutdown..."; break;
    }
    LOGW(msg);

    eventLoopStop(mod->el);
}

static void signalEventFreeHandler(void *e) {
    CLR_EVENT(e);
}
