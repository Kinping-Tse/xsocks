#ifndef __XS_EVENT_H
#define __XS_EVENT_H

struct event;

typedef void (*eventHandler)(struct event* e);

enum {
    EVENT_TYPE_IO = 0,
    EVENT_TYPE_TIME = 1,
};

enum {
    EVENT_FLAG_READ = 0,
    EVENT_FLAG_WRITE = 1,
};

enum {
    EVENT_OK = 0,
    EVENT_ERR = -1
};

typedef struct eventLoop {
    struct eventLoopContext *ctx;
} eventLoop;

typedef struct event {
    int id;
    int type;
    int flags;
    eventHandler handler;
    void *data;
    struct eventLoop *el;
    struct eventContext *ctx;
} event;

eventLoop *eventLoopNew();
void eventLoopFree(eventLoop *el);
void eventLoopRun(eventLoop *el);
void eventLoopStop(eventLoop *el);

event *eventNew(int id, int type, int flags, eventHandler handler, void *data);
void eventFree(event* e);
int eventAdd(eventLoop *el, event* e);
void eventDel(event* e);

#endif /* __XS_EVENT_H */
