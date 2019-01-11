
#ifndef __XS_EVENT_LIBEV_H
#define __XS_EVENT_LIBEV_H

#include "ev.h"

typedef struct eventContext {
    struct ev_loop *el;
} eventContext;

static int eventApiNew(eventLoop *el) {
    eventContext *ctx = xs_calloc(sizeof(*ctx));

#if EV_MULTIPLICITY
    ctx->el = EV_DEFAULT;
#else
    ctx->el = NULL;
#endif

    el->ctx = ctx;

    return EVENT_OK;
}

static void eventApiFree(eventLoop *el) {
    eventContext *ctx = el->ctx;
    xs_free(ctx);
}

static int eventApiAddEvent(eventLoop *el, event* e) {
    return 0;
}

static void eventApiDelEvent(eventLoop *el, event* e) {
}

static void eventApiRun(eventLoop *el) {
    eventContext *ctx = el->ctx;
    ev_run(ctx->el, 0);
}

#endif /* __XS_EVENT_LIBEV_H */
