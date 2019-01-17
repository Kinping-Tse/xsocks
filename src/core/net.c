#include "common.h"
#include "net.h"

#include "anet.h"
#include <stdarg.h>

static void anetSetError(char *err, const char *fmt, ...) {
    va_list ap;

    if (!err) return;
    va_start(ap, fmt);
    vsnprintf(err, ANET_ERR_LEN, fmt, ap);
    va_end(ap);
}

static int anetSetReuseAddr(char *err, int fd) {
    int yes = 1;
    /* Make sure connection-intensive things like the redis benchmark
     * will be able to close/open sockets a zillion of times */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        anetSetError(err, "setsockopt SO_REUSEADDR: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

static int anetBind(char *err, int s, sockAddr *saddr, socklen_t slen) {
    if (bind(s, saddr, slen) == -1) {
        anetSetError(err, "bind: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

static int anetV6Only(char *err, int s) {
    int yes = 1;
    if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)) == -1) {
        anetSetError(err, "setsockopt: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

static int _netUdpServer(char *err, int port, char *bindaddr, int af) {
    int s = -1, rv;
    char port_s[PORT_MAX_STR_LEN];
    addrInfo hints, *servinfo, *p;

    snprintf(port_s, 6, "%d", port);
    bzero(&hints, sizeof(hints));

    hints.ai_family = af;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;    /* No effect if bindaddr != NULL */

    if ((rv = getaddrinfo(bindaddr, port_s, &hints, &servinfo)) != 0) {
        anetSetError(err, "%s", gai_strerror(rv));
        return ANET_ERR;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (af == AF_INET6 && anetV6Only(err, s) == ANET_ERR) goto error;
        if (anetSetReuseAddr(err, s) == ANET_ERR) goto error;
        if (anetBind(err, s, p->ai_addr,p->ai_addrlen) == ANET_ERR) goto error;

        goto end;
    }
    if (p == NULL) {
        anetSetError(err, "unable to bind socket, errno: %d", errno);
        goto error;
    }

error:
    if (s != -1) close(s);
    s = ANET_ERR;
end:
    freeaddrinfo(servinfo);
    return s;
}

int netUdpServer(char *err, int port, char *bindaddr) {
    return _netUdpServer(err, port, bindaddr, AF_INET);
}

int netUdp6Server(char *err, int port, char *bindaddr) {
    return _netUdpServer(err, port, bindaddr, AF_INET6);
}
