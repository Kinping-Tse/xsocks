
#include "common.h"
#include "config.h"
#include "utils.h"

#include "json.h"

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
    {NULL, 0}
};

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
    GETOPT_VAL_FAST_OPEN,
    GETOPT_VAL_NODELAY,
    // GETOPT_VAL_ACL,
    // GETOPT_VAL_MPTCP,
    GETOPT_VAL_PASSWORD,
    GETOPT_VAL_KEY,
};

xsocksConfig* configNew() {
    xsocksConfig* config = xs_calloc(sizeof(*config));

    config->remote_addr = "127.0.0.1";
    config->remote_port = 8388;
    config->local_addr = "127.0.0.1";
    config->local_port = 1080;
    config->password = "xsocks";
    config->key = NULL;
    config->method = "aes-256-cfb";
    config->timeout = 60;
    config->fast_open = 0;
    config->reuse_port = 0;
    config->mode = MODE_TCP_ONLY;
    config->mtu = 0;
    config->loglevel = LOGLEVEL_NOTICE;
    config->logfile = "";

    config->no_delay = 0;
    config->help = 0;
    return config;
}

#define INT_DIGITS 19           /* enough for 64 bit integer */
char * xs_itoa(int i)
{
    /* Room for INT_DIGITS digits, - and '\0' */
    static char buf[INT_DIGITS + 2];
    char *p = buf + INT_DIGITS + 1;     /* points to terminating '\0' */
    if (i >= 0) {
        do {
            *--p = '0' + (i % 10);
            i   /= 10;
        } while (i != 0);
        return p;
    } else {                     /* i < 0 */
        do {
            *--p = '0' - (i % 10);
            i   /= 10;
        } while (i != 0);
        *--p = '-';
    }
    return p;
}

#define check_json_value_type(value, expected_type, message) do { \
    if ((value)->type != (expected_type)) \
        FATAL((message)); \
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

static char * to_string(const json_value *value) {
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

void configLoad(xsocksConfig* config, char *filename) {
    char *err = NULL;
    json_value *obj = NULL;
    char *buf = NULL;
    FILE *fp = NULL;
    long pos;

    if ((fp = fopen(filename, "rb")) == NULL) goto loaderr;

    fseek(fp, 0, SEEK_END);
    pos = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = xs_calloc(pos+1);
    if (fread(buf, pos, 1, fp) != 1) {
        err = "Failed to read the config file.";
        goto loaderr;
    }

    if ((obj = json_parse(buf, pos)) == NULL) goto loaderr;
    if (obj->type != json_object) goto loaderr;

    for (int i = 0; i < obj->u.object.length; i++) {
        char *name        = obj->u.object.values[i].name;
        json_value *value = obj->u.object.values[i].value;
        if (strcmp(name, "server") == 0) {
            config->remote_addr = to_string(value);
        } else if (strcmp(name, "server_port") == 0) {
            config->remote_port = to_integer(value);
        } else if (strcmp(name, "local_address") == 0) {
            config->local_addr = to_string(value);
        } else if (strcmp(name, "local_port") == 0) {
            config->local_port = to_integer(value);
        } else if (strcmp(name, "password") == 0) {
            config->password = to_string(value);
        } else if (strcmp(name, "key") == 0) {
            config->key = to_string(value);
        } else if (strcmp(name, "method") == 0) {
            config->method = to_string(value);
        } else if (strcmp(name, "timeout") == 0) {
            config->timeout = to_integer(value);
        } else if (strcmp(name, "user") == 0) {
            // conf.user = to_string(value);
        } else if (strcmp(name, "fast_open") == 0) {
            check_json_value_type(value, json_boolean,
                                  "invalid config file: option 'fast_open' must be a boolean");
            config->fast_open = to_integer(value);
        } else if (strcmp(name, "reuse_port") == 0) {
            check_json_value_type(value, json_boolean,
                                  "invalid config file: option 'reuse_port' must be a boolean");
            config->reuse_port = to_integer(value);
        } else if (strcmp(name, "logfile") == 0) {
            config->logfile = to_string(value);
        } else if (strcmp(name, "loglevel") == 0) {
            config->loglevel = configEnumGetValue(loglevel_enum, to_string(value));
            if (config->loglevel == INT_MIN) {
                err = "Invalid log level. "
                      "Must be one of debug, info, notice, warning, error";
                goto loaderr;
            }
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
                LOGI("ignore unknown mode: %s, use tcp_only as fallback", mode_str);

            xs_free(mode_str);
        } else if (strcmp(name, "mtu") == 0) {
            check_json_value_type(value, json_integer,
                                  "invalid config file: option 'mtu' must be an integer");
            config->mtu = to_integer(value);
        } else if (strcmp(name, "use_syslog") == 0) {
            check_json_value_type(value, json_boolean,
                                  "invalid config file: option 'use_syslog' must be a boolean");
            config->use_syslog = to_integer(value);
        } else if (strcmp(name, "no_delay") == 0) {
            check_json_value_type(
                value, json_boolean,
                "invalid config file: option 'no_delay' must be a boolean");
            config->no_delay = to_integer(value);
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

int configParse(xsocksConfig* config, int argc, char *argv[]) {
    struct option long_options[] = {
        { "help",        no_argument,       NULL, GETOPT_VAL_HELP        },
        { "reuse-port",  no_argument,       NULL, GETOPT_VAL_REUSE_PORT  },
        { "mtu",         required_argument, NULL, GETOPT_VAL_MTU         },
        { "loglevel",    required_argument, NULL, GETOPT_VAL_LOGLEVEL    },
        { "fast-open",   no_argument,       NULL, GETOPT_VAL_FAST_OPEN   },
        { "no-delay",    no_argument,       NULL, GETOPT_VAL_NODELAY     },
        { "password",    required_argument, NULL, GETOPT_VAL_PASSWORD    },
        { "key",         required_argument, NULL, GETOPT_VAL_KEY         },
        { NULL,          0,                 NULL, 0                      }
    };

    char *conf_path = NULL;
    char *err = NULL;
    int c;
    while ((c = getopt_long(argc, argv, "f:s:p:l:k:t:m:i:c:b:a:n:huUv6A",
                            long_options, NULL)) != -1) {
        switch (c) {
            case GETOPT_VAL_FAST_OPEN: config->fast_open = 1; break;
            case GETOPT_VAL_MTU: config->mtu = atoi(optarg);
                break;
            case GETOPT_VAL_NODELAY: config->no_delay = 1; break;
            case GETOPT_VAL_KEY: config->key = optarg; break;
            case GETOPT_VAL_REUSE_PORT: config->reuse_port = 1; break;
            case GETOPT_VAL_LOGLEVEL:
                config->loglevel = configEnumGetValue(loglevel_enum, optarg);
                if (config->loglevel == INT_MIN)
                    err = "Invalid log level. "
                          "Must be one of debug, info, notice, warning, error";
                break;
            case 's': config->remote_addr = optarg; break;
            case 'p': config->remote_port = atoi(optarg); break;
            case 'b': config->local_addr = optarg; break;
            case 'l': config->local_port = atoi(optarg); break;
            case GETOPT_VAL_PASSWORD:
            case 'k':
                config->password = optarg;
                break;
            // case 'f': pid_flags = 1; pid_path  = optarg; break;
            case 't': config->timeout = atoi(optarg); break;
            case 'm': config->method = optarg; break;
            case 'c': conf_path = optarg; break;
            case 'u': config->mode = MODE_TCP_AND_UDP; break;
            case 'U': config->mode = MODE_UDP_ONLY; break;
            case 'v': config->loglevel = LOGLEVEL_DEBUG; break;
            case 'h':
            case GETOPT_VAL_HELP:
                config->help = 1;
                break;
            // case '6':
            //     ipv6first = 1;
            //     break;
            case '?':
                // The option character is not recognized.
                err = "Unrecognized option";
                break;
        }
    }

    if (conf_path != NULL) {
        configLoad(config, conf_path);
    }

    if (err != NULL) {
        LOGE(err);
        return CONFIG_ERR;
    }
    return CONFIG_OK;
}

void configFree(xsocksConfig* config) {
    xs_free(config);
}
