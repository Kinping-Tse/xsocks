/*
 * This file is part of xsocks, a lightweight proxy tool for science online.
 *
 * Copyright (C) 2019 XJP09_HK <jianping_xie@aliyun.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __MODULE_H
#define __MODULE_H

#include "lib/core/common.h"

#include "lib/core/config.h"
#include "lib/core/time.h"
#include "lib/core/utils.h"
#include "lib/event/event.h"

#include "redis/adlist.h"
#include "shadowsocks-libev/crypto.h"
#include "shadowsocks-libev/acl.h"

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

int moduleMain(int type, moduleHook hook, module *m, int argc, char *argv[]);

#endif /* __MODULE_H */
