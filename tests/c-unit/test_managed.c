#include "test_support.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/runtime/managed.h"

static const RudaManagedArrayDescriptor ruda_test_managed_i64_descriptor = {
    RUDA_MANAGED_CELL_RAW64,
    NULL
};

static const RudaManagedArrayDescriptor ruda_test_managed_recursive_descriptor = {
    RUDA_MANAGED_CELL_REFERENCE,
    &ruda_test_managed_recursive_descriptor
};

static RudaManagedCell ruda_test_managed_f64(uint64_t bits) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static RudaManagedCell ruda_test_managed_reference(
    RudaManagedReference reference
) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.reference = reference;
    return cell;
}

static bool ruda_test_managed_init(
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

static void ruda_test_managed_reference_safety(void) {
    RudaManagedHeap first;
    RudaManagedHeap second;
    RudaManagedReference reference = ruda_managed_invalid_reference();
    RudaManagedReference stale = ruda_managed_invalid_reference();
    RudaManagedReference replacement = ruda_managed_invalid_reference();
    RudaManagedCell value = ruda_test_managed_f64(UINT64_C(0x8000000000000000));
    size_t length = 99;
    RUDA_EXPECT(ruda_test_managed_init(
        &first,
        ruda_system_allocator(),
        false
    ));
    RUDA_EXPECT(ruda_test_managed_init(
        &second,
        ruda_system_allocator(),
        false
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &first,
        &ruda_managed_array_f64_descriptor,
        &value,
        1,
        &reference
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(!ruda_managed_reference_is_invalid(reference));
    RUDA_EXPECT(ruda_managed_array_length(
        &second,
        reference,
        &length
    ) == RUDA_MANAGED_INVALID_REFERENCE);
    stale = reference;
    RUDA_EXPECT(ruda_managed_heap_collect(&first) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&first) == 0);
    RUDA_EXPECT(ruda_managed_array_length(
        &first,
        stale,
        &length
    ) == RUDA_MANAGED_INVALID_REFERENCE);
    RUDA_EXPECT(ruda_managed_array_new(
        &first,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &replacement
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(replacement.slot == stale.slot);
    RUDA_EXPECT(replacement.generation != stale.generation);
    RUDA_EXPECT(!ruda_managed_reference_equal(replacement, stale));
    ruda_managed_heap_dispose(&second);
    ruda_managed_heap_dispose(&first);
}

static void ruda_test_managed_roots_and_nested_edges(void) {
    RudaManagedHeap heap;
    RudaManagedReference first = ruda_managed_invalid_reference();
    RudaManagedReference second = ruda_managed_invalid_reference();
    RudaManagedReference outer = ruda_managed_invalid_reference();
    RudaManagedReference first_identity = ruda_managed_invalid_reference();
    RudaManagedReference second_stale = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&outer};
    RudaManagedRootFrame frame;
    RudaManagedCell inner_cells[] = {
        ruda_test_managed_f64(UINT64_C(0x3ff0000000000000)),
        ruda_test_managed_f64(UINT64_C(0x8000000000000000))
    };
    RudaManagedCell outer_cell;
    RudaManagedCell observed;
    size_t length = 0;
    memset(&frame, 0, sizeof(frame));
    RUDA_EXPECT(ruda_test_managed_init(
        &heap,
        ruda_system_allocator(),
        true
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_f64_descriptor,
        inner_cells,
        2,
        &first
    ) == RUDA_MANAGED_OK);
    outer_cell = ruda_test_managed_reference(first);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_array_f64_descriptor,
        &outer_cell,
        1,
        &outer
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(
        &heap,
        &frame,
        locations,
        1
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &second
    ) == RUDA_MANAGED_OK);
    first_identity = first;
    second_stale = second;
    first = ruda_managed_invalid_reference();
    second = ruda_managed_invalid_reference();
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&heap) == 2);
    RUDA_EXPECT(ruda_managed_array_length(
        &heap,
        second_stale,
        &length
    ) == RUDA_MANAGED_INVALID_REFERENCE);
    RUDA_EXPECT(ruda_managed_array_get(
        &heap,
        outer,
        0,
        &observed,
        &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_reference_equal(
        observed.reference,
        first_identity
    ));
    RUDA_EXPECT(ruda_managed_array_get(
        &heap,
        observed.reference,
        1,
        &observed,
        &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(observed.f64_bits == UINT64_C(0x8000000000000000));
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame)
        == RUDA_MANAGED_INVALID_ARGUMENT);
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&heap) == 0);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_managed_generic_descriptors_and_cycles(void) {
    RudaManagedHeap heap;
    RudaManagedReference scalar = ruda_managed_invalid_reference();
    RudaManagedReference disguised = ruda_managed_invalid_reference();
    RudaManagedReference container = ruda_managed_invalid_reference();
    RudaManagedReference child = ruda_managed_invalid_reference();
    RudaManagedReference cycle = ruda_managed_invalid_reference();
    RudaManagedReference child_identity = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&container, &cycle};
    RudaManagedRootFrame frame;
    RudaManagedCell cell;
    RudaManagedCell observed;
    const RudaManagedArrayDescriptor *descriptor = NULL;
    size_t length = 0;
    memset(&frame, 0, sizeof(frame));
    RUDA_EXPECT(ruda_test_managed_init(
        &heap,
        ruda_system_allocator(),
        false
    ));

    /* A scalar payload that resembles a valid reference must not be traced. */
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &disguised
    ) == RUDA_MANAGED_OK);
    cell = ruda_test_managed_reference(disguised);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_test_managed_i64_descriptor,
        &cell,
        1,
        &scalar
    ) == RUDA_MANAGED_OK);
    heap.options.collect_before_allocation = true;
    RUDA_EXPECT(ruda_managed_array_describe(
        &heap,
        scalar,
        &descriptor
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(descriptor == &ruda_test_managed_i64_descriptor);
    disguised = ruda_managed_invalid_reference();
    container = scalar;
    RUDA_EXPECT(ruda_managed_root_push(
        &heap,
        &frame,
        locations,
        2
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&heap) == 1);

    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_test_managed_recursive_descriptor,
        NULL,
        0,
        &cycle
    ) == RUDA_MANAGED_OK);
    cell = ruda_test_managed_reference(cycle);
    RUDA_EXPECT(ruda_managed_array_push(&heap, cycle, cell)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&heap) == 2);
    RUDA_EXPECT(ruda_managed_array_get(
        &heap,
        cycle,
        0,
        &observed,
        &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_reference_equal(observed.reference, cycle));
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&heap) == 0);

    /* Push must temporarily root both an unrooted receiver and child. */
    heap.options.collect_before_allocation = false;
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_array_f64_descriptor,
        NULL,
        0,
        &container
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &child
    ) == RUDA_MANAGED_OK);
    heap.options.collect_before_allocation = true;
    child_identity = child;
    cell = ruda_test_managed_reference(child);
    RUDA_EXPECT(ruda_managed_array_push(&heap, container, cell)
        == RUDA_MANAGED_OK);
    child = ruda_managed_invalid_reference();
    memset(&frame, 0, sizeof(frame));
    RUDA_EXPECT(ruda_managed_root_push(
        &heap,
        &frame,
        locations,
        1
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&heap) == 2);
    RUDA_EXPECT(ruda_managed_array_get(
        &heap,
        container,
        0,
        &observed,
        &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_reference_equal(
        observed.reference,
        child_identity
    ));
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_managed_root_discipline(void) {
    RudaManagedHeap first;
    RudaManagedHeap second;
    RudaManagedReference reference = ruda_managed_invalid_reference();
    RudaManagedReference foreign = ruda_managed_invalid_reference();
    RudaManagedReference victim = ruda_managed_invalid_reference();
    RudaManagedReference original = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&reference};
    RudaManagedRootFrame outer;
    RudaManagedRootFrame inner;
    memset(&outer, 0, sizeof(outer));
    memset(&inner, 0, sizeof(inner));
    RUDA_EXPECT(ruda_test_managed_init(
        &first,
        ruda_system_allocator(),
        false
    ));
    RUDA_EXPECT(ruda_test_managed_init(
        &second,
        ruda_system_allocator(),
        false
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &first,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &reference
    ) == RUDA_MANAGED_OK);
    original = reference;
    RUDA_EXPECT(ruda_managed_array_new(
        &second,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &foreign
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(&first, &outer, locations, 1)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(&first, &inner, NULL, 0)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_pop(&first, &outer)
        == RUDA_MANAGED_INVALID_ARGUMENT);
    RUDA_EXPECT(ruda_managed_root_pop(&second, &inner)
        == RUDA_MANAGED_INVALID_ARGUMENT);
    RUDA_EXPECT(ruda_managed_root_pop(&first, &inner) == RUDA_MANAGED_OK);

    RUDA_EXPECT(ruda_managed_array_new(
        &first,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &victim
    ) == RUDA_MANAGED_OK);
    reference = foreign;
    RUDA_EXPECT(ruda_managed_heap_collect(&first)
        == RUDA_MANAGED_INVALID_REFERENCE);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&first) == 2);
    reference = original;
    victim = ruda_managed_invalid_reference();
    RUDA_EXPECT(ruda_managed_heap_collect(&first) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&first) == 1);
    ruda_managed_heap_dispose(&first);
    RUDA_EXPECT(!outer.linked);
    ruda_managed_heap_dispose(&second);
}

static void ruda_test_managed_access_and_failure_atomicity(void) {
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedReference inner = ruda_managed_invalid_reference();
    RudaManagedReference outer = ruda_managed_invalid_reference();
    RudaManagedCell initial = ruda_test_managed_f64(UINT64_C(0x7ff0000000000001));
    RudaManagedCell update = ruda_test_managed_f64(UINT64_C(0x7ff8000000000042));
    RudaManagedCell wrong;
    RudaManagedCell observed;
    size_t length = 0;
    RUDA_EXPECT(ruda_test_managed_init(
        &heap,
        ruda_system_allocator(),
        false
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_f64_descriptor,
        &initial,
        1,
        &array
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_get(
        &heap,
        array,
        INT64_MIN,
        &observed,
        &length
    ) == RUDA_MANAGED_BOUNDS);
    RUDA_EXPECT(length == 1);
    RUDA_EXPECT(ruda_managed_array_get(
        &heap,
        array,
        INT64_MAX,
        &observed,
        &length
    ) == RUDA_MANAGED_BOUNDS);
    RUDA_EXPECT(ruda_managed_array_set(
        &heap,
        array,
        -1,
        update,
        &length
    ) == RUDA_MANAGED_BOUNDS);
    RUDA_EXPECT(ruda_managed_array_get(
        &heap,
        array,
        0,
        &observed,
        &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(observed.f64_bits == initial.f64_bits);
    RUDA_EXPECT(ruda_managed_array_set(
        &heap,
        array,
        0,
        update,
        &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_get(
        &heap,
        array,
        0,
        &observed,
        &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(observed.f64_bits == update.f64_bits);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &inner
    ) == RUDA_MANAGED_OK);
    wrong = ruda_test_managed_reference(inner);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_array_f64_descriptor,
        &wrong,
        1,
        &outer
    ) == RUDA_MANAGED_OK);
    wrong = ruda_test_managed_reference(outer);
    RUDA_EXPECT(ruda_managed_array_set(
        &heap,
        outer,
        0,
        wrong,
        &length
    ) == RUDA_MANAGED_TYPE_MISMATCH);
    RUDA_EXPECT(ruda_managed_array_length(
        &heap,
        outer,
        &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(length == 1);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_managed_limits(void) {
    RudaManagedHeap heap;
    RudaManagedHeapOptions options = ruda_managed_heap_default_options();
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&array};
    RudaManagedRootFrame frame;
    RudaManagedCell cell = ruda_test_managed_f64(0);
    size_t length = 0;
    memset(&frame, 0, sizeof(frame));
#if SIZE_MAX > INT64_MAX
    options.max_array_length = SIZE_MAX;
    RUDA_EXPECT(!ruda_managed_heap_init(
        &heap,
        ruda_system_allocator(),
        options
    ));
#endif
    options = ruda_managed_heap_default_options();
    options.max_objects = 4;
    options.max_live_bytes = 4096;
    options.max_array_length = 1;
    RUDA_EXPECT(ruda_managed_heap_init(
        &heap,
        ruda_system_allocator(),
        options
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &array
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(
        &heap,
        &frame,
        locations,
        1
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_push(&heap, array, cell)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_push(&heap, array, cell)
        == RUDA_MANAGED_LIMIT_REACHED);
    RUDA_EXPECT(ruda_managed_array_length(&heap, array, &length)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(length == 1);
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame) == RUDA_MANAGED_OK);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_managed_collection_before_limits(void) {
    RudaManagedHeap heap;
    RudaManagedHeap calibration;
    RudaManagedHeapOptions options = ruda_managed_heap_default_options();
    RudaManagedReference stale = ruda_managed_invalid_reference();
    RudaManagedReference replacement = ruda_managed_invalid_reference();
    size_t one_object_bytes = 0;
    options.max_objects = 1;
    options.max_live_bytes = 4096;
    options.max_array_length = 4;
    RUDA_EXPECT(ruda_managed_heap_init(
        &heap,
        ruda_system_allocator(),
        options
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &stale
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &replacement
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(replacement.slot == stale.slot);
    RUDA_EXPECT(replacement.generation != stale.generation);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&heap) == 1);
    ruda_managed_heap_dispose(&heap);

    options.max_objects = 2;
    options.max_live_bytes = 4096;
    RUDA_EXPECT(ruda_managed_heap_init(
        &calibration,
        ruda_system_allocator(),
        options
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &calibration,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &stale
    ) == RUDA_MANAGED_OK);
    one_object_bytes = ruda_managed_heap_live_bytes(&calibration);
    RUDA_EXPECT(one_object_bytes > 0);
    ruda_managed_heap_dispose(&calibration);

    options.max_live_bytes = one_object_bytes;
    RUDA_EXPECT(ruda_managed_heap_init(
        &heap,
        ruda_system_allocator(),
        options
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &stale
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap,
        &ruda_managed_array_f64_descriptor,
        NULL,
        0,
        &replacement
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_live_objects(&heap) == 1);
    RUDA_EXPECT(ruda_managed_heap_live_bytes(&heap) == one_object_bytes);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_managed_allocation_sweep(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 32; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaManagedHeap heap;
        RudaManagedReference array = ruda_managed_invalid_reference();
        RudaManagedReference *locations[] = {&array};
        RudaManagedRootFrame frame;
        RudaManagedCell cell = ruda_test_managed_f64(UINT64_C(0x3ff0000000000000));
        bool initialized = false;
        RudaManagedStatus new_status = RUDA_MANAGED_ALLOCATION_FAILED;
        RudaManagedStatus push_status = RUDA_MANAGED_ALLOCATION_FAILED;
        memset(&frame, 0, sizeof(frame));
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        initialized = ruda_test_managed_init(&heap, allocator, true);
        if (initialized) {
            new_status = ruda_managed_array_new(
                &heap,
                &ruda_managed_array_f64_descriptor,
                NULL,
                0,
                &array
            );
            if (new_status == RUDA_MANAGED_OK) {
                RUDA_EXPECT(ruda_managed_root_push(
                    &heap,
                    &frame,
                    locations,
                    1
                ) == RUDA_MANAGED_OK);
                push_status = ruda_managed_array_push(&heap, array, cell);
                RUDA_EXPECT(push_status == RUDA_MANAGED_OK
                    || push_status == RUDA_MANAGED_ALLOCATION_FAILED);
                RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame)
                    == RUDA_MANAGED_OK);
            } else {
                RUDA_EXPECT(new_status == RUDA_MANAGED_ALLOCATION_FAILED);
                RUDA_EXPECT(ruda_managed_reference_is_invalid(array));
            }
            ruda_managed_heap_dispose(&heap);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (initialized && new_status == RUDA_MANAGED_OK
            && push_status == RUDA_MANAGED_OK) {
            reached_success = true;
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_managed_nonempty_allocation_sweep(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 48; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaManagedHeap heap;
        RudaManagedReference array = ruda_managed_invalid_reference();
        RudaManagedReference child = ruda_managed_invalid_reference();
        RudaManagedReference outer = ruda_managed_invalid_reference();
        RudaManagedReference *locations[] = {&array, &outer};
        RudaManagedRootFrame frame;
        RudaManagedCell initial[] = {
            ruda_test_managed_f64(UINT64_C(0x3ff0000000000000)),
            ruda_test_managed_f64(UINT64_C(0x4000000000000000))
        };
        RudaManagedCell nested;
        RudaManagedStatus status = RUDA_MANAGED_ALLOCATION_FAILED;
        memset(&frame, 0, sizeof(frame));
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_test_managed_init(&heap, allocator, true));
        status = ruda_managed_array_new(
            &heap,
            &ruda_managed_array_f64_descriptor,
            initial,
            2,
            &array
        );
        if (status == RUDA_MANAGED_OK) {
            RUDA_EXPECT(ruda_managed_root_push(
                &heap,
                &frame,
                locations,
                2
            ) == RUDA_MANAGED_OK);
            status = ruda_managed_array_push(&heap, array, initial[0]);
            if (status == RUDA_MANAGED_OK) {
                status = ruda_managed_array_new(
                    &heap,
                    &ruda_managed_array_f64_descriptor,
                    initial,
                    1,
                    &child
                );
            }
            if (status == RUDA_MANAGED_OK) {
                nested = ruda_test_managed_reference(child);
                status = ruda_managed_array_new(
                    &heap,
                    &ruda_managed_array_array_f64_descriptor,
                    &nested,
                    1,
                    &outer
                );
            }
            RUDA_EXPECT(status == RUDA_MANAGED_OK
                || status == RUDA_MANAGED_ALLOCATION_FAILED);
            RUDA_EXPECT(ruda_managed_root_pop(&heap, &frame)
                == RUDA_MANAGED_OK);
        } else {
            RUDA_EXPECT(status == RUDA_MANAGED_ALLOCATION_FAILED);
            RUDA_EXPECT(ruda_managed_reference_is_invalid(array));
        }
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

void ruda_test_managed(void) {
    ruda_test_managed_reference_safety();
    ruda_test_managed_roots_and_nested_edges();
    ruda_test_managed_generic_descriptors_and_cycles();
    ruda_test_managed_root_discipline();
    ruda_test_managed_access_and_failure_atomicity();
    ruda_test_managed_limits();
    ruda_test_managed_collection_before_limits();
    ruda_test_managed_allocation_sweep();
    ruda_test_managed_nonempty_allocation_sweep();
}
