
#include "../core/common.h"
#include "../core/config.h"
#include "../core/utils.h"
#include "../core/net.h"
#include "../core/socks5.h"
#include "../core/version.h"
#include "../event/event.h"

#include "shadowsocks-libev/crypto.h"

typedef struct moduleHook {
    void (*init)();
    void (*run)();
    void (*exit)();
} moduleHook;

typedef struct module {
    int type;
    moduleHook hook;
    xsocksConfig *config;
    eventLoop *el;
    crypto_t *crypto;
} module;

enum {
    MODULE_OK = 0,
    MODULE_ERR = -1
};

enum {
    MODULE_REMOTE = 0,
    MODULE_SERVER = MODULE_REMOTE,
    MODULE_LOCAL,
    MODULE_TUNNEL,
};

void moduleInit(int type, moduleHook hook, module *m, int argc, char *argv[]);
void moduleRun();
void moduleExit();
