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

typedef struct in_addr ipV4Addr;
typedef struct in6_addr ipV6Addr;
typedef struct sockaddr sockAddr;
typedef struct sockaddr_in sockAddrIpV4;
typedef struct sockaddr_in6 sockAddrIpV6;
typedef struct addrinfo addrInfo;

int netUdpServer(char *err, int port, char *bindaddr);
int netUdp6Server(char *err, int port, char *bindaddr);

#endif /* __NET_H */
