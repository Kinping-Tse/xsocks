
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
