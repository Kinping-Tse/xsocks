
#ifndef __MODULE_TCP_H
#define __MODULE_TCP_H

#define STAGE_ERROR     -1  /* Error detected                   */
#define STAGE_INIT       0  /* Initial stage                    */
#define STAGE_HANDSHAKE  1  /* Handshake with client            */
#define STAGE_SNI        3  /* Parse HTTP/SNI header            */
#define STAGE_RESOLVE    4  /* Resolve the hostname             */
#define STAGE_STREAM     5  /* Stream between client and server */

typedef struct tcpServer {
    int fd;
    event *re;
    int client_count;
    int remote_count;
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

tcpServer *tcpServerNew(int fd);
void tcpServerFree(tcpServer *server);
void tcpConnectionFree(tcpClient *client);

tcpClient *tcpClientNew(int fd);
void tcpClientFree(tcpClient *client);
extern eventHandler tcpClientReadHandler;

tcpRemote *tcpRemoteNew(int fd);
void tcpRemoteFree(tcpRemote *remote);

#endif /* __MODULE_TCP_H */
