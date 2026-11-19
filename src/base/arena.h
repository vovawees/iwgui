#pragma once

#include "base/types.h"

typedef struct {
    u8 *base;
    usize reserved;
    usize committed;
    usize pos;
    usize commit_step;
} Arena;

typedef struct {
    Arena *arena;
    usize pos;
} ArenaTemp;

void arena_init(Arena *a, usize reserve_bytes);
void arena_destroy(Arena *a);

[[nodiscard]] void *arena_push_raw(Arena *a, usize size, usize align);
[[nodiscard]] void *arena_push_zero(Arena *a, usize size, usize align);

void arena_pop_to(Arena *a, usize pos);
void arena_clear(Arena *a);

[[nodiscard]] static inline ArenaTemp arena_temp_begin(Arena *a) {
    return (ArenaTemp){ .arena = a, .pos = a->pos };
}

static inline void arena_temp_end(ArenaTemp t) {
    if (t.arena != nullptr) {
        arena_pop_to(t.arena, t.pos);
    }
}

[[nodiscard]] ArenaTemp arena_scratch_get(Arena *const *conflicts, usize conflict_count);

static inline void arena_scratch_release(ArenaTemp t) {
    arena_temp_end(t);
}

void arena_scratch_shutdown(void);

#define arena_new(a, T) \
((T *)arena_push_zero((a), sizeof(T), alignof(T)))

#define arena_new_array(a, T, count) \
((T *)arena_push_zero((a), sizeof(T) * (usize)(count), alignof(T)))

#define arena_new_array_raw(a, T, count) \
((T *)arena_push_raw((a), sizeof(T) * (usize)(count), alignof(T)))
