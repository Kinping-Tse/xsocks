
#ifndef __ERROR_H
#define __ERROR_H

#define XS_ERR_LEN 1024

#define FATAL(...)         \
    do {                   \
        LOGE(__VA_ARGS__); \
        exit(EXIT_ERR);    \
    } while (0)

#define STRERR (strerror(errno))
#define LOG_STRERROR(err) do { LOGE("%s: %s", err, STRERR); } while (0)

void errorSet(char *err, const char *fmt, ...);

#define xs_error errorSet

#endif /* __ERROR_H */
