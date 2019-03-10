
#include "tcp.h"

typedef struct tcpRawConn {
    tcpConn conn;
} tcpRawConn;

tcpRawConn *tcpRawConnNew(tcpConn *conn);
