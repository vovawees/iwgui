#pragma once

#include "base/types.h"

typedef enum : u8 {
    IW_LOG_TRACE = 0,
    IW_LOG_DEBUG,
    IW_LOG_INFO,
    IW_LOG_WARN,
    IW_LOG_ERROR,
} IwLogLevel;

void iw_log_set_level(IwLogLevel level);
[[nodiscard]] IwLogLevel iw_log_get_level(void);

[[gnu::format(printf, 3, 4)]]
void iw_log_write(IwLogLevel level, const char *tag, const char *fmt, ...);

#define IW_LOGT(tag, fmt, ...) iw_log_write(IW_LOG_TRACE, (tag), (fmt) __VA_OPT__(,) __VA_ARGS__)
#define IW_LOGD(tag, fmt, ...) iw_log_write(IW_LOG_DEBUG, (tag), (fmt) __VA_OPT__(,) __VA_ARGS__)
#define IW_LOGI(tag, fmt, ...) iw_log_write(IW_LOG_INFO,  (tag), (fmt) __VA_OPT__(,) __VA_ARGS__)
#define IW_LOGW(tag, fmt, ...) iw_log_write(IW_LOG_WARN,  (tag), (fmt) __VA_OPT__(,) __VA_ARGS__)
#define IW_LOGE(tag, fmt, ...) iw_log_write(IW_LOG_ERROR, (tag), (fmt) __VA_OPT__(,) __VA_ARGS__)
