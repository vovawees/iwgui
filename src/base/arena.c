#include "base/arena.h"

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#if defined(__SANITIZE_ADDRESS__)
#define IW_HAS_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define IW_HAS_ASAN 1
#endif
#endif

#if defined(IW_HAS_ASAN)
#include <sanitizer/asan_interface.h>
#define IW_ASAN_POISON(addr, size)   ASAN_POISON_MEMORY_REGION((addr), (size))
#define IW_ASAN_UNPOISON(addr, size) ASAN_UNPOISON_MEMORY_REGION((addr), (size))
#else
#define IW_ASAN_POISON(addr, size)   ((void)(addr), (void)(size))
#define IW_ASAN_UNPOISON(addr, size) ((void)(addr), (void)(size))
#endif

static constexpr usize k_default_commit_step = IW_KB(64);
static constexpr usize k_scratch_reserve = IW_MB(16);

static usize os_page_size(void) {
    static usize cached = 0;
    if (IW_UNLIKELY(cached == 0)) {
        long ps = sysconf(_SC_PAGESIZE);
        cached = (ps > 0 && iw_is_pow2((usize)ps)) ? (usize)ps : 4096u;
    }
    return cached;
}

void arena_init(Arena *a, usize reserve_bytes) {
    IW_ASSERT(a != nullptr);
    IW_ASSERT(reserve_bytes > 0);

    usize page = os_page_size();
    usize reserved = iw_align_up(reserve_bytes, page);
    usize step = iw_align_up(k_default_commit_step, page);

    void *mem = mmap(nullptr, reserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (IW_UNLIKELY(mem == MAP_FAILED)) {
        IW_PANIC("arena_init: mmap reserve failed");
    }

    *a = (Arena){
        .base = (u8 *)mem,
        .reserved = reserved,
        .committed = 0,
        .pos = 0,
        .commit_step = step,
    };
}

void arena_destroy(Arena *a) {
    if (a == nullptr || a->base == nullptr) {
        return;
    }
    if (a->committed > 0) {
        IW_ASAN_UNPOISON(a->base, a->committed);
    }
    (void)munmap(a->base, a->reserved);
    *a = (Arena){};
}

static void arena_ensure_committed(Arena *a, usize needed) {
    if (IW_LIKELY(needed <= a->committed)) {
        return;
    }
    if (IW_UNLIKELY(needed > a->reserved)) {
        IW_PANIC("arena out of reserved virtual memory");
    }

    usize new_commit = iw_align_up(needed, a->commit_step);
    if (new_commit > a->reserved) {
        new_commit = a->reserved;
    }

    usize delta = new_commit - a->committed;
    if (IW_UNLIKELY(mprotect(a->base + a->committed, delta, PROT_READ | PROT_WRITE) != 0)) {
        IW_PANIC("arena_ensure_committed: mprotect failed");
    }

    IW_ASAN_POISON(a->base + a->committed, delta);
    a->committed = new_commit;
}

void *arena_push_raw(Arena *a, usize size, usize align) {
    IW_ASSERT(a != nullptr && a->base != nullptr);
    IW_ASSERT(iw_is_pow2(align));

    usize aligned = iw_align_up(a->pos, align);
    if (IW_UNLIKELY(aligned > a->reserved || size > a->reserved - aligned)) {
        IW_PANIC("arena overflow");
    }

    usize new_pos = aligned + size;
    arena_ensure_committed(a, new_pos);

    if (size > 0) {
        IW_ASAN_UNPOISON(a->base + aligned, size);
    }

    a->pos = new_pos;
    return a->base + aligned;
}

void *arena_push_zero(Arena *a, usize size, usize align) {
    void *ptr = arena_push_raw(a, size, align);
    if (size > 0) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void arena_pop_to(Arena *a, usize pos) {
    IW_ASSERT(a != nullptr && a->base != nullptr);
    IW_ASSERT(pos <= a->pos);

    if (pos < a->pos) {
        IW_ASAN_POISON(a->base + pos, a->pos - pos);
        a->pos = pos;
    }
}

void arena_clear(Arena *a) {
    arena_pop_to(a, 0);
}

static thread_local Arena g_scratch[2] = {};

ArenaTemp arena_scratch_get(Arena *const *conflicts, usize conflict_count) {
    for (usize i = 0; i < IW_ARRLEN(g_scratch); ++i) {
        Arena *cand = &g_scratch[i];
        bool collides = false;
        for (usize j = 0; j < conflict_count; ++j) {
            if (cand == conflicts[j]) {
                collides = true;
                break;
            }
        }
        if (!collides) {
            if (IW_UNLIKELY(cand->base == nullptr)) {
                arena_init(cand, k_scratch_reserve);
            }
            return arena_temp_begin(cand);
        }
    }
    IW_PANIC("arena_scratch_get: all scratch arenas conflict");
}

void arena_scratch_shutdown(void) {
    for (usize i = 0; i < IW_ARRLEN(g_scratch); ++i) {
        arena_destroy(&g_scratch[i]);
    }
}
