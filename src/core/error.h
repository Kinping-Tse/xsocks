
#ifndef __ERROR_H
#define __ERROR_H

#define XS_ERR_LEN 1024

#define FATAL(...)          \
    do {                    \
        LOGE(__VA_ARGS__);  \
        exit(EXIT_ERR); \
    } while (0)

#define LOG_STRERROR(err)                     \
    do {                                      \
        LOGE("%s: %s", err, strerror(errno)); \
    } while (0)

#define STRERR (strerror(errno))

void errorSet(char *err, const char *fmt, ...);

#define xs_error errorSet

#endif /* __ERROR_H */
