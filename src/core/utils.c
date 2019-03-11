
#include "common.h"

#include "utils.h"

#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>

void xs_daemonize() {
    int fd;

    if (fork() != 0) exit(0);

    // umask(0);
    setsid();
    // chdir("/");

    if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }
}

void setupIgnoreHandlers() {
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
}

void hexdump(const void *memory, size_t bytes) {
    const unsigned char *p, *q;
    int i;

    p = memory;
    while (bytes) {
        q = p;
        LOGDR("%p: ", (void *)p);
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

#define INT_DIGITS 19 /* enough for 64 bit integer */

char *xs_itoa(int i) {
    /* Room for INT_DIGITS digits, - and '\0' */
    static char buf[INT_DIGITS + 2];
    char *p = buf + INT_DIGITS + 1; /* points to terminating '\0' */
    if (i >= 0) {
        do {
            *--p = '0' + (i % 10);
            i /= 10;
        } while (i != 0);
        return p;
    } else { /* i < 0 */
        do {
            *--p = '0' - (i % 10);
            i /= 10;
        } while (i != 0);
        *--p = '-';
    }
    return p;
}

int isIPv6Addr(char *ip) {
    return strchr(ip, ':') ? 1 : 0;
}
