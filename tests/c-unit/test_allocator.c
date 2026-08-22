#include "test_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"

void mmay_test_allocator(void) {
    MarimayAllocator system = mmay_system_allocator();
    MarimayFailAllocator fail_state;
    MarimayAllocator failing;
    size_t result = 0;
    unsigned char *bytes = NULL;
    unsigned char *resized = NULL;

    MMAY_EXPECT(mmay_size_add(2, 3, &result));
    MMAY_EXPECT(result == 5);
    MMAY_EXPECT(!mmay_size_add(SIZE_MAX, 1, &result));
    MMAY_EXPECT(mmay_size_multiply(7, 9, &result));
    MMAY_EXPECT(result == 63);
    MMAY_EXPECT(!mmay_size_multiply(SIZE_MAX, 2, &result));
    MMAY_EXPECT(mmay_allocate_array(&system, SIZE_MAX, 2) == NULL);
    MMAY_EXPECT(mmay_allocate(&system, 0) == NULL);

    bytes = mmay_allocate(&system, 8);
    MMAY_EXPECT(bytes != NULL);
    if (bytes != NULL) {
        memset(bytes, 0x5a, 8);
        mmay_deallocate(&system, bytes, 8);
    }

    mmay_fail_allocator_init(&fail_state, system, 0);
    failing = mmay_fail_allocator(&fail_state);
    MMAY_EXPECT(mmay_allocate(&failing, 16) == NULL);
    MMAY_EXPECT(fail_state.attempts == 1);
    MMAY_EXPECT(fail_state.live_allocations == 0);
    MMAY_EXPECT(fail_state.live_bytes == 0);

    mmay_fail_allocator_init(&fail_state, system, 1);
    failing = mmay_fail_allocator(&fail_state);
    bytes = mmay_allocate(&failing, 8);
    MMAY_EXPECT(bytes != NULL);
    if (bytes != NULL) {
        memset(bytes, 0xa5, 8);
        resized = mmay_resize(&failing, bytes, 8, 16);
        MMAY_EXPECT(resized == NULL);
        MMAY_EXPECT(bytes[0] == 0xa5);
        MMAY_EXPECT(fail_state.live_allocations == 1);
        MMAY_EXPECT(fail_state.live_bytes == 8);
        mmay_deallocate(&failing, bytes, 8);
    }
    MMAY_EXPECT(fail_state.live_allocations == 0);
    MMAY_EXPECT(fail_state.live_bytes == 0);

    mmay_fail_allocator_init(&fail_state, system, SIZE_MAX);
    failing = mmay_fail_allocator(&fail_state);
    bytes = mmay_allocate(&failing, 8);
    MMAY_EXPECT(bytes != NULL);
    if (bytes != NULL) {
        resized = mmay_resize(&failing, bytes, 8, 24);
        MMAY_EXPECT(resized != NULL);
        if (resized != NULL) {
            MMAY_EXPECT(fail_state.live_allocations == 1);
            MMAY_EXPECT(fail_state.live_bytes == 24);
            bytes = resized;
            resized = mmay_resize(&failing, bytes, 24, 10);
            MMAY_EXPECT(resized != NULL);
            if (resized != NULL) {
                MMAY_EXPECT(fail_state.live_allocations == 1);
                MMAY_EXPECT(fail_state.live_bytes == 10);
                MMAY_EXPECT(mmay_resize(&failing, resized, 10, 0) == NULL);
            } else {
                mmay_deallocate(&failing, bytes, 24);
            }
        } else {
            mmay_deallocate(&failing, bytes, 8);
        }
    }
    MMAY_EXPECT(fail_state.live_allocations == 0);
    MMAY_EXPECT(fail_state.live_bytes == 0);
}
