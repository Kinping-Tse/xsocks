
#include <socks5.h>
#include <sds.h>

enum {
    SOCKS5_OK = 0,
    SOCKS5_ERR = -1
};

typedef struct method_select_request socks5AuthReq;
typedef struct method_select_response socks5AuthResp;
typedef struct socks5_request socks5Req;
typedef struct socks5_response socks5Resp;

sds socks5AddrInit(char *err, char *host, int port);
int socks5AddrParse(char *addr_buf, int buf_len, int *atyp,
                    char *host, int *host_len, int *port);
