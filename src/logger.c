#include "common.h"

#include <stdarg.h>
#include <syslog.h>
#include <time.h>
#include <sys/time.h>

static logger *xsocks_logger;

logger* loggerNew() {
    logger* log = xs_calloc(sizeof(*log));

    log->file = LOGGER_DEFAULT_FILE;
    log->level = LOGGER_DEFAULT_LEVEL;
    log->color_enabled = LOGGER_DEFAULT_COLOR_ENABLED;
    log->syslog_enabled = LOGGER_DEFAULT_SYSLOG_ENABLED;
    log->syslog_ident = LOGGER_DEFAULT_SYSLOG_IDENT;
    log->syslog_facility = LOGGER_DEFAULT_SYSLOG_FACILITY;
    log->file_line_enabled = LOGGER_DEFAULT_FILE_LINE_ENABLED;

    return log;
}

void loggerFree(logger* log) {
    xs_free(log);
}

void loggerLogRaw(logger* log, int level, const char* file, int line, const char *msg) {
    const int syslogLevelMap[] = { LOG_DEBUG, LOG_INFO, LOG_NOTICE, LOG_WARNING, LOG_ERR };
    FILE * fp;
    int rawmode = (level & LOGLEVEL_RAW);
    int log_to_stdout = log->file[0] == '\0' || log->file == NULL;

    level &= 0xff; /* clear flags */
    if (level < log->level) return;

    fp = log_to_stdout ? stdout : fopen(log->file, "a");
    if (!fp) return;

    if (rawmode) {
        fprintf(fp, "%s", msg);
    } else {
        char buf_fl[1024] = {0};
        char buf_tm[64];

        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        int off = strftime(buf_tm, sizeof(buf_tm), "%Y-%m-%d %H:%M:%S.", tm);
        snprintf(buf_tm+off, sizeof(buf_tm)-off,"%03d", tv.tv_usec/1000);

        if (log->file_line_enabled)
            snprintf(buf_fl, sizeof(buf_fl), "%s:%d" , file, line);

        const char *loglevelMap[] = {"Debug", "Info", "Notice", "Warning", "Error"};
        const char *colorMap[] = {"\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};

        if (log->color_enabled && log_to_stdout)
            fprintf(fp, "%s %s[%d] %s<%s>\x1b[0m\x1b[90m%s\x1b[0m: %s\n", buf_tm, log->syslog_ident,
                    getpid(), colorMap[level], loglevelMap[level], buf_fl, msg);
        else
            fprintf(fp, "%s %s[%d] <%s> %s: %s\n", buf_tm, log->syslog_ident,
                    getpid(), loglevelMap[level], buf_fl, msg);
    }
    fflush(fp);

    if (!log_to_stdout) fclose(fp);
    if (log->syslog_enabled) {
        openlog(log->syslog_ident, LOG_PID|LOG_NDELAY|LOG_NOWAIT, log->syslog_facility);
        syslog(syslogLevelMap[level], "%s", msg);
        closelog();
    }
}

void loggerLog(logger* log, int level, const char *file, int line, const char *fmt, ...) {
    if (log == NULL) log = xsocks_logger;

    if ((level & 0xFF) < log->level) return;

    va_list ap;
    char msg[LOG_MAX_LEN];

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    loggerLogRaw(log, level, file, line, msg);
}

void setLogger(logger* log) {
    xsocks_logger = log;
}

logger* getLogger() {
    return xsocks_logger;
}
