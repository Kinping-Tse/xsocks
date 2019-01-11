#ifndef __XS_EVENT_H
#define __XS_EVENT_H

struct eventLoop;
struct event;
struct eventContext;

typedef void (*eventCallback)(struct eventLoop *el, struct event* e);

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

typedef struct event {
    int id;
    int type;
    int flags;
    eventCallback fn;
    void *data;
} event;

typedef struct eventLoop {
    struct eventContext *ctx;
} eventLoop;

eventLoop *eventLoopNew();
void eventLoopFree(eventLoop *el);
void eventLoopRun(eventLoop *el);
void eventLoopStop(eventLoop *el);

int eventAdd(eventLoop *el, event* e);
void eventDel(eventLoop *el, event* e);

#endif /* __XS_EVENT_H */
