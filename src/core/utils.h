#ifndef __UTILS_H
#define __UTILS_H

#define XS_ERR_LEN 1024
#define DUMP hexdump

#define FATAL(...)          \
    do {                    \
        LOGE(__VA_ARGS__);  \
        exit(EXIT_FAILURE); \
    } while (0)

#define LOG_STRERROR(err)                     \
    do {                                      \
        LOGE("%s: %s", err, strerror(errno)); \
    } while (0)

#define STRERR (strerror(errno))

void hexdump(const void *memory, size_t bytes);
char *xs_itoa(int i);
void xs_error(char *err, const char *fmt, ...);
int isIPv6Addr(char *ip);

#endif /* __UTILS_H */
