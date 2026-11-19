#include "base/map.h"

#include <string.h>

u64 iw_hash_str8(Str8 s) {
    u64 h = 14695981039346656037ULL;
    for (usize i = 0; i < s.len; ++i) {
        h ^= s.data[i];
        h *= 1099511628211ULL;
    }
    /* SplitMix64 avalanche finalizer */
    h ^= h >> 30;
    h *= 0xbf58476d1ce4e5b9ULL;
    h ^= h >> 27;
    h *= 0x94d049bb133111ebULL;
    h ^= h >> 31;
    return h;
}

void iw_map_init(IwMap *m, Arena *a, usize cap_pow2) {
    IW_ASSERT(m != nullptr && a != nullptr);
    IW_ASSERT(cap_pow2 >= 4u && iw_is_pow2(cap_pow2));

    *m = (IwMap){
        .slots = arena_new_array(a, IwMapSlot, cap_pow2),
        .cap = cap_pow2,
        .len = 0,
    };
}

void iw_map_clear(IwMap *m) {
    IW_ASSERT(m != nullptr && m->slots != nullptr);
    memset(m->slots, 0, m->cap * sizeof(*m->slots));
    m->len = 0;
}

bool iw_map_put(IwMap *m, Str8 key, u64 val) {
    IW_ASSERT(m != nullptr && m->slots != nullptr);

    u64 h = iw_hash_str8(key);
    usize mask = m->cap - 1u;
    usize idx = (usize)h & mask;

    for (usize probe = 0; probe < m->cap; ++probe) {
        IwMapSlot *slot = &m->slots[idx];
        if (!slot->occupied) {
            /* Оставляем минимум 1 пустой слот для гарантии завершения циклов */
            if (IW_UNLIKELY(m->len + 1u >= m->cap)) {
                return false;
            }
            *slot = (IwMapSlot){
                .hash = h,
                .val = val,
                .key = key,
                .occupied = true,
            };
            m->len++;
            return true;
        }
        if (slot->hash == h && str8_eq(slot->key, key)) {
            slot->val = val;
            slot->key = key;
            return true;
        }
        idx = (idx + 1u) & mask;
    }
    return false;
}

bool iw_map_get(const IwMap *m, Str8 key, u64 *out_val) {
    IW_ASSERT(m != nullptr && m->slots != nullptr);

    u64 h = iw_hash_str8(key);
    usize mask = m->cap - 1u;
    usize idx = (usize)h & mask;

    for (usize probe = 0; probe < m->cap; ++probe) {
        const IwMapSlot *slot = &m->slots[idx];
        if (!slot->occupied) {
            return false;
        }
        if (slot->hash == h && str8_eq(slot->key, key)) {
            if (out_val != nullptr) {
                *out_val = slot->val;
            }
            return true;
        }
        idx = (idx + 1u) & mask;
    }
    return false;
}

bool iw_map_del(IwMap *m, Str8 key) {
    IW_ASSERT(m != nullptr && m->slots != nullptr);

    u64 h = iw_hash_str8(key);
    usize mask = m->cap - 1u;
    usize i = (usize)h & mask;

    bool found = false;
    for (usize probe = 0; probe < m->cap; ++probe) {
        if (!m->slots[i].occupied) {
            return false;
        }
        if (m->slots[i].hash == h && str8_eq(m->slots[i].key, key)) {
            found = true;
            break;
        }
        i = (i + 1u) & mask;
    }
    if (!found) {
        return false;
    }

    m->len--;

    /* Backward-shift deletion: без томбстоунов, цепочки пробирования остаются плотными */
    usize j = (i + 1u) & mask;
    while (m->slots[j].occupied) {
        usize ideal = (usize)m->slots[j].hash & mask;
        if (((i - ideal) & mask) < ((j - ideal) & mask)) {
            m->slots[i] = m->slots[j];
            i = j;
        }
        j = (j + 1u) & mask;
    }
    m->slots[i] = (IwMapSlot){};
    return true;
}
