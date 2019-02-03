#ifndef __XS_COMMON_H
#define __XS_COMMON_H

#include "fmacros.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <unistd.h>

#include "logger.h"
#include "xsalloc.h"

#define UNUSED(V) ((void)V)

#define INVALID_FD -1
#define EXIT_OK  EXIT_SUCCESS
#define EXIT_ERR EXIT_FAILURE

#define bool int
#define true (1)
#define false (0)

#endif /* __XS_COMMON_H */
