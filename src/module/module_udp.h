
#ifndef __MODULE_UDP_H
#define __MODULE_UDP_H

#include "../protocol/udp.h"

typedef struct udpServer {
    udpConn *conn;
    int remote_count;
} udpServer;

typedef struct udpClient {
    udpServer *server;
    struct udpRemote *remote;
    sockAddrEx sa_client;
    sockAddrEx sa_remote;
} udpClient;

typedef struct udpRemote {
    udpConn *conn;
    udpClient *client;
} udpRemote;

udpServer *udpServerNew(char *host, int port, int type, udpEventHandler onRead);
void udpServerFree(udpServer *server);

udpClient *udpClientNew(udpServer *server);
udpRemote *udpRemoteNew(udpClient *client, int type, char *host, int port);
void udpConnectionFree(udpClient *client);

#endif /* __MODULE_UDP_H */
