
#ifndef __MODULE_H
#define __MODULE_H

#include "../core/common.h"

#include "../core/config.h"
#include "../core/error.h"
#include "../core/time.h"
#include "../core/utils.h"
#include "../event/event.h"

#include "redis/adlist.h"
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
    list *sigexit_events;
} module;

enum {
    MODULE_OK = 0,
    MODULE_ERR = -1,
};

enum {
    MODULE_REMOTE = 0,
    MODULE_SERVER = MODULE_REMOTE,
    MODULE_LOCAL = 1,
    MODULE_CLIENT = MODULE_LOCAL,
    MODULE_TUNNEL = 2,
    MODULE_REDIR = 3,
};

extern module *app;

#define ADD_EVENT(e) eventAdd(app->el, e)

int moduleMain(int type, moduleHook hook, module *m, int argc, char *argv[]);

#endif /* __MODULE_H */
