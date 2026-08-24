#include "../test_support.h"

#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"
#include "mmay/runtime/string.h"

static bool mmay_test_managed_string_heap(
    MarimayManagedHeap *heap,
    MarimayAllocator allocator
) {
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    options.max_objects = 16;
    options.max_live_bytes = 4096;
    options.collect_before_allocation = true;
    return mmay_managed_heap_init(heap, allocator, options);
}

static void mmay_test_managed_string_concat_bytes(void) {
    static const unsigned char left[] = {'a', 0, 0xce, 0xbb};
    static const unsigned char right[] = {'{', 'x', '}'};
    static const unsigned char expected[] = {
        'a', 0, 0xce, 0xbb, '{', 'x', '}'
    };
    MarimayManagedHeap heap;
    MarimayManagedReference result = mmay_managed_invalid_reference();
    MarimayManagedHeap invalid_heap;
    const unsigned char *bytes = NULL;
    size_t length = 0;
    memset(&invalid_heap, 0, sizeof(invalid_heap));
    MMAY_EXPECT(mmay_test_managed_string_heap(
        &heap, mmay_system_allocator()
    ));
    MMAY_EXPECT(mmay_managed_string_concat(
        &heap, left, sizeof(left), right, sizeof(right), &result
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_string_bytes(
        &heap, result, &bytes, &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(length == sizeof(expected));
    MMAY_EXPECT(memcmp(bytes, expected, sizeof(expected)) == 0);
    MMAY_EXPECT(mmay_managed_string_concat(
        &heap, NULL, 1, right, sizeof(right), &result
    ) == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_string_concat(
        &heap, left, sizeof(left), NULL, 1, &result
    ) == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_string_concat(
        &invalid_heap, left, sizeof(left), right, sizeof(right), &result
    ) == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_string_concat(
        &heap, left, SIZE_MAX, right, 1, &result
    ) == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_string_concat(
        &heap, NULL, 0, NULL, 0, &result
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_string_bytes(
        &heap, result, &bytes, &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(length == 0);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_string_concat_limit(void) {
    static const unsigned char bytes[] = "too large";
    MarimayManagedHeap heap;
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    MarimayManagedReference result = mmay_managed_invalid_reference();
    options.max_objects = 4;
    options.max_live_bytes = 1;
    options.collect_before_allocation = true;
    MMAY_EXPECT(mmay_managed_heap_init(
        &heap, mmay_system_allocator(), options
    ));
    MMAY_EXPECT(mmay_managed_string_concat(
        &heap, bytes, sizeof(bytes) - 1, NULL, 0, &result
    ) == MMAY_MANAGED_LIMIT_REACHED);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_string_concat_failures(void) {
    static const unsigned char left[] = "allocation";
    static const unsigned char right[] = "-sweep";
    size_t failure_index;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 128; failure_index += 1U) {
        MarimayFailAllocator state;
        MarimayManagedHeap heap;
        MarimayManagedReference result = mmay_managed_invalid_reference();
        MarimayManagedStatus status = MMAY_MANAGED_ALLOCATION_FAILED;
        bool initialized;
        memset(&heap, 0, sizeof(heap));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        initialized = mmay_test_managed_string_heap(
            &heap, mmay_fail_allocator(&state)
        );
        if (initialized) {
            status = mmay_managed_string_concat(
                &heap, left, sizeof(left) - 1,
                right, sizeof(right) - 1, &result
            );
            MMAY_EXPECT(status == MMAY_MANAGED_ALLOCATION_FAILED
                || status == MMAY_MANAGED_OK);
            mmay_managed_heap_dispose(&heap);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (initialized && status == MMAY_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_managed_string(void) {
    mmay_test_managed_string_concat_bytes();
    mmay_test_managed_string_concat_limit();
    mmay_test_managed_string_concat_failures();
}
