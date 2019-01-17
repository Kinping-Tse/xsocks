
#ifndef __XS_CONFIG_H
#define __XS_CONFIG_H

/* Error codes */
enum {
    CONFIG_OK = 0,
    CONFIG_ERR = -1
};

enum {
    MODE_TCP_ONLY = 1<<0,
    MODE_UDP_ONLY = 1<<1,
    MODE_TCP_AND_UDP = MODE_TCP_ONLY|MODE_UDP_ONLY,
};

typedef struct xsocksConfig {
    char *remote_addr;
    int remote_port;
    char *local_addr;
    int local_port;
    char *password;
    char *key;
    char *method;
    int timeout;
    // char *user;
    int fast_open;
    int reuse_port;
    // int nofile;
    // char *nameserver;
    int mode;
    int mtu;
    int loglevel;
    char *logfile;
    int use_syslog;
    int help;
    // int ipv6_first;
    int no_delay;
    //
    // int max_clients;
} xsocksConfig;

xsocksConfig *configNew();
void configFree(xsocksConfig *config);
int configParse(xsocksConfig *config, int argc, char *argv[]);

#endif /* __XS_CONFIG_H */
