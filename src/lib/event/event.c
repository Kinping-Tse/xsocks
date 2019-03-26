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

#include "../core/common.h"

#include "event.h"

#ifdef USE_AE
    #include "event_ae.h"
#elif USE_LIBEV
    #include "event_libev.h"
#else
    #error "Must use one event"
#endif

eventLoop *eventLoopNew(int size) {
    eventLoop *el = xs_calloc(sizeof(*el));
    el->ctx = eventApiNewLoop(size);
    return el;
}

void eventLoopFree(eventLoop *el) {
    eventApiFreeLoop(el->ctx);
    xs_free(el);
}

void eventLoopRun(eventLoop *el) {
    eventApiRun(el->ctx);
}

void eventLoopStop(eventLoop *el) {
    eventApiStop(el->ctx);
}

event *eventNew(int id, int type, int flags, eventHandler handler, void *data) {
    event *e = xs_calloc(sizeof(*e));
    e->id = id;
    e->type = type;
    e->flags = flags;
    e->handler = handler;
    e->data = data;
    e->el = NULL;
    e->ctx = eventApiNewEvent(e);
    return e;
}

void eventFree(event *e) {
    if (!e) return;

    eventApiFreeEvent(e->ctx);
    xs_free(e);
}

int eventAdd(eventLoop *el, event *e) {
    if (e->el) return EVENT_OK;

    e->el = el;
    if (eventApiAddEvent(el->ctx, e->ctx) == EVENT_ERR) {
        LOGE("Add Event error, please check the max open file size!");
        return EVENT_ERR;
    }
    return EVENT_OK;
}

void eventDel(event *e) {
    if (!e || !e->el) return;

    eventApiDelEvent(e->el->ctx, e->ctx);
    e->el = NULL;
}

char *eventGetApiName() {
    return eventApiName();
}
