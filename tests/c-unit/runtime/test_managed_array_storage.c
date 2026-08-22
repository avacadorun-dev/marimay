#include "../test_support.h"

#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/runtime/managed.h"

#include "../../../runtime/common/managed_internal.h"

static const RudaManagedArrayDescriptor ruda_test_storage_raw8 = {
    RUDA_MANAGED_CELL_RAW64,
    RUDA_MANAGED_ARRAY_STORAGE_RAW8,
    sizeof(uint8_t),
    RUDA_MANAGED_OBJECT_INVALID,
    NULL
};

static const RudaManagedArrayDescriptor ruda_test_storage_raw16 = {
    RUDA_MANAGED_CELL_RAW64,
    RUDA_MANAGED_ARRAY_STORAGE_RAW16,
    sizeof(uint16_t),
    RUDA_MANAGED_OBJECT_INVALID,
    NULL
};

static const RudaManagedArrayDescriptor ruda_test_storage_raw32 = {
    RUDA_MANAGED_CELL_RAW64,
    RUDA_MANAGED_ARRAY_STORAGE_RAW32,
    sizeof(uint32_t),
    RUDA_MANAGED_OBJECT_INVALID,
    NULL
};

static const RudaManagedArrayDescriptor ruda_test_storage_raw64 = {
    RUDA_MANAGED_CELL_RAW64,
    RUDA_MANAGED_ARRAY_STORAGE_RAW64,
    sizeof(uint64_t),
    RUDA_MANAGED_OBJECT_INVALID,
    NULL
};

static const RudaManagedArrayDescriptor ruda_test_storage_recursive = {
    RUDA_MANAGED_CELL_REFERENCE,
    RUDA_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(RudaManagedCell),
    RUDA_MANAGED_OBJECT_ARRAY,
    &ruda_test_storage_recursive
};

static RudaManagedCell ruda_test_storage_raw(uint64_t bits) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static RudaManagedCell ruda_test_storage_reference(
    RudaManagedReference reference
) {
    RudaManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.reference = reference;
    return cell;
}

static bool ruda_test_storage_heap_init(
    RudaManagedHeap *heap,
    RudaAllocator allocator,
    size_t max_live_bytes,
    bool collect_before_allocation
) {
    RudaManagedHeapOptions options = ruda_managed_heap_default_options();
    options.max_objects = 128;
    options.max_live_bytes = max_live_bytes;
    options.max_array_length = 256;
    options.collect_before_allocation = collect_before_allocation;
    return ruda_managed_heap_init(heap, allocator, options);
}

static void ruda_test_storage_descriptor_contract(void) {
    RudaManagedHeap heap;
    RudaFailAllocator state;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedArrayDescriptor invalid[] = {
        {RUDA_MANAGED_CELL_RAW64, RUDA_MANAGED_ARRAY_STORAGE_INVALID, 0,
            RUDA_MANAGED_OBJECT_INVALID, NULL},
        {RUDA_MANAGED_CELL_RAW64, RUDA_MANAGED_ARRAY_STORAGE_RAW32,
            sizeof(uint64_t), RUDA_MANAGED_OBJECT_INVALID, NULL},
        {RUDA_MANAGED_CELL_REFERENCE, RUDA_MANAGED_ARRAY_STORAGE_RAW64,
            sizeof(uint64_t), RUDA_MANAGED_OBJECT_ARRAY,
            &ruda_test_storage_recursive},
        {RUDA_MANAGED_CELL_RAW64, RUDA_MANAGED_ARRAY_STORAGE_CELL,
            sizeof(RudaManagedCell), RUDA_MANAGED_OBJECT_INVALID, NULL},
        {RUDA_MANAGED_CELL_REFERENCE, RUDA_MANAGED_ARRAY_STORAGE_CELL,
            sizeof(RudaManagedCell), RUDA_MANAGED_OBJECT_INVALID, NULL}
    };
    size_t index = 0;
    RUDA_EXPECT(sizeof(RudaManagedCell) >= sizeof(RudaManagedReference));
    if (sizeof(void *) == 8) {
        RUDA_EXPECT(sizeof(RudaManagedCell) == 16);
    }
    RUDA_EXPECT(ruda_managed_array_f64_descriptor.storage_kind
        == RUDA_MANAGED_ARRAY_STORAGE_RAW64);
    RUDA_EXPECT(ruda_managed_array_f64_descriptor.storage_stride == 8);
    RUDA_EXPECT(ruda_managed_array_array_f64_descriptor.storage_kind
        == RUDA_MANAGED_ARRAY_STORAGE_CELL);
    RUDA_EXPECT(ruda_managed_array_array_f64_descriptor.storage_stride
        == sizeof(RudaManagedCell));
    RUDA_EXPECT(ruda_managed_array_storage_alignment(
        &ruda_test_storage_raw8
    ) == 1);
    RUDA_EXPECT(ruda_managed_array_storage_alignment(
        &ruda_test_storage_recursive
    ) == _Alignof(RudaManagedCell));
    ruda_fail_allocator_init(&state, ruda_system_allocator(), 0);
    RUDA_EXPECT(ruda_test_storage_heap_init(
        &heap, ruda_fail_allocator(&state), 64U * 1024U, false
    ));
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, &invalid[index], NULL, 0, &array
        ) == RUDA_MANAGED_INVALID_ARGUMENT);
        RUDA_EXPECT(ruda_managed_reference_is_invalid(array));
    }
    RUDA_EXPECT(state.attempts == 0);
    ruda_managed_heap_dispose(&heap);
    RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    {
        RudaManagedArrayDescriptor mutable_descriptor = ruda_test_storage_raw64;
        RudaManagedCell initial = ruda_test_storage_raw(UINT64_MAX);
        RudaManagedCell observed;
        size_t length = 0;
        RUDA_EXPECT(ruda_test_storage_heap_init(
            &heap, ruda_system_allocator(), 64U * 1024U, false
        ));
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, &mutable_descriptor, &initial, 1, &array
        ) == RUDA_MANAGED_OK);
        mutable_descriptor.storage_stride = sizeof(uint32_t);
        RUDA_EXPECT(ruda_managed_array_access_get(
            &heap, array, &mutable_descriptor, 0,
            RUDA_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
        ) == RUDA_MANAGED_INVALID_ARGUMENT);
        mutable_descriptor.storage_stride = sizeof(uint64_t);
        ruda_managed_heap_dispose(&heap);
    }
}

static void ruda_test_storage_round_trip_case(
    const RudaManagedArrayDescriptor *descriptor,
    const uint64_t patterns[5]
) {
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedReference alias = ruda_managed_invalid_reference();
    RudaManagedCell cells[5];
    RudaManagedCell observed;
    RudaManagedCell popped;
    RudaManagedArray *private_array = NULL;
    RudaManagedSlot *slot = NULL;
    bool did_pop = false;
    size_t length = 0;
    size_t index = 0;
    for (index = 0; index < 5; index += 1) {
        cells[index] = ruda_test_storage_raw(patterns[index]);
    }
    RUDA_EXPECT(ruda_test_storage_heap_init(
        &heap, ruda_system_allocator(), 64U * 1024U, false
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, descriptor, cells, 5, &array
    ) == RUDA_MANAGED_OK);
    alias = array;
    slot = ruda_managed_resolve(&heap, array);
    RUDA_EXPECT(slot != NULL && slot->as.array != NULL);
    private_array = slot->as.array;
    RUDA_EXPECT(private_array->capacity == 5);
    RUDA_EXPECT(ruda_managed_array_storage_bytes(private_array)
        == 5 * descriptor->storage_stride);
    RUDA_EXPECT(ruda_managed_slot_object_bytes(slot)
        == sizeof(RudaManagedArray) + 5 * descriptor->storage_stride);
    for (index = 0; index < 5; index += 1) {
        RUDA_EXPECT(ruda_managed_array_get(
            &heap, alias, (int64_t)index, &observed, &length
        ) == RUDA_MANAGED_OK);
        RUDA_EXPECT(length == 5 && observed.f64_bits == patterns[index]);
    }
    RUDA_EXPECT(ruda_managed_array_set(
        &heap, array, 1, cells[4], &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_get(
        &heap, alias, 1, &observed, &length
    ) == RUDA_MANAGED_OK && observed.f64_bits == patterns[4]);
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, 6)
        == RUDA_MANAGED_OK);
    private_array = ruda_managed_resolve(&heap, array)->as.array;
    for (index = private_array->length * descriptor->storage_stride;
        index < ruda_managed_array_storage_bytes(private_array); index += 1) {
        RUDA_EXPECT(private_array->storage[index] == 0);
    }
    RUDA_EXPECT(ruda_managed_array_pop(
        &heap, array, &popped, &did_pop
    ) == RUDA_MANAGED_OK && did_pop && popped.f64_bits == patterns[4]);
    private_array = ruda_managed_resolve(&heap, array)->as.array;
    for (index = private_array->length * descriptor->storage_stride;
        index < (private_array->length + 1) * descriptor->storage_stride;
        index += 1) {
        RUDA_EXPECT(private_array->storage[index] == 0);
    }
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_storage_all_raw_bits(void) {
    static const uint64_t raw8[] = {0, 1, 0x7f, 0x80, 0xff};
    static const uint64_t raw16[] = {0, 1, 0x7fff, 0x8000, 0xffff};
    static const uint64_t raw32[] = {
        0, 1, UINT64_C(0x7fffffff), UINT64_C(0x80000000),
        UINT64_C(0xffc01234)
    };
    static const uint64_t raw64[] = {
        0, UINT64_C(0x8000000000000000), UINT64_C(0x7ff0000000000001),
        UINT64_C(0x7ff8000000000042), UINT64_MAX
    };
    ruda_test_storage_round_trip_case(&ruda_test_storage_raw8, raw8);
    ruda_test_storage_round_trip_case(&ruda_test_storage_raw16, raw16);
    ruda_test_storage_round_trip_case(&ruda_test_storage_raw32, raw32);
    ruda_test_storage_round_trip_case(&ruda_test_storage_raw64, raw64);
}

static void ruda_test_storage_tracing_boundary(void) {
    RudaManagedHeap heap;
    RudaManagedReference raw = ruda_managed_invalid_reference();
    RudaManagedReference outer = ruda_managed_invalid_reference();
    RudaManagedReference retained = ruda_managed_invalid_reference();
    RudaManagedReference hidden = ruda_managed_invalid_reference();
    RudaManagedReference hidden_stale = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {&raw, &outer};
    RudaManagedRootFrame roots;
    RudaManagedArray *private_outer = NULL;
    RudaManagedCell raw_cell = ruda_test_storage_raw(UINT64_C(0x100000001));
    RudaManagedCell reference_cell;
    size_t length = 0;
    memset(&roots, 0, sizeof(roots));
    RUDA_EXPECT(ruda_test_storage_heap_init(
        &heap, ruda_system_allocator(), 64U * 1024U, false
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_storage_raw64, &raw_cell, 1, &raw
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_storage_recursive, NULL, 0, &outer
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(&heap, &roots, locations, 2)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_reserve(&heap, outer, 2)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_storage_recursive, NULL, 0, &hidden
    ) == RUDA_MANAGED_OK);
    hidden_stale = hidden;
    private_outer = ruda_managed_resolve(&heap, outer)->as.array;
    RUDA_EXPECT(ruda_managed_array_storage_store(
        private_outer, 1, ruda_test_storage_reference(hidden)
    ) == RUDA_MANAGED_OK);
    hidden = ruda_managed_invalid_reference();
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_length(&heap, hidden_stale, &length)
        == RUDA_MANAGED_INVALID_REFERENCE);
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_storage_recursive, NULL, 0, &retained
    ) == RUDA_MANAGED_OK);
    reference_cell = ruda_test_storage_reference(retained);
    RUDA_EXPECT(ruda_managed_array_push(&heap, outer, reference_cell)
        == RUDA_MANAGED_OK);
    retained = ruda_managed_invalid_reference();
    RUDA_EXPECT(ruda_managed_heap_collect(&heap) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_get(
        &heap, outer, 0, &reference_cell, &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_length(
        &heap, reference_cell.reference, &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(((uintptr_t)private_outer->storage
        % _Alignof(RudaManagedCell)) == 0);
    RUDA_EXPECT(ruda_managed_root_pop(&heap, &roots) == RUDA_MANAGED_OK);
    ruda_managed_heap_dispose(&heap);
}

static void ruda_test_storage_budget_density(void) {
    const RudaManagedArrayDescriptor *descriptors[] = {
        &ruda_test_storage_raw8,
        &ruda_test_storage_raw16,
        &ruda_test_storage_raw32,
        &ruda_test_storage_raw64
    };
    RudaManagedHeap heap;
    RudaManagedReference array = ruda_managed_invalid_reference();
    RudaManagedCell cells[65];
    size_t base = 0;
    size_t descriptor_index = 0;
    size_t index = 0;
    for (index = 0; index < 65; index += 1) {
        cells[index] = ruda_test_storage_raw((uint64_t)index);
    }
    RUDA_EXPECT(ruda_test_storage_heap_init(
        &heap, ruda_system_allocator(), 64U * 1024U, false
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &heap, &ruda_test_storage_raw64, NULL, 0, &array
    ) == RUDA_MANAGED_OK);
    base = ruda_managed_heap_live_bytes(&heap);
    ruda_managed_heap_dispose(&heap);
    for (descriptor_index = 0;
        descriptor_index < sizeof(descriptors) / sizeof(descriptors[0]);
        descriptor_index += 1) {
        size_t count = 64 / descriptors[descriptor_index]->storage_stride;
        RUDA_EXPECT(ruda_test_storage_heap_init(
            &heap, ruda_system_allocator(), base + 64, false
        ));
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, descriptors[descriptor_index], cells, count, &array
        ) == RUDA_MANAGED_OK);
        RUDA_EXPECT(ruda_managed_heap_live_bytes(&heap) == base + 64);
        ruda_managed_heap_dispose(&heap);
        RUDA_EXPECT(ruda_test_storage_heap_init(
            &heap, ruda_system_allocator(), base + 64, false
        ));
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, descriptors[descriptor_index], cells, count + 1, &array
        ) == RUDA_MANAGED_LIMIT_REACHED);
        ruda_managed_heap_dispose(&heap);
    }
}

static void ruda_test_storage_failure_sweeps(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        RudaFailAllocator state;
        RudaManagedHeap heap;
        RudaManagedReference array = ruda_managed_invalid_reference();
        RudaManagedCell cells[] = {
            ruda_test_storage_raw(1),
            ruda_test_storage_raw(UINT32_MAX),
            ruda_test_storage_raw(UINT64_C(0x80000000))
        };
        RudaManagedStatus status;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        RUDA_EXPECT(ruda_test_storage_heap_init(
            &heap, ruda_fail_allocator(&state), 64U * 1024U, true
        ));
        status = ruda_managed_array_new(
            &heap, &ruda_test_storage_raw32, cells, 3, &array
        );
        RUDA_EXPECT(status == RUDA_MANAGED_OK
            || status == RUDA_MANAGED_ALLOCATION_FAILED);
        reached_success = status == RUDA_MANAGED_OK;
        ruda_managed_heap_dispose(&heap);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    {
        RudaFailAllocator state;
        RudaManagedHeap heap;
        RudaManagedReference array = ruda_managed_invalid_reference();
        RudaManagedCell initial = ruda_test_storage_raw(UINT8_MAX);
        RudaManagedArray *private_array = NULL;
        size_t live = 0;
        size_t capacity = 0;
        size_t length = 0;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_storage_heap_init(
            &heap, ruda_fail_allocator(&state), 64U * 1024U, false
        ));
        RUDA_EXPECT(ruda_managed_array_new(
            &heap, &ruda_test_storage_raw8, &initial, 1, &array
        ) == RUDA_MANAGED_OK);
        private_array = ruda_managed_resolve(&heap, array)->as.array;
        live = ruda_managed_heap_live_bytes(&heap);
        capacity = private_array->capacity;
        state.failure_index = state.attempts;
        RUDA_EXPECT(ruda_managed_array_reserve(&heap, array, 8)
            == RUDA_MANAGED_ALLOCATION_FAILED);
        RUDA_EXPECT(ruda_managed_heap_live_bytes(&heap) == live);
        private_array = ruda_managed_resolve(&heap, array)->as.array;
        RUDA_EXPECT(private_array->capacity == capacity);
        RUDA_EXPECT(ruda_managed_array_length(&heap, array, &length)
            == RUDA_MANAGED_OK && length == 1);
        RUDA_EXPECT(private_array->storage[0] == UINT8_MAX);
        ruda_managed_heap_dispose(&heap);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}

void ruda_test_managed_array_storage(void) {
    ruda_test_storage_descriptor_contract();
    ruda_test_storage_all_raw_bits();
    ruda_test_storage_tracing_boundary();
    ruda_test_storage_budget_density();
    ruda_test_storage_failure_sweeps();
}
