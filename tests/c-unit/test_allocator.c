#include "test_support.h"

#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"

void ruda_test_allocator(void) {
    RudaAllocator system = ruda_system_allocator();
    RudaFailAllocator fail_state;
    RudaAllocator failing;
    size_t result = 0;
    unsigned char *bytes = NULL;
    unsigned char *resized = NULL;

    RUDA_EXPECT(ruda_size_add(2, 3, &result));
    RUDA_EXPECT(result == 5);
    RUDA_EXPECT(!ruda_size_add(SIZE_MAX, 1, &result));
    RUDA_EXPECT(ruda_size_multiply(7, 9, &result));
    RUDA_EXPECT(result == 63);
    RUDA_EXPECT(!ruda_size_multiply(SIZE_MAX, 2, &result));
    RUDA_EXPECT(ruda_allocate_array(&system, SIZE_MAX, 2) == NULL);
    RUDA_EXPECT(ruda_allocate(&system, 0) == NULL);

    bytes = ruda_allocate(&system, 8);
    RUDA_EXPECT(bytes != NULL);
    if (bytes != NULL) {
        memset(bytes, 0x5a, 8);
        ruda_deallocate(&system, bytes, 8);
    }

    ruda_fail_allocator_init(&fail_state, system, 0);
    failing = ruda_fail_allocator(&fail_state);
    RUDA_EXPECT(ruda_allocate(&failing, 16) == NULL);
    RUDA_EXPECT(fail_state.attempts == 1);
    RUDA_EXPECT(fail_state.live_allocations == 0);
    RUDA_EXPECT(fail_state.live_bytes == 0);

    ruda_fail_allocator_init(&fail_state, system, 1);
    failing = ruda_fail_allocator(&fail_state);
    bytes = ruda_allocate(&failing, 8);
    RUDA_EXPECT(bytes != NULL);
    if (bytes != NULL) {
        memset(bytes, 0xa5, 8);
        resized = ruda_resize(&failing, bytes, 8, 16);
        RUDA_EXPECT(resized == NULL);
        RUDA_EXPECT(bytes[0] == 0xa5);
        RUDA_EXPECT(fail_state.live_allocations == 1);
        RUDA_EXPECT(fail_state.live_bytes == 8);
        ruda_deallocate(&failing, bytes, 8);
    }
    RUDA_EXPECT(fail_state.live_allocations == 0);
    RUDA_EXPECT(fail_state.live_bytes == 0);

    ruda_fail_allocator_init(&fail_state, system, SIZE_MAX);
    failing = ruda_fail_allocator(&fail_state);
    bytes = ruda_allocate(&failing, 8);
    RUDA_EXPECT(bytes != NULL);
    if (bytes != NULL) {
        resized = ruda_resize(&failing, bytes, 8, 24);
        RUDA_EXPECT(resized != NULL);
        if (resized != NULL) {
            RUDA_EXPECT(fail_state.live_allocations == 1);
            RUDA_EXPECT(fail_state.live_bytes == 24);
            RUDA_EXPECT(ruda_resize(&failing, resized, 24, 0) == NULL);
        } else {
            ruda_deallocate(&failing, bytes, 8);
        }
    }
    RUDA_EXPECT(fail_state.live_allocations == 0);
    RUDA_EXPECT(fail_state.live_bytes == 0);
}
