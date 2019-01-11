
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
    eventApiNew(el);
    return el;
}

void eventLoopFree(eventLoop *el) {
    eventApiFree(el);
    xs_free(el);
}

void eventLoopRun(eventLoop *el) {
    eventApiRun(el);
}

void eventLoopStop(eventLoop *el) {
}

int eventAdd(eventLoop *el, event* e) {
    eventApiAddEvent(el, e);
    return 0;
}

void eventDel(eventLoop *el, event* e) {
    eventApiDelEvent(el, e);
}
