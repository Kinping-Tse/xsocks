
#ifndef __PROTOCOL_RAW_H
#define __PROTOCOL_RAW_H

#include "tcp.h"
#include "udp.h"

typedef struct tcpRawConn {
    tcpConn conn;
} tcpRawConn;

typedef struct udpRawConn {
    udpConn conn;
} udpRawConn;

tcpRawConn *tcpRawConnNew(tcpConn *conn);
udpRawConn *udpRawConnNew(udpConn *conn);

#endif /* __PROTOCOL_RAW_H */
