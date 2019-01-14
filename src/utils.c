
#include <stdarg.h>
#include <ctype.h>

#include "common.h"
#include "version.h"

#include "utils.h"

#define eprintf(...) fprintf(stderr, __VA_ARGS__)

void xs_usage(int module) {
    eprintf("\n");
    eprintf("xsocks %s\n\n", XS_VERSION);
    eprintf("  maintained by XJP09_HK <jianping_xie@aliyun.com>\n\n");
    eprintf("  usage:\n\n");

    switch (module) {
        case MODULE_LOCAL: eprintf("    xs-local\n"); break;
        case MODULE_REMOTE: eprintf("    xs-server\n"); break;
        default:
            // eprintf("    ss-tunnel\n");
            // eprintf("    ss-redir\n");
            // eprintf("    ss-manager\n");
            break;

    }

    eprintf("\n");
    eprintf("       -s <server_host>           Host name or IP address of your remote server.\n");
    eprintf("       -p <server_port>           Port number of your remote server.\n");
    eprintf("       -l <local_port>            Port number of your local server.\n");
    eprintf("       -k <password>              Password of your remote server.\n");
    eprintf("       -m <encrypt_method>        Encrypt method: rc4-md5,\n");
    eprintf("                                  aes-128-gcm, aes-192-gcm, aes-256-gcm,\n");
    eprintf("                                  aes-128-cfb, aes-192-cfb, aes-256-cfb,\n");
    eprintf("                                  aes-128-ctr, aes-192-ctr, aes-256-ctr,\n");
    // eprintf("                                  camellia-128-cfb, camellia-192-cfb,\n");
    // eprintf("                                  camellia-256-cfb, bf-cfb,\n");
    // eprintf("                                  chacha20-ietf-poly1305,\n");
#ifdef FS_HAVE_XCHACHA20IETF
    // eprintf("                                  xchacha20-ietf-poly1305,\n");
#endif
    eprintf("                                  salsa20, chacha20 and chacha20-ietf.\n");
    eprintf("                                  The default cipher is aes-256-cfb.\n");
    eprintf("\n");
    // eprintf("       [-a <user>]                Run as another user.\n");
    // eprintf("       [-f <pid_file>]            The file path to store pid.\n");
    eprintf("       [-t <timeout>]             Socket timeout in seconds.\n");
    eprintf("       [-c <config_file>]         The path to config file.\n");
    // eprintf("       [-n <number>]              Max number of open files.\n");
#ifndef MODULE_REDIR
    // eprintf("       [-i <interface>]           Network interface to bind.\n");
#endif
    eprintf("       [-b <local_address>]       Local address to bind.\n");
    eprintf("\n");
    eprintf("       [-u]                       Enable UDP relay.\n");
#ifdef MODULE_REDIR
    // eprintf("                                  TPROXY is required in redir mode.\n");
#endif
    eprintf("       [-U]                       Enable UDP relay and disable TCP relay.\n");
#ifdef MODULE_REMOTE
    // eprintf("       [-6]                       Resovle hostname to IPv6 address first.\n");
#endif
    eprintf("\n");
#ifdef MODULE_TUNNEL
    // eprintf("       [-L <addr>:<port>]         Destination server address and port\n");
    // eprintf("                                  for local port forwarding.\n");
#endif
#ifdef MODULE_REMOTE
    // eprintf("       [-d <addr>]                Name servers for internal DNS resolver.\n");
#endif
    eprintf("       [--reuse-port]             Enable port reuse.\n");
#if defined(MODULE_REMOTE) || defined(MODULE_LOCAL) || defined(MODULE_REDIR)
    // eprintf("       [--fast-open]              Enable TCP fast open.\n");
    // eprintf("                                  with Linux kernel > 3.7.0.\n");
#endif
#if defined(MODULE_REMOTE) || defined(MODULE_LOCAL)
    // eprintf("       [--acl <acl_file>]         Path to ACL (Access Control List).\n");
#endif
#if defined(MODULE_REMOTE) || defined(MODULE_MANAGER)
    // eprintf("       [--manager-address <addr>] UNIX domain socket address.\n");
#endif
#ifdef MODULE_MANAGER
    // eprintf("       [--executable <path>]      Path to the executable of ss-server.\n");
#endif
    eprintf("       [--mtu <MTU>]              MTU of your network interface.\n");
#ifdef __linux__
    // eprintf("       [--mptcp]                  Enable Multipath TCP on MPTCP Kernel.\n");
#endif
#ifndef MODULE_MANAGER
    // eprintf("       [--no-delay]               Enable TCP_NODELAY.\n");
    // eprintf("       [--key <key_in_base64>]    Key of your remote server.\n");
#endif
    // eprintf("       [--plugin <name>]          Enable SIP003 plugin. (Experimental)\n");
    // eprintf("       [--plugin-opts <options>]  Set SIP003 plugin options. (Experimental)\n");
    eprintf("\n");
    eprintf("       [--logfile <file>]         Log file.\n");
    eprintf("       [--loglevel <level>]       Log level.\n");
    eprintf("       [-v]                       Verbose mode.\n");
    eprintf("       [-h, --help]               Print this message.\n");
    eprintf("\n");
}

void hexdump(const void *memory, size_t bytes) {
    const unsigned char * p, * q;
    int i;

    p = memory;
    while (bytes) {
        q = p;
        LOGDR("%p: ", (void *) p);
        for (i = 0; i < 16 && bytes; ++i) {
            LOGDR("%02X %s", *p, i == 7 ? " " : "");
            ++p;
            --bytes;
        }
        bytes += i;
        while (i < 16) {
            LOGDR("XX %s", i == 7 ? " " : "");
            ++i;
        }
        LOGDR("| ");
        p = q;
        for (i = 0; i < 16 && bytes; ++i) {
            LOGDR("%c", isprint(*p) && !isspace(*p) ? *p : '.');
            ++p;
            --bytes;
        }
         while (i < 16) {
            LOGDR(" ");
            ++i;
        }
        LOGDR(" |\n");
    }
}
