#include "../test_support.h"

#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/runtime/managed.h"

static bool ruda_test_record_view_heap_init(
    RudaManagedHeap *heap,
    RudaAllocator allocator
) {
    RudaManagedHeapOptions options = ruda_managed_heap_default_options();
    options.max_objects = 16;
    options.max_live_bytes = 16U * 1024U;
    options.max_array_length = 16;
    options.collect_before_allocation = false;
    return ruda_managed_heap_init(heap, allocator, options);
}

void ruda_test_managed_record_view(void) {
    static const RudaManagedFieldDescriptor fields[] = {
        {RUDA_MANAGED_CELL_RAW64, true, RUDA_MANAGED_OBJECT_INVALID, NULL},
        {RUDA_MANAGED_CELL_RAW64, false, RUDA_MANAGED_OBJECT_INVALID, NULL}
    };
    static const RudaManagedRecordDescriptor descriptor = {2, fields};
    static const RudaManagedRecordDescriptor wrong_descriptor = {2, fields};
    RudaFailAllocator state;
    RudaManagedHeap heap;
    RudaManagedReference record = ruda_managed_invalid_reference();
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedReference stale = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&record, &array};
    RudaManagedRootFrame roots;
    RudaManagedRecordView view;
    RudaManagedRecordView rejected;
    RudaManagedCell initial[2];
    RudaManagedCell observed;
    size_t attempts = 0;
    memset(&roots, 0, sizeof(roots));
    memset(&view, 0, sizeof(view));
    memset(&rejected, 0xff, sizeof(rejected));
    memset(initial, 0, sizeof(initial));
    memset(&observed, 0, sizeof(observed));
    initial[0].f64_bits = UINT64_C(7);
    initial[1].f64_bits = UINT64_C(11);
    ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
    RUDA_EXPECT(ruda_test_record_view_heap_init(
        &heap, ruda_fail_allocator(&state)
    ));
    RUDA_EXPECT(ruda_managed_record_new(
        &heap, &descriptor, initial, 2, &record
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_managed_array_f64_descriptor, NULL, 0, &array
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(
        &heap, &roots, locations, 2
    ) == RUDA_MANAGED_OK);

    attempts = state.attempts;
    state.failure_index = attempts;
    RUDA_EXPECT(ruda_managed_record_view_bind(
        &heap, record, &descriptor, &view
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(state.attempts == attempts);
    RUDA_EXPECT(view.object != NULL && view.fields != NULL
        && view.descriptor == &descriptor
        && ruda_managed_reference_equal(view.reference, record));
    RUDA_EXPECT(view.fields[0].f64_bits == UINT64_C(7)
        && view.fields[1].f64_bits == UINT64_C(11));
    view.fields[0].f64_bits = UINT64_C(13);
    RUDA_EXPECT(ruda_managed_record_get(&heap, record, 0, &observed)
        == RUDA_MANAGED_OK && observed.f64_bits == UINT64_C(13));
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(view.fields[0].f64_bits == UINT64_C(13));

    RUDA_EXPECT(ruda_managed_record_view_bind(
        &heap, record, &wrong_descriptor, &rejected
    ) == RUDA_MANAGED_TYPE_MISMATCH);
    RUDA_EXPECT(rejected.object == NULL && rejected.fields == NULL);
    RUDA_EXPECT(ruda_managed_record_view_bind(
        &heap, array, &descriptor, &rejected
    ) == RUDA_MANAGED_TYPE_MISMATCH);
    stale = record;
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &roots) == RUDA_MANAGED_OK);
    record = ruda_managed_invalid_reference();
    array = ruda_managed_invalid_reference();
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_record_view_bind(
        &heap, stale, &descriptor, &rejected
    ) == RUDA_MANAGED_INVALID_REFERENCE);
    ruda_managed_heap_dispose(&heap);
    RUDA_EXPECT(state.live_allocations == 0);
    RUDA_EXPECT(state.live_bytes == 0);
}
