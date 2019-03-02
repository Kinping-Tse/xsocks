
#include "common.h"
#include "utils.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/time.h>
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

void xs_error(char *err, const char *fmt, ...) {
    va_list ap;

    if (!err) return;
    va_start(ap, fmt);
    vsnprintf(err, XS_ERR_LEN, fmt, ap);
    va_end(ap);
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

#define MICROSECOND_UNIT 1000000
#define MICROSECOND_UNIT_F (double)1000000.0

uint64_t timerStart() {
    struct timeval time;

    if (gettimeofday(&time, NULL) == -1) return 0;

    return time.tv_sec * MICROSECOND_UNIT + time.tv_usec;
}

double timerStop(uint64_t start_time, uint64_t *stop_time) {
    struct timeval time;

    if (gettimeofday(&time, NULL) == -1) return 0;

    uint64_t t = time.tv_sec * MICROSECOND_UNIT + time.tv_usec;
    if (stop_time) *stop_time = t;

    return t/MICROSECOND_UNIT_F - start_time/MICROSECOND_UNIT_F;
}
