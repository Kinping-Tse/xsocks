#ifndef __XSALLOC_H
#define __XSALLOC_H

#include "redis/zmalloc.h"

#define xs_malloc zmalloc
#define xs_calloc zcalloc
#define xs_realloc zrealloc
#define xs_free(p) do { zfree(p); p = NULL; } while (0)
#define xs_strdup zstrdup

#define CALLOC_P(p) (p = xs_calloc(sizeof(*p)))
#define FREE_P xs_free

#endif /* __XSALLOC_H */
