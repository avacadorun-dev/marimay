#include "test_support.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"

void mmay_test_managed_objects(void);

static const MarimayManagedArrayDescriptor mmay_test_managed_i64_descriptor = {
    MMAY_MANAGED_CELL_RAW64,
    MMAY_MANAGED_ARRAY_STORAGE_RAW64,
    sizeof(uint64_t),
    MMAY_MANAGED_OBJECT_INVALID,
    NULL
};

static const MarimayManagedArrayDescriptor mmay_test_managed_recursive_descriptor = {
    MMAY_MANAGED_CELL_REFERENCE,
    MMAY_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(MarimayManagedCell),
    MMAY_MANAGED_OBJECT_ARRAY,
    &mmay_test_managed_recursive_descriptor
};

static MarimayManagedCell mmay_test_managed_f64(uint64_t bits) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static MarimayManagedCell mmay_test_managed_reference(
    MarimayManagedReference reference
) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.reference = reference;
    return cell;
}

static bool mmay_test_managed_init(
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

static void mmay_test_managed_reference_safety(void) {
    MarimayManagedHeap first;
    MarimayManagedHeap second;
    MarimayManagedReference reference = mmay_managed_invalid_reference();
    MarimayManagedReference stale = mmay_managed_invalid_reference();
    MarimayManagedReference replacement = mmay_managed_invalid_reference();
    MarimayManagedCell value = mmay_test_managed_f64(UINT64_C(0x8000000000000000));
    size_t length = 99;
    MMAY_EXPECT(mmay_test_managed_init(
        &first,
        mmay_system_allocator(),
        false
    ));
    MMAY_EXPECT(mmay_test_managed_init(
        &second,
        mmay_system_allocator(),
        false
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &first,
        &mmay_managed_array_f64_descriptor,
        &value,
        1,
        &reference
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(!mmay_managed_reference_is_invalid(reference));
    MMAY_EXPECT(mmay_managed_array_length(
        &second,
        reference,
        &length
    ) == MMAY_MANAGED_INVALID_REFERENCE);
    stale = reference;
    MMAY_EXPECT(mmay_managed_heap_collect(&first) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&first) == 0);
    MMAY_EXPECT(mmay_managed_array_length(
        &first,
        stale,
        &length
    ) == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(mmay_managed_array_new(
        &first,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &replacement
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(replacement.slot == stale.slot);
    MMAY_EXPECT(replacement.generation != stale.generation);
    MMAY_EXPECT(!mmay_managed_reference_equal(replacement, stale));
    mmay_managed_heap_dispose(&second);
    mmay_managed_heap_dispose(&first);
}

static void mmay_test_managed_roots_and_nested_edges(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference first = mmay_managed_invalid_reference();
    MarimayManagedReference second = mmay_managed_invalid_reference();
    MarimayManagedReference outer = mmay_managed_invalid_reference();
    MarimayManagedReference first_identity = mmay_managed_invalid_reference();
    MarimayManagedReference second_stale = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&outer};
    MarimayManagedRootFrame frame;
    MarimayManagedCell inner_cells[] = {
        mmay_test_managed_f64(UINT64_C(0x3ff0000000000000)),
        mmay_test_managed_f64(UINT64_C(0x8000000000000000))
    };
    MarimayManagedCell outer_cell;
    MarimayManagedCell observed;
    size_t length = 0;
    memset(&frame, 0, sizeof(frame));
    MMAY_EXPECT(mmay_test_managed_init(
        &heap,
        mmay_system_allocator(),
        true
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_f64_descriptor,
        inner_cells,
        2,
        &first
    ) == MMAY_MANAGED_OK);
    outer_cell = mmay_test_managed_reference(first);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_array_f64_descriptor,
        &outer_cell,
        1,
        &outer
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(
        &heap,
        &frame,
        locations,
        1
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &second
    ) == MMAY_MANAGED_OK);
    first_identity = first;
    second_stale = second;
    first = mmay_managed_invalid_reference();
    second = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 2);
    MMAY_EXPECT(mmay_managed_array_length(
        &heap,
        second_stale,
        &length
    ) == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(mmay_managed_array_get(
        &heap,
        outer,
        0,
        &observed,
        &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_reference_equal(
        observed.reference,
        first_identity
    ));
    MMAY_EXPECT(mmay_managed_array_get(
        &heap,
        observed.reference,
        1,
        &observed,
        &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(observed.f64_bits == UINT64_C(0x8000000000000000));
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame)
        == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 0);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_generic_descriptors_and_cycles(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference scalar = mmay_managed_invalid_reference();
    MarimayManagedReference disguised = mmay_managed_invalid_reference();
    MarimayManagedReference container = mmay_managed_invalid_reference();
    MarimayManagedReference child = mmay_managed_invalid_reference();
    MarimayManagedReference cycle = mmay_managed_invalid_reference();
    MarimayManagedReference child_identity = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&container, &cycle};
    MarimayManagedRootFrame frame;
    MarimayManagedCell cell;
    MarimayManagedCell observed;
    const MarimayManagedArrayDescriptor *descriptor = NULL;
    size_t length = 0;
    memset(&frame, 0, sizeof(frame));
    MMAY_EXPECT(mmay_test_managed_init(
        &heap,
        mmay_system_allocator(),
        false
    ));

    /* A scalar payload that resembles a valid reference must not be traced. */
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &disguised
    ) == MMAY_MANAGED_OK);
    cell = mmay_test_managed_reference(disguised);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_test_managed_i64_descriptor,
        &cell,
        1,
        &scalar
    ) == MMAY_MANAGED_OK);
    heap.options.collect_before_allocation = true;
    MMAY_EXPECT(mmay_managed_array_describe(
        &heap,
        scalar,
        &descriptor
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(descriptor == &mmay_test_managed_i64_descriptor);
    disguised = mmay_managed_invalid_reference();
    container = scalar;
    MMAY_EXPECT(mmay_managed_root_push(
        &heap,
        &frame,
        locations,
        2
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 1);

    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_test_managed_recursive_descriptor,
        NULL,
        0,
        &cycle
    ) == MMAY_MANAGED_OK);
    cell = mmay_test_managed_reference(cycle);
    MMAY_EXPECT(mmay_managed_array_push(&heap, cycle, cell)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 2);
    MMAY_EXPECT(mmay_managed_array_get(
        &heap,
        cycle,
        0,
        &observed,
        &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_reference_equal(observed.reference, cycle));
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 0);

    /* Push must temporarily root both an unrooted receiver and child. */
    heap.options.collect_before_allocation = false;
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_array_f64_descriptor,
        NULL,
        0,
        &container
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &child
    ) == MMAY_MANAGED_OK);
    heap.options.collect_before_allocation = true;
    child_identity = child;
    cell = mmay_test_managed_reference(child);
    MMAY_EXPECT(mmay_managed_array_push(&heap, container, cell)
        == MMAY_MANAGED_OK);
    child = mmay_managed_invalid_reference();
    memset(&frame, 0, sizeof(frame));
    MMAY_EXPECT(mmay_managed_root_push(
        &heap,
        &frame,
        locations,
        1
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 2);
    MMAY_EXPECT(mmay_managed_array_get(
        &heap,
        container,
        0,
        &observed,
        &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_reference_equal(
        observed.reference,
        child_identity
    ));
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_root_discipline(void) {
    MarimayManagedHeap first;
    MarimayManagedHeap second;
    MarimayManagedReference reference = mmay_managed_invalid_reference();
    MarimayManagedReference foreign = mmay_managed_invalid_reference();
    MarimayManagedReference victim = mmay_managed_invalid_reference();
    MarimayManagedReference original = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&reference};
    MarimayManagedRootFrame outer;
    MarimayManagedRootFrame inner;
    memset(&outer, 0, sizeof(outer));
    memset(&inner, 0, sizeof(inner));
    MMAY_EXPECT(mmay_test_managed_init(
        &first,
        mmay_system_allocator(),
        false
    ));
    MMAY_EXPECT(mmay_test_managed_init(
        &second,
        mmay_system_allocator(),
        false
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &first,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &reference
    ) == MMAY_MANAGED_OK);
    original = reference;
    MMAY_EXPECT(mmay_managed_array_new(
        &second,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &foreign
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(&first, &outer, locations, 1)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(&first, &inner, NULL, 0)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_pop(&first, &outer)
        == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_root_pop(&second, &inner)
        == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_root_pop(&first, &inner) == MMAY_MANAGED_OK);

    MMAY_EXPECT(mmay_managed_array_new(
        &first,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &victim
    ) == MMAY_MANAGED_OK);
    reference = foreign;
    MMAY_EXPECT(mmay_managed_heap_collect(&first)
        == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&first) == 2);
    reference = original;
    victim = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_heap_collect(&first) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&first) == 1);
    mmay_managed_heap_dispose(&first);
    MMAY_EXPECT(!outer.linked);
    mmay_managed_heap_dispose(&second);
}

static void mmay_test_managed_access_and_failure_atomicity(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedReference inner = mmay_managed_invalid_reference();
    MarimayManagedReference outer = mmay_managed_invalid_reference();
    MarimayManagedCell initial = mmay_test_managed_f64(UINT64_C(0x7ff0000000000001));
    MarimayManagedCell update = mmay_test_managed_f64(UINT64_C(0x7ff8000000000042));
    MarimayManagedCell wrong;
    MarimayManagedCell observed;
    size_t length = 0;
    MMAY_EXPECT(mmay_test_managed_init(
        &heap,
        mmay_system_allocator(),
        false
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_f64_descriptor,
        &initial,
        1,
        &array
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_get(
        &heap,
        array,
        INT64_MIN,
        &observed,
        &length
    ) == MMAY_MANAGED_BOUNDS);
    MMAY_EXPECT(length == 1);
    MMAY_EXPECT(mmay_managed_array_get(
        &heap,
        array,
        INT64_MAX,
        &observed,
        &length
    ) == MMAY_MANAGED_BOUNDS);
    MMAY_EXPECT(mmay_managed_array_set(
        &heap,
        array,
        -1,
        update,
        &length
    ) == MMAY_MANAGED_BOUNDS);
    MMAY_EXPECT(mmay_managed_array_get(
        &heap,
        array,
        0,
        &observed,
        &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(observed.f64_bits == initial.f64_bits);
    MMAY_EXPECT(mmay_managed_array_set(
        &heap,
        array,
        0,
        update,
        &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_get(
        &heap,
        array,
        0,
        &observed,
        &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(observed.f64_bits == update.f64_bits);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &inner
    ) == MMAY_MANAGED_OK);
    wrong = mmay_test_managed_reference(inner);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_array_f64_descriptor,
        &wrong,
        1,
        &outer
    ) == MMAY_MANAGED_OK);
    wrong = mmay_test_managed_reference(outer);
    MMAY_EXPECT(mmay_managed_array_set(
        &heap,
        outer,
        0,
        wrong,
        &length
    ) == MMAY_MANAGED_TYPE_MISMATCH);
    MMAY_EXPECT(mmay_managed_array_length(
        &heap,
        outer,
        &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(length == 1);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_limits(void) {
    MarimayManagedHeap heap;
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&array};
    MarimayManagedRootFrame frame;
    MarimayManagedCell cell = mmay_test_managed_f64(0);
    size_t length = 0;
    memset(&frame, 0, sizeof(frame));
#if SIZE_MAX > INT64_MAX
    options.max_array_length = SIZE_MAX;
    MMAY_EXPECT(!mmay_managed_heap_init(
        &heap,
        mmay_system_allocator(),
        options
    ));
#endif
    options = mmay_managed_heap_default_options();
    options.max_objects = 4;
    options.max_live_bytes = 4096;
    options.max_array_length = 1;
    MMAY_EXPECT(mmay_managed_heap_init(
        &heap,
        mmay_system_allocator(),
        options
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &array
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(
        &heap,
        &frame,
        locations,
        1
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_push(&heap, array, cell)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_push(&heap, array, cell)
        == MMAY_MANAGED_LIMIT_REACHED);
    MMAY_EXPECT(mmay_managed_array_length(&heap, array, &length)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(length == 1);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_collection_before_limits(void) {
    MarimayManagedHeap heap;
    MarimayManagedHeap calibration;
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    MarimayManagedReference stale = mmay_managed_invalid_reference();
    MarimayManagedReference replacement = mmay_managed_invalid_reference();
    size_t one_object_bytes = 0;
    options.max_objects = 1;
    options.max_live_bytes = 4096;
    options.max_array_length = 4;
    MMAY_EXPECT(mmay_managed_heap_init(
        &heap,
        mmay_system_allocator(),
        options
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &stale
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &replacement
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(replacement.slot == stale.slot);
    MMAY_EXPECT(replacement.generation != stale.generation);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 1);
    mmay_managed_heap_dispose(&heap);

    options.max_objects = 2;
    options.max_live_bytes = 4096;
    MMAY_EXPECT(mmay_managed_heap_init(
        &calibration,
        mmay_system_allocator(),
        options
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &calibration,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &stale
    ) == MMAY_MANAGED_OK);
    one_object_bytes = mmay_managed_heap_live_bytes(&calibration);
    MMAY_EXPECT(one_object_bytes > 0);
    mmay_managed_heap_dispose(&calibration);

    options.max_live_bytes = one_object_bytes;
    MMAY_EXPECT(mmay_managed_heap_init(
        &heap,
        mmay_system_allocator(),
        options
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &stale
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap,
        &mmay_managed_array_f64_descriptor,
        NULL,
        0,
        &replacement
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 1);
    MMAY_EXPECT(mmay_managed_heap_live_bytes(&heap) == one_object_bytes);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_allocation_sweep(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 32; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayManagedHeap heap;
        MarimayManagedReference array = mmay_managed_invalid_reference();
        MarimayManagedReference *locations[] = {&array};
        MarimayManagedRootFrame frame;
        MarimayManagedCell cell = mmay_test_managed_f64(UINT64_C(0x3ff0000000000000));
        bool initialized = false;
        MarimayManagedStatus new_status = MMAY_MANAGED_ALLOCATION_FAILED;
        MarimayManagedStatus push_status = MMAY_MANAGED_ALLOCATION_FAILED;
        memset(&frame, 0, sizeof(frame));
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_index
        );
        allocator = mmay_fail_allocator(&state);
        initialized = mmay_test_managed_init(&heap, allocator, true);
        if (initialized) {
            new_status = mmay_managed_array_new(
                &heap,
                &mmay_managed_array_f64_descriptor,
                NULL,
                0,
                &array
            );
            if (new_status == MMAY_MANAGED_OK) {
                MMAY_EXPECT(mmay_managed_root_push(
                    &heap,
                    &frame,
                    locations,
                    1
                ) == MMAY_MANAGED_OK);
                push_status = mmay_managed_array_push(&heap, array, cell);
                MMAY_EXPECT(push_status == MMAY_MANAGED_OK
                    || push_status == MMAY_MANAGED_ALLOCATION_FAILED);
                MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame)
                    == MMAY_MANAGED_OK);
            } else {
                MMAY_EXPECT(new_status == MMAY_MANAGED_ALLOCATION_FAILED);
                MMAY_EXPECT(mmay_managed_reference_is_invalid(array));
            }
            mmay_managed_heap_dispose(&heap);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (initialized && new_status == MMAY_MANAGED_OK
            && push_status == MMAY_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_managed_nonempty_allocation_sweep(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 48; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayManagedHeap heap;
        MarimayManagedReference array = mmay_managed_invalid_reference();
        MarimayManagedReference child = mmay_managed_invalid_reference();
        MarimayManagedReference outer = mmay_managed_invalid_reference();
        MarimayManagedReference *locations[] = {&array, &outer};
        MarimayManagedRootFrame frame;
        MarimayManagedCell initial[] = {
            mmay_test_managed_f64(UINT64_C(0x3ff0000000000000)),
            mmay_test_managed_f64(UINT64_C(0x4000000000000000))
        };
        MarimayManagedCell nested;
        MarimayManagedStatus status = MMAY_MANAGED_ALLOCATION_FAILED;
        memset(&frame, 0, sizeof(frame));
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_index
        );
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_test_managed_init(&heap, allocator, true));
        status = mmay_managed_array_new(
            &heap,
            &mmay_managed_array_f64_descriptor,
            initial,
            2,
            &array
        );
        if (status == MMAY_MANAGED_OK) {
            MMAY_EXPECT(mmay_managed_root_push(
                &heap,
                &frame,
                locations,
                2
            ) == MMAY_MANAGED_OK);
            status = mmay_managed_array_push(&heap, array, initial[0]);
            if (status == MMAY_MANAGED_OK) {
                status = mmay_managed_array_new(
                    &heap,
                    &mmay_managed_array_f64_descriptor,
                    initial,
                    1,
                    &child
                );
            }
            if (status == MMAY_MANAGED_OK) {
                nested = mmay_test_managed_reference(child);
                status = mmay_managed_array_new(
                    &heap,
                    &mmay_managed_array_array_f64_descriptor,
                    &nested,
                    1,
                    &outer
                );
            }
            MMAY_EXPECT(status == MMAY_MANAGED_OK
                || status == MMAY_MANAGED_ALLOCATION_FAILED);
            MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame)
                == MMAY_MANAGED_OK);
        } else {
            MMAY_EXPECT(status == MMAY_MANAGED_ALLOCATION_FAILED);
            MMAY_EXPECT(mmay_managed_reference_is_invalid(array));
        }
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

void mmay_test_managed(void) {
    mmay_test_managed_reference_safety();
    mmay_test_managed_roots_and_nested_edges();
    mmay_test_managed_generic_descriptors_and_cycles();
    mmay_test_managed_root_discipline();
    mmay_test_managed_access_and_failure_atomicity();
    mmay_test_managed_limits();
    mmay_test_managed_collection_before_limits();
    mmay_test_managed_allocation_sweep();
    mmay_test_managed_nonempty_allocation_sweep();
    mmay_test_managed_objects();
}
