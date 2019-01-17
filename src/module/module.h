
#include "../core/common.h"
#include "../core/config.h"
#include "../core/utils.h"
#include "../core/net.h"
#include "../event/event.h"

#include "crypto.h"

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

void moduleInit(int type, moduleHook hook, module *m, int argc, char *argv[]);
void moduleRun();
void moduleExit();
