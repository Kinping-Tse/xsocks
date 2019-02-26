
#ifndef __MODULE_H
#define __MODULE_H

#include "../core/common.h"
#include "../core/config.h"
#include "../core/utils.h"
#include "../core/net.h"
#include "../core/socks5.h"
#include "../core/version.h"
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
    MODULE_ERR = -1
};

enum {
    MODULE_REMOTE = 0,
    MODULE_SERVER = MODULE_REMOTE,
    MODULE_LOCAL = 1,
    MODULE_CLIENT = MODULE_LOCAL,
    MODULE_TUNNEL = 2,
};

extern module *app;

#define NEW_EVENT_READ(fd, handler, data) eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, handler, data)
#define NEW_EVENT_WRITE(fd, handler, data) eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_WRITE, handler, data)
#define NEW_EVENT_ONCE(timeout, handler, data) eventNew(timeout, EVENT_TYPE_TIME, EVENT_FLAG_TIME_ONCE, handler, data)
#define NEW_EVENT_REPEAT(timeout, handler, data) eventNew(timeout, EVENT_TYPE_TIME, EVENT_FLAG_TIME_REPEAT, handler, data)
#define NEW_EVENT_SIGNAL(signal, handler, data) eventNew(signal, EVENT_TYPE_SIGNAL, 0, handler, data)
#define ADD_EVENT(e) eventAdd(app->el, e)
#define DEL_EVENT(e) eventDel(e)
#define CLR_EVENT(e) do { DEL_EVENT(e); eventFree(e); } while (0)

int moduleMain(int type, moduleHook hook, module *m, int argc, char *argv[]);

#endif /* __MODULE_H */
