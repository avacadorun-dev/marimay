#include "../test_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"

#include "../../../runtime/common/managed_internal.h"

typedef struct MarimayTestCapacityCase {
    size_t current;
    size_t minimum;
    size_t maximum;
    bool succeeds;
    size_t planned;
} MarimayTestCapacityCase;

static MarimayManagedCell mmay_test_capacity_cell(uint64_t bits) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static bool mmay_test_capacity_heap(
    MarimayManagedHeap *heap,
    MarimayAllocator allocator,
    size_t maximum_length,
    size_t maximum_bytes
) {
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    options.max_objects = 64;
    options.max_array_length = maximum_length;
    options.max_live_bytes = maximum_bytes;
    options.collect_before_allocation = true;
    return mmay_managed_heap_init(heap, allocator, options);
}

static void mmay_test_capacity_planner(void) {
    static const MarimayTestCapacityCase cases[] = {
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
        bool succeeds = mmay_managed_array_plan_capacity(
            cases[index].current,
            cases[index].minimum,
            cases[index].maximum,
            &planned
        );
        MMAY_EXPECT(succeeds == cases[index].succeeds);
        MMAY_EXPECT(planned == cases[index].planned);
    }
    MMAY_EXPECT(!mmay_managed_array_plan_capacity(0, 1, 8, NULL));
}

static void mmay_test_capacity_exact_construction(void) {
    static const size_t counts[] = {0, 1, 3, 4, 5, 8, 16};
    MarimayManagedCell cells[16];
    size_t case_index = 0;
    size_t cell_index = 0;
    for (cell_index = 0; cell_index < 16; cell_index += 1) {
        cells[cell_index] = mmay_test_capacity_cell((uint64_t)cell_index + 1);
    }
    for (case_index = 0;
        case_index < sizeof(counts) / sizeof(counts[0]);
        case_index += 1) {
        MarimayFailAllocator state;
        MarimayManagedHeap heap;
        MarimayManagedReference array = mmay_managed_invalid_reference();
        MarimayManagedSlot *slot = NULL;
        size_t length = 99;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_capacity_heap(
            &heap, mmay_fail_allocator(&state), 32, 64U * 1024U
        ));
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_managed_array_f64_descriptor,
            cells, counts[case_index], &array
        ) == MMAY_MANAGED_OK);
        slot = mmay_managed_resolve(&heap, array);
        MMAY_EXPECT(slot != NULL && slot->as.array != NULL);
        MMAY_EXPECT(slot->as.array->capacity == counts[case_index]);
        MMAY_EXPECT(slot->as.array->length == counts[case_index]);
        MMAY_EXPECT(mmay_managed_array_length(&heap, array, &length)
            == MMAY_MANAGED_OK && length == counts[case_index]);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
}

static void mmay_test_capacity_reserve_and_growth(void) {
    MarimayFailAllocator state;
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&array};
    MarimayManagedRootFrame roots;
    size_t attempts = 0;
    size_t index = 0;
    memset(&roots, 0, sizeof(roots));
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_test_capacity_heap(
        &heap, mmay_fail_allocator(&state), 32, 64U * 1024U
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, NULL, 0, &array
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(&heap, &roots, locations, 1)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, 5)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_resolve(&heap, array)->as.array->capacity == 8);
    attempts = state.attempts;
    for (index = 0; index < 8; index += 1) {
        MMAY_EXPECT(mmay_managed_array_push(
            &heap, array, mmay_test_capacity_cell((uint64_t)index)
        ) == MMAY_MANAGED_OK);
    }
    MMAY_EXPECT(state.attempts == attempts);
    MMAY_EXPECT(mmay_managed_array_push(
        &heap, array, mmay_test_capacity_cell(8)
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(state.attempts == attempts + 1);
    MMAY_EXPECT(mmay_managed_resolve(&heap, array)->as.array->capacity == 16);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &roots) == MMAY_MANAGED_OK);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}

static void mmay_test_capacity_failure_atomic(void) {
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 8 && !reached_success;
        failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayManagedHeap heap;
        MarimayManagedReference array = mmay_managed_invalid_reference();
        MarimayManagedReference *locations[] = {&array};
        MarimayManagedRootFrame roots;
        MarimayManagedStatus status;
        MarimayManagedArray *resolved = NULL;
        size_t before_bytes = 0;
        memset(&roots, 0, sizeof(roots));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_capacity_heap(
            &heap, mmay_fail_allocator(&state), 32, 64U * 1024U
        ));
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_managed_array_f64_descriptor, NULL, 0, &array
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_root_push(&heap, &roots, locations, 1)
            == MMAY_MANAGED_OK);
        before_bytes = mmay_managed_heap_live_bytes(&heap);
        state.failure_index = state.attempts + failure_offset;
        status = mmay_managed_array_reserve(&heap, array, 9);
        resolved = mmay_managed_resolve(&heap, array)->as.array;
        MMAY_EXPECT(status == MMAY_MANAGED_OK
            || status == MMAY_MANAGED_ALLOCATION_FAILED);
        if (status == MMAY_MANAGED_OK) {
            reached_success = true;
            MMAY_EXPECT(resolved->capacity == 16);
        } else {
            MMAY_EXPECT(resolved->capacity == 0 && resolved->length == 0);
            MMAY_EXPECT(mmay_managed_heap_live_bytes(&heap) == before_bytes);
        }
        MMAY_EXPECT(mmay_managed_root_pop(&heap, &roots) == MMAY_MANAGED_OK);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_managed_array_capacity(void) {
    mmay_test_capacity_planner();
    mmay_test_capacity_exact_construction();
    mmay_test_capacity_reserve_and_growth();
    mmay_test_capacity_failure_atomic();
}
