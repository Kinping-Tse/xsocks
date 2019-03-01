
#ifndef __MODULE_TCP_H
#define __MODULE_TCP_H

#include "../core/net.h"
#include "../event/event.h"

#include "redis/sds.h"
#include "shadowsocks-libev/crypto.h"

#define STAGE_ERROR     -1  /* Error detected                   */
#define STAGE_INIT       0  /* Initial stage                    */
#define STAGE_HANDSHAKE  1  /* Handshake with client            */
#define STAGE_SNI        3  /* Parse HTTP/SNI header            */
#define STAGE_RESOLVE    4  /* Resolve the hostname             */
#define STAGE_STREAM     5  /* Stream between client and server */

enum {
    TCP_OK = 0,
    TCP_ERR = -1,
};

struct tcpClient;
typedef int (*clientReadHandler)(struct tcpClient *client);

typedef struct tcpServer {
    int fd;
    event *re;
    int client_count;
    int remote_count;
    clientReadHandler crHandler;
} tcpServer;

typedef struct tcpClient {
    int fd;
    int stage;
    event *re;
    event *we;
    event *te;
    buffer_t buf;
    int buf_off;
    sds addr_buf;
    tcpServer *server;
    struct tcpRemote *remote;
    cipher_ctx_t *e_ctx;
    cipher_ctx_t *d_ctx;
    char client_addr_info[ADDR_INFO_STR_LEN];
    char remote_addr_info[ADDR_INFO_STR_LEN];
} tcpClient;

typedef struct tcpRemote {
    int fd;
    buffer_t buf;
    int buf_off;
    event *re;
    event *we;
    event *te;
    tcpClient *client;
} tcpRemote;

tcpServer *tcpServerCreate(char *host, int port, clientReadHandler handler);
tcpServer *tcpServerNew(int fd);
void tcpServerFree(tcpServer *server);
void tcpConnectionFree(tcpClient *client);

tcpClient *tcpClientNew(int fd);
void tcpClientFree(tcpClient *client);

tcpRemote *tcpRemoteNew(int fd);
void tcpRemoteFree(tcpRemote *remote);

#endif /* __MODULE_TCP_H */
