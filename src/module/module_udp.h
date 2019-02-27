
#ifndef __MODULE_UDP_H
#define __MODULE_UDP_H

typedef struct udpServer {
    int fd;
    event *re;
    int remote_count;
} udpServer;

udpServer *udpServerCreate(char *host, int port);
udpServer *udpServerNew(int fd);
void udpServerFree(udpServer *server);

#endif /* __MODULE_UDP_H */
