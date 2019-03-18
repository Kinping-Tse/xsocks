
#include "udp_raw.h"

udpRawConn *udpRawConnNew(udpConn *conn) {
    udpRawConn *c;

    c = xs_realloc(conn, sizeof(*c));
    if (!c) return c;

    conn = &c->conn;

    udpInit(conn);

    return c;
}
