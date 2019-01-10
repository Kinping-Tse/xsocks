#ifndef __UTILS_H
#define __UTILS_H

#define MODULE_REMOTE 0
#define MODULE_LOCAL  1

void usage(int module);

#define FATAL(...) do { \
    LOGE(__VA_ARGS__); \
    exit(EXIT_FAILURE); \
} while (0)

#endif /* __UTILS_H */
