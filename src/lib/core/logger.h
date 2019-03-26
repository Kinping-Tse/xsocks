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

#ifndef __XS_LOGGER_H
#define __XS_LOGGER_H

/* Log levels */
enum {
    LOGLEVEL_DEBUG = 0,
    LOGLEVEL_INFO,
    LOGLEVEL_NOTICE,
    LOGLEVEL_WARNING,
    LOGLEVEL_ERROR,
    LOGLEVEL_RAW = 1 << 10, /* Modifier to log without timestamp */
};

#define LOGGER_DEFAULT_FILE ""
#define LOGGER_DEFAULT_LEVEL LOGLEVEL_NOTICE
#define LOGGER_DEFAULT_SYSLOG_ENABLED 0
#define LOGGER_DEFAULT_SYSLOG_IDENT ""
#define LOGGER_DEFAULT_SYSLOG_FACILITY LOG_LOCAL0
#define LOGGER_DEFAULT_COLOR_ENABLED 0
#define LOGGER_DEFAULT_FILE_LINE_ENABLED 0

#define LOG_MAX_LEN 1024 /* Default maximum length of syslog messages.*/

typedef struct logger {
    char *file;
    int level;
    int syslog_enabled;
    char *syslog_ident;
    int syslog_facility;
    int color_enabled;
    int file_line_enabled;
} logger;

logger *loggerNew();
void loggerFree(logger *log);
void loggerLog(logger *log, int level, const char *file, int line, const char *fmt, ...);
void setLogger(logger *log);
logger *getLogger();

#define log(level, ...) loggerLog(NULL, level, __FILE__, __LINE__, __VA_ARGS__)
#define logDebug(...) loggerLog(NULL, LOGLEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define logInfo(...) loggerLog(NULL, LOGLEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define logNotice(...) loggerLog(NULL, LOGLEVEL_NOTICE, __FILE__, __LINE__, __VA_ARGS__)
#define logWarn(...) loggerLog(NULL, LOGLEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define logErr(...) loggerLog(NULL, LOGLEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define logRaw(level, ...) loggerLog(NULL, level|LOGLEVEL_RAW, __FILE__, __LINE__, __VA_ARGS__)

#define LOGD logDebug
#define LOGI logInfo
#define LOGN logNotice
#define LOGW logWarn
#define LOGE logErr
#define LOG  log
#define LOGR logRaw

#define LOGDR(...) LOGR(LOGLEVEL_DEBUG, __VA_ARGS__)
#define LOGIR(...) LOGR(LOGLEVEL_INFO, __VA_ARGS__)
#define LOGNR(...) LOGR(LOGLEVEL_NOTICE, __VA_ARGS__)
#define LOGWR(...) LOGR(LOGLEVEL_WARNING, __VA_ARGS__)
#define LOGER(...) LOGR(LOGLEVEL_ERROR, __VA_ARGS__)

#endif /* __XS_LOGGER_H */
