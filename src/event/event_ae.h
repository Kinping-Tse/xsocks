#ifndef __XS_EVENT_AE_H
#define __XS_EVENT_AE_H

#include "redis/ae.h"

typedef struct eventLoopContext {
    aeEventLoop *el;
} eventLoopContext;

typedef struct eventContext {
    event* e;
    int mask;
} eventContext;

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
    e->handler(e);

    return e->flags == EVENT_FLAG_TIME_ONCE ? AE_NOMORE : e->id*1000;
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
    } else if (e->type == EVENT_TYPE_TIME)
        eCtx->mask = aeCreateTimeEvent(elCtx->el, e->id*1000, eventTimeHandler, e, NULL);
    else
        return EVENT_ERR;

    return EVENT_OK;
}

static void eventApiDelEvent(eventLoopContext *elCtx, eventContext* eCtx) {
    if (eCtx->e->type == EVENT_TYPE_IO)
        aeDeleteFileEvent(elCtx->el, eCtx->e->id, eCtx->mask);
    else if (eCtx->e->type == EVENT_TYPE_TIME)
        aeDeleteTimeEvent(elCtx->el, eCtx->mask);
    else
        assert("Invalid event type");
}

static void eventApiRun(eventLoopContext *ctx) {
    aeMain(ctx->el);
}

static void eventApiStop(eventLoopContext *ctx) {
    UNUSED(ctx);
}

static char *eventApiName() {
    return "ae";
}

#endif /* __XS_EVENT_AE_H */
