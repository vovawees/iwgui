#pragma once

#include "base/arena.h"
#include "base/str.h"
#include "base/types.h"

typedef struct {
    u64 hash;
    u64 val;
    Str8 key;
    bool occupied;
} IwMapSlot;

typedef struct {
    IwMapSlot *slots;
    usize cap;
    usize len;
} IwMap;

[[nodiscard]] u64 iw_hash_str8(Str8 s);

void iw_map_init(IwMap *m, Arena *a, usize cap_pow2);
void iw_map_clear(IwMap *m);

/*
 * Примечание по времени жизни ключа: IwMap хранит срез Str8 без копирования.
 * Ключ должен оставаться валидным, пока запись находится в таблице (например,
 * храниться в постоянной арене или внутри слота IwPool до вызова iw_map_del).
 */
bool iw_map_put(IwMap *m, Str8 key, u64 val);
[[nodiscard]] bool iw_map_get(const IwMap *m, Str8 key, u64 *out_val);
bool iw_map_del(IwMap *m, Str8 key);
