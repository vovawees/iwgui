#pragma once

#include "base/arena.h"
#include "base/types.h"

typedef struct {
    const u8 *data;
    usize len;
} Str8;

#define STR8(lit)      ((Str8){ .data = (const u8 *)(lit), .len = sizeof(lit) - 1u })
#define STR8_EMPTY     ((Str8){ .data = nullptr, .len = 0 })
#define STR8_FMT       "%.*s"
#define STR8_ARG(s)    (int)(s).len, (const char *)(s).data

[[nodiscard]] Str8 str8_from_cstr(const char *cstr);

[[nodiscard]] bool str8_eq(Str8 a, Str8 b);
[[nodiscard]] bool str8_eq_icase(Str8 a, Str8 b);
[[nodiscard]] bool str8_starts_with(Str8 s, Str8 prefix);
[[nodiscard]] bool str8_ends_with(Str8 s, Str8 suffix);
[[nodiscard]] bool str8_contains_icase(Str8 haystack, Str8 needle);

[[nodiscard]] Str8 str8_slice(Str8 s, usize start, usize end);
[[nodiscard]] Str8 str8_trim(Str8 s);
[[nodiscard]] isize str8_find_byte(Str8 s, u8 byte);
bool str8_split_once(Str8 s, u8 delim, Str8 *out_left, Str8 *out_right);

[[nodiscard]] Str8 str8_dup(Arena *a, Str8 s);
[[nodiscard]] const char *str8_to_cstr(Arena *a, Str8 s);

[[nodiscard, gnu::format(printf, 2, 3)]]
Str8 str8_fmt(Arena *a, const char *fmt, ...);

[[nodiscard]] bool str8_parse_i64(Str8 s, i64 *out);
[[nodiscard]] bool str8_parse_u64(Str8 s, u64 *out);

typedef struct Str8Node Str8Node;
struct Str8Node {
    Str8Node *next;
    Str8 chunk;
};

typedef struct {
    Arena *arena;
    Str8Node *head;
    Str8Node *tail;
    usize total_len;
} Str8Builder;

void str8_builder_init(Str8Builder *b, Arena *scratch);
void str8_builder_push(Str8Builder *b, Str8 s);
void str8_builder_push_byte(Str8Builder *b, u8 c);

[[gnu::format(printf, 2, 3)]]
void str8_builder_fmt(Str8Builder *b, const char *fmt, ...);

[[nodiscard]] Str8 str8_builder_finish(const Str8Builder *b, Arena *out);
