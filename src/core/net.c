#include "common.h"
#include "net.h"
#include "utils.h"

#include "redis/anet.h"
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

        if (af == AF_INET6 && netSetIpV6Only(err, s, 1) == ANET_ERR) goto error;
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

int netSendTimeout(char *err, int fd, int s) {
    struct timeval tv = {
        .tv_sec = s,
        .tv_usec = 0,
    };
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1) {
        anetSetError(err, "setsockopt SO_SNDTIMEO: %s", STRERR);
        return ANET_ERR;
    }
    return ANET_OK;
}

int netRecvTimeout(char *err, int fd, int s) {
    struct timeval tv = {
        .tv_sec = s,
        .tv_usec = 0,
    };
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
        anetSetError(err, "setsockopt SO_RCVTIMEO: %s", STRERR);
        return ANET_ERR;
    }
    return ANET_OK;
}

int netSetIpV6Only(char *err, int fd, int ipv6_only) {
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only, sizeof(ipv6_only)) == -1) {
        anetSetError(err, "setsockopt: %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}

int netNoSigPipe(char *err, int fd) {
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes)) == -1) {
        anetSetError(err, "setsockopt SO_NOSIGPIPE: %s", STRERR);
        return NET_ERR;
    }
    return NET_OK;
}

void netSockAddrExInit(sockAddrEx* sa) {
    socklen_t slen = sizeof(*sa);
    bzero(sa, slen);
    sa->sa_len = slen;
}

int netUdpGetSockAddrEx(char *err, char *host, int port, int ipv6_first, sockAddrEx *sa) {
    int s = NET_OK, rv;
    char port_s[PORT_MAX_STR_LEN];
    addrInfo hints, *servinfo, *p;

    snprintf(port_s, 6, "%d", port);
    bzero(&hints, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;    /* No effect if bindaddr != NULL */

    if ((rv = getaddrinfo(host, port_s, &hints, &servinfo)) != 0) {
        anetSetError(err, "%s", gai_strerror(rv));
        return NET_ERR;
    }

    int prefer_af = ipv6_first ? AF_INET6 : AF_INET;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if (p->ai_family == prefer_af) {
            if (p->ai_family == AF_INET) {
                memcpy(&sa->sa, p->ai_addr, sizeof(sockAddrIpV4));
                sa->sa_len = sizeof(sockAddrIpV4);
            } else if (p->ai_family == AF_INET6) {
                memcpy(&sa->sa, p->ai_addr, sizeof(sockAddrIpV6));
                sa->sa_len = sizeof(sockAddrIpV6);
            }
            break;
        }

        goto end;
    }

    if (p == NULL) {
        for (p = servinfo; p != NULL; p = p->ai_next) {
            if (p->ai_family == AF_INET) {
                memcpy(&sa->sa, p->ai_addr, sizeof(sockAddrIpV4));
                sa->sa_len = sizeof(sockAddrIpV4);
            } else if (p->ai_family == AF_INET6) {
                memcpy(&sa->sa, p->ai_addr, sizeof(sockAddrIpV6));
                sa->sa_len = sizeof(sockAddrIpV6);
            }
            break;
        }
    }

    if (p == NULL) {
        anetSetError(err, "Failed to resolve addr");
        goto error;
    }

    goto end;

error:
    s = NET_ERR;
end:
    freeaddrinfo(servinfo);
    return s;
}

int netIpPresentBySockAddr(char *err, char *ip, int ip_len, int *port, sockAddrEx* sae) {
    sockAddrStorage *s = &sae->sa;
    if (s->ss_family == AF_INET) {
        sockAddrIpV4 *sa = (sockAddrIpV4*)s;
        if (ip && netIpPresentByIpAddr(err, ip, ip_len, (void*)&(sa->sin_addr), 0) == NET_ERR)
            return NET_ERR;
        if (port) *port = ntohs(sa->sin_port);
    } else {
        sockAddrIpV6 *sa = (sockAddrIpV6*)s;
        if (ip && netIpPresentByIpAddr(err, ip, ip_len, (void*)&(sa->sin6_addr), 1) == NET_ERR)
            return NET_ERR;
        if (port) *port = ntohs(sa->sin6_port);
    }
    return NET_OK;
}

int netIpPresentByIpAddr(char *err, char *ip, int ip_len, void *addr, int is_ipv6) {
    if (!inet_ntop(!is_ipv6 ? AF_INET : AF_INET6, addr, ip, ip_len)) {
        anetSetError(err, "inet_ntop error: %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}
