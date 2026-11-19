#include "base/log.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static IwLogLevel g_log_level = IW_LOG_INFO;
static bool g_tty_checked = false;
static bool g_use_color = false;
static u64 g_start_ms = 0;

static u64 monotonic_ms(void) {
    struct timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 1000u + (u64)(ts.tv_nsec / 1000000L);
}

static void log_lazy_init(void) {
    if (!g_tty_checked) {
        g_use_color = isatty(STDERR_FILENO) == 1;
        g_start_ms = monotonic_ms();
        g_tty_checked = true;
    }
}

void iw_log_set_level(IwLogLevel level) {
    g_log_level = level;
}

IwLogLevel iw_log_get_level(void) {
    return g_log_level;
}

void iw_log_write(IwLogLevel level, const char *tag, const char *fmt, ...) {
    if (level < g_log_level || fmt == nullptr) {
        return;
    }
    log_lazy_init();

    static const char *const k_names[] = {
        [IW_LOG_TRACE] = "TRC",
        [IW_LOG_DEBUG] = "DBG",
        [IW_LOG_INFO]  = "INF",
        [IW_LOG_WARN]  = "WRN",
        [IW_LOG_ERROR] = "ERR",
    };

    static const char *const k_colors[] = {
        [IW_LOG_TRACE] = "\x1b[90m",
        [IW_LOG_DEBUG] = "\x1b[36m",
        [IW_LOG_INFO]  = "\x1b[32m",
        [IW_LOG_WARN]  = "\x1b[33m",
        [IW_LOG_ERROR] = "\x1b[31;1m",
    };

    u64 elapsed = monotonic_ms() - g_start_ms;
    u64 sec = elapsed / 1000u;
    u64 ms = elapsed % 1000u;

    char buf[1024];
    int head = 0;
    if (g_use_color) {
        head = snprintf(
            buf, sizeof(buf),
                        "\x1b[90m%4" PRIu64 ".%03" PRIu64 "\x1b[0m %s%s\x1b[0m [%s] ",
                        sec, ms, k_colors[level], k_names[level], tag
        );
    } else {
        head = snprintf(
            buf, sizeof(buf),
                        "%4" PRIu64 ".%03" PRIu64 " %s [%s] ",
                        sec, ms, k_names[level], tag
        );
    }

    if (head < 0) {
        return;
    }

    usize pos = (usize)head;
    if (pos >= sizeof(buf) - 2u) {
        pos = sizeof(buf) - 2u;
    }

    va_list ap;
    va_start(ap, fmt);
    int body = vsnprintf(buf + pos, sizeof(buf) - pos - 1u, fmt, ap);
    va_end(ap);

    if (body > 0) {
        usize rem = sizeof(buf) - pos - 1u;
        pos += ((usize)body < rem) ? (usize)body : (rem - 1u);
    }

    buf[pos++] = '\n';
    (void)fwrite(buf, 1, pos, stderr);
}
