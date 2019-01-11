#ifndef __UTILS_H
#define __UTILS_H

enum {
    MODULE_REMOTE = 0,
    MODULE_LOCAL
};

void usage(int module);

#define FATAL(...)          \
    do {                    \
        LOGE(__VA_ARGS__);  \
        exit(EXIT_FAILURE); \
    } while (0)

#define LOG_STRERROR(err)                     \
    do {                                      \
        LOGE("%s: %s", err, strerror(errno)); \
    } while (0)

#endif /* __UTILS_H */
