#include "base/str.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static inline u8 ascii_lower(u8 c) {
    return (c >= 'A' && c <= 'Z') ? (u8)(c + ('a' - 'A')) : c;
}

static inline bool ascii_is_space(u8 c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

Str8 str8_from_cstr(const char *cstr) {
    if (cstr == nullptr) {
        return STR8_EMPTY;
    }
    return (Str8){ .data = (const u8 *)cstr, .len = strlen(cstr) };
}

bool str8_eq(Str8 a, Str8 b) {
    if (a.len != b.len) {
        return false;
    }
    if (a.len == 0 || a.data == b.data) {
        return true;
    }
    return memcmp(a.data, b.data, a.len) == 0;
}

bool str8_eq_icase(Str8 a, Str8 b) {
    if (a.len != b.len) {
        return false;
    }
    for (usize i = 0; i < a.len; ++i) {
        if (ascii_lower(a.data[i]) != ascii_lower(b.data[i])) {
            return false;
        }
    }
    return true;
}

bool str8_starts_with(Str8 s, Str8 prefix) {
    if (prefix.len > s.len) {
        return false;
    }
    return str8_eq((Str8){ .data = s.data, .len = prefix.len }, prefix);
}

bool str8_ends_with(Str8 s, Str8 suffix) {
    if (suffix.len > s.len) {
        return false;
    }
    return str8_eq(
        (Str8){ .data = s.data + (s.len - suffix.len), .len = suffix.len },
                   suffix
    );
}

bool str8_contains_icase(Str8 haystack, Str8 needle) {
    if (needle.len == 0) {
        return true;
    }
    if (needle.len > haystack.len) {
        return false;
    }

    u8 first = ascii_lower(needle.data[0]);
    usize limit = haystack.len - needle.len;
    for (usize i = 0; i <= limit; ++i) {
        if (ascii_lower(haystack.data[i]) != first) {
            continue;
        }
        bool match = true;
        for (usize j = 1; j < needle.len; ++j) {
            if (ascii_lower(haystack.data[i + j]) != ascii_lower(needle.data[j])) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

Str8 str8_slice(Str8 s, usize start, usize end) {
    if (start > s.len) {
        start = s.len;
    }
    if (end > s.len) {
        end = s.len;
    }
    if (end <= start) {
        return STR8_EMPTY;
    }
    return (Str8){ .data = s.data + start, .len = end - start };
}

Str8 str8_trim(Str8 s) {
    usize start = 0;
    while (start < s.len && ascii_is_space(s.data[start])) {
        start++;
    }
    usize end = s.len;
    while (end > start && ascii_is_space(s.data[end - 1u])) {
        end--;
    }
    return str8_slice(s, start, end);
}

isize str8_find_byte(Str8 s, u8 byte) {
    if (s.len == 0) {
        return -1;
    }
    const u8 *p = (const u8 *)memchr(s.data, byte, s.len);
    return (p != nullptr) ? (isize)(p - s.data) : -1;
}

bool str8_split_once(Str8 s, u8 delim, Str8 *out_left, Str8 *out_right) {
    isize idx = str8_find_byte(s, delim);
    if (idx < 0) {
        if (out_left != nullptr) *out_left = s;
        if (out_right != nullptr) *out_right = STR8_EMPTY;
        return false;
    }
    usize pos = (usize)idx;
    if (out_left != nullptr) *out_left = str8_slice(s, 0, pos);
    if (out_right != nullptr) *out_right = str8_slice(s, pos + 1u, s.len);
    return true;
}

Str8 str8_dup(Arena *a, Str8 s) {
    u8 *buf = arena_new_array_raw(a, u8, s.len + 1u);
    if (s.len > 0) {
        memcpy(buf, s.data, s.len);
    }
    buf[s.len] = '\0';
    return (Str8){ .data = buf, .len = s.len };
}

const char *str8_to_cstr(Arena *a, Str8 s) {
    return (const char *)str8_dup(a, s).data;
}

[[gnu::format(printf, 2, 0)]]
static Str8 str8_vfmt(Arena *a, const char *fmt, va_list ap) {
    if (IW_UNLIKELY(fmt == nullptr)) {
        return str8_dup(a, STR8_EMPTY);
    }

    va_list ap_len;
    va_copy(ap_len, ap);
    int n = vsnprintf(nullptr, 0, fmt, ap_len);
    va_end(ap_len);

    if (n <= 0) {
        return str8_dup(a, STR8_EMPTY);
    }

    usize len = (usize)n;
    u8 *buf = arena_new_array_raw(a, u8, len + 1u);
    (void)vsnprintf((char *)buf, len + 1u, fmt, ap);
    return (Str8){ .data = buf, .len = len };
}

Str8 str8_fmt(Arena *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    Str8 res = str8_vfmt(a, fmt, ap);
    va_end(ap);
    return res;
}

bool str8_parse_u64(Str8 s, u64 *out) {
    s = str8_trim(s);
    if (s.len == 0) {
        return false;
    }

    usize i = 0;
    if (s.data[0] == '+') {
        i = 1;
        if (i >= s.len) return false;
    }

    u64 acc = 0;
    for (; i < s.len; ++i) {
        u8 c = s.data[i];
        if (c < '0' || c > '9') {
            return false;
        }
        u64 d = (u64)(c - '0');
        if (acc > (UINT64_MAX - d) / 10u) {
            return false;
        }
        acc = acc * 10u + d;
    }

    if (out != nullptr) {
        *out = acc;
    }
    return true;
}

bool str8_parse_i64(Str8 s, i64 *out) {
    s = str8_trim(s);
    if (s.len == 0) {
        return false;
    }

    bool neg = false;
    usize i = 0;
    if (s.data[0] == '-') {
        neg = true;
        i = 1;
    } else if (s.data[0] == '+') {
        i = 1;
    }

    if (i >= s.len) {
        return false;
    }

    u64 limit = neg ? ((u64)INT64_MAX + 1u) : (u64)INT64_MAX;
    u64 acc = 0;
    for (; i < s.len; ++i) {
        u8 c = s.data[i];
        if (c < '0' || c > '9') {
            return false;
        }
        u64 d = (u64)(c - '0');
        if (acc > (limit - d) / 10u) {
            return false;
        }
        acc = acc * 10u + d;
    }

    if (out != nullptr) {
        if (neg) {
            *out = (acc == (u64)INT64_MAX + 1u) ? INT64_MIN : -(i64)acc;
        } else {
            *out = (i64)acc;
        }
    }
    return true;
}

void str8_builder_init(Str8Builder *b, Arena *scratch) {
    IW_ASSERT(b != nullptr && scratch != nullptr);
    *b = (Str8Builder){ .arena = scratch };
}

void str8_builder_push(Str8Builder *b, Str8 s) {
    IW_ASSERT(b != nullptr && b->arena != nullptr);
    if (s.len == 0) {
        return;
    }
    Str8Node *node = arena_new(b->arena, Str8Node);
    node->chunk = s;
    if (b->tail != nullptr) {
        b->tail->next = node;
    } else {
        b->head = node;
    }
    b->tail = node;
    b->total_len += s.len;
}

void str8_builder_push_byte(Str8Builder *b, u8 c) {
    u8 *p = arena_new_array_raw(b->arena, u8, 1);
    *p = c;
    str8_builder_push(b, (Str8){ .data = p, .len = 1 });
}

void str8_builder_fmt(Str8Builder *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    Str8 chunk = str8_vfmt(b->arena, fmt, ap);
    va_end(ap);
    str8_builder_push(b, chunk);
}

Str8 str8_builder_finish(const Str8Builder *b, Arena *out) {
    IW_ASSERT(b != nullptr && out != nullptr);

    u8 *buf = arena_new_array_raw(out, u8, b->total_len + 1u);
    usize off = 0;
    for (const Str8Node *n = b->head; n != nullptr; n = n->next) {
        memcpy(buf + off, n->chunk.data, n->chunk.len);
        off += n->chunk.len;
    }
    buf[off] = '\0';
    return (Str8){ .data = buf, .len = b->total_len };
}
