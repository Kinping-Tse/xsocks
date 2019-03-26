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

#ifndef __XS_EVENT_H
#define __XS_EVENT_H

enum {
    EVENT_OK = 0,
    EVENT_ERR = -1,
};

enum {
    EVENT_TYPE_IO = 0,
    EVENT_TYPE_TIME = 1,
    EVENT_TYPE_SIGNAL = 2,
};

enum {
    EVENT_FLAG_READ = 0,
    EVENT_FLAG_WRITE = 1,
    EVENT_FLAG_TIME_ONCE = 0,
    EVENT_FLAG_TIME_REPEAT = 1,
};

typedef struct eventLoop {
    struct eventLoopContext *ctx;
} eventLoop;

struct event;
typedef void (*eventHandler)(struct event *e);

typedef struct event {
    int id;
    int type;
    int flags;
    eventHandler handler;
    void *data;
    struct eventLoop *el;
    struct eventContext *ctx;
} event;

#define NEW_EVENT_READ(fd, handler, data) eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, handler, data)
#define NEW_EVENT_WRITE(fd, handler, data) eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_WRITE, handler, data)
#define NEW_EVENT_ONCE(timeout, handler, data) eventNew(timeout, EVENT_TYPE_TIME, EVENT_FLAG_TIME_ONCE, handler, data)
#define NEW_EVENT_REPEAT(timeout, handler, data) eventNew(timeout, EVENT_TYPE_TIME, EVENT_FLAG_TIME_REPEAT, handler, data)
#define NEW_EVENT_SIGNAL(signal, handler, data) eventNew(signal, EVENT_TYPE_SIGNAL, 0, handler, data)
#define DEL_EVENT(e) eventDel(e)
#define CLR_EVENT(e) do { eventDel(e); eventFree(e); e = NULL; } while (0)

eventLoop *eventLoopNew(int size);
void eventLoopFree(eventLoop *el);
void eventLoopRun(eventLoop *el);
void eventLoopStop(eventLoop *el);

event *eventNew(int id, int type, int flags, eventHandler handler, void *data);
void eventFree(event *e);
int eventAdd(eventLoop *el, event *e);
void eventDel(event *e);

char *eventGetApiName();

#endif /* __XS_EVENT_H */
