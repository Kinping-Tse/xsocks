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

#ifndef __TIME_H
#define __TIME_H

#define SECOND_UNIT 1
#define MILLISECOND_UNIT 1000
#define MICROSECOND_UNIT 1000000
#define NANOSECOND_UNIT 1000000000

#define SECOND_UNIT_F (double)1.0
#define MILLISECOND_UNIT_F (double)1000.0
#define MICROSECOND_UNIT_F (double)1000000.0
#define NANOSECOND_UNIT_F (double)1000000000.0

uint64_t timerStart();
double timerStop(uint64_t start_time, int unit, uint64_t *stop_time);

#endif /* __TIME_H */
