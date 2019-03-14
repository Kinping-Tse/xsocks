
#include "common.h"

#include "net.h"
#include "error.h"

#include "redis/anet.h"

#include <stdarg.h>

#ifdef __linux__
#include <linux/if.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#endif

#define anetSetError errorSet

static int _netUdpServer(char *err, int port, char *bindaddr, int af);
static int anetSetReuseAddr(char *err, int fd);
static int anetBind(char *err, int s, sockAddr *saddr, socklen_t slen);

int isIPv6Addr(char *ip) {
    return strchr(ip, ':') ? 1 : 0;
}

int netTcpRead(char *err, int fd, char *buf, int buflen, int *closed) {
    int nread;
    int total_len = 0;
    if (closed) *closed = 0;

    while (total_len < buflen) {
        nread = read(fd, buf + total_len, buflen - total_len);
        if (nread <= 0) break;

        total_len += nread;
    }

    if (total_len == 0) {
        if (nread == 0 && closed) *closed = 1;

        if (nread == -1) {
            if (errno != EAGAIN) {
                errorSet(err, "%s", STRERR);
                return NET_ERR;
            }
        }
    }

    return total_len;
}

int netTcpWrite(char *err, int fd, char *buf, int buflen) {
    int nwrite;
    int total_len = 0;

    while (total_len < buflen) {
        nwrite = write(fd, buf + total_len, buflen - total_len);
        if (nwrite <= 0) break;

        total_len += nwrite;
    }

    if (total_len == 0 && nwrite == -1 && errno != EAGAIN) {
        errorSet(err, "%s", STRERR);
        return NET_ERR;
    }

    return total_len;
}

int netUdpRead(char *err, int fd, char *buf, int buflen, sockAddrEx *sa) {
    int nread;
    sockAddr *psa = sa ? (sockAddr *)&sa->sa : NULL;
    socklen_t *slen = sa ? &sa->sa_len : NULL;

    nread = recvfrom(fd, buf, buflen, 0, psa, slen);
    if (nread == -1) {
        anetSetError(err, "%s", STRERR);
        return NET_ERR;
    }
    return nread;
}

int netUdpWrite(char *err, int fd, char *buf, int buflen, sockAddrEx *sa) {
    int nwrite = sendto(fd, buf, buflen, 0, (sockAddr *)&sa->sa, sa->sa_len);
    if (nwrite == -1) {
        anetSetError(err, "%s", STRERR);
        return NET_ERR;
    }
    return nwrite;
}

int netTcpNonBlockConnect(char *err, char *addr, int port, sockAddrEx *sa) {
    int s = NET_ERR, rv;
    char portstr[6]; /* strlen("65535") + 1; */
    addrInfo hints, *servinfo, *p;

    snprintf(portstr, sizeof(portstr), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(addr, portstr, &hints, &servinfo)) != 0) {
        anetSetError(err, "%s", gai_strerror(rv));
        return NET_ERR;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;
        if (anetSetReuseAddr(err, s) == ANET_ERR) goto error;
        if (anetNonBlock(err, s) == ANET_ERR) goto error;
        if (connect(s, p->ai_addr, p->ai_addrlen) == -1) {
            if (errno == EINPROGRESS) break;

            close(s);
            s = NET_ERR;
            continue;
        }
        break;
    }
    if (p) {
        if (sa) {
            memcpy(&sa->sa, p->ai_addr, p->ai_addrlen);
            sa->sa_len = p->ai_addrlen;
        }
        goto end;
    }

    anetSetError(err, "creating socket: %s", STRERR);

error:
    if (s != NET_ERR) {
        close(s);
        s = NET_ERR;
    }

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
        anetSetError(err, "setsockopt: %s", STRERR);
        return NET_ERR;
    }
    return NET_OK;
}

int netNoSigPipe(char *err, int fd) {
#ifdef SO_NOSIGPIPE
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes)) == -1) {
        anetSetError(err, "setsockopt SO_NOSIGPIPE: %s", STRERR);
        return NET_ERR;
    }
#else
    UNUSED(err);
    UNUSED(fd);
#endif
    return NET_OK;
}

void netSockAddrExInit(sockAddrEx *sa) {
    socklen_t slen = sizeof(*sa);
    bzero(sa, slen);
    sa->sa_len = slen;
}

int netTcpGetDestSockAddr(char *err, int fd, int ipv6_first, sockAddrEx *sa) {
#ifndef __linux__
    UNUSED(err);
    UNUSED(fd);
    UNUSED(ipv6_first);
    UNUSED(sa);
#else
    int res = -1;

    netSockAddrExInit(sa);

    if (ipv6_first) res = getsockopt(fd, SOL_IPV6, IP6T_SO_ORIGINAL_DST, &sa->sa, &sa->sa_len);
    if (res == -1) res = getsockopt(fd, SOL_IP, SO_ORIGINAL_DST, &sa->sa, &sa->sa_len);
    if (!ipv6_first && res == -1) res = getsockopt(fd, SOL_IPV6, IP6T_SO_ORIGINAL_DST, &sa->sa, &sa->sa_len);

    if (res == -1) {
        anetSetError(err, "getsockopt SO_ORIGINAL_DST: %s", STRERR);
        return NET_ERR;
    }
#endif
    return NET_OK;
}

int netUdpGetSockAddrEx(char *err, char *host, int port, int ipv6_first, sockAddrEx *sa) {
    int s = NET_OK, rv;
    char port_s[PORT_MAX_STR_LEN];
    addrInfo hints, *servinfo, *p;

    snprintf(port_s, 6, "%d", port);
    bzero(&hints, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; /* No effect if bindaddr != NULL */

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

int netIpPresentBySockAddr(char *err, char *ip, int ip_len, int *port, sockAddrEx *sae) {
    sockAddrStorage *s = &sae->sa;
    if (s->ss_family == AF_INET) {
        sockAddrIpV4 *sa = (sockAddrIpV4 *)s;
        if (ip && netIpPresentByIpAddr(err, ip, ip_len, (void *)&(sa->sin_addr), 0) == NET_ERR) return NET_ERR;
        if (port) *port = ntohs(sa->sin_port);
    } else {
        sockAddrIpV6 *sa = (sockAddrIpV6 *)s;
        if (ip && netIpPresentByIpAddr(err, ip, ip_len, (void *)&(sa->sin6_addr), 1) == NET_ERR) return NET_ERR;
        if (port) *port = ntohs(sa->sin6_port);
    }
    return NET_OK;
}

int netIpPresentByIpAddr(char *err, char *ip, int ip_len, void *addr, int is_ipv6) {
    if (!inet_ntop(!is_ipv6 ? AF_INET : AF_INET6, addr, ip, ip_len)) {
        anetSetError(err, "inet_ntop error: %s", STRERR);
        return NET_ERR;
    }
    return NET_OK;
}

int netHostPortParse(char *addr, char *host, int *port) {
    if (!addr) return NET_ERR;

    char *p = strrchr(addr, ':');
    if (p) {
        int offset = p - addr;
        if (host) {
            memcpy(host, addr, offset);
            host[offset] = '\0';
        }
        if (port) *port = atoi(p+1);
        return NET_OK;
    }
    return NET_ERR;
}

static int _netUdpServer(char *err, int port, char *bindaddr, int af) {
    int s = -1, rv;
    char port_s[PORT_MAX_STR_LEN];
    addrInfo hints, *servinfo, *p;

    snprintf(port_s, 6, "%d", port);
    bzero(&hints, sizeof(hints));

    hints.ai_family = af;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; /* No effect if bindaddr != NULL */

    if ((rv = getaddrinfo(bindaddr, port_s, &hints, &servinfo)) != 0) {
        anetSetError(err, "%s", gai_strerror(rv));
        return ANET_ERR;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) continue;

        if (af == AF_INET6 && netSetIpV6Only(err, s, 1) == ANET_ERR) goto error;
        if (anetSetReuseAddr(err, s) == ANET_ERR) goto error;
        if (anetBind(err, s, p->ai_addr, p->ai_addrlen) == ANET_ERR) goto error;

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

static int anetSetReuseAddr(char *err, int fd) {
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        anetSetError(err, "setsockopt SO_REUSEADDR: %s", STRERR);
        return ANET_ERR;
    }
    return ANET_OK;
}

static int anetBind(char *err, int s, sockAddr *saddr, socklen_t slen) {
    if (bind(s, saddr, slen) == -1) {
        anetSetError(err, "bind: %s", STRERR);
        return ANET_ERR;
    }
    return ANET_OK;
}
