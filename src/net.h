#ifndef __NET_H
#define __NET_H

#include <arpa/inet.h>

#define HOSTNAME_MAX_LEN 256
#define NET_IPV4_STR_LEN INET_ADDRSTRLEN /*  INET_ADDRSTRLEN  */
#define NET_IPV6_STR_LEN INET6_ADDRSTRLEN /*  46  */
#define NET_IP_MAX_STR_LEN NET_IPV6_STR_LEN
#define NET_IOBUF_LEN  (1024*16)  /* Generic I/O buffer size */

typedef struct in_addr ipV4Addr;
typedef struct in6_addr ipV6Addr;
typedef struct sockaddr_in sockAddrIpV4;
typedef struct sockaddr_in6 sockAddrIpV6;

#endif /* __NET_H */
