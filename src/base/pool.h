#pragma once

#include "base/arena.h"
#include "base/types.h"

typedef struct {
    u32 idx;
    u32 gen;
} IwHandle;

#define IW_HANDLE_NULL ((IwHandle){ .idx = 0, .gen = 0 })

[[nodiscard]] static inline bool iw_handle_is_null(IwHandle h) {
    return h.gen == 0;
}

[[nodiscard]] static inline bool iw_handle_eq(IwHandle a, IwHandle b) {
    return a.idx == b.idx && a.gen == b.gen;
}

[[nodiscard]] static inline u64 iw_handle_pack(IwHandle h) {
    return ((u64)h.gen << 32) | (u64)h.idx;
}

[[nodiscard]] static inline IwHandle iw_handle_unpack(u64 v) {
    return (IwHandle){
        .idx = (u32)(v & 0xFFFFFFFFu),
        .gen = (u32)(v >> 32),
    };
}

[[nodiscard]] static inline void *iw_handle_to_ptr(IwHandle h) {
    static_assert(sizeof(void *) >= sizeof(u64), "64-bit pointer required for packed handles");
    return (void *)(uintptr_t)iw_handle_pack(h);
}

[[nodiscard]] static inline IwHandle iw_handle_from_ptr(const void *p) {
    return iw_handle_unpack((u64)(uintptr_t)p);
}

typedef struct {
    u8 *data;
    u32 *gens;
    u32 *free_next;
    u8 *used;
    usize stride;
    u32 cap;
    u32 count;
    u32 free_head;
} IwPool;

typedef struct {
    const IwPool *pool;
    u32 cursor;
} IwPoolIter;

void iw_pool_init(IwPool *p, Arena *a, u32 cap, usize elem_size, usize elem_align);

[[nodiscard]] void *iw_pool_alloc(IwPool *p, IwHandle *out_handle);
[[nodiscard]] void *iw_pool_get(const IwPool *p, IwHandle h);
bool iw_pool_free(IwPool *p, IwHandle h);
void iw_pool_clear(IwPool *p);

[[nodiscard]] static inline IwPoolIter iw_pool_iter_begin(const IwPool *p) {
    return (IwPoolIter){ .pool = p, .cursor = 0 };
}

[[nodiscard]] bool iw_pool_iter_next(IwPoolIter *it, IwHandle *out_handle, void **out_ptr);

#define iw_pool_init_type(p, a, cap, T) \
iw_pool_init((p), (a), (cap), sizeof(T), alignof(T))
