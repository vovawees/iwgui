#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;

typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;

typedef size_t    usize;
typedef ptrdiff_t isize;

typedef float     f32;
typedef double    f64;

#define IW_KB(n) ((usize)(n) * 1024u)
#define IW_MB(n) (IW_KB(n) * 1024u)
#define IW_GB(n) (IW_MB(n) * 1024u)

#define IW_ARRLEN(a) (sizeof(a) / sizeof((a)[0]))

#define IW_LIKELY(x)   __builtin_expect(!!(x), 1)
#define IW_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define IW_UNREACHABLE() __builtin_unreachable()

[[noreturn]] static inline void iw_panic_impl(const char *file, int line, const char *msg) {
    fprintf(stderr, "[iwgui PANIC] %s:%d: %s\n", file, line, msg);
    fflush(stderr);
    abort();
}

#define IW_PANIC(msg) iw_panic_impl(__FILE__, __LINE__, (msg))

#ifndef NDEBUG
#define IW_ASSERT(cond)                                                          \
do {                                                                         \
    if (IW_UNLIKELY(!(cond))) {                                              \
        iw_panic_impl(__FILE__, __LINE__, "assertion failed: " #cond);       \
    }                                                                        \
} while (0)
#else
#define IW_ASSERT(cond) ((void)sizeof(cond))
#endif

[[nodiscard]] static inline bool iw_is_pow2(usize x) {
    return x != 0 && (x & (x - 1u)) == 0;
}

[[nodiscard]] static inline usize iw_align_up(usize n, usize align) {
    IW_ASSERT(iw_is_pow2(align));
    return (n + (align - 1u)) & ~(align - 1u);
}

[[nodiscard]] static inline usize iw_min_usize(usize a, usize b) {
    return a < b ? a : b;
}

[[nodiscard]] static inline usize iw_max_usize(usize a, usize b) {
    return a > b ? a : b;
}

[[nodiscard]] static inline i32 iw_clamp_i32(i32 v, i32 lo, i32 hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
