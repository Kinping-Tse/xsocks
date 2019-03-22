/*
 * This file is part of xsocks, a lightweight proxy tool for science online.
 *
 * Copyright (C) 2019 XJP09_HK <jianping_xie@aliyun.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

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
