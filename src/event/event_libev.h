
#ifndef __XS_EVENT_LIBEV_H
#define __XS_EVENT_LIBEV_H

#include "libev/ev.h"

typedef struct eventLoopContext {
    struct ev_loop *el;
} eventLoopContext;

typedef struct eventContext {
    struct ev_io e;
} eventContext;

static void eventHandlerWrapper(EV_P_ struct ev_io *w, int revents) {
#if EV_MULTIPLICITY
    UNUSED(loop);
#endif
    UNUSED(revents);

    event *e = w->data;
    e->handler(e);
}

static eventLoopContext *eventApiNewLoop() {
    eventLoopContext *ctx = xs_calloc(sizeof(*ctx));

#if EV_MULTIPLICITY
    ctx->el = EV_DEFAULT;
#else
    ctx->el = NULL;
#endif

    return ctx;
}

static void eventApiFreeLoop(eventLoopContext *ctx) {
    xs_free(ctx);
}

static eventContext *eventApiNewEvent(event *e) {
    eventContext *ctx = xs_calloc(sizeof(*ctx));
    int events = EV_UNDEF;
    if (e->flags == EVENT_FLAG_READ)
        events = EV_READ;
    else if (e->flags == EVENT_FLAG_WRITE)
        events = EV_WRITE;

    ctx->e.data = e;
    ev_io_init(&ctx->e, eventHandlerWrapper, e->id, events);

    return ctx;
}

static void eventApiFreeEvent(eventContext* ctx) {
    xs_free(ctx);
}

static int eventApiAddEvent(eventLoopContext *elCtx, eventContext* eCtx) {
    ev_io_start(elCtx->el, &eCtx->e);

    return EVENT_OK;
}

static void eventApiDelEvent(eventLoopContext *elCtx, eventContext* eCtx) {
    ev_io_stop(elCtx->el, &eCtx->e);
}

static void eventApiRun(eventLoopContext *ctx) {
    ev_run(ctx->el, 0);
}

static void eventApiStop(eventLoopContext *ctx) {
    UNUSED(ctx);
}

static char *eventApiName() {
    return "libev";
}

#endif /* __XS_EVENT_LIBEV_H */
