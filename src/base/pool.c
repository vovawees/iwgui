#include "base/pool.h"

#include <string.h>

static constexpr u32 k_pool_none = UINT32_MAX;

void iw_pool_init(IwPool *p, Arena *a, u32 cap, usize elem_size, usize elem_align) {
    IW_ASSERT(p != nullptr && a != nullptr);
    IW_ASSERT(cap > 0 && cap < k_pool_none);
    IW_ASSERT(elem_size > 0 && iw_is_pow2(elem_align));

    usize stride = iw_align_up(elem_size, elem_align);
    u8 *data = (u8 *)arena_push_zero(a, stride * (usize)cap, elem_align);
    u32 *gens = arena_new_array(a, u32, cap);
    u32 *free_next = arena_new_array(a, u32, cap);
    u8 *used = arena_new_array(a, u8, cap);

    for (u32 i = 0; i < cap; ++i) {
        gens[i] = 1u;
        free_next[i] = (i + 1u < cap) ? (i + 1u) : k_pool_none;
    }

    *p = (IwPool){
        .data = data,
        .gens = gens,
        .free_next = free_next,
        .used = used,
        .stride = stride,
        .cap = cap,
        .count = 0,
        .free_head = 0,
    };
}

void *iw_pool_alloc(IwPool *p, IwHandle *out_handle) {
    IW_ASSERT(p != nullptr);

    if (IW_UNLIKELY(p->free_head == k_pool_none)) {
        if (out_handle != nullptr) {
            *out_handle = IW_HANDLE_NULL;
        }
        return nullptr;
    }

    u32 idx = p->free_head;
    p->free_head = p->free_next[idx];
    p->used[idx] = 1u;
    p->count++;

    void *slot = p->data + (usize)idx * p->stride;
    memset(slot, 0, p->stride);

    if (out_handle != nullptr) {
        *out_handle = (IwHandle){ .idx = idx, .gen = p->gens[idx] };
    }
    return slot;
}

void *iw_pool_get(const IwPool *p, IwHandle h) {
    IW_ASSERT(p != nullptr);

    if (h.gen == 0 || h.idx >= p->cap || !p->used[h.idx] || p->gens[h.idx] != h.gen) {
        return nullptr;
    }
    return p->data + (usize)h.idx * p->stride;
}

bool iw_pool_free(IwPool *p, IwHandle h) {
    IW_ASSERT(p != nullptr);

    if (h.gen == 0 || h.idx >= p->cap || !p->used[h.idx] || p->gens[h.idx] != h.gen) {
        return false;
    }

    p->used[h.idx] = 0;
    p->gens[h.idx]++;
    if (IW_UNLIKELY(p->gens[h.idx] == 0)) {
        p->gens[h.idx] = 1u;
    }

    p->free_next[h.idx] = p->free_head;
    p->free_head = h.idx;
    p->count--;
    return true;
}

void iw_pool_clear(IwPool *p) {
    IW_ASSERT(p != nullptr);

    for (u32 i = 0; i < p->cap; ++i) {
        if (p->used[i]) {
            p->used[i] = 0;
            p->gens[i]++;
            if (IW_UNLIKELY(p->gens[i] == 0)) {
                p->gens[i] = 1u;
            }
        }
        p->free_next[i] = (i + 1u < p->cap) ? (i + 1u) : k_pool_none;
    }
    p->free_head = 0;
    p->count = 0;
}

bool iw_pool_iter_next(IwPoolIter *it, IwHandle *out_handle, void **out_ptr) {
    IW_ASSERT(it != nullptr && it->pool != nullptr);

    const IwPool *p = it->pool;
    while (it->cursor < p->cap) {
        u32 idx = it->cursor++;
        if (p->used[idx]) {
            if (out_handle != nullptr) {
                *out_handle = (IwHandle){ .idx = idx, .gen = p->gens[idx] };
            }
            if (out_ptr != nullptr) {
                *out_ptr = p->data + (usize)idx * p->stride;
            }
            return true;
        }
    }
    return false;
}
