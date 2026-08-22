#include "../test_support.h"

#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/runtime/managed.h"

#include "../../../runtime/common/managed_internal.h"

typedef struct RudaTestCapacityCase {
    size_t current;
    size_t minimum;
    size_t maximum;
    bool succeeds;
    size_t planned;
} RudaTestCapacityCase;

static RudaManagedCell ruda_test_capacity_cell(uint64_t bits) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static bool ruda_test_capacity_heap(
    RudaManagedHeap *heap,
    RudaAllocator allocator,
    size_t maximum_length,
    size_t maximum_bytes
) {
    RudaManagedHeapOptions options = ruda_managed_heap_default_options();
    options.max_objects = 64;
    options.max_array_length = maximum_length;
    options.max_live_bytes = maximum_bytes;
    options.collect_before_allocation = true;
    return ruda_managed_heap_init(heap, allocator, options);
}

static void ruda_test_capacity_planner(void) {
    static const RudaTestCapacityCase cases[] = {
        {0, 0, 16, true, 0},
        {0, 1, 16, true, 4},
        {0, 4, 16, true, 4},
        {0, 5, 16, true, 8},
        {3, 3, 16, true, 3},
        {3, 4, 16, true, 6},
        {5, 6, 16, true, 10},
        {8, 9, 12, true, 12},
        {12, 13, 12, false, 0},
        {13, 12, 12, false, 0},
        {SIZE_MAX / 2 + 1, SIZE_MAX, SIZE_MAX, true, SIZE_MAX}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        size_t planned = 99;
        bool succeeds = ruda_managed_array_plan_capacity(
            cases[index].current,
            cases[index].minimum,
            cases[index].maximum,
            &planned
        );
        RUDA_EXPECT(succeeds == cases[index].succeeds);
        RUDA_EXPECT(planned == cases[index].planned);
    }
    RUDA_EXPECT(!ruda_managed_array_plan_capacity(0, 1, 8, NULL));
}

static void ruda_test_capacity_exact_construction(void) {
    static const size_t counts[] = {0, 1, 3, 4, 5, 8, 16};
    RudaManagedCell cells[16];
    size_t case_index = 0;
    size_t cell_index = 0;
    for (cell_index = 0; cell_index < 16; cell_index += 1) {
        cells[cell_index] = ruda_test_capacity_cell((uint64_t)cell_index + 1);
    }
    for (case_index = 0;
        case_index < sizeof(counts) / sizeof(counts[0]);
        case_index += 1) {
        RudaFailAllocator state;
        RudaManagedHeap heap;
        RudaManagedReference array = ruda_managed_invalid_reference();
        RudaManagedSlot *slot = NULL;
        size_t length = 99;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_capacity_heap(
            &heap, ruda_fail_allocator(&state), 32, 64U * 1024U
        ));
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, &ruda_managed_array_f64_descriptor,
            cells, counts[case_index], &array
        ) == RUDA_MANAGED_OK);
        slot = ruda_managed_resolve(&heap, array);
        RUDA_EXPECT(slot != NULL && slot->as.array != NULL);
        RUDA_EXPECT(slot->as.array->capacity == counts[case_index]);
        RUDA_EXPECT(slot->as.array->length == counts[case_index]);
        RUDA_EXPECT(ruda_managed_array_length(&heap, array, &length)
            == RUDA_MANAGED_OK && length == counts[case_index]);
        ruda_managed_heap_dispose(&heap);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
}

static void ruda_test_capacity_reserve_and_growth(void) {
    RudaFailAllocator state;
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&array};
    RudaManagedRootFrame roots;
    size_t attempts = 0;
    size_t index = 0;
    memset(&roots, 0, sizeof(roots));
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    RUDA_EXPECT(ruda_test_capacity_heap(
        &heap, ruda_fail_allocator(&state), 32, 64U * 1024U
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, NULL, 0, &array
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(&heap, &roots, locations, 1)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, 5)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_resolve(&heap, array)->as.array->capacity == 8);
    attempts = state.attempts;
    for (index = 0; index < 8; index += 1) {
        RUDA_EXPECT(ruda_managed_array_push(
            &heap, array, ruda_test_capacity_cell((uint64_t)index)
        ) == RUDA_MANAGED_OK);
    }
    RUDA_EXPECT(state.attempts == attempts);
    RUDA_EXPECT(ruda_managed_array_push(
        &heap, array, ruda_test_capacity_cell(8)
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(state.attempts == attempts + 1);
    RUDA_EXPECT(ruda_managed_resolve(&heap, array)->as.array->capacity == 16);
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &roots) == RUDA_MANAGED_OK);
    ruda_managed_heap_dispose(&heap);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);
}

static void ruda_test_capacity_failure_atomic(void) {
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 8 && !reached_success;
        failure_offset += 1) {
        RudaFailAllocator state;
        RudaManagedHeap heap;
        RudaManagedReference array = ruda_managed_invalid_reference();
        RudaManagedReference *locations[] = {&array};
        RudaManagedRootFrame roots;
        RudaManagedStatus status;
        RudaManagedArray *resolved = NULL;
        size_t before_bytes = 0;
        memset(&roots, 0, sizeof(roots));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_capacity_heap(
            &heap, ruda_fail_allocator(&state), 32, 64U * 1024U
        ));
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, &ruda_managed_array_f64_descriptor, NULL, 0, &array
        ) == RUDA_MANAGED_OK);
        RUDA_EXPECT(ruda_managed_root_push(&heap, &roots, locations, 1)
            == RUDA_MANAGED_OK);
        before_bytes = ruda_managed_heap_live_bytes(&heap);
        state.failure_index = state.attempts + failure_offset;
        status = ruda_managed_array_reserve(&heap, array, 9);
        resolved = ruda_managed_resolve(&heap, array)->as.array;
        RUDA_EXPECT(status == RUDA_MANAGED_OK
            || status == RUDA_MANAGED_ALLOCATION_FAILED);
        if (status == RUDA_MANAGED_OK) {
            reached_success = true;
            RUDA_EXPECT(resolved->capacity == 16);
        } else {
            RUDA_EXPECT(resolved->capacity == 0 && resolved->length == 0);
            RUDA_EXPECT(ruda_managed_heap_live_bytes(&heap) == before_bytes);
        }
        RUDA_EXPECT(ruda_managed_root_pop(&heap, &roots) == RUDA_MANAGED_OK);
        ruda_managed_heap_dispose(&heap);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_managed_array_capacity(void) {
    ruda_test_capacity_planner();
    ruda_test_capacity_exact_construction();
    ruda_test_capacity_reserve_and_growth();
    ruda_test_capacity_failure_atomic();
}
