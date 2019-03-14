#ifndef __UTILS_H
#define __UTILS_H

#define DUMP hexdump

void xs_daemonize();

void setupIgnoreHandlers();

void hexdump(const void *memory, size_t bytes);
char *xs_itoa(int i);

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#endif /* __UTILS_H */
