/*
 * This file is part of xsocks, a lightweight proxy tool for science online.
 *
 * Copyright (C) 2019 XJP09_HK <jianping_xie@aliyun.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __XS_CONFIG_H
#define __XS_CONFIG_H

/* Error codes */
enum {
    CONFIG_OK = 0,
    CONFIG_ERR = -1,
};

enum {
    MODE_TCP_ONLY = 1<<0,
    MODE_UDP_ONLY = 1<<1,
    MODE_TCP_AND_UDP = MODE_TCP_ONLY|MODE_UDP_ONLY,
};

#define CONFIG_DEFAULT_DAEMONIZE 0
#define CONFIG_DEFAULT_PASSWORD "foobar"
#define CONFIG_DEFAULT_METHOD "aes-256-cfb"
#define CONFIG_DEFAULT_REMOTE_ADDR "127.0.0.1"
#define CONFIG_DEFAULT_REMOTE_PORT 8388
#define CONFIG_DEFAULT_LOCAL_PORT 1080
#define CONFIG_DEFAULT_TUNNEL_ADDRESS "8.8.8.8:53"
#define CONFIG_DEFAULT_TIMEOUT 60
#define CONFIG_DEFAULT_MODE MODE_TCP_ONLY
#define CONFIG_DEFAULT_MTU 0
#define CONFIG_DEFAULT_LOGLEVEL LOGLEVEL_NOTICE
#define CONFIG_DEFAULT_SYSLOG_ENABLED 1

typedef struct xsocksConfig {
    char *pidfile;
    int daemonize;
    char *remote_addr;
    int remote_port;
    char *local_addr;
    int local_port;
    char *password;
    char *tunnel_address;
    char *tunnel_addr; // parse from tunnel_address
    int tunnel_port; // parse from tunnel_address
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
    int logfile_line;
    int use_syslog;
    int ipv6_first;
    int ipv6_only;
    int no_delay;
    char *acl;
    //
    // int max_clients;
} xsocksConfig;

xsocksConfig *configNew();
void configFree(xsocksConfig *config);
int configParse(xsocksConfig *config, int argc, char *argv[]);

#endif /* __XS_CONFIG_H */
