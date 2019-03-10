
#include "common.h"
#include "error.h"

#include <stdarg.h>

void errorSet(char *err, const char *fmt, ...) {
    va_list ap;

    if (!err) return;
    va_start(ap, fmt);
    vsnprintf(err, XS_ERR_LEN, fmt, ap);
    va_end(ap);
}
