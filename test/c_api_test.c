#include "quickheap.h"

#include <stddef.h>

#include <stdint.h>

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            return __LINE__;                                                    \
        }                                                                       \
    } while (0)

#define DEFINE_SMOKE(name, type, handle_type, prefix, min_value, max_value)    \
    static int smoke_##name(void) {                                             \
        handle_type *heap = prefix##_new();                                     \
        CHECK(heap != NULL);                                                    \
        CHECK(prefix##_len(heap) == 0);                                        \
        CHECK(prefix##_is_empty(heap));                                        \
        type out = (type)71;                                                    \
        CHECK(!prefix##_pop(heap, &out));                                       \
        CHECK(out == (type)71);                                                 \
        prefix##_push(heap, (type)(max_value));                                 \
        prefix##_push(heap, (type)0);                                           \
        prefix##_push(heap, (type)(min_value));                                 \
        prefix##_push(heap, (type)(min_value));                                 \
        prefix##_push(heap, (type)(max_value));                                 \
        CHECK(prefix##_len(heap) == 5);                                        \
        CHECK(!prefix##_is_empty(heap));                                       \
        CHECK(prefix##_capacity(heap) >= prefix##_len(heap));                  \
        const uintptr_t filled_capacity = prefix##_capacity(heap);            \
        CHECK(prefix##_pop(heap, &out) && out == (type)(min_value));            \
        CHECK(prefix##_pop(heap, &out) && out == (type)(min_value));            \
        CHECK(prefix##_pop(heap, &out) && out == (type)0);                      \
        CHECK(prefix##_pop(heap, &out) && out == (type)(max_value));            \
        CHECK(prefix##_pop(heap, &out) && out == (type)(max_value));            \
        CHECK(prefix##_is_empty(heap));                                        \
        CHECK(prefix##_len(heap) == 0);                                        \
        CHECK(prefix##_capacity(heap) >= filled_capacity);                    \
        out = (type)71;                                                         \
        CHECK(!prefix##_pop(heap, &out) && out == (type)71);                    \
        prefix##_free(heap);                                                    \
        return 0;                                                               \
    }

DEFINE_SMOKE(i32, int32_t, QuickHeapI32, quickheap_i32, INT32_MIN, INT32_MAX)
DEFINE_SMOKE(u32, uint32_t, QuickHeapU32, quickheap_u32, 0, UINT32_MAX)
DEFINE_SMOKE(i64, int64_t, QuickHeapI64, quickheap_i64, INT64_MIN, INT64_MAX)
DEFINE_SMOKE(u64, uint64_t, QuickHeapU64, quickheap_u64, 0, UINT64_MAX)

#undef DEFINE_SMOKE
#undef CHECK

int quickheap_c_api_smoke(void) {
    int result = smoke_i32();
    if (result != 0) return result;
    result = smoke_u32();
    if (result != 0) return result;
    result = smoke_i64();
    if (result != 0) return result;
    return smoke_u64();
}
