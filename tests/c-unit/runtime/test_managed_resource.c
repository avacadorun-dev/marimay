#include "../test_support.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"

static const MarimayManagedArrayDescriptor mmay_test_resource_recursive = {
    MMAY_MANAGED_CELL_REFERENCE,
    MMAY_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(MarimayManagedCell),
    MMAY_MANAGED_OBJECT_ARRAY,
    &mmay_test_resource_recursive
};

static MarimayManagedCell mmay_test_resource_raw(uint64_t bits) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static MarimayManagedCell mmay_test_resource_reference(
    MarimayManagedReference reference
) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.reference = reference;
    return cell;
}

static bool mmay_test_resource_init(
    MarimayManagedHeap *heap,
    MarimayAllocator allocator,
    size_t max_live_bytes
) {
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    options.max_objects = 32;
    options.max_live_bytes = max_live_bytes;
    options.max_array_length = 64;
    return mmay_managed_heap_init(heap, allocator, options);
}

static void mmay_test_resource_default_and_exact_boundary(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedResourceFailure failure;
    MarimayManagedCell cells[] = {
        mmay_test_resource_raw(UINT64_C(1)),
        mmay_test_resource_raw(UINT64_C(2))
    };
    size_t exact = 0;
    MarimayManagedHeapOptions defaults = mmay_managed_heap_default_options();
    MMAY_EXPECT(defaults.max_live_bytes == 67108864U);
    MMAY_EXPECT(mmay_test_resource_init(
        &heap, mmay_system_allocator(), 1
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, cells, 2, &array
    ) == MMAY_MANAGED_LIMIT_REACHED);
    memset(&failure, 0, sizeof(failure));
    MMAY_EXPECT(mmay_managed_heap_last_resource_failure(&heap, &failure));
    MMAY_EXPECT(failure.classification
        == MMAY_MANAGED_RESOURCE_FAILURE_BUDGET);
    MMAY_EXPECT(failure.live_bytes == 0 && failure.requested_bytes > 1
        && failure.limit_bytes == 1);
    exact = failure.requested_bytes;
    mmay_managed_heap_dispose(&heap);

    MMAY_EXPECT(mmay_test_resource_init(
        &heap, mmay_system_allocator(), exact
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, cells, 2, &array
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_bytes(&heap) == exact);
    MMAY_EXPECT(!mmay_managed_heap_last_resource_failure(&heap, &failure));
    mmay_managed_heap_dispose(&heap);

    MMAY_EXPECT(mmay_test_resource_init(
        &heap, mmay_system_allocator(), exact - 1
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, cells, 2, &array
    ) == MMAY_MANAGED_LIMIT_REACHED);
    MMAY_EXPECT(mmay_managed_heap_last_resource_failure(&heap, &failure));
    MMAY_EXPECT(failure.requested_bytes == exact
        && failure.limit_bytes == exact - 1);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_resource_growth_and_semantic_precedence(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedResourceFailure failure;
    MarimayManagedCell initial = mmay_test_resource_raw(UINT64_C(7));
    size_t live = 0;
    size_t growth = 4 * sizeof(uint64_t) - sizeof(uint64_t);
    MMAY_EXPECT(mmay_test_resource_init(
        &heap, mmay_system_allocator(), 64U * 1024U
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, &initial, 1, &array
    ) == MMAY_MANAGED_OK);
    live = mmay_managed_heap_live_bytes(&heap);
    heap.options.max_live_bytes = live + growth;
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, 3)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_bytes(&heap) == live + growth);
    mmay_managed_heap_dispose(&heap);

    MMAY_EXPECT(mmay_test_resource_init(
        &heap, mmay_system_allocator(), 64U * 1024U
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, &initial, 1, &array
    ) == MMAY_MANAGED_OK);
    live = mmay_managed_heap_live_bytes(&heap);
    heap.options.max_live_bytes = live + growth - 1;
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, 3)
        == MMAY_MANAGED_LIMIT_REACHED);
    MMAY_EXPECT(mmay_managed_heap_last_resource_failure(&heap, &failure));
    MMAY_EXPECT(failure.classification
        == MMAY_MANAGED_RESOURCE_FAILURE_BUDGET);
    MMAY_EXPECT(failure.live_bytes == live
        && failure.requested_bytes == growth
        && failure.limit_bytes == live + growth - 1);
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, INT64_MAX)
        == MMAY_MANAGED_LENGTH_LIMIT);
    MMAY_EXPECT(!mmay_managed_heap_last_resource_failure(&heap, &failure));
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_resource_budget_preflight_skips_allocator(void) {
    MarimayFailAllocator state;
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedResourceFailure failure;
    MarimayManagedCell cell = mmay_test_resource_raw(UINT64_C(1));
    mmay_fail_allocator_init(&state, mmay_system_allocator(), 0);
    MMAY_EXPECT(mmay_test_resource_init(
        &heap, mmay_fail_allocator(&state), 1
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, &cell, 1, &array
    ) == MMAY_MANAGED_LIMIT_REACHED);
    MMAY_EXPECT(state.attempts == 0);
    MMAY_EXPECT(mmay_managed_heap_last_resource_failure(&heap, &failure));
    MMAY_EXPECT(failure.classification
        == MMAY_MANAGED_RESOURCE_FAILURE_BUDGET);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static void mmay_test_resource_reference_push_is_atomic(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference child = mmay_managed_invalid_reference();
    MarimayManagedReference outer = mmay_managed_invalid_reference();
    MarimayManagedResourceFailure failure;
    MarimayManagedCell child_cell;
    size_t live = 0;
    size_t length = 99;
    MMAY_EXPECT(mmay_test_resource_init(
        &heap, mmay_system_allocator(), 64U * 1024U
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_resource_recursive, NULL, 0, &child
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_resource_recursive, NULL, 0, &outer
    ) == MMAY_MANAGED_OK);
    live = mmay_managed_heap_live_bytes(&heap);
    heap.options.max_live_bytes = live + 4 * sizeof(MarimayManagedCell) - 1;
    child_cell = mmay_test_resource_reference(child);
    MMAY_EXPECT(mmay_managed_array_push(&heap, outer, child_cell)
        == MMAY_MANAGED_LIMIT_REACHED);
    MMAY_EXPECT(mmay_managed_heap_last_resource_failure(&heap, &failure));
    MMAY_EXPECT(failure.classification
        == MMAY_MANAGED_RESOURCE_FAILURE_BUDGET);
    MMAY_EXPECT(failure.live_bytes == live
        && failure.requested_bytes == 4 * sizeof(MarimayManagedCell));
    MMAY_EXPECT(mmay_managed_array_length(&heap, outer, &length)
        == MMAY_MANAGED_OK && length == 0);
    MMAY_EXPECT(mmay_managed_array_length(&heap, child, &length)
        == MMAY_MANAGED_OK && length == 0);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_resource_host_failure_sweep(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayManagedHeap heap;
        MarimayManagedReference array = mmay_managed_invalid_reference();
        MarimayManagedResourceFailure failure;
        MarimayManagedCell cells[] = {
            mmay_test_resource_raw(UINT64_C(1)),
            mmay_test_resource_raw(UINT64_C(2))
        };
        MarimayManagedStatus result;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        MMAY_EXPECT(mmay_test_resource_init(
            &heap, mmay_fail_allocator(&state), 64U * 1024U
        ));
        result = mmay_managed_array_new(
            &heap, &mmay_managed_array_f64_descriptor, cells, 2, &array
        );
        if (result == MMAY_MANAGED_ALLOCATION_FAILED) {
            MMAY_EXPECT(mmay_managed_heap_last_resource_failure(
                &heap, &failure
            ));
            MMAY_EXPECT(failure.classification
                == MMAY_MANAGED_RESOURCE_FAILURE_HOST_ALLOCATION);
            MMAY_EXPECT(failure.requested_bytes > 0);
            MMAY_EXPECT(mmay_managed_reference_is_invalid(array));
        } else {
            MMAY_EXPECT(result == MMAY_MANAGED_OK);
            reached_success = result == MMAY_MANAGED_OK;
        }
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_resource_reserve_host_failure(void) {
    MarimayFailAllocator state;
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedResourceFailure failure;
    size_t length = 99;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_test_resource_init(
        &heap, mmay_fail_allocator(&state), 64U * 1024U
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, NULL, 0, &array
    ) == MMAY_MANAGED_OK);
    state.failure_index = state.attempts;
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, 4)
        == MMAY_MANAGED_ALLOCATION_FAILED);
    MMAY_EXPECT(mmay_managed_heap_last_resource_failure(&heap, &failure));
    MMAY_EXPECT(failure.classification
        == MMAY_MANAGED_RESOURCE_FAILURE_HOST_ALLOCATION);
    MMAY_EXPECT(failure.requested_bytes == 4 * sizeof(uint64_t));
    MMAY_EXPECT(mmay_managed_array_length(&heap, array, &length)
        == MMAY_MANAGED_OK && length == 0);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

void mmay_test_managed_resource(void) {
    mmay_test_resource_default_and_exact_boundary();
    mmay_test_resource_growth_and_semantic_precedence();
    mmay_test_resource_budget_preflight_skips_allocator();
    mmay_test_resource_reference_push_is_atomic();
    mmay_test_resource_host_failure_sweep();
    mmay_test_resource_reserve_host_failure();
}
