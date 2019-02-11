
#ifndef __MODULE_UDP_H
#define __MODULE_UDP_H

typedef struct udpServer {
    int fd;
    event *re;
} udpServer;

typedef struct udpRemote {
    int fd;
    event *re;
    sockAddrEx sa_client;
    sockAddrEx sa_remote;
    udpServer *server;
} udpRemote;

udpServer *udpServerNew(int fd);
void udpServerFree(udpServer *server);

udpRemote *udpRemoteCreate(char *host);
void udpRemoteFree(udpRemote *remote);

#endif /* __MODULE_UDP_H */
