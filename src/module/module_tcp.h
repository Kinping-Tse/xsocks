
#ifndef __MODULE_TCP_H
#define __MODULE_TCP_H

#include "../protocol/tcp.h"

typedef struct tcpServer {
    tcpListener *ln;
    int client_count;
    int remote_count;
} tcpServer;

typedef struct tcpClient {
    int type;
    tcpConn *conn;
    tcpServer *server;
    struct tcpRemote *remote;
} tcpClient;

typedef struct tcpRemote {
    int type;
    tcpConn *conn;
    tcpClient *client;
} tcpRemote;

tcpServer *tcpServerNew(char *host, int port, tcpEventHandler onAccept);
void tcpServerFree(tcpServer *server);

tcpClient *tcpClientNew(tcpServer *server, int type, tcpEventHandler onRead);
tcpRemote *tcpRemoteNew(tcpClient *client, int type, char *host, int port, tcpConnectHandler onConnect);
void tcpConnectionFree(tcpClient *client);

#endif /* __MODULE_TCP_H */
