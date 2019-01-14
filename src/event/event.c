
#include "../common.h"
#include "event.h"

#ifdef USE_AE
    #include "event_ae.h"
#elif USE_LIBEV
    #include "event_libev.h"
#else
    #error "Must use one event"
#endif

eventLoop *eventLoopNew() {
    eventLoop* el = xs_calloc(sizeof(*el));
    el->ctx = eventApiNewLoop();
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
    event* e = xs_calloc(sizeof(*e));
    e->id = id;
    e->type = type;
    e->flags = flags;
    e->handler = handler;
    e->data = data;
    e->ctx = eventApiNewEvent(e);
    return e;
}

void eventFree(event* e) {
    eventApiFreeEvent(e->ctx);
    xs_free(e);
}

int eventAdd(eventLoop *el, event* e) {
    e->el = el;
    return eventApiAddEvent(el->ctx, e->ctx);
}

void eventDel(event* e) {
    if (e->el == NULL) return;

    eventApiDelEvent(e->el->ctx, e->ctx);
    e->el = NULL;
}

char *eventGetApiName() {
    return eventApiName();
}
