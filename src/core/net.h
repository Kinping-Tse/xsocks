/*
 * This file is part of xsocks, a lightweight proxy tool for science online.
 *
 * Copyright (C) 2019 XJP09_HK <jianping_xie@aliyun.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __NET_H
#define __NET_H

#include <arpa/inet.h>
#include <netdb.h>

#define HOSTNAME_MAX_LEN 256
#define PORT_MAX_STR_LEN 6  /* strlen("65535") */
#define ADDR_INFO_STR_LEN (HOSTNAME_MAX_LEN+PORT_MAX_STR_LEN) /* for dump addr */

#define NET_IPV4_STR_LEN INET_ADDRSTRLEN /*  INET_ADDRSTRLEN  */
#define NET_IPV6_STR_LEN INET6_ADDRSTRLEN /*  46  */
#define NET_IP_MAX_STR_LEN NET_IPV6_STR_LEN
#define NET_IOBUF_LEN  (1024*16)  /* Generic I/O buffer size */

#define IOBUF_MIN_LEN  (1024)  /* Generic I/O buffer size */

typedef struct in_addr ipV4Addr;
typedef struct in6_addr ipV6Addr;
typedef struct sockaddr_storage sockAddrStorage;
typedef struct sockaddr sockAddr;
typedef struct sockaddr_in sockAddrIpV4;
typedef struct sockaddr_in6 sockAddrIpV6;
typedef struct addrinfo addrInfo;

typedef struct sockAddrEx {
    sockAddrStorage sa;
    socklen_t sa_len;
} sockAddrEx;

enum {
    NET_OK = 0,
    NET_ERR = -1,
    NET_ERR_LEN = 256,
};

int isIPv6Addr(char *ip);

int netTcpRead(char *err, int fd, char *buf, int buflen, int *closed);
int netTcpWrite(char *err, int fd, char *buf, int buflen);

int netUdpRead(char *err, int fd, char *buf, int buflen, sockAddrEx *sa);
int netUdpWrite(char *err, int fd, char *buf, int buflen, sockAddrEx *sa);

int netTcpNonBlockConnect(char *err, char *host, int port, sockAddrEx *sa);

int netUdpServer(char *err, int port, char *bindaddr);
int netUdp6Server(char *err, int port, char *bindaddr);

int netSendTimeout(char *err, int fd, int s);
int netRecvTimeout(char *err, int fd, int s);
int netSetIpV6Only(char *err, int fd, int ipv6_only);
int netNoSigPipe(char *err, int fd);

void netSockAddrExInit(sockAddrEx *sa);
int netTcpGetDestSockAddr(char *err, int fd, int ipv6_first, sockAddrEx *sa);
int netUdpGetSockAddrEx(char *err, char *host, int port, int ipv6_first, sockAddrEx *sa);
int netIpPresentBySockAddr(char *err, char *ip, int ip_len, int *port, sockAddrEx *sae);
int netIpPresentByIpAddr(char *err, char *ip, int ip_len, void *addr, int is_ipv6);
int netHostPortParse(char *addr, char *host, int *port);

#endif /* __NET_H */
