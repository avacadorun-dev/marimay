#include "../test_support.h"

#include <limits.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/runtime/managed.h"

static const RudaManagedArrayDescriptor ruda_test_resource_recursive = {
    RUDA_MANAGED_CELL_REFERENCE,
    RUDA_MANAGED_OBJECT_ARRAY,
    &ruda_test_resource_recursive
};

static RudaManagedCell ruda_test_resource_raw(uint64_t bits) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static RudaManagedCell ruda_test_resource_reference(
    RudaManagedReference reference
) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.reference = reference;
    return cell;
}

static bool ruda_test_resource_init(
    RudaManagedHeap *heap,
    RudaAllocator allocator,
    size_t max_live_bytes
) {
    RudaManagedHeapOptions options = ruda_managed_heap_default_options();
    options.max_objects = 32;
    options.max_live_bytes = max_live_bytes;
    options.max_array_length = 64;
    return ruda_managed_heap_init(heap, allocator, options);
}

static void ruda_test_resource_default_and_exact_boundary(void) {
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedResourceFailure failure;
    RudaManagedCell cells[] = {
        ruda_test_resource_raw(UINT64_C(1)),
        ruda_test_resource_raw(UINT64_C(2))
    };
    size_t exact = 0;
    RudaManagedHeapOptions defaults = ruda_managed_heap_default_options();
    RUDA_EXPECT(defaults.max_live_bytes == 67108864U);
    RUDA_EXPECT(ruda_test_resource_init(
        &heap, ruda_system_allocator(), 1
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, cells, 2, &array
    ) == RUDA_MANAGED_LIMIT_REACHED);
    memset(&failure, 0, sizeof(failure));
    RUDA_EXPECT(ruda_managed_heap_last_resource_failure(&heap, &failure));
    RUDA_EXPECT(failure.classification
        == RUDA_MANAGED_RESOURCE_FAILURE_BUDGET);
    RUDA_EXPECT(failure.live_bytes == 0 && failure.requested_bytes > 1
        && failure.limit_bytes == 1);
    exact = failure.requested_bytes;
    ruda_managed_heap_dispose(&heap);

    RUDA_EXPECT(ruda_test_resource_init(
        &heap, ruda_system_allocator(), exact
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, cells, 2, &array
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_bytes(&heap) == exact);
    RUDA_EXPECT(!ruda_managed_heap_last_resource_failure(&heap, &failure));
    ruda_managed_heap_dispose(&heap);

    RUDA_EXPECT(ruda_test_resource_init(
        &heap, ruda_system_allocator(), exact - 1
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, cells, 2, &array
    ) == RUDA_MANAGED_LIMIT_REACHED);
    RUDA_EXPECT(ruda_managed_heap_last_resource_failure(&heap, &failure));
    RUDA_EXPECT(failure.requested_bytes == exact
        && failure.limit_bytes == exact - 1);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_resource_growth_and_semantic_precedence(void) {
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedResourceFailure failure;
    RudaManagedCell initial = ruda_test_resource_raw(UINT64_C(7));
    size_t live = 0;
    size_t growth = 4 * sizeof(RudaManagedCell) - sizeof(RudaManagedCell);
    RUDA_EXPECT(ruda_test_resource_init(
        &heap, ruda_system_allocator(), 64U * 1024U
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, &initial, 1, &array
    ) == RUDA_MANAGED_OK);
    live = ruda_managed_heap_live_bytes(&heap);
    heap.options.max_live_bytes = live + growth;
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, 3)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_bytes(&heap) == live + growth);
    ruda_managed_heap_dispose(&heap);

    RUDA_EXPECT(ruda_test_resource_init(
        &heap, ruda_system_allocator(), 64U * 1024U
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, &initial, 1, &array
    ) == RUDA_MANAGED_OK);
    live = ruda_managed_heap_live_bytes(&heap);
    heap.options.max_live_bytes = live + growth - 1;
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, 3)
        == RUDA_MANAGED_LIMIT_REACHED);
    RUDA_EXPECT(ruda_managed_heap_last_resource_failure(&heap, &failure));
    RUDA_EXPECT(failure.classification
        == RUDA_MANAGED_RESOURCE_FAILURE_BUDGET);
    RUDA_EXPECT(failure.live_bytes == live
        && failure.requested_bytes == growth
        && failure.limit_bytes == live + growth - 1);
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, INT64_MAX)
        == RUDA_MANAGED_LENGTH_LIMIT);
    RUDA_EXPECT(!ruda_managed_heap_last_resource_failure(&heap, &failure));
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_resource_budget_preflight_skips_allocator(void) {
    RudaFailAllocator state;
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedResourceFailure failure;
    RudaManagedCell cell = ruda_test_resource_raw(UINT64_C(1));
    ruda_fail_allocator_init(&state, ruda_system_allocator(), 0);
    RUDA_EXPECT(ruda_test_resource_init(
        &heap, ruda_fail_allocator(&state), 1
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, &cell, 1, &array
    ) == RUDA_MANAGED_LIMIT_REACHED);
    RUDA_EXPECT(state.attempts == 0);
    RUDA_EXPECT(ruda_managed_heap_last_resource_failure(&heap, &failure));
    RUDA_EXPECT(failure.classification
        == RUDA_MANAGED_RESOURCE_FAILURE_BUDGET);
    ruda_managed_heap_dispose(&heap);
    RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static void ruda_test_resource_reference_push_is_atomic(void) {
    RudaManagedHeap heap;
    RudaManagedReference child = ruda_managed_invalid_reference();
    RudaManagedReference outer = ruda_managed_invalid_reference();
    RudaManagedResourceFailure failure;
    RudaManagedCell child_cell;
    size_t live = 0;
    size_t length = 99;
    RUDA_EXPECT(ruda_test_resource_init(
        &heap, ruda_system_allocator(), 64U * 1024U
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_resource_recursive, NULL, 0, &child
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_resource_recursive, NULL, 0, &outer
    ) == RUDA_MANAGED_OK);
    live = ruda_managed_heap_live_bytes(&heap);
    heap.options.max_live_bytes = live + 4 * sizeof(RudaManagedCell) - 1;
    child_cell = ruda_test_resource_reference(child);
    RUDA_EXPECT(ruda_managed_array_push(&heap, outer, child_cell)
        == RUDA_MANAGED_LIMIT_REACHED);
    RUDA_EXPECT(ruda_managed_heap_last_resource_failure(&heap, &failure));
    RUDA_EXPECT(failure.classification
        == RUDA_MANAGED_RESOURCE_FAILURE_BUDGET);
    RUDA_EXPECT(failure.live_bytes == live
        && failure.requested_bytes == 4 * sizeof(RudaManagedCell));
    RUDA_EXPECT(ruda_managed_array_length(&heap, outer, &length)
        == RUDA_MANAGED_OK && length == 0);
    RUDA_EXPECT(ruda_managed_array_length(&heap, child, &length)
        == RUDA_MANAGED_OK && length == 0);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_resource_host_failure_sweep(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        RudaFailAllocator state;
        RudaManagedHeap heap;
        RudaManagedReference array = ruda_managed_invalid_reference();
        RudaManagedResourceFailure failure;
        RudaManagedCell cells[] = {
            ruda_test_resource_raw(UINT64_C(1)),
            ruda_test_resource_raw(UINT64_C(2))
        };
        RudaManagedStatus result;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        RUDA_EXPECT(ruda_test_resource_init(
            &heap, ruda_fail_allocator(&state), 64U * 1024U
        ));
        result = ruda_managed_array_new(
            &heap, &ruda_managed_array_f64_descriptor, cells, 2, &array
        );
        if (result == RUDA_MANAGED_ALLOCATION_FAILED) {
            RUDA_EXPECT(ruda_managed_heap_last_resource_failure(
                &heap, &failure
            ));
            RUDA_EXPECT(failure.classification
                == RUDA_MANAGED_RESOURCE_FAILURE_HOST_ALLOCATION);
            RUDA_EXPECT(failure.requested_bytes > 0);
            RUDA_EXPECT(ruda_managed_reference_is_invalid(array));
        } else {
            RUDA_EXPECT(result == RUDA_MANAGED_OK);
            reached_success = result == RUDA_MANAGED_OK;
        }
        ruda_managed_heap_dispose(&heap);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_resource_reserve_host_failure(void) {
    RudaFailAllocator state;
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedResourceFailure failure;
    size_t length = 99;
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    RUDA_EXPECT(ruda_test_resource_init(
        &heap, ruda_fail_allocator(&state), 64U * 1024U
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, NULL, 0, &array
    ) == RUDA_MANAGED_OK);
    state.failure_index = state.attempts;
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, 4)
        == RUDA_MANAGED_ALLOCATION_FAILED);
    RUDA_EXPECT(ruda_managed_heap_last_resource_failure(&heap, &failure));
    RUDA_EXPECT(failure.classification
        == RUDA_MANAGED_RESOURCE_FAILURE_HOST_ALLOCATION);
    RUDA_EXPECT(failure.requested_bytes == 4 * sizeof(RudaManagedCell));
    RUDA_EXPECT(ruda_managed_array_length(&heap, array, &length)
        == RUDA_MANAGED_OK && length == 0);
    ruda_managed_heap_dispose(&heap);
    RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

void ruda_test_managed_resource(void) {
    ruda_test_resource_default_and_exact_boundary();
    ruda_test_resource_growth_and_semantic_precedence();
    ruda_test_resource_budget_preflight_skips_allocator();
    ruda_test_resource_reference_push_is_atomic();
    ruda_test_resource_host_failure_sweep();
    ruda_test_resource_reserve_host_failure();
}
