#ifndef __XS_EVENT_AE_H
#define __XS_EVENT_AE_H

#include "redis/ae.h"

#include <signal.h>

typedef struct eventLoopContext {
    aeEventLoop *el;
} eventLoopContext;

typedef struct eventContext {
    event* e;
    int mask;
} eventContext;

#define _MAX_SIGNUM NSIG

static void *signals[_MAX_SIGNUM] = {NULL};

static void eventIoHandler(aeEventLoop *el, int fd, void *data, int mask) {
    UNUSED(el);
    UNUSED(fd);
    UNUSED(mask);

    event *e = data;
    e->handler(e);
}

static int eventTimeHandler(aeEventLoop *el, long long id, void *data) {
    UNUSED(el);
    UNUSED(id);

    event *e = data;
    int next_time = EVENT_FLAG_TIME_ONCE ? AE_NOMORE : e->id*1000;

    e->handler(e);

    return next_time;
}

static void eventSignalHandler(int signal, siginfo_t *siginfo, void *data) {
    UNUSED(signal);
    UNUSED(siginfo);
    UNUSED(data);

    event *e = signals[signal];
    e->handler(e);
}

static eventLoopContext *eventApiNewLoop() {
    eventLoopContext *ctx = xs_calloc(sizeof(*ctx));
    ctx->el = aeCreateEventLoop(64);

    return ctx;
}

static void eventApiFreeLoop(eventLoopContext *ctx) {
    aeDeleteEventLoop(ctx->el);
    xs_free(ctx);
}

static eventContext *eventApiNewEvent(event *e) {
    eventContext *ctx = xs_calloc(sizeof(*ctx));

    int mask = AE_NONE;
    if (e->flags == EVENT_FLAG_READ)
        mask = AE_READABLE;
    else if (e->flags == EVENT_FLAG_WRITE)
        mask = AE_WRITABLE;

    ctx->mask = mask;
    ctx->e = e;

    return ctx;
}

static void eventApiFreeEvent(eventContext* ctx) {
    xs_free(ctx);
}

static int eventApiAddEvent(eventLoopContext *elCtx, eventContext* eCtx) {
    event* e = eCtx->e;

    if (e->type == EVENT_TYPE_IO) {
        if (aeCreateFileEvent(elCtx->el, e->id, eCtx->mask, eventIoHandler, e) == AE_ERR)
            return EVENT_ERR;
    } else if (e->type == EVENT_TYPE_TIME) {
        if ((eCtx->mask = aeCreateTimeEvent(elCtx->el, e->id*1000, eventTimeHandler, e, NULL)) == AE_ERR)
            return EVENT_ERR;
    } else if (e->type == EVENT_TYPE_SIGNAL) {
        if (signals[e->id]) return EVENT_ERR;

        struct sigaction act;
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_SIGINFO;
        act.sa_sigaction = eventSignalHandler;

        if (sigaction(e->id, &act, NULL) == -1) return EVENT_ERR;
        signals[e->id] = e;
    } else
        return EVENT_ERR;

    return EVENT_OK;
}

static void eventApiDelEvent(eventLoopContext *elCtx, eventContext* eCtx) {
    event *e = eCtx->e;
    switch (e->type) {
        case EVENT_TYPE_IO: aeDeleteFileEvent(elCtx->el, e->id, eCtx->mask); break;
        case EVENT_TYPE_TIME: aeDeleteTimeEvent(elCtx->el, eCtx->mask); break;
        case EVENT_TYPE_SIGNAL: signals[e->id] = NULL; signal(e->id, SIG_DFL); break;
        default: LOGE("Unknown event type!"); break;
    }
}

static void eventApiRun(eventLoopContext *ctx) {
    aeMain(ctx->el);
}

static void eventApiStop(eventLoopContext *ctx) {
    aeStop(ctx->el);
}

static char *eventApiName() {
    return "ae";
}

#endif /* __XS_EVENT_AE_H */
