
#ifndef __PROTOCOL_PROXY_H
#define __PROTOCOL_PROXY_H

#include "../core/common.h"

#include "../core/net.h"
#include "../core/time.h"
#include "../core/utils.h"
#include "../event/event.h"

#include "redis/anet.h"
#include "redis/sds.h"

enum {
    CONN_TYPE_SHADOWSOCKS = 1,
    CONN_TYPE_RAW,
    CONN_TYPE_SOCKS5,
};

#define ADD_EVENT(c, e) do { assert(c->el); assert(e); eventAdd(c->el, e); } while (0)
#define ADD_EVENT_READ(c) ADD_EVENT(c, c->re)
#define ADD_EVENT_WRITE(c) ADD_EVENT(c, c->we)
#define ADD_EVENT_TIME(c) do { if (c->timeout > 0) ADD_EVENT(c, c->te); } while (0)
#define DEL_EVENT_READ(c) DEL_EVENT(c->re)
#define DEL_EVENT_WRITE(c) DEL_EVENT(c->we)
#define DEL_EVENT_TIME(c) DEL_EVENT(c->te)
#define CLR_EVENT_READ(c) CLR_EVENT(c->re)
#define CLR_EVENT_WRITE(c) CLR_EVENT(c->we)
#define CLR_EVENT_TIME(c) CLR_EVENT(c->te)

#define CONN_ON_READ(c, h) (c)->onRead = (h)
#define CONN_ON_WRITE(c, h) (c)->onWrite = (h)
#define CONN_ON_CONNECT(c, h) (c)->onConnect = (h)
#define CONN_ON_TIMEOUT(c, h) (c)->onTimeout = (h)
#define CONN_ON_CLOSE(c, h) (c)->onClose = (h)
#define CONN_ON_ERROR(c, h) (c)->onError = (h)

#define TCP_ON_ACCEPT(c, h) (c)->onAccept = (h)

#define TCP_READ(c, buf, len) (c)->read(c, buf, len)
#define TCP_WRITE(c, buf, len) (c)->write(c, buf, len)

#define UDP_READ(c, buf, len, sa) (c)->read(c, buf, len, sa)
#define UDP_WRITE(c, buf, len, sa) (c)->write(c, buf, len, sa)

#define CONN_CLOSE(c) do { if (c) (c)->close(c); } while (0)
#define CONN_GET_ADDRINFO(c) (c)->getAddrinfo(c)

#define FIRE_READ(c) do { if (c->onRead) c->onRead(c->data); } while (0)
#define FIRE_WRITE(c) do { if (c->onWrite) c->onWrite(c->data); } while (0)
#define FIRE_TIMEOUT(c) do { if (c->onTimeout) c->onTimeout(c->data); } while (0)
#define FIRE_CONNECT(c, s) do { if (c->onConnect) c->onConnect(c->data, s); } while (0)
#define FIRE_CLOSE(c) do { if (c->onClose) c->onClose(c->data); } while (0)
#define FIRE_ERROR(c) do { if (c->onError) c->onError(c->data); } while (0)

#endif /* __PROTOCOL_PROXY_H */
