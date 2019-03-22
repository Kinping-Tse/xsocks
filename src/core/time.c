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

#include "common.h"

#include "time.h"

#include <sys/time.h>

uint64_t timerStart() {
    struct timeval time;

    if (gettimeofday(&time, NULL) == -1) return 0;

    return time.tv_sec * MICROSECOND_UNIT + time.tv_usec;
}

double timerStop(uint64_t start_time, int unit, uint64_t *stop_time) {
    struct timeval time;
    uint64_t now;
    double duration;

    if (gettimeofday(&time, NULL) == -1) return 0;

    now = time.tv_sec * MICROSECOND_UNIT + time.tv_usec;
    if (stop_time) *stop_time = now;

    switch (unit) {
        case SECOND_UNIT: duration = now/MICROSECOND_UNIT_F - start_time/MICROSECOND_UNIT_F; break;
        case MILLISECOND_UNIT: duration = now/MILLISECOND_UNIT_F - start_time/MILLISECOND_UNIT_F; break;
        case MICROSECOND_UNIT: duration = now/SECOND_UNIT_F - start_time/SECOND_UNIT_F; break;
        case NANOSECOND_UNIT: duration = (now/SECOND_UNIT_F - start_time/SECOND_UNIT_F)*MILLISECOND_UNIT_F; break;
        default: duration = 0; break;
    }

    return duration;
}
