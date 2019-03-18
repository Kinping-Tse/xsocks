
#ifndef __MODULE_UDP_H
#define __MODULE_UDP_H

#include "../core/net.h"
#include "../event/event.h"

#include "shadowsocks-libev/crypto.h"

enum {
    UDP_OK = 0,
    UDP_ERR = -1,
};

typedef int (*udpHandler)(void *data);

typedef struct udpHook {
    udpHandler init;
    udpHandler process;
    udpHandler free;
} udpHook;

typedef struct udpServer {
    int fd;
    event *re;
    int remote_count;
    udpHook hook;
    void *data;
} udpServer;

typedef struct udpClient {
    buffer_t buf;
    int buf_off;
    udpServer *server;
    struct udpRemote *remote;
    sockAddrEx sa_client;
    sockAddrEx sa_remote;
} udpClient;

typedef struct udpRemote {
    int fd;
    event *re;
    event *te;
    buffer_t buf;
    int buf_off;
    udpClient *client;
    udpHook hook;
    void *data;
} udpRemote;

udpServer *moduleUdpServerCreate(char *host, int port, udpHook hook, void *data);
void moduleUdpServerFree(udpServer *server);

udpRemote *udpRemoteCreate(udpHook *hook, void *data);
void udpRemoteFree(udpRemote *remote);

#endif /* __MODULE_UDP_H */
