
#ifndef __PROTOCOL_UDP_RAW_H
#define __PROTOCOL_UDP_RAW_H

#include "udp.h"

typedef struct udpRawConn {
    udpConn conn;
} udpRawConn;

udpRawConn *udpRawConnNew(udpConn *conn);

#endif /* __PROTOCOL_UDP_RAW_H */
