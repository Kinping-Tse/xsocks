
#ifndef __XS_EVENT_LIBEV_H
#define __XS_EVENT_LIBEV_H

#include "../core/time.h"

#include "libev/ev.h"

typedef struct eventLoopContext {
    struct ev_loop *el;
} eventLoopContext;

typedef struct eventContext {
    union {
        struct ev_io io;
        struct ev_timer t;
        struct ev_signal sig;
    } w;
    event *e;
} eventContext;

static void eventIoHandler(EV_P_ struct ev_io *w, int revents) {
#if EV_MULTIPLICITY
    UNUSED(loop);
#endif
    UNUSED(revents);

    event *e = w->data;
    e->handler(e);
}

static void eventTimeHandler(EV_P_ struct ev_timer *w, int revents) {
#if EV_MULTIPLICITY
    UNUSED(loop);
#endif
    UNUSED(revents);

    event *e = w->data;
    e->handler(e);
}

static void eventSignalHandler(EV_P_ struct ev_signal *w, int revents) {
#if EV_MULTIPLICITY
    UNUSED(loop);
#endif
    if (revents & EV_SIGNAL) {
        event *e = w->data;
        e->handler(e);
    }
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

    if (e->type == EVENT_TYPE_IO) {
        int events = EV_UNDEF;
        if (e->flags == EVENT_FLAG_READ)
            events = EV_READ;
        else if (e->flags == EVENT_FLAG_WRITE)
            events = EV_WRITE;

        ev_io_init(&ctx->w.io, eventIoHandler, e->id, events);
        ctx->w.io.data = e;
    } else if (e->type == EVENT_TYPE_TIME) {
        int repeat = e->flags == EVENT_FLAG_TIME_ONCE ? 0 : e->id;
        ev_timer_init(&ctx->w.t, eventTimeHandler, e->id/MILLISECOND_UNIT_F, repeat);
        ctx->w.t.data = e;
    } else if (e->type == EVENT_TYPE_SIGNAL) {
        ev_signal_init(&ctx->w.sig, eventSignalHandler, e->id);
        ctx->w.sig.data = e;
    }
    ctx->e = e;

    return ctx;
}

static void eventApiFreeEvent(eventContext *ctx) {
    xs_free(ctx);
}

static int eventApiAddEvent(eventLoopContext *elCtx, eventContext *eCtx) {
    switch (eCtx->e->type) {
        case EVENT_TYPE_IO: ev_io_start(elCtx->el, &eCtx->w.io); break;
        case EVENT_TYPE_TIME: ev_timer_start(elCtx->el, &eCtx->w.t); break;
        case EVENT_TYPE_SIGNAL: ev_signal_start(elCtx->el, &eCtx->w.sig); break;
        default: return EVENT_ERR;
    }
    return EVENT_OK;
}

static void eventApiDelEvent(eventLoopContext *elCtx, eventContext *eCtx) {
    switch (eCtx->e->type) {
        case EVENT_TYPE_IO: ev_io_stop(elCtx->el, &eCtx->w.io); break;
        case EVENT_TYPE_TIME: ev_timer_stop(elCtx->el, &eCtx->w.t); break;
        case EVENT_TYPE_SIGNAL: ev_signal_stop(elCtx->el, &eCtx->w.sig); break;
        default: LOGE("Unknown event type!"); break;
    }
}

static void eventApiRun(eventLoopContext *ctx) {
    ev_run(ctx->el, 0);
}

static void eventApiStop(eventLoopContext *ctx) {
    ev_break(ctx->el, EVUNLOOP_ALL);
}

static char *eventApiName() {
    return "libev";
}

#endif /* __XS_EVENT_LIBEV_H */
