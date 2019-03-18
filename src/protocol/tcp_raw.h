
#ifndef __PROTOCOL_TCP_RAW_H
#define __PROTOCOL_TCP_RAW_H

#include "tcp.h"

typedef struct tcpRawConn {
    tcpConn conn;
} tcpRawConn;

tcpRawConn *tcpRawConnNew(tcpConn *conn);

#endif /* __PROTOCOL_TCP_RAW_H */
