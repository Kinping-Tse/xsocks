
#ifndef __PROTOCOL_PROXY_H
#define __PROTOCOL_PROXY_H

#include "../core/common.h"
#include "../core/net.h"
#include "../core/time.h"
#include "../core/error.h"
#include "../core/utils.h"
#include "../event/event.h"

#include "redis/anet.h"
#include "redis/sds.h"

#define ADD_EVENT(c, e) do { assert(c->el); assert(e); eventAdd(c->el, e); } while (0)
#define ADD_EVENT_READ(c) ADD_EVENT(c, c->re)
#define ADD_EVENT_WRITE(c) ADD_EVENT(c, c->we)
#define ADD_EVENT_TIME(c) ADD_EVENT(c, c->te)
#define DEL_EVENT_READ(c) DEL_EVENT(c->re)
#define DEL_EVENT_WRITE(c) DEL_EVENT(c->we)
#define DEL_EVENT_TIME(c) DEL_EVENT(c->te)
#define CLR_EVENT_READ(c) CLR_EVENT(c->re)
#define CLR_EVENT_WRITE(c) CLR_EVENT(c->we)
#define CLR_EVENT_TIME(c) CLR_EVENT(c->te)

#endif /* __PROTOCOL_PROXY_H */
