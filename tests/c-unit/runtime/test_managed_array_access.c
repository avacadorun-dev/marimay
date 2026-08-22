#include "../test_support.h"

#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/runtime/managed.h"

static const RudaManagedArrayDescriptor ruda_test_access_strings = {
    RUDA_MANAGED_CELL_REFERENCE,
    RUDA_MANAGED_OBJECT_STRING,
    NULL
};

static RudaManagedCell ruda_test_access_raw(uint64_t bits) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static RudaManagedCell ruda_test_access_reference(
    RudaManagedReference reference
) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.reference = reference;
    return cell;
}

static bool ruda_test_access_heap_init(
    RudaManagedHeap *heap,
    RudaAllocator allocator,
    bool collect_before_allocation
) {
    RudaManagedHeapOptions options = ruda_managed_heap_default_options();
    options.max_objects = 32;
    options.max_live_bytes = 64U * 1024U;
    options.max_array_length = 64;
    options.collect_before_allocation = collect_before_allocation;
    return ruda_managed_heap_init(heap, allocator, options);
}

static void ruda_test_access_raw_alias_and_bounds(void) {
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedReference alias = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&array, &alias};
    RudaManagedRootFrame roots;
    RudaManagedCell initial[] = {
        ruda_test_access_raw(UINT64_C(11)),
        ruda_test_access_raw(UINT64_C(22))
    };
    RudaManagedCell observed = ruda_test_access_raw(UINT64_MAX);
    size_t length = SIZE_MAX;
    memset(&roots, 0, sizeof(roots));
    RUDA_EXPECT(ruda_test_access_heap_init(
        &heap, ruda_system_allocator(), true
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, initial, 2, &array
    ) == RUDA_MANAGED_OK);
    alias = array;
    RUDA_EXPECT(ruda_managed_root_push(
        &heap, &roots, locations, 2
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_access_length(
        &heap, alias, &ruda_managed_array_f64_descriptor, &length
    ) == RUDA_MANAGED_OK && length == 2);
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, alias, &ruda_managed_array_f64_descriptor, 1,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_OK && observed.f64_bits == UINT64_C(22)
        && length == 2);
    RUDA_EXPECT(ruda_managed_array_access_set(
        &heap, array, &ruda_managed_array_f64_descriptor, 0,
        RUDA_MANAGED_ARRAY_ACCESS_IN_BOUNDS,
        ruda_test_access_raw(UINT64_C(33)), &length
    ) == RUDA_MANAGED_OK && length == 2);
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, alias, &ruda_managed_array_f64_descriptor, 0,
        RUDA_MANAGED_ARRAY_ACCESS_IN_BOUNDS, &observed, &length
    ) == RUDA_MANAGED_OK && observed.f64_bits == UINT64_C(33));
    RUDA_EXPECT(ruda_managed_array_access_require(
        &heap, array, &ruda_managed_array_f64_descriptor, -1, &length
    ) == RUDA_MANAGED_BOUNDS && length == 2);
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, array, &ruda_managed_array_f64_descriptor, 2,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_BOUNDS && length == 2
        && observed.f64_bits == UINT64_C(0));
    RUDA_EXPECT(ruda_managed_array_access_set(
        &heap, array, &ruda_managed_array_f64_descriptor, INT64_MAX,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED,
        ruda_test_access_raw(UINT64_C(44)), &length
    ) == RUDA_MANAGED_BOUNDS && length == 2);
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, array, &ruda_managed_array_array_f64_descriptor, 0,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_TYPE_MISMATCH && length == 0);
    RUDA_EXPECT(ruda_managed_array_push(
        &heap, alias, ruda_test_access_raw(UINT64_C(44))
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, array, &ruda_managed_array_f64_descriptor, 2,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_OK && observed.f64_bits == UINT64_C(44)
        && length == 3);
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &roots) == RUDA_MANAGED_OK);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_access_reference_and_stale(void) {
    static const unsigned char text[] = "ruda";
    RudaManagedHeap heap;
    RudaManagedReference string = ruda_managed_invalid_reference();
    RudaManagedReference strings = ruda_managed_invalid_reference();
    RudaManagedReference raw = ruda_managed_invalid_reference();
    RudaManagedReference stale = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&string, &strings, &raw};
    RudaManagedRootFrame roots;
    RudaManagedCell observed;
    size_t length = 0;
    memset(&roots, 0, sizeof(roots));
    memset(&observed, 0, sizeof(observed));
    RUDA_EXPECT(ruda_test_access_heap_init(
        &heap, ruda_system_allocator(), false
    ));
    RUDA_EXPECT(ruda_managed_string_new(
        &heap, text, sizeof(text) - 1, &string
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_access_strings, NULL, 0, &strings
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, NULL, 0, &raw
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(
        &heap, &roots, locations, 3
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_push(
        &heap, strings, ruda_test_access_reference(string)
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, strings, &ruda_test_access_strings, 0,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_OK
        && ruda_managed_reference_equal(observed.reference, string));
    RUDA_EXPECT(ruda_managed_array_access_set(
        &heap, strings, &ruda_test_access_strings, 0,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED,
        ruda_test_access_reference(raw), &length
    ) == RUDA_MANAGED_TYPE_MISMATCH);
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &roots) == RUDA_MANAGED_OK);
    stale = strings;
    string = ruda_managed_invalid_reference();
    strings = ruda_managed_invalid_reference();
    raw = ruda_managed_invalid_reference();
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_access_length(
        &heap, stale, &ruda_test_access_strings, &length
    ) == RUDA_MANAGED_INVALID_REFERENCE);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_access_performs_no_allocation(void) {
    RudaFailAllocator state;
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedCell initial = ruda_test_access_raw(UINT64_C(7));
    RudaManagedCell observed;
    size_t attempts = 0;
    size_t length = 0;
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    RUDA_EXPECT(ruda_test_access_heap_init(
        &heap, ruda_fail_allocator(&state), false
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, &initial, 1, &array
    ) == RUDA_MANAGED_OK);
    attempts = state.attempts;
    state.failure_index = attempts;
    RUDA_EXPECT(ruda_managed_array_access_get(
        &heap, array, &ruda_managed_array_f64_descriptor, 0,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == RUDA_MANAGED_OK && observed.f64_bits == UINT64_C(7));
    RUDA_EXPECT(ruda_managed_array_access_set(
        &heap, array, &ruda_managed_array_f64_descriptor, 0,
        RUDA_MANAGED_ARRAY_ACCESS_CHECKED,
        ruda_test_access_raw(UINT64_C(8)), &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(state.attempts == attempts);
    ruda_managed_heap_dispose(&heap);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);
}

void ruda_test_managed_array_access(void) {
    ruda_test_access_raw_alias_and_bounds();
    ruda_test_access_reference_and_stale();
    ruda_test_access_performs_no_allocation();
}
