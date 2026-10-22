#include "quickheap.h"

#include <stdint.h>
#include <stddef.h>

int main(void) {
    QuickHeapI32 *heap = quickheap_i32_new();
    int32_t value = 17;
    if (heap == NULL || quickheap_i32_pop(heap, &value) || value != 17) {
        return 1;
    }
    quickheap_i32_push(heap, 8);
    quickheap_i32_push(heap, -3);
    if (quickheap_i32_len(heap) != 2 || !quickheap_i32_pop(heap, &value) || value != -3) {
        return 2;
    }
    quickheap_i32_free(heap);
    return 0;
}
