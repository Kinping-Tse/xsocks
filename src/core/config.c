
#include "config.h"
#include "common.h"
#include "error.h"
#include "net.h"
#include "utils.h"

#include "json-parser/json.h"
#include "redis/sds.h"
#include <getopt.h>

typedef struct configEnum {
    const char *name;
    const int val;
} configEnum;

configEnum loglevel_enum[] = {
    {"debug", LOGLEVEL_DEBUG},
    {"info", LOGLEVEL_INFO},
    {"notice", LOGLEVEL_NOTICE},
    {"warning", LOGLEVEL_WARNING},
    {"error", LOGLEVEL_ERROR},
    {NULL, 0},
};

#define configStringDup(d, s) \
    do { \
        char *_s = s; \
        if (_s) { xs_free(d); d = xs_strdup(_s); } \
    } while (0)

#define configIntDup(d, s) do { if (s > -1) d = s; } while (0)

/* Get enum value from name. If there is no match INT_MIN is returned. */
int configEnumGetValue(configEnum *ce, char *name) {
    while (ce->name != NULL) {
        if (!strcasecmp(ce->name, name)) return ce->val;
        ce++;
    }
    return INT_MIN;
}

enum {
    GETOPT_VAL_HELP = 257,
    GETOPT_VAL_REUSE_PORT,
    GETOPT_VAL_MTU,
    GETOPT_VAL_LOGLEVEL,
    GETOPT_VAL_LOGFILE,
    GETOPT_VAL_FAST_OPEN,
    GETOPT_VAL_NODELAY,
    // GETOPT_VAL_ACL,
    // GETOPT_VAL_MPTCP,
    GETOPT_VAL_PASSWORD,
    GETOPT_VAL_KEY,
};

xsocksConfig *configNew() {
    xsocksConfig *config = xs_calloc(sizeof(*config));

    config->pidfile = NULL;
    config->daemonize = CONFIG_DEFAULT_DAEMONIZE;
    config->remote_addr = NULL;
    config->remote_port = CONFIG_DEFAULT_REMOTE_PORT;
    config->local_addr = NULL;
    config->local_port = CONFIG_DEFAULT_LOCAL_PORT;
    configStringDup(config->password, CONFIG_DEFAULT_PASSWORD);
    config->tunnel_address = NULL;
    config->key = NULL;
    configStringDup(config->method, CONFIG_DEFAULT_METHOD);
    config->timeout = CONFIG_DEFAULT_TIMEOUT;
    config->fast_open = 0;
    config->reuse_port = 0;
    config->mode = CONFIG_DEFAULT_MODE;
    config->mtu = CONFIG_DEFAULT_MTU;
    config->loglevel = CONFIG_DEFAULT_LOGLEVEL;
    config->logfile = NULL;
    config->logfile_line = 0;
    config->use_syslog = CONFIG_DEFAULT_SYSLOG_ENABLED;
    config->ipv6_first = 0;
    config->ipv6_only = 1;
    config->no_delay = 0;

    return config;
}

#define check_json_value_type(value, expected_type, message)    \
    do {                                                        \
        if ((value)->type != (expected_type)) FATAL((message)); \
    } while (0)

static int to_integer(const json_value *value) {
    if (value->type == json_string) {
        return atoi(value->u.string.ptr);
    } else if (value->type == json_integer) {
        return value->u.integer;
    } else if (value->type == json_boolean) {
        return (int)value->u.boolean;
    } else if (value->type == json_null) {
        return 0;
    }

    LOGE("%d", value->type);
    FATAL("Invalid config format.");
}

static char *to_string(const json_value *value) {
    if (value->type == json_string) {
        return xs_strdup(value->u.string.ptr);
    } else if (value->type == json_integer) {
        return xs_strdup(xs_itoa(value->u.integer));
    } else if (value->type == json_null) {
        return NULL;
    }

    LOGE("%d", value->type);
    FATAL("Invalid config format.");
}

static int testLogfile(char **err, char *file) {
    if (file && file[0] != '\0') {
        FILE *fp = fopen(file, "a");
        if (!fp) {
            if (err) {
                char *tmp_err = xs_calloc(256);
                snprintf(tmp_err, 256, "Can't open the log file: %s", strerror(errno));
                *err = tmp_err;
            }
            return CONFIG_ERR;
        }
        fclose(fp);
    }
    return CONFIG_OK;
}

void configLoad(xsocksConfig *config, char *filename) {
    char *err = NULL;
    json_value *obj = NULL;
    char *buf = NULL;
    FILE *fp = NULL;
    long pos;

    if ((fp = fopen(filename, "rb")) == NULL) goto loaderr;

    fseek(fp, 0, SEEK_END);
    pos = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = xs_calloc(pos + 1);
    if (fread(buf, pos, 1, fp) != 1) {
        err = "Failed to read the config file.";
        goto loaderr;
    }

    if ((obj = json_parse(buf, pos)) == NULL) goto loaderr;
    if (obj->type != json_object) goto loaderr;

    for (uint64_t i = 0; i < obj->u.object.length; i++) {
        char *name = obj->u.object.values[i].name;
        json_value *value = obj->u.object.values[i].value;

        if (strcmp(name, "server") == 0) {
            config->remote_addr = to_string(value);
        } else if (strcmp(name, "server_port") == 0) {
            config->remote_port = to_integer(value);
        } else if (strcmp(name, "port_password") == 0) {
        } else if (strcmp(name, "local_address") == 0) {
            config->local_addr = to_string(value);
        } else if (strcmp(name, "local_port") == 0) {
            config->local_port = to_integer(value);
        } else if (strcmp(name, "password") == 0) {
            config->password = to_string(value);
        } else if (strcmp(name, "key") == 0) {
            config->key = to_string(value);
        } else if (strcmp(name, "method") == 0) {
            xs_free(config->method);
            config->method = to_string(value);
        } else if (strcmp(name, "timeout") == 0) {
            config->timeout = to_integer(value);
        } else if (strcmp(name, "user") == 0) {
            // conf.user = to_string(value);
        } else if (strcmp(name, "fast_open") == 0) {
            check_json_value_type(value, json_boolean, "invalid config file: option 'fast_open' must be a boolean");
            config->fast_open = to_integer(value);
        } else if (strcmp(name, "reuse_port") == 0) {
            check_json_value_type(value, json_boolean, "invalid config file: option 'reuse_port' must be a boolean");
            config->reuse_port = to_integer(value);
        } else if (strcmp(name, "logfile") == 0) {
            config->logfile = to_string(value);
            if (testLogfile(&err, config->logfile) == CONFIG_ERR) goto loaderr;
        } else if (strcmp(name, "loglevel") == 0) {
            char *loglevel = to_string(value);
            config->loglevel = configEnumGetValue(loglevel_enum, loglevel);
            xs_free(loglevel);

            if (config->loglevel == INT_MIN) {
                err = "Invalid log level. "
                      "Must be one of debug, info, notice, warning, error";
                goto loaderr;
            }
        } else if (strcmp(name, "logfile_line") == 0) {
            check_json_value_type(value, json_boolean, "invalid config file: option 'logfile_line' must be a boolean");
            config->logfile_line = to_integer(value);
        } else if (strcmp(name, "pidfile") == 0) {
            config->pidfile = to_string(value);
        } else if (strcmp(name, "daemonize") == 0) {
            check_json_value_type(value, json_boolean, "invalid config file: option 'daemonize' must be a boolean");
            config->daemonize = to_integer(value);
        } else if (strcmp(name, "tunnel_address") == 0) {
            config->tunnel_address = to_string(value);
        } else if (strcmp(name, "mode") == 0) {
            char *mode_str = to_string(value);

            if (mode_str == NULL)
                config->mode = MODE_TCP_ONLY;
            else if (strcmp(mode_str, "tcp_only") == 0)
                config->mode = MODE_TCP_ONLY;
            else if (strcmp(mode_str, "tcp_and_udp") == 0)
                config->mode = MODE_TCP_AND_UDP;
            else if (strcmp(mode_str, "udp_only") == 0)
                config->mode = MODE_UDP_ONLY;
            else
                LOGW("ignore unknown mode: %s, use tcp_only as fallback", mode_str);

            xs_free(mode_str);
        } else if (strcmp(name, "mtu") == 0) {
            check_json_value_type(value, json_integer, "invalid config file: option 'mtu' must be an integer");
            config->mtu = to_integer(value);
        } else if (strcmp(name, "ipv6_first") == 0) {
            check_json_value_type(value, json_boolean, "invalid config file: option 'ipv6_first' must be a boolean");
            config->ipv6_first = to_integer(value);
        } else if (strcmp(name, "ipv6_only") == 0) {
            check_json_value_type(value, json_boolean, "invalid config file: option 'ipv6_only' must be a boolean");
            config->ipv6_only = to_integer(value);
        } else if (strcmp(name, "use_syslog") == 0) {
            check_json_value_type(value, json_boolean, "invalid config file: option 'use_syslog' must be a boolean");
            config->use_syslog = to_integer(value);
        } else if (strcmp(name, "no_delay") == 0) {
            check_json_value_type(value, json_boolean, "invalid config file: option 'no_delay' must be a boolean");
            config->no_delay = to_integer(value);
        } else {
            err = sdscatprintf(sdsempty(), "Bad directive: %s", name);
            goto loaderr;
        }
    }

    fclose(fp);
    xs_free(buf);
    json_value_free(obj);

    return;

loaderr:
    if (err == NULL) err = "invalid config file";
    FATAL(err);
}

int configParse(xsocksConfig *config, int argc, char *argv[]) {
    struct option long_options[] = {
        { "help",        no_argument,       NULL, GETOPT_VAL_HELP        },
        { "reuse-port",  no_argument,       NULL, GETOPT_VAL_REUSE_PORT  },
        { "mtu",         required_argument, NULL, GETOPT_VAL_MTU         },
        { "loglevel",    required_argument, NULL, GETOPT_VAL_LOGLEVEL    },
        { "logfile",     required_argument, NULL, GETOPT_VAL_LOGFILE     },
        { "fast-open",   no_argument,       NULL, GETOPT_VAL_FAST_OPEN   },
        { "no-delay",    no_argument,       NULL, GETOPT_VAL_NODELAY     },
        { "password",    required_argument, NULL, GETOPT_VAL_PASSWORD    },
        { "key",         required_argument, NULL, GETOPT_VAL_KEY         },
        { NULL,          0,                 NULL, 0                      },
    };

    char *conf_path = NULL;
    char *key = NULL;
    char *logfile = NULL;
    char *remote_addr = NULL;
    char *local_addr = NULL;
    char *tunnel_address = NULL;
    char *password = NULL;
    char *pidfile = NULL;
    char *method = NULL;
    int fast_open = -1;
    int mtu = -1;
    int no_delay = -1;
    int reuse_port = -1;
    int loglevel = -1;
    int remote_port = -1;
    int local_port = -1;
    int daemonize = -1;
    int timeout = -1;
    int mode = -1;
    int ipv6_first = -1;
    int help = 0;

    char *err = NULL;
    int c;

    while ((c = getopt_long(argc, argv, "f:s:p:l:L:k:t:m:c:b:huUv6", long_options, NULL)) != -1) {
        switch (c) {
            case GETOPT_VAL_FAST_OPEN: fast_open = 1; break;
            case GETOPT_VAL_MTU: mtu = atoi(optarg); break;
            case GETOPT_VAL_NODELAY: no_delay = 1; break;
            case GETOPT_VAL_KEY: key = optarg; break;
            case GETOPT_VAL_REUSE_PORT: reuse_port = 1; break;
            case GETOPT_VAL_LOGLEVEL:
                loglevel = configEnumGetValue(loglevel_enum, optarg);
                if (loglevel == INT_MIN)
                    err = "Invalid log level. "
                          "Must be one of debug, info, notice, warning, error";
                break;
            case GETOPT_VAL_LOGFILE:
                logfile = optarg;
                testLogfile(&err, logfile);
                break;
            case 's': remote_addr = optarg; break;
            case 'p': remote_port = atoi(optarg); break;
            case 'b': local_addr = optarg; break;
            case 'l': local_port = atoi(optarg); break;
            case 'L': tunnel_address = optarg; break;
            case GETOPT_VAL_PASSWORD:
            case 'k': password = optarg; break;
            case 'f':
                daemonize = 1;
                pidfile = optarg;
                break;
            case 't': timeout = atoi(optarg); break;
            case 'm': method = optarg; break;
            case 'c': conf_path = optarg; break;
            case 'u': mode = MODE_TCP_AND_UDP; break;
            case 'U': mode = MODE_UDP_ONLY; break;
            case 'v': loglevel = LOGLEVEL_DEBUG; break;
            case 'h':
            case GETOPT_VAL_HELP: help = 1; break;
            case '6': ipv6_first = 1; break;
            case '?':
                // The option character is not recognized.
                err = "Unrecognized option";
                break;
        }
    }

    if (conf_path != NULL) configLoad(config, conf_path);

    configStringDup(config->key, key);
    configStringDup(config->logfile, logfile);
    configStringDup(config->remote_addr, remote_addr);
    configStringDup(config->local_addr, local_addr);
    configStringDup(config->tunnel_address, tunnel_address);
    configStringDup(config->password, password);
    configStringDup(config->pidfile, pidfile);
    configStringDup(config->method, method);
    configIntDup(config->loglevel, loglevel);
    configIntDup(config->remote_port, remote_port);
    configIntDup(config->local_port, local_port);
    configIntDup(config->daemonize, daemonize);
    configIntDup(config->timeout, timeout);
    configIntDup(config->mode, mode);
    configIntDup(config->reuse_port, reuse_port);
    configIntDup(config->ipv6_first, ipv6_first);
    configIntDup(config->no_delay, no_delay);
    configIntDup(config->mtu, mtu);
    configIntDup(config->fast_open, fast_open);

    if (config->tunnel_address) {
        config->tunnel_addr = xs_malloc(HOSTNAME_MAX_LEN);
        netHostPortParse(config->tunnel_address, config->tunnel_addr, &config->tunnel_port);
        xs_free(config->tunnel_address);
        config->tunnel_address = NULL;
    }

    if (err != NULL) FATAL(err);

    return help ? CONFIG_ERR : CONFIG_OK;
}

void configFree(xsocksConfig *config) {
    xs_free(config->pidfile);
    xs_free(config->remote_addr);
    xs_free(config->local_addr);
    xs_free(config->password);
    xs_free(config->tunnel_address);
    xs_free(config->tunnel_addr);
    xs_free(config->key);
    xs_free(config->method);
    xs_free(config->logfile);

    xs_free(config);
}
