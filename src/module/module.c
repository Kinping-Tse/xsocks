
#include "module.h"

#include <signal.h>

static module *mod;

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

void moduleUsage(int module) {
    eprintf("xsocks %s\n", XS_VERSION);
    eprintf("  maintained by XJP09_HK <jianping_xie@aliyun.com>\n\n");
    eprintf("  usage:\n");

    switch (module) {
        case MODULE_LOCAL: eprintf("    xs-local\n"); break;
        case MODULE_REMOTE: eprintf("    xs-server\n"); break;
        case MODULE_TUNNEL: eprintf("    xs-tunnel\n"); break;
        default:
            // eprintf("    xs-redir\n");
            break;

    }

    eprintf("       -s <server_host>           Host name or IP address of your remote server.\n");
    eprintf("       -p <server_port>           Port number of your remote server.\n");
    eprintf("       -l <local_port>            Port number of your local server.\n");
    eprintf("       -k <password>              Password of your remote server.\n");
    eprintf("       -m <encrypt_method>        Encrypt method: rc4-md5,\n");
    eprintf("                                  aes-128-gcm, aes-192-gcm, aes-256-gcm,\n");
    eprintf("                                  aes-128-cfb, aes-192-cfb, aes-256-cfb,\n");
    eprintf("                                  aes-128-ctr, aes-192-ctr, aes-256-ctr,\n");
    eprintf("                                  camellia-128-cfb, camellia-192-cfb,\n");
    eprintf("                                  camellia-256-cfb, bf-cfb,\n");
    eprintf("                                  chacha20-ietf-poly1305,\n");
#ifdef FS_HAVE_XCHACHA20IETF
    eprintf("                                  xchacha20-ietf-poly1305,\n");
#endif
    eprintf("                                  salsa20, chacha20 and chacha20-ietf.\n");
    eprintf("                                  The default cipher is aes-256-cfb.\n");
    eprintf("\n");
    if (module == MODULE_TUNNEL) {
        eprintf(
            "       -L <addr>:<port>           Destination server address and port\n"
            "                                  for local port forwarding.\n");
    }
    // eprintf("       [-a <user>]                Run as another user.\n");
    eprintf("       [-f <pid_file>]            The file path to store pid.\n");
    eprintf("       [-t <timeout>]             Socket timeout in seconds.\n");
    eprintf("       [-c <config_file>]         The path to config file.\n");
    // eprintf("       [-n <number>]              Max number of open files.\n");
#ifndef MODULE_REDIR
    // eprintf("       [-i <interface>]           Network interface to bind.\n");
#endif
    eprintf("       [-b <local_address>]       Local address to bind.\n");
    eprintf("\n");
    eprintf("       [-u]                       Enable UDP relay.\n");
#ifdef MODULE_REDIR
    // eprintf("                                  TPROXY is required in redir mode.\n");
#endif
    eprintf("       [-U]                       Enable UDP relay and disable TCP relay.\n");
    eprintf("       [-6]                       Use IPv6 address first.\n");
    eprintf("\n");
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
#if defined(MODULE_REMOTE) || defined(MODULE_MANAGER)
    // eprintf("       [--manager-address <addr>] UNIX domain socket address.\n");
#endif
#ifdef MODULE_MANAGER
    // eprintf("       [--executable <path>]      Path to the executable of ss-server.\n");
#endif
    eprintf("       [--mtu <MTU>]              MTU of your network interface.\n");
#ifdef __linux__
    // eprintf("       [--mptcp]                  Enable Multipath TCP on MPTCP Kernel.\n");
#endif
#ifndef MODULE_MANAGER
    // eprintf("       [--no-delay]               Enable TCP_NODELAY.\n");
    eprintf("       [--key <key_in_base64>]    Key of your remote server.\n");
#endif
    // eprintf("       [--plugin <name>]          Enable SIP003 plugin. (Experimental)\n");
    // eprintf("       [--plugin-opts <options>]  Set SIP003 plugin options. (Experimental)\n");
    eprintf("\n");
    eprintf("       [--logfile <file>]         Log file.\n");
    eprintf("       [--loglevel <level>]       Log level.\n");
    eprintf("       [-v]                       Verbose mode.\n");
    eprintf("       [-h, --help]               Print this message.\n");
    eprintf("\n");
}

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
    log->file_line_enabled = config->logfile_line;
    // log->syslog_facility = LOG_USER;
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

void moduleInit(int type, moduleHook hook, module *m, int argc, char *argv[]) {
    mod = m;
    mod->type = type;
    mod->hook = hook;

    setLogger(loggerNew());

    xsocksConfig *config = configNew();
    if (configParse(config, argc, argv) == CONFIG_ERR) {
        moduleUsage(type);
        exit(EXIT_SUCCESS);
    }

    mod->config = config;

    initLogger();
    if (config->daemonize) xs_daemonize();
    createPidFile();

    initCrypto();
    mod->el = eventLoopNew();

    signal(SIGPIPE, SIG_IGN);

    if (mod->hook.init) mod->hook.init();
}

void moduleRun() {
    xsocksConfig* config = mod->config;

    LOGN("Use crypto method: %s", config->method);
    LOGN("Use crypto password: %s", config->password);
    LOGN("Use crypto key: %s", config->key);
    if (config->mode & MODE_TCP_ONLY) LOGI("Enable TCP mode");
    if (config->mode & MODE_UDP_ONLY) LOGI("Enable UDP mode");
    if (config->mtu) LOGI("Set MTU to %d", config->mtu);
    if (config->no_delay) LOGI("Enable TCP no-delay");
    if (config->ipv6_first) LOGI("Use IPv6 address first");
    // if (config->ipv6_only) LOGI("Use IPv6 address only");
    if (config->timeout) LOGI("Use timeout: %d", config->timeout);
    LOGI("Use local addr: %s:%d", config->local_addr, config->local_port);
    LOGI("Use remote addr: %s:%d", config->remote_addr, config->remote_port);
    LOGI("Start event loop with: %s", eventGetApiName());
    if (config->pidfile) LOGI("Process id save in file: %s", config->pidfile);
    if (config->daemonize) LOGI("Enable daemonize");
    if (config->use_syslog) LOGI("Enable syslog");

    if (mod->hook.run) mod->hook.run();

    eventLoopRun(mod->el);
}

void moduleExit() {
    if (mod->hook.exit) mod->hook.exit();

    eventLoopFree(mod->el);
    loggerFree(getLogger());
}
