#ifndef __XS_EVENT_AE_H
#define __XS_EVENT_AE_H

#include "ae.h"

typedef struct eventContext {
    aeEventLoop *el;
    // int fd;
    // int reading;
    // int writing;
} eventContext;

static int eventApiNew(eventLoop *el) {
    eventContext *ctx = xs_calloc(sizeof(*ctx));
    ctx->el = aeCreateEventLoop(64);

    el->ctx = ctx;

    return EVENT_OK;
}

static void eventApiFree(eventLoop *el) {
    eventContext *ctx = el->ctx;
    aeDeleteEventLoop(ctx->el);
    xs_free(ctx);
}

static int eventApiAddEvent(eventLoop *el, event* e) {
    return 0;
}

static void eventApiDelEvent(eventLoop *el, event* e) {
}

static void eventApiRun(eventLoop *el) {
    eventContext *ctx = el->ctx;
    aeMain(ctx->el);
}

#endif /* __XS_EVENT_AE_H */
