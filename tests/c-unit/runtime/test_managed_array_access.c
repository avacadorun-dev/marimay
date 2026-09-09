#include "../test_support.h"

#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"
#include "mmay/runtime/array_scalar_view.h"

static const MarimayManagedArrayDescriptor mmay_test_access_strings = {
    MMAY_MANAGED_CELL_REFERENCE,
    MMAY_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(MarimayManagedCell),
    MMAY_MANAGED_OBJECT_STRING,
    NULL
};

static MarimayManagedCell mmay_test_access_raw(uint64_t bits) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static MarimayManagedCell mmay_test_access_reference(
    MarimayManagedReference reference
) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.reference = reference;
    return cell;
}

static bool mmay_test_access_heap_init(
    MarimayManagedHeap *heap,
    MarimayAllocator allocator,
    bool collect_before_allocation
) {
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    options.max_objects = 32;
    options.max_live_bytes = 64U * 1024U;
    options.max_array_length = 64;
    options.collect_before_allocation = collect_before_allocation;
    return mmay_managed_heap_init(heap, allocator, options);
}

static void mmay_test_access_raw_alias_and_bounds(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedReference alias = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&array, &alias};
    MarimayManagedRootFrame roots;
    MarimayManagedCell initial[] = {
        mmay_test_access_raw(UINT64_C(11)),
        mmay_test_access_raw(UINT64_C(22))
    };
    MarimayManagedCell observed = mmay_test_access_raw(UINT64_MAX);
    size_t length = SIZE_MAX;
    memset(&roots, 0, sizeof(roots));
    MMAY_EXPECT(mmay_test_access_heap_init(
        &heap, mmay_system_allocator(), true
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, initial, 2, &array
    ) == MMAY_MANAGED_OK);
    alias = array;
    MMAY_EXPECT(mmay_managed_root_push(
        &heap, &roots, locations, 2
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_access_length(
        &heap, alias, &mmay_managed_array_f64_descriptor, &length
    ) == MMAY_MANAGED_OK && length == 2);
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, alias, &mmay_managed_array_f64_descriptor, 1,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_OK && observed.f64_bits == UINT64_C(22)
        && length == 2);
    MMAY_EXPECT(mmay_managed_array_access_set(
        &heap, array, &mmay_managed_array_f64_descriptor, 0,
        MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS,
        mmay_test_access_raw(UINT64_C(33)), &length
    ) == MMAY_MANAGED_OK && length == 2);
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, alias, &mmay_managed_array_f64_descriptor, 0,
        MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS, &observed, &length
    ) == MMAY_MANAGED_OK && observed.f64_bits == UINT64_C(33));
    MMAY_EXPECT(mmay_managed_array_access_require(
        &heap, array, &mmay_managed_array_f64_descriptor, -1, &length
    ) == MMAY_MANAGED_BOUNDS && length == 2);
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, array, &mmay_managed_array_f64_descriptor, 2,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_BOUNDS && length == 2
        && observed.f64_bits == UINT64_C(0));
    MMAY_EXPECT(mmay_managed_array_access_set(
        &heap, array, &mmay_managed_array_f64_descriptor, INT64_MAX,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED,
        mmay_test_access_raw(UINT64_C(44)), &length
    ) == MMAY_MANAGED_BOUNDS && length == 2);
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, array, &mmay_managed_array_array_f64_descriptor, 0,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_TYPE_MISMATCH && length == 0);
    MMAY_EXPECT(mmay_managed_array_push(
        &heap, alias, mmay_test_access_raw(UINT64_C(44))
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, array, &mmay_managed_array_f64_descriptor, 2,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_OK && observed.f64_bits == UINT64_C(44)
        && length == 3);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &roots) == MMAY_MANAGED_OK);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_access_reference_and_stale(void) {
    static const unsigned char text[] = "mmay";
    MarimayManagedHeap heap;
    MarimayManagedReference string = mmay_managed_invalid_reference();
    MarimayManagedReference strings = mmay_managed_invalid_reference();
    MarimayManagedReference raw = mmay_managed_invalid_reference();
    MarimayManagedReference stale = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&string, &strings, &raw};
    MarimayManagedRootFrame roots;
    MarimayManagedCell observed;
    size_t length = 0;
    memset(&roots, 0, sizeof(roots));
    memset(&observed, 0, sizeof(observed));
    MMAY_EXPECT(mmay_test_access_heap_init(
        &heap, mmay_system_allocator(), false
    ));
    MMAY_EXPECT(mmay_managed_string_new(
        &heap, text, sizeof(text) - 1, &string
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_access_strings, NULL, 0, &strings
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, NULL, 0, &raw
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(
        &heap, &roots, locations, 3
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_push(
        &heap, strings, mmay_test_access_reference(string)
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, strings, &mmay_test_access_strings, 0,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_OK
        && mmay_managed_reference_equal(observed.reference, string));
    MMAY_EXPECT(mmay_managed_array_access_set(
        &heap, strings, &mmay_test_access_strings, 0,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED,
        mmay_test_access_reference(raw), &length
    ) == MMAY_MANAGED_TYPE_MISMATCH);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &roots) == MMAY_MANAGED_OK);
    stale = strings;
    string = mmay_managed_invalid_reference();
    strings = mmay_managed_invalid_reference();
    raw = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_access_length(
        &heap, stale, &mmay_test_access_strings, &length
    ) == MMAY_MANAGED_INVALID_REFERENCE);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_access_performs_no_allocation(void) {
    MarimayFailAllocator state;
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedCell initial = mmay_test_access_raw(UINT64_C(7));
    MarimayManagedCell observed;
    size_t attempts = 0;
    size_t length = 0;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_test_access_heap_init(
        &heap, mmay_fail_allocator(&state), false
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, &initial, 1, &array
    ) == MMAY_MANAGED_OK);
    attempts = state.attempts;
    state.failure_index = attempts;
    MMAY_EXPECT(mmay_managed_array_access_get(
        &heap, array, &mmay_managed_array_f64_descriptor, 0,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_OK && observed.f64_bits == UINT64_C(7));
    MMAY_EXPECT(mmay_managed_array_access_set(
        &heap, array, &mmay_managed_array_f64_descriptor, 0,
        MMAY_MANAGED_ARRAY_ACCESS_CHECKED,
        mmay_test_access_raw(UINT64_C(8)), &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(state.attempts == attempts);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}

static void mmay_test_access_bound_view_growth_and_collection(void) {
    MarimayFailAllocator state;
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedReference stale = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&array};
    MarimayManagedRootFrame roots;
    MarimayManagedArrayView view;
    MarimayManagedArrayView rejected;
    MarimayManagedCell initial = mmay_test_access_raw(UINT64_C(7));
    MarimayManagedCell observed;
    size_t attempts = 0;
    size_t length = 0;
    uint64_t bits = 0;
    uint64_t generation = 0;
    bool popped = false;
    memset(&roots, 0, sizeof(roots));
    memset(&view, 0, sizeof(view));
    memset(&rejected, 0xff, sizeof(rejected));
    memset(&observed, 0, sizeof(observed));
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_test_access_heap_init(
        &heap, mmay_fail_allocator(&state), false
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, &initial, 1, &array
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(
        &heap, &roots, locations, 1
    ) == MMAY_MANAGED_OK);
    attempts = state.attempts;
    state.failure_index = attempts;
    MMAY_EXPECT(mmay_managed_array_view_bind(
        &heap, array, &mmay_managed_array_f64_descriptor, &view
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(view.object != NULL && view.length != NULL
        && view.storage != NULL && view.mutation_generation != NULL
        && mmay_managed_reference_equal(view.reference, array));
    MMAY_EXPECT(mmay_managed_array_view_length_fast(&view, &length)
        == MMAY_MANAGED_OK && length == 1);
    MMAY_EXPECT(mmay_managed_array_view_get_raw64_fast(
        &view, 0, MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &bits, &length
    ) == MMAY_MANAGED_OK && bits == UINT64_C(7) && length == 1);
    MMAY_EXPECT(mmay_managed_array_view_set_raw64_fast(
        &view, 0, MMAY_MANAGED_ARRAY_ACCESS_CHECKED,
        UINT64_C(8), &length
    ) == MMAY_MANAGED_OK && length == 1);
    MMAY_EXPECT(state.attempts == attempts);
    generation = *view.mutation_generation;
    MMAY_EXPECT(generation == 1);

    MMAY_EXPECT(mmay_managed_array_view_push(
        &heap, &view, mmay_test_access_raw(UINT64_C(9))
    ) == MMAY_MANAGED_ALLOCATION_FAILED);
    MMAY_EXPECT(*view.length == 1 && *view.mutation_generation == generation);
    MMAY_EXPECT(mmay_managed_array_view_get_raw64_fast(
        &view, 0, MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &bits, &length
    ) == MMAY_MANAGED_OK && bits == UINT64_C(8));

    state.failure_index = SIZE_MAX;
    MMAY_EXPECT(mmay_managed_array_view_push(
        &heap, &view, mmay_test_access_raw(UINT64_C(9))
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(*view.length == 2
        && *view.mutation_generation == generation + UINT64_C(1));
    MMAY_EXPECT(mmay_managed_array_view_get(
        &view, 1, MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
    ) == MMAY_MANAGED_OK && observed.f64_bits == UINT64_C(9)
        && length == 2);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_view_get_raw64_fast(
        &view, 0, MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &bits, &length
    ) == MMAY_MANAGED_OK && bits == UINT64_C(8));
    MMAY_EXPECT(mmay_managed_array_view_pop(
        &view, &observed, &popped
    ) == MMAY_MANAGED_OK && popped
        && observed.f64_bits == UINT64_C(9) && *view.length == 1);

    MMAY_EXPECT(mmay_managed_array_view_bind(
        &heap, array, &mmay_managed_array_array_f64_descriptor, &rejected
    ) == MMAY_MANAGED_TYPE_MISMATCH);
    MMAY_EXPECT(rejected.object == NULL && rejected.length == NULL
        && rejected.storage == NULL);
    stale = array;
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &roots) == MMAY_MANAGED_OK);
    array = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_view_bind(
        &heap, stale, &mmay_managed_array_f64_descriptor, &rejected
    ) == MMAY_MANAGED_INVALID_REFERENCE);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}


static void mmay_test_access_scalar_views(void) {
    static const MarimayManagedArrayDescriptor descriptors[] = {
        {MMAY_MANAGED_CELL_RAW64, MMAY_MANAGED_ARRAY_STORAGE_RAW8, 1,
            MMAY_MANAGED_OBJECT_INVALID, NULL},
        {MMAY_MANAGED_CELL_RAW64, MMAY_MANAGED_ARRAY_STORAGE_RAW16, 2,
            MMAY_MANAGED_OBJECT_INVALID, NULL},
        {MMAY_MANAGED_CELL_RAW64, MMAY_MANAGED_ARRAY_STORAGE_RAW32, 4,
            MMAY_MANAGED_OBJECT_INVALID, NULL},
        {MMAY_MANAGED_CELL_RAW64, MMAY_MANAGED_ARRAY_STORAGE_RAW64, 8,
            MMAY_MANAGED_OBJECT_INVALID, NULL}
    };
    static const uint64_t limits[] = {UINT8_MAX, UINT16_MAX, UINT32_MAX, UINT64_MAX};
    for (size_t test = 0; test < 4; test += 1) {
        MarimayFailAllocator state;
        MarimayManagedHeap heap;
        MarimayManagedReference array = mmay_managed_invalid_reference();
        MarimayManagedReference *locations[] = {&array};
        MarimayManagedRootFrame roots = {0};
        MarimayManagedArrayView view;
        MarimayManagedCell initial[] = {mmay_test_access_raw(limits[test]),
            mmay_test_access_raw(0)};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_access_heap_init(&heap, mmay_fail_allocator(&state), true));
        MMAY_EXPECT(mmay_managed_array_new(&heap, &descriptors[test], initial, 2,
            &array) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_root_push(&heap, &roots, locations, 1) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_array_view_bind(&heap, array, &descriptors[test],
            &view) == MMAY_MANAGED_OK);
        size_t attempts = state.attempts;
        state.failure_index = attempts;
        for (int64_t index = -1; index <= 2; index += 1) {
            uint64_t bits = 123;
            size_t length = 0, expected_length = 0;
            MarimayManagedCell expected;
            MarimayManagedStatus status = mmay_managed_array_get(&heap, array,
                index, &expected, &expected_length);
            MMAY_EXPECT(mmay_managed_array_scalar_view_get_fast(&view, index,
                MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &bits, &length) == status);
            MMAY_EXPECT(length == expected_length && bits == expected.f64_bits);
            uint64_t generation = *view.mutation_generation;
            status = mmay_managed_array_scalar_view_set_fast(&view, index,
                MMAY_MANAGED_ARRAY_ACCESS_CHECKED, limits[test], &length);
            MMAY_EXPECT(status == (index < 0 || index >= 2
                ? MMAY_MANAGED_BOUNDS : MMAY_MANAGED_OK));
            MMAY_EXPECT(*view.mutation_generation == generation + (status == MMAY_MANAGED_OK));
        }
        size_t length = 0;
        uint64_t bits = 0;
        MMAY_EXPECT(mmay_managed_array_scalar_view_set_fast(&view, 1,
            MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS, 37, &length) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_array_scalar_view_get_fast(&view, 1,
            MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS, &bits, &length) == MMAY_MANAGED_OK
            && bits == 37);
        uint64_t generation = *view.mutation_generation;
        if (test < 3) {
            MMAY_EXPECT(mmay_managed_array_scalar_view_set_fast(&view, 1,
                MMAY_MANAGED_ARRAY_ACCESS_CHECKED, limits[test] + 1, &length)
                == MMAY_MANAGED_TYPE_MISMATCH);
            MMAY_EXPECT(*view.mutation_generation == generation);
        }
        *view.mutation_generation = UINT64_MAX;
        MMAY_EXPECT(mmay_managed_array_scalar_view_set_fast(&view, 1,
            MMAY_MANAGED_ARRAY_ACCESS_CHECKED, 12, &length) == MMAY_MANAGED_LIMIT_REACHED);
        MMAY_EXPECT(*view.mutation_generation == UINT64_MAX);
        MMAY_EXPECT(mmay_managed_array_scalar_view_get_fast(&view, 1,
            MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &bits, &length) == MMAY_MANAGED_OK
            && bits == 37);
        MarimayManagedArrayView rejected = view;
        MarimayManagedArrayDescriptor wrong = descriptors[test];
        wrong.storage_stride += 1;
        rejected.descriptor = &wrong;
        MMAY_EXPECT(mmay_managed_array_scalar_view_get_fast(&rejected, 0,
            MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &bits, &length) == MMAY_MANAGED_INVALID_ARGUMENT);
        MMAY_EXPECT(mmay_managed_array_scalar_view_set_fast(&rejected, 0,
            MMAY_MANAGED_ARRAY_ACCESS_CHECKED, 0, &length) == MMAY_MANAGED_INVALID_ARGUMENT);
        rejected.descriptor = &mmay_test_access_strings;
        MMAY_EXPECT(mmay_managed_array_scalar_view_get_fast(&rejected, 0,
            MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &bits, &length) == MMAY_MANAGED_INVALID_ARGUMENT);
        MMAY_EXPECT(mmay_managed_array_scalar_view_set_fast(NULL, 0,
            MMAY_MANAGED_ARRAY_ACCESS_CHECKED, 0, &length) == MMAY_MANAGED_INVALID_ARGUMENT);
        MMAY_EXPECT(state.attempts == attempts);
        state.failure_index = SIZE_MAX;
        *view.mutation_generation = generation;
        MMAY_EXPECT(mmay_managed_array_view_push(&heap, &view, mmay_test_access_raw(41))
            == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_array_scalar_view_get_fast(&view, 2,
            MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &bits, &length) == MMAY_MANAGED_OK
            && bits == 41 && length == 3);
        MMAY_EXPECT(mmay_managed_root_pop(&heap, &roots) == MMAY_MANAGED_OK);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}

void mmay_test_managed_array_access(void) {
    mmay_test_access_scalar_views();
    mmay_test_access_raw_alias_and_bounds();
    mmay_test_access_reference_and_stale();
    mmay_test_access_performs_no_allocation();
    mmay_test_access_bound_view_growth_and_collection();
}
