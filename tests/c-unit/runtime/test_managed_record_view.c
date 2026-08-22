#include "../test_support.h"

#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"

static bool mmay_test_record_view_heap_init(
    MarimayManagedHeap *heap,
    MarimayAllocator allocator
) {
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    options.max_objects = 16;
    options.max_live_bytes = 16U * 1024U;
    options.max_array_length = 16;
    options.collect_before_allocation = false;
    return mmay_managed_heap_init(heap, allocator, options);
}

void mmay_test_managed_record_view(void) {
    static const MarimayManagedFieldDescriptor fields[] = {
        {MMAY_MANAGED_CELL_RAW64, true, MMAY_MANAGED_OBJECT_INVALID, NULL},
        {MMAY_MANAGED_CELL_RAW64, false, MMAY_MANAGED_OBJECT_INVALID, NULL}
    };
    static const MarimayManagedRecordDescriptor descriptor = {2, fields};
    static const MarimayManagedRecordDescriptor wrong_descriptor = {2, fields};
    MarimayFailAllocator state;
    MarimayManagedHeap heap;
    MarimayManagedReference record = mmay_managed_invalid_reference();
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedReference stale = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&record, &array};
    MarimayManagedRootFrame roots;
    MarimayManagedRecordView view;
    MarimayManagedRecordView rejected;
    MarimayManagedCell initial[2];
    MarimayManagedCell observed;
    size_t attempts = 0;
    memset(&roots, 0, sizeof(roots));
    memset(&view, 0, sizeof(view));
    memset(&rejected, 0xff, sizeof(rejected));
    memset(initial, 0, sizeof(initial));
    memset(&observed, 0, sizeof(observed));
    initial[0].f64_bits = UINT64_C(7);
    initial[1].f64_bits = UINT64_C(11);
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_test_record_view_heap_init(
        &heap, mmay_fail_allocator(&state)
    ));
    MMAY_EXPECT(mmay_managed_record_new(
        &heap, &descriptor, initial, 2, &record
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_managed_array_f64_descriptor, NULL, 0, &array
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(
        &heap, &roots, locations, 2
    ) == MMAY_MANAGED_OK);

    attempts = state.attempts;
    state.failure_index = attempts;
    MMAY_EXPECT(mmay_managed_record_view_bind(
        &heap, record, &descriptor, &view
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(state.attempts == attempts);
    MMAY_EXPECT(view.object != NULL && view.fields != NULL
        && view.descriptor == &descriptor
        && mmay_managed_reference_equal(view.reference, record));
    MMAY_EXPECT(view.fields[0].f64_bits == UINT64_C(7)
        && view.fields[1].f64_bits == UINT64_C(11));
    view.fields[0].f64_bits = UINT64_C(13);
    MMAY_EXPECT(mmay_managed_record_get(&heap, record, 0, &observed)
        == MMAY_MANAGED_OK && observed.f64_bits == UINT64_C(13));
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(view.fields[0].f64_bits == UINT64_C(13));

    MMAY_EXPECT(mmay_managed_record_view_bind(
        &heap, record, &wrong_descriptor, &rejected
    ) == MMAY_MANAGED_TYPE_MISMATCH);
    MMAY_EXPECT(rejected.object == NULL && rejected.fields == NULL);
    MMAY_EXPECT(mmay_managed_record_view_bind(
        &heap, array, &descriptor, &rejected
    ) == MMAY_MANAGED_TYPE_MISMATCH);
    stale = record;
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &roots) == MMAY_MANAGED_OK);
    record = mmay_managed_invalid_reference();
    array = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_record_view_bind(
        &heap, stale, &descriptor, &rejected
    ) == MMAY_MANAGED_INVALID_REFERENCE);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
}
