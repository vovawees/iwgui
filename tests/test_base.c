#include "base/arena.h"
#include "base/log.h"
#include "base/map.h"
#include "base/memsec.h"
#include "base/pool.h"
#include "base/str.h"

#include <stdio.h>
#include <string.h>

#define CHECK(cond)                                                              \
do {                                                                         \
    if (!(cond)) {                                                           \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
        return 1;                                                            \
    }                                                                        \
} while (0)

static int test_arena(void) {
    Arena a = {};
    arena_init(&a, IW_MB(4));
    CHECK(a.base != nullptr);
    CHECK(a.committed == 0);
    CHECK(a.pos == 0);

    u64 *x = arena_new(&a, u64);
    CHECK(x != nullptr && *x == 0);
    CHECK(((uintptr_t)x % alignof(u64)) == 0);
    *x = 0xDEADBEEFCAFEBABEULL;

    ArenaTemp t = arena_temp_begin(&a);
    u8 *big = arena_new_array(&a, u8, IW_KB(200));
    CHECK(big[0] == 0 && big[IW_KB(200) - 1u] == 0);
    big[IW_KB(200) - 1u] = 42;
    CHECK(a.committed >= IW_KB(200));
    arena_temp_end(t);

    CHECK(a.pos == t.pos);
    CHECK(*x == 0xDEADBEEFCAFEBABEULL);

    /* Проверка отсутствия конфликта двух scratch-арен */
    ArenaTemp s1 = arena_scratch_get(nullptr, 0);
    Arena *conflicts[1] = { s1.arena };
    ArenaTemp s2 = arena_scratch_get(conflicts, 1);
    CHECK(s1.arena != s2.arena);

    u32 *v1 = arena_new(s1.arena, u32);
    u32 *v2 = arena_new(s2.arena, u32);
    *v1 = 111;
    *v2 = 222;
    CHECK(*v1 == 111 && *v2 == 222);

    arena_scratch_release(s2);
    arena_scratch_release(s1);

    arena_clear(&a);
    CHECK(a.pos == 0);

    arena_destroy(&a);
    CHECK(a.base == nullptr);
    return 0;
}

static int test_pool(void) {
    typedef struct {
        i32 signal_dbm;
        char name[32];
    } DummyNet;

    Arena a = {};
    arena_init(&a, IW_MB(1));

    IwPool pool = {};
    iw_pool_init_type(&pool, &a, 4, DummyNet);

    IwHandle h0 = IW_HANDLE_NULL;
    IwHandle h1 = IW_HANDLE_NULL;
    DummyNet *n0 = (DummyNet *)iw_pool_alloc(&pool, &h0);
    DummyNet *n1 = (DummyNet *)iw_pool_alloc(&pool, &h1);
    CHECK(n0 != nullptr && n1 != nullptr);
    CHECK(!iw_handle_is_null(h0) && !iw_handle_eq(h0, h1));
    n0->signal_dbm = -52;
    n1->signal_dbm = -71;

    /* Проверка упаковки хендла в void* (для GTK-коллбэков) */
    void *packed = iw_handle_to_ptr(h1);
    IwHandle unpacked = iw_handle_from_ptr(packed);
    CHECK(iw_handle_eq(h1, unpacked));
    CHECK(iw_pool_get(&pool, unpacked) == n1);

    /* Удаляем h0 — старый хендл обязан протухнуть */
    CHECK(iw_pool_free(&pool, h0));
    CHECK(!iw_pool_free(&pool, h0));
    CHECK(iw_pool_get(&pool, h0) == nullptr);

    /* Новый слот переиспользует индекс h0, но с новым generation */
    IwHandle h0_new = IW_HANDLE_NULL;
    DummyNet *n0_new = (DummyNet *)iw_pool_alloc(&pool, &h0_new);
    CHECK(n0_new != nullptr);
    CHECK(h0_new.idx == h0.idx);
    CHECK(h0_new.gen != h0.gen);
    CHECK(iw_pool_get(&pool, h0) == nullptr);
    CHECK(iw_pool_get(&pool, h0_new) == n0_new);

    /* Итерация по живым элементам */
    u32 seen = 0;
    IwPoolIter it = iw_pool_iter_begin(&pool);
    IwHandle cur_h = {};
    void *cur_p = nullptr;
    while (iw_pool_iter_next(&it, &cur_h, &cur_p)) {
        CHECK(iw_pool_get(&pool, cur_h) == cur_p);
        seen++;
    }
    CHECK(seen == 2);

    /* iw_pool_clear инвалидирует все выданные хендлы */
    iw_pool_clear(&pool);
    CHECK(pool.count == 0);
    CHECK(iw_pool_get(&pool, h1) == nullptr);
    CHECK(iw_pool_get(&pool, h0_new) == nullptr);

    /* Проверка переполнения пула */
    for (u32 i = 0; i < 4; ++i) {
        CHECK(iw_pool_alloc(&pool, nullptr) != nullptr);
    }
    IwHandle fail_h = h1;
    CHECK(iw_pool_alloc(&pool, &fail_h) == nullptr);
    CHECK(iw_handle_is_null(fail_h));

    arena_destroy(&a);
    return 0;
}

static int test_str(void) {
    Arena a = {};
    arena_init(&a, IW_MB(1));

    Str8 s = STR8("  Home_WiFi_5GHz  ");
    Str8 trimmed = str8_trim(s);
    CHECK(str8_eq(trimmed, STR8("Home_WiFi_5GHz")));
    CHECK(str8_eq_icase(trimmed, STR8("home_wifi_5ghz")));
    CHECK(str8_starts_with(trimmed, STR8("Home")));
    CHECK(str8_ends_with(trimmed, STR8("5GHz")));
    CHECK(str8_contains_icase(trimmed, STR8("wifi")));
    CHECK(!str8_contains_icase(trimmed, STR8("6ghz")));

    Str8 left = {}, right = {};
    CHECK(str8_split_once(STR8("key = value"), '=', &left, &right));
    CHECK(str8_eq(str8_trim(left), STR8("key")));
    CHECK(str8_eq(str8_trim(right), STR8("value")));

    Str8 formatted = str8_fmt(&a, "RSSI: %d dBm (%s)", -58, "5 GHz");
    CHECK(str8_eq(formatted, STR8("RSSI: -58 dBm (5 GHz)")));
    CHECK(formatted.data[formatted.len] == '\0');

    Str8Builder b = {};
    str8_builder_init(&b, &a);
    str8_builder_push(&b, STR8("WIFI:T:WPA;S:"));
    str8_builder_push(&b, STR8("MyNet"));
    str8_builder_push_byte(&b, ';');
    str8_builder_fmt(&b, "P:%s;;", "secret");
    Str8 built = str8_builder_finish(&b, &a);
    CHECK(str8_eq(built, STR8("WIFI:T:WPA;S:MyNet;P:secret;;")));

    i64 iv = 0;
    CHECK(str8_parse_i64(STR8(" -72 "), &iv) && iv == -72);
    CHECK(str8_parse_i64(STR8("-9223372036854775808"), &iv) && iv == INT64_MIN);
    CHECK(str8_parse_i64(STR8("9223372036854775807"), &iv) && iv == INT64_MAX);
    CHECK(!str8_parse_i64(STR8("9223372036854775808"), &iv));
    CHECK(!str8_parse_i64(STR8("12abc"), &iv));

    u64 uv = 0;
    CHECK(str8_parse_u64(STR8("18446744073709551615"), &uv) && uv == UINT64_MAX);
    CHECK(!str8_parse_u64(STR8("18446744073709551616"), &uv));
    CHECK(!str8_parse_u64(STR8("-1"), &uv));

    arena_destroy(&a);
    return 0;
}

static int test_map(void) {
    Arena a = {};
    arena_init(&a, IW_MB(1));

    IwMap m = {};
    iw_map_init(&m, &a, 512);

    constexpr usize k_count = 300;
    Str8 *keys = arena_new_array(&a, Str8, k_count);

    for (usize i = 0; i < k_count; ++i) {
        keys[i] = str8_fmt(&a, "/net/connman/iwd/0/3/net_%zu", i);
        CHECK(iw_map_put(&m, keys[i], (u64)(i * 10u + 7u)));
    }
    CHECK(m.len == k_count);

    /* Удаляем каждый второй ключ (стресс-тест backward-shift) */
    for (usize i = 0; i < k_count; i += 2) {
        CHECK(iw_map_del(&m, keys[i]));
        CHECK(!iw_map_del(&m, keys[i]));
    }
    CHECK(m.len == k_count / 2u);

    /* Проверяем все оставшиеся и удалённые ключи */
    for (usize i = 0; i < k_count; ++i) {
        u64 val = 0;
        bool ok = iw_map_get(&m, keys[i], &val);
        if ((i % 2u) == 0) {
            CHECK(!ok);
        } else {
            CHECK(ok && val == (u64)(i * 10u + 7u));
        }
    }

    /* Перезапись существующего ключа */
    CHECK(iw_map_put(&m, keys[1], 99999u));
    u64 v1 = 0;
    CHECK(iw_map_get(&m, keys[1], &v1) && v1 == 99999u);

    iw_map_clear(&m);
    CHECK(m.len == 0);
    CHECK(!iw_map_get(&m, keys[1], &v1));

    arena_destroy(&a);
    return 0;
}

static int test_memsec(void) {
    char secret[32] = "super-secret-wifi-pass";
    iw_memzero_secure(secret, sizeof(secret));
    for (usize i = 0; i < sizeof(secret); ++i) {
        CHECK(secret[i] == 0);
    }
    return 0;
}

int main(void) {
    iw_log_set_level(IW_LOG_DEBUG);

    if (test_arena() != 0) return 1;
    if (test_pool() != 0) return 1;
    if (test_str() != 0) return 1;
    if (test_map() != 0) return 1;
    if (test_memsec() != 0) return 1;

    arena_scratch_shutdown();
    IW_LOGI("test_base", "all base tests passed");
    return 0;
}
