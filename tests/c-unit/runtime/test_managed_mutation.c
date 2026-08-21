#include "../test_support.h"

#include <limits.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/runtime/managed.h"

#include "../../../runtime/common/managed_internal.h"

static const RudaManagedArrayDescriptor ruda_test_mutation_recursive = {
    RUDA_MANAGED_CELL_REFERENCE,
    RUDA_MANAGED_OBJECT_ARRAY,
    &ruda_test_mutation_recursive
};

static RudaManagedCell ruda_test_mutation_raw(uint64_t bits) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static RudaManagedCell ruda_test_mutation_reference(
    RudaManagedReference reference
) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.reference = reference;
    return cell;
}

static bool ruda_test_mutation_heap_init(
    RudaManagedHeap *heap,
    RudaAllocator allocator,
    bool collect_before_allocation
) {
    RudaManagedHeapOptions options = ruda_managed_heap_default_options();
    options.max_objects = 32;
    options.max_live_bytes = 64U * 1024U;
    options.max_array_length = 32;
    options.collect_before_allocation = collect_before_allocation;
    return ruda_managed_heap_init(heap, allocator, options);
}

static void ruda_test_mutation_raw_operations(void) {
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedReference stale = ruda_managed_invalid_reference();
    RudaManagedCell initial[] = {
        ruda_test_mutation_raw(UINT64_C(1)),
        ruda_test_mutation_raw(UINT64_C(2)),
        ruda_test_mutation_raw(UINT64_C(3))
    };
    RudaManagedCell result;
    RudaManagedCell observed;
    uint64_t generation = 99;
    size_t length = 99;
    bool popped = true;
    RUDA_EXPECT(ruda_test_mutation_heap_init(
        &heap, ruda_system_allocator(), true
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, initial, 3, &array
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_mutation_generation(
        &heap, array, &generation
    ) == RUDA_MANAGED_OK && generation == 0);
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, 0) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, -1)
        == RUDA_MANAGED_NEGATIVE_ADDITIONAL);
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, INT64_MAX)
        == RUDA_MANAGED_LENGTH_LIMIT);
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, 5) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_mutation_generation(
        &heap, array, &generation
    ) == RUDA_MANAGED_OK && generation == 0);
    RUDA_EXPECT(ruda_managed_array_set(
        &heap, array, 1, ruda_test_mutation_raw(UINT64_C(22)), &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_check_mutation_generation(
        &heap, array, 0
    ) == RUDA_MANAGED_ITERATOR_INVALIDATED);
    RUDA_EXPECT(ruda_managed_array_mutation_generation(
        &heap, array, &generation
    ) == RUDA_MANAGED_OK && generation == 1);
    RUDA_EXPECT(ruda_managed_array_pop(&heap, array, &result, &popped)
        == RUDA_MANAGED_OK && popped && result.f64_bits == UINT64_C(3));
    RUDA_EXPECT(ruda_managed_array_length(&heap, array, &length)
        == RUDA_MANAGED_OK && length == 2);
    RUDA_EXPECT(ruda_managed_array_truncate(&heap, array, 2) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_truncate(&heap, array, -1)
        == RUDA_MANAGED_TRUNCATE_INVALID_LENGTH);
    RUDA_EXPECT(ruda_managed_array_truncate(&heap, array, 3)
        == RUDA_MANAGED_TRUNCATE_INVALID_LENGTH);
    RUDA_EXPECT(ruda_managed_array_truncate(&heap, array, 1) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_get(&heap, array, 0, &observed, &length)
        == RUDA_MANAGED_OK && observed.f64_bits == UINT64_C(1));
    RUDA_EXPECT(ruda_managed_array_clear(&heap, array) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_clear(&heap, array) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_pop(&heap, array, &result, &popped)
        == RUDA_MANAGED_OK && !popped);
    RUDA_EXPECT(ruda_managed_array_mutation_generation(
        &heap, array, &generation
    ) == RUDA_MANAGED_OK && generation == 4);
    stale = array;
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_length(&heap, stale, &length)
        == RUDA_MANAGED_INVALID_REFERENCE);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_mutation_reference_release(void) {
    RudaManagedHeap heap;
    RudaManagedReference child = ruda_managed_invalid_reference();
    RudaManagedReference outer = ruda_managed_invalid_reference();
    RudaManagedReference popped_root = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&outer, &popped_root};
    RudaManagedRootFrame frame;
    RudaManagedCell child_cell;
    RudaManagedCell popped;
    bool did_pop = false;
    size_t length = 0;
    memset(&frame, 0, sizeof(frame));
    RUDA_EXPECT(ruda_test_mutation_heap_init(
        &heap, ruda_system_allocator(), true
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_mutation_recursive, NULL, 0, &child
    ) == RUDA_MANAGED_OK);
    child_cell = ruda_test_mutation_reference(child);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_mutation_recursive, &child_cell, 1, &outer
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(&heap, &frame, locations, 2)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_pop(&heap, outer, &popped, &did_pop)
        == RUDA_MANAGED_OK && did_pop);
    popped_root = popped.reference;
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_length(&heap, child, &length)
        == RUDA_MANAGED_OK && length == 0);
    popped_root = ruda_managed_invalid_reference();
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_length(&heap, child, &length)
        == RUDA_MANAGED_INVALID_REFERENCE);
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_mutation_generation_exhaustion(void) {
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedArray *private_array = NULL;
    RudaManagedCell value = ruda_test_mutation_raw(UINT64_C(8));
    size_t length = 0;
    RUDA_EXPECT(ruda_test_mutation_heap_init(
        &heap, ruda_system_allocator(), false
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, NULL, 0, &array
    ) == RUDA_MANAGED_OK);
    private_array = ruda_managed_resolve(&heap, array)->as.array;
    private_array->mutation_generation = UINT64_MAX;
    RUDA_EXPECT(ruda_managed_array_push(&heap, array, value)
        == RUDA_MANAGED_LIMIT_REACHED);
    RUDA_EXPECT(ruda_managed_array_length(&heap, array, &length)
        == RUDA_MANAGED_OK && length == 0);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_mutation_reference_truncate_release(void) {
    RudaManagedHeap heap;
    RudaManagedReference first = ruda_managed_invalid_reference();
    RudaManagedReference second = ruda_managed_invalid_reference();
    RudaManagedReference outer = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&outer};
    RudaManagedRootFrame frame;
    RudaManagedCell children[2];
    size_t length = 0;
    memset(&frame, 0, sizeof(frame));
    RUDA_EXPECT(ruda_test_mutation_heap_init(
        &heap, ruda_system_allocator(), false
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_mutation_recursive, NULL, 0, &first
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_mutation_recursive, NULL, 0, &second
    ) == RUDA_MANAGED_OK);
    children[0] = ruda_test_mutation_reference(first);
    children[1] = ruda_test_mutation_reference(second);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_mutation_recursive, children, 2, &outer
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(&heap, &frame, locations, 1)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_truncate(&heap, outer, 1)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_length(&heap, first, &length)
        == RUDA_MANAGED_OK && length == 0);
    RUDA_EXPECT(ruda_managed_array_length(&heap, second, &length)
        == RUDA_MANAGED_INVALID_REFERENCE);
    RUDA_EXPECT(ruda_managed_array_clear(&heap, outer) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_length(&heap, first, &length)
        == RUDA_MANAGED_INVALID_REFERENCE);
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_mutation_reserve_capacity_guarantee(void) {
    RudaFailAllocator state;
    RudaAllocator allocator;
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&array};
    RudaManagedRootFrame frame;
    size_t attempts_after_reserve = 0;
    size_t index = 0;
    memset(&frame, 0, sizeof(frame));
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    allocator = ruda_fail_allocator(&state);
    RUDA_EXPECT(ruda_test_mutation_heap_init(&heap, allocator, true));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, NULL, 0, &array
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(&heap, &frame, locations, 1)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, 8) == RUDA_MANAGED_OK);
    attempts_after_reserve = state.attempts;
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, 4) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, 8) == RUDA_MANAGED_OK);
    RUDA_EXPECT(state.attempts == attempts_after_reserve);
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, 9) == RUDA_MANAGED_OK);
    RUDA_EXPECT(state.attempts > attempts_after_reserve);
    attempts_after_reserve = state.attempts;
    for (index = 0; index < 8; index += 1) {
        RUDA_EXPECT(ruda_managed_array_push(
            &heap, array, ruda_test_mutation_raw((uint64_t)index)
        ) == RUDA_MANAGED_OK);
    }
    RUDA_EXPECT(state.attempts == attempts_after_reserve);
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
    ruda_managed_heap_dispose(&heap);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);
}

static void ruda_test_mutation_reserve_failure_sweep(void) {
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 8; failure_offset += 1) {
        RudaFailAllocator state;
        RudaManagedHeap heap;
        RudaManagedReference array = ruda_managed_invalid_reference();
        RudaManagedReference *locations[] = {&array};
        RudaManagedRootFrame frame;
        RudaAllocator allocator;
        RudaManagedStatus status;
        uint64_t generation = 99;
        size_t length = 99;
        size_t capacity_before = 0;
        memset(&frame, 0, sizeof(frame));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_test_mutation_heap_init(&heap, allocator, true));
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, &ruda_managed_array_f64_descriptor, NULL, 0, &array
        ) == RUDA_MANAGED_OK);
        RUDA_EXPECT(ruda_managed_root_push(&heap, &frame, locations, 1)
            == RUDA_MANAGED_OK);
        capacity_before = ruda_managed_resolve(&heap, array)->as.array->capacity;
        state.failure_index = state.attempts + failure_offset;
        status = ruda_managed_array_reserve(&heap, array, 8);
        RUDA_EXPECT(status == RUDA_MANAGED_OK
            || status == RUDA_MANAGED_ALLOCATION_FAILED);
        RUDA_EXPECT(ruda_managed_array_length(&heap, array, &length)
            == RUDA_MANAGED_OK && length == 0);
        RUDA_EXPECT(ruda_managed_array_mutation_generation(
            &heap, array, &generation
        ) == RUDA_MANAGED_OK && generation == 0);
        if (status != RUDA_MANAGED_OK) {
            RUDA_EXPECT(
                ruda_managed_resolve(&heap, array)->as.array->capacity
                    == capacity_before
            );
        }
        RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
        ruda_managed_heap_dispose(&heap);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (status == RUDA_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_mutation_reference_push_failure_sweep(void) {
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 8; failure_offset += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaManagedHeap heap;
        RudaManagedReference container = ruda_managed_invalid_reference();
        RudaManagedReference child = ruda_managed_invalid_reference();
        RudaManagedReference *locations[] = {&container, &child};
        RudaManagedRootFrame frame;
        RudaManagedCell child_cell;
        RudaManagedStatus status;
        size_t length = 99;
        uint64_t generation = 99;
        memset(&frame, 0, sizeof(frame));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_test_mutation_heap_init(&heap, allocator, true));
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, &ruda_test_mutation_recursive, NULL, 0, &container
        ) == RUDA_MANAGED_OK);
        RUDA_EXPECT(ruda_managed_root_push(&heap, &frame, locations, 2)
            == RUDA_MANAGED_OK);
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, &ruda_test_mutation_recursive, NULL, 0, &child
        ) == RUDA_MANAGED_OK);
        child_cell = ruda_test_mutation_reference(child);
        state.failure_index = state.attempts + failure_offset;
        status = ruda_managed_array_push(&heap, container, child_cell);
        RUDA_EXPECT(status == RUDA_MANAGED_OK
            || status == RUDA_MANAGED_ALLOCATION_FAILED);
        RUDA_EXPECT(ruda_managed_array_length(&heap, container, &length)
            == RUDA_MANAGED_OK && length == (status == RUDA_MANAGED_OK ? 1 : 0));
        RUDA_EXPECT(ruda_managed_array_mutation_generation(
            &heap, container, &generation
        ) == RUDA_MANAGED_OK && generation == (status == RUDA_MANAGED_OK ? 1 : 0));
        RUDA_EXPECT(ruda_managed_array_length(&heap, child, &length)
            == RUDA_MANAGED_OK && length == 0);
        RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
        ruda_managed_heap_dispose(&heap);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (status == RUDA_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_managed_mutation(void) {
    ruda_test_mutation_raw_operations();
    ruda_test_mutation_reference_release();
    ruda_test_mutation_reference_truncate_release();
    ruda_test_mutation_generation_exhaustion();
    ruda_test_mutation_reserve_capacity_guarantee();
    ruda_test_mutation_reserve_failure_sweep();
    ruda_test_mutation_reference_push_failure_sweep();
}
