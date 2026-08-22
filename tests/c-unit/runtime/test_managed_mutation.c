#include "../test_support.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"

#include "../../../runtime/common/managed_internal.h"

static const MarimayManagedArrayDescriptor mmay_test_mutation_recursive = {
    MMAY_MANAGED_CELL_REFERENCE,
    MMAY_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(MarimayManagedCell),
    MMAY_MANAGED_OBJECT_ARRAY,
    &mmay_test_mutation_recursive
};

static MarimayManagedCell mmay_test_mutation_raw(uint64_t bits) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static MarimayManagedCell mmay_test_mutation_reference(
    MarimayManagedReference reference
) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.reference = reference;
    return cell;
}

static bool mmay_test_mutation_heap_init(
    MarimayManagedHeap *heap,
    MarimayAllocator allocator,
    bool collect_before_allocation
) {
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    options.max_objects = 32;
    options.max_live_bytes = 64U * 1024U;
    options.max_array_length = 32;
    options.collect_before_allocation = collect_before_allocation;
    return mmay_managed_heap_init(heap, allocator, options);
}

static void mmay_test_mutation_raw_operations(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedReference stale = mmay_managed_invalid_reference();
    MarimayManagedCell initial[] = {
        mmay_test_mutation_raw(UINT64_C(1)),
        mmay_test_mutation_raw(UINT64_C(2)),
        mmay_test_mutation_raw(UINT64_C(3))
    };
    MarimayManagedCell result;
    MarimayManagedCell observed;
    uint64_t generation = 99;
    size_t length = 99;
    bool popped = true;
    MMAY_EXPECT(mmay_test_mutation_heap_init(
        &heap, mmay_system_allocator(), true
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, initial, 3, &array
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_mutation_generation(
        &heap, array, &generation
    ) == MMAY_MANAGED_OK && generation == 0);
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, 0) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, -1)
        == MMAY_MANAGED_NEGATIVE_ADDITIONAL);
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, INT64_MAX)
        == MMAY_MANAGED_LENGTH_LIMIT);
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, 5) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_mutation_generation(
        &heap, array, &generation
    ) == MMAY_MANAGED_OK && generation == 0);
    MMAY_EXPECT(mmay_managed_array_set(
        &heap, array, 1, mmay_test_mutation_raw(UINT64_C(22)), &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_check_mutation_generation(
        &heap, array, 0
    ) == MMAY_MANAGED_ITERATOR_INVALIDATED);
    MMAY_EXPECT(mmay_managed_array_mutation_generation(
        &heap, array, &generation
    ) == MMAY_MANAGED_OK && generation == 1);
    MMAY_EXPECT(mmay_managed_array_pop(&heap, array, &result, &popped)
        == MMAY_MANAGED_OK && popped && result.f64_bits == UINT64_C(3));
    MMAY_EXPECT(mmay_managed_array_length(&heap, array, &length)
        == MMAY_MANAGED_OK && length == 2);
    MMAY_EXPECT(mmay_managed_array_truncate(&heap, array, 2) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_truncate(&heap, array, -1)
        == MMAY_MANAGED_TRUNCATE_INVALID_LENGTH);
    MMAY_EXPECT(mmay_managed_array_truncate(&heap, array, 3)
        == MMAY_MANAGED_TRUNCATE_INVALID_LENGTH);
    MMAY_EXPECT(mmay_managed_array_truncate(&heap, array, 1) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_get(&heap, array, 0, &observed, &length)
        == MMAY_MANAGED_OK && observed.f64_bits == UINT64_C(1));
    MMAY_EXPECT(mmay_managed_array_clear(&heap, array) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_clear(&heap, array) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_pop(&heap, array, &result, &popped)
        == MMAY_MANAGED_OK && !popped);
    MMAY_EXPECT(mmay_managed_array_mutation_generation(
        &heap, array, &generation
    ) == MMAY_MANAGED_OK && generation == 4);
    stale = array;
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_length(&heap, stale, &length)
        == MMAY_MANAGED_INVALID_REFERENCE);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_mutation_reference_release(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference child = mmay_managed_invalid_reference();
    MarimayManagedReference outer = mmay_managed_invalid_reference();
    MarimayManagedReference popped_root = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&outer, &popped_root};
    MarimayManagedRootFrame frame;
    MarimayManagedCell child_cell;
    MarimayManagedCell popped;
    bool did_pop = false;
    size_t length = 0;
    memset(&frame, 0, sizeof(frame));
    MMAY_EXPECT(mmay_test_mutation_heap_init(
        &heap, mmay_system_allocator(), true
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_mutation_recursive, NULL, 0, &child
    ) == MMAY_MANAGED_OK);
    child_cell = mmay_test_mutation_reference(child);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_mutation_recursive, &child_cell, 1, &outer
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, locations, 2)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_pop(&heap, outer, &popped, &did_pop)
        == MMAY_MANAGED_OK && did_pop);
    popped_root = popped.reference;
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_length(&heap, child, &length)
        == MMAY_MANAGED_OK && length == 0);
    popped_root = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_length(&heap, child, &length)
        == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_mutation_generation_exhaustion(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedArray *private_array = NULL;
    MarimayManagedCell value = mmay_test_mutation_raw(UINT64_C(8));
    size_t length = 0;
    MMAY_EXPECT(mmay_test_mutation_heap_init(
        &heap, mmay_system_allocator(), false
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, NULL, 0, &array
    ) == MMAY_MANAGED_OK);
    private_array = mmay_managed_resolve(&heap, array)->as.array;
    private_array->mutation_generation = UINT64_MAX;
    MMAY_EXPECT(mmay_managed_array_push(&heap, array, value)
        == MMAY_MANAGED_LIMIT_REACHED);
    MMAY_EXPECT(mmay_managed_array_length(&heap, array, &length)
        == MMAY_MANAGED_OK && length == 0);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_mutation_reference_truncate_release(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference first = mmay_managed_invalid_reference();
    MarimayManagedReference second = mmay_managed_invalid_reference();
    MarimayManagedReference outer = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&outer};
    MarimayManagedRootFrame frame;
    MarimayManagedCell children[2];
    size_t length = 0;
    memset(&frame, 0, sizeof(frame));
    MMAY_EXPECT(mmay_test_mutation_heap_init(
        &heap, mmay_system_allocator(), false
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_mutation_recursive, NULL, 0, &first
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_mutation_recursive, NULL, 0, &second
    ) == MMAY_MANAGED_OK);
    children[0] = mmay_test_mutation_reference(first);
    children[1] = mmay_test_mutation_reference(second);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_mutation_recursive, children, 2, &outer
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, locations, 1)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_truncate(&heap, outer, 1)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_length(&heap, first, &length)
        == MMAY_MANAGED_OK && length == 0);
    MMAY_EXPECT(mmay_managed_array_length(&heap, second, &length)
        == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(mmay_managed_array_clear(&heap, outer) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_length(&heap, first, &length)
        == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_mutation_reserve_capacity_guarantee(void) {
    MarimayFailAllocator state;
    MarimayAllocator allocator;
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&array};
    MarimayManagedRootFrame frame;
    size_t attempts_after_reserve = 0;
    size_t index = 0;
    memset(&frame, 0, sizeof(frame));
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    allocator = mmay_fail_allocator(&state);
    MMAY_EXPECT(mmay_test_mutation_heap_init(&heap, allocator, true));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, NULL, 0, &array
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, locations, 1)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, 8) == MMAY_MANAGED_OK);
    attempts_after_reserve = state.attempts;
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, 4) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, 8) == MMAY_MANAGED_OK);
    MMAY_EXPECT(state.attempts == attempts_after_reserve);
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, 9) == MMAY_MANAGED_OK);
    MMAY_EXPECT(state.attempts > attempts_after_reserve);
    attempts_after_reserve = state.attempts;
    for (index = 0; index < 8; index += 1) {
        MMAY_EXPECT(mmay_managed_array_push(
            &heap, array, mmay_test_mutation_raw((uint64_t)index)
        ) == MMAY_MANAGED_OK);
    }
    MMAY_EXPECT(state.attempts == attempts_after_reserve);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}

static void mmay_test_mutation_reserve_failure_sweep(void) {
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 8; failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayManagedHeap heap;
        MarimayManagedReference array = mmay_managed_invalid_reference();
        MarimayManagedReference *locations[] = {&array};
        MarimayManagedRootFrame frame;
        MarimayAllocator allocator;
        MarimayManagedStatus status;
        uint64_t generation = 99;
        size_t length = 99;
        size_t capacity_before = 0;
        memset(&frame, 0, sizeof(frame));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_test_mutation_heap_init(&heap, allocator, true));
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_managed_array_f64_descriptor, NULL, 0, &array
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, locations, 1)
            == MMAY_MANAGED_OK);
        capacity_before = mmay_managed_resolve(&heap, array)->as.array->capacity;
        state.failure_index = state.attempts + failure_offset;
        status = mmay_managed_array_reserve(&heap, array, 8);
        MMAY_EXPECT(status == MMAY_MANAGED_OK
            || status == MMAY_MANAGED_ALLOCATION_FAILED);
        MMAY_EXPECT(mmay_managed_array_length(&heap, array, &length)
            == MMAY_MANAGED_OK && length == 0);
        MMAY_EXPECT(mmay_managed_array_mutation_generation(
            &heap, array, &generation
        ) == MMAY_MANAGED_OK && generation == 0);
        if (status != MMAY_MANAGED_OK) {
            MMAY_EXPECT(
                mmay_managed_resolve(&heap, array)->as.array->capacity
                    == capacity_before
            );
        }
        MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (status == MMAY_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_mutation_reference_push_failure_sweep(void) {
    size_t failure_offset = 0;
    bool reached_success = false;
    for (failure_offset = 0; failure_offset < 8; failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayManagedHeap heap;
        MarimayManagedReference container = mmay_managed_invalid_reference();
        MarimayManagedReference child = mmay_managed_invalid_reference();
        MarimayManagedReference *locations[] = {&container, &child};
        MarimayManagedRootFrame frame;
        MarimayManagedCell child_cell;
        MarimayManagedStatus status;
        size_t length = 99;
        uint64_t generation = 99;
        memset(&frame, 0, sizeof(frame));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_test_mutation_heap_init(&heap, allocator, true));
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_test_mutation_recursive, NULL, 0, &container
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, locations, 2)
            == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_test_mutation_recursive, NULL, 0, &child
        ) == MMAY_MANAGED_OK);
        child_cell = mmay_test_mutation_reference(child);
        state.failure_index = state.attempts + failure_offset;
        status = mmay_managed_array_push(&heap, container, child_cell);
        MMAY_EXPECT(status == MMAY_MANAGED_OK
            || status == MMAY_MANAGED_ALLOCATION_FAILED);
        MMAY_EXPECT(mmay_managed_array_length(&heap, container, &length)
            == MMAY_MANAGED_OK && length == (status == MMAY_MANAGED_OK ? 1 : 0));
        MMAY_EXPECT(mmay_managed_array_mutation_generation(
            &heap, container, &generation
        ) == MMAY_MANAGED_OK && generation == (status == MMAY_MANAGED_OK ? 1 : 0));
        MMAY_EXPECT(mmay_managed_array_length(&heap, child, &length)
            == MMAY_MANAGED_OK && length == 0);
        MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (status == MMAY_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_managed_mutation(void) {
    mmay_test_mutation_raw_operations();
    mmay_test_mutation_reference_release();
    mmay_test_mutation_reference_truncate_release();
    mmay_test_mutation_generation_exhaustion();
    mmay_test_mutation_reserve_capacity_guarantee();
    mmay_test_mutation_reserve_failure_sweep();
    mmay_test_mutation_reference_push_failure_sweep();
}
