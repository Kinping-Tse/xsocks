
#include "common.h"

#include "socks5.h"
#include "error.h"
#include "net.h"

static int socks5HostParse(char *host) {
    if (strchr(host, ':')) {
        return SOCKS5_ATYP_IPV6;
    } else {
        for (char *c = host; *c != '\0'; c++) {
            switch (*c) {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                case '.': break;
                default: return SOCKS5_ATYP_DOMAIN;
            }
        }
        return SOCKS5_ATYP_IPV4;
    }
}

/*
 * Socks5 addr buffer
 *
 *    +------+----------+----------+
 *    | ATYP | DST.ADDR | DST.PORT |
 *    +------+----------+----------+
 *    |  1   | Variable |    2     |
 *    +------+----------+----------+
 */
sds socks5AddrInit(char *err, char *host, int port) {
    sds addr = sdsempty();
    int addr_type = socks5HostParse(host);
    int addr_len;

    addr = sdscatlen(addr, (uint8_t *)(&addr_type), 1);

    if (addr_type == SOCKS5_ATYP_IPV4) {
        ipV4Addr addr_v4;
        addr_len = sizeof(addr_v4);
        if (inet_pton(AF_INET, host, &addr_v4) == -1) {
            xs_error(err, strerror(errno));
            goto error;
        }
        addr = sdscatlen(addr, &addr_v4, addr_len);
    } else if (addr_type == SOCKS5_ATYP_IPV6) {
        ipV6Addr addr_v6;
        addr_len = sizeof(addr_v6);
        if (inet_pton(AF_INET6, host, &addr_v6) == -1) {
            xs_error(err, strerror(errno));
            goto error;
        }
        addr = sdscatlen(addr, &addr_v6, addr_len);
    } else if (addr_type == SOCKS5_ATYP_DOMAIN) {
        addr_len = strlen(host);
        addr = sdscatlen(addr, (uint8_t *)(&addr_len), 1);
        addr = sdscatlen(addr, host, addr_len);
    } else {
        xs_error(err, "Invalid socks5 addr type");
        goto error;
    }

    port = htons(port);
    addr = sdscatlen(addr, (uint16_t *)(&port), 2);

    return addr;

error:
    sdsfree(addr);
    return NULL;
}

int socks5AddrParse(char *addr_buf, int buf_len, int *atyp, char *host, int *host_len, int *port) {
    int real_buf_len = 0;
    int addr_type = *addr_buf++;
    buf_len--;
    real_buf_len++;

    if (atyp) *atyp = (uint8_t)addr_type;

    int addr_len;
    if (addr_type == SOCKS5_ATYP_IPV4 || addr_type == SOCKS5_ATYP_IPV6) {
        int is_v6 = addr_type == SOCKS5_ATYP_IPV6;
        addr_len = !is_v6 ? sizeof(ipV4Addr) : sizeof(ipV6Addr);
        if (buf_len < addr_len+2) return SOCKS5_ERR;

        if (port)
            *port = ntohs(*(uint16_t *)(addr_buf+addr_len));

        if (host) {
            assert(host_len);
            inet_ntop(!is_v6 ? AF_INET : AF_INET6, addr_buf, host, *host_len);
        }

        real_buf_len += addr_len + 2;
    } else if (addr_type == SOCKS5_ATYP_DOMAIN) {
        addr_len = (uint8_t)*addr_buf++;
        if (buf_len < 1+addr_len+2) return SOCKS5_ERR;

        if (host) {
            memcpy(host, addr_buf, addr_len);
            host[addr_len] = '\0';
        }

        real_buf_len += 1 + addr_len + 2;
    } else {
        return SOCKS5_ERR;
    }

    if (host_len) *host_len = addr_len;
    if (port) *port = ntohs(*(uint16_t *)(addr_buf+addr_len));

    return real_buf_len;
}
