
#include "module.h"
#include "module_udp.h"

#include "redis/anet.h"

static void udpServerReadHandler(event *e);

static udpRemote *udpRemoteNew(int fd);
static void udpRemoteReadHandler(event *e);
static void udpRemoteReadTimeHandler(event *e);

udpServer *udpServerNew(int fd) {
    udpServer *server = xs_calloc(sizeof(*server));

    server->fd = fd;
    server->re = eventNew(fd, EVENT_TYPE_IO, EVENT_FLAG_READ, udpServerReadHandler, server);

    anetNonBlock(NULL, server->fd);
    eventAdd(app->el, server->re);

    return server;
}

void udpServerFree(udpServer *server) {
    if (!server) return;

    eventDel(server->re);
    eventFree(server->re);
    close(server->fd);

    xs_free(server);
}

static void udpServerReadHandler(event *e) {
    udpServer *server = e->data;
    udpRemote *remote = NULL;

    int readlen = NET_IOBUF_LEN;
    int sfd = server->fd;
    int nread;

    sockAddrEx caddr;
    buffer_t tmp_buf = {0,0,0,NULL};

    balloc(&tmp_buf, NET_IOBUF_LEN);
    netSockAddrExInit(&caddr);

    // Read local client buffer
    nread = recvfrom(sfd, tmp_buf.data, readlen, 0,
                    (sockAddr *)&caddr.sa, &caddr.sa_len);
    if (nread == -1) {
        LOGW("Local server read UDP error: %s", strerror(errno));
        goto error;
    }
    tmp_buf.len = nread;

    char cip[HOSTNAME_MAX_LEN];
    int cport;

    netIpPresentBySockAddr(NULL, cip, HOSTNAME_MAX_LEN, &cport, &caddr);
    LOGD("Local server read UDP from [%s:%d]", cip, cport);

    // Decrypt local client buffer
    if (app->crypto->decrypt_all(&tmp_buf, app->crypto->cipher, NET_IOBUF_LEN)) {
        LOGW("Local server decrypt UDP buffer error");
        goto error;
    }

    // Validate the buffer
    char rip[HOSTNAME_MAX_LEN];
    int rip_len = HOSTNAME_MAX_LEN;
    int rport;
    int raddr_len;

    if ((raddr_len= socks5AddrParse(tmp_buf.data, tmp_buf.len, NULL, rip, &rip_len, &rport)) == -1) {
        LOGW("Local server parse UDP buffer error");
        goto error;
    }

    char raddr_info[ADDR_INFO_STR_LEN];
    anetFormatAddr(raddr_info, ADDR_INFO_STR_LEN, rip, rport);
    LOGI("Local client request UDP addr: %s", raddr_info);

    // Write to remote server
    sockAddrEx raddr;
    char err[ANET_ERR_LEN];
    if (netUdpGetSockAddrEx(err, rip, rport, app->config->ipv6_first, &raddr) == NET_ERR) {
        LOGW("Remote server get sockaddr error: %s", err);
        goto error;
    }

    remote = udpRemoteCreate(rip);
    remote->server = server;
    memcpy(&remote->sa_client, &caddr, sizeof(caddr));

    int rfd = remote->fd;
    if (sendto(rfd, tmp_buf.data+raddr_len, tmp_buf.len-raddr_len, 0,
               (sockAddr *)&raddr.sa, raddr.sa_len) == -1) {
        LOGW("Remote client send UDP buffer error: %s", strerror(errno));
        goto error;
    }

    server->remote_count++;
    LOGD("Udp remote current count: %d", server->remote_count);

    goto end;

error:
    udpRemoteFree(remote);

end:
    bfree(&tmp_buf);
}

udpRemote *udpRemoteCreate(char *host) {
    char err[ANET_ERR_LEN];
    int port = 0;
    int fd;

    if (host && isIPv6Addr(host))
        fd = netUdp6Server(err, 0, NULL);
    else
        fd = netUdpServer(err, 0, NULL);

    if (fd == ANET_ERR) {
        FATAL("Could not create remote server UDP socket %s:%d: %s",
              host, port, err);
    }

    return udpRemoteNew(fd);
}

udpRemote *udpRemoteNew(int fd) {
    udpRemote *remote = xs_calloc(sizeof(*remote));

    remote->fd = fd;
    remote->re = NEW_EVENT_READ(fd, udpRemoteReadHandler, remote);
    remote->te = NEW_EVENT_ONCE(app->config->timeout, udpRemoteReadTimeHandler, remote);

    anetNonBlock(NULL, remote->fd);
    netSockAddrExInit(&remote->sa_client);
    ADD_EVENT(remote->re);
    ADD_EVENT(remote->te);

    return remote;
}

void udpRemoteFree(udpRemote *remote) {
    if (!remote) return;

    remote->server->remote_count--;
    LOGD("Udp remote current count: %d", remote->server->remote_count);

    eventDel(remote->re);
    eventDel(remote->te);
    eventFree(remote->re);
    eventFree(remote->te);
    close(remote->fd);

    xs_free(remote);
}

static void udpRemoteReadHandler(event *e) {
    udpRemote *remote = e->data;
    udpServer *server = remote->server;

    int readlen = NET_IOBUF_LEN;
    int rfd = remote->fd;
    int nread;
    char buf[NET_IOBUF_LEN];
    sds sbuf = NULL;
    buffer_t tmp_buf = {0,0,0,NULL};
    sockAddrEx remote_addr;

    balloc(&tmp_buf, NET_IOBUF_LEN);
    netSockAddrExInit(&remote_addr);

    // Read remote server buffer
    nread = recvfrom(rfd, buf, readlen, 0,
                    (sockAddr *)&remote_addr.sa, &remote_addr.sa_len);
    if (nread == -1) {
        LOGW("Remote client UDP read error: %s", strerror(errno));
        goto error;
    }

    // Log remote server info
    char rip[HOSTNAME_MAX_LEN];
    int rport;
    netIpPresentBySockAddr(NULL, rip, HOSTNAME_MAX_LEN, &rport, &remote_addr);
    LOGD("Remote client read UDP from [%s:%d] ", rip, rport);

    // Append address buffer
    sbuf = socks5AddrInit(NULL, rip, rport);
    sbuf = sdscatlen(sbuf, buf, nread);
    memcpy(tmp_buf.data, sbuf, sdslen(sbuf));
    tmp_buf.len = sdslen(sbuf);

    // Encrypt remote buffer
    if (app->crypto->encrypt_all(&tmp_buf, app->crypto->cipher, NET_IOBUF_LEN)) {
        LOGW("Remote client encrypt UDP buffer error");
        goto error;
    }

    // Write to local client
    int sfd = server->fd;
    int nwrite = sendto(sfd, tmp_buf.data, tmp_buf.len, 0,
                        (sockAddr *)&remote->sa_client.sa, remote->sa_client.sa_len);
    if (nwrite == -1) {
        LOGW("Local server UDP write error: %s", strerror(errno));
        goto error;
    }

    goto end;

error:
end:
    udpRemoteFree(remote);
    sdsfree(sbuf);
    bfree(&tmp_buf);
}

static void udpRemoteReadTimeHandler(event *e) {
    udpRemote *remote = e->data;

    LOGD("Udp remote read timeout");

    udpRemoteFree(remote);
}
