#include "../test_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"

#include "../../../runtime/common/managed_internal.h"

static const MarimayManagedArrayDescriptor mmay_test_storage_raw8 = {
    MMAY_MANAGED_CELL_RAW64,
    MMAY_MANAGED_ARRAY_STORAGE_RAW8,
    sizeof(uint8_t),
    MMAY_MANAGED_OBJECT_INVALID,
    NULL
};

static const MarimayManagedArrayDescriptor mmay_test_storage_raw16 = {
    MMAY_MANAGED_CELL_RAW64,
    MMAY_MANAGED_ARRAY_STORAGE_RAW16,
    sizeof(uint16_t),
    MMAY_MANAGED_OBJECT_INVALID,
    NULL
};

static const MarimayManagedArrayDescriptor mmay_test_storage_raw32 = {
    MMAY_MANAGED_CELL_RAW64,
    MMAY_MANAGED_ARRAY_STORAGE_RAW32,
    sizeof(uint32_t),
    MMAY_MANAGED_OBJECT_INVALID,
    NULL
};

static const MarimayManagedArrayDescriptor mmay_test_storage_raw64 = {
    MMAY_MANAGED_CELL_RAW64,
    MMAY_MANAGED_ARRAY_STORAGE_RAW64,
    sizeof(uint64_t),
    MMAY_MANAGED_OBJECT_INVALID,
    NULL
};

static const MarimayManagedArrayDescriptor mmay_test_storage_recursive = {
    MMAY_MANAGED_CELL_REFERENCE,
    MMAY_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(MarimayManagedCell),
    MMAY_MANAGED_OBJECT_ARRAY,
    &mmay_test_storage_recursive
};

static MarimayManagedCell mmay_test_storage_raw(uint64_t bits) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.f64_bits = bits;
    return cell;
}

static MarimayManagedCell mmay_test_storage_reference(
    MarimayManagedReference reference
) {
    MarimayManagedCell cell;
    memset(&cell, 0, sizeof(cell));
    cell.reference = reference;
    return cell;
}

static bool mmay_test_storage_heap_init(
    MarimayManagedHeap *heap,
    MarimayAllocator allocator,
    size_t max_live_bytes,
    bool collect_before_allocation
) {
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    options.max_objects = 128;
    options.max_live_bytes = max_live_bytes;
    options.max_array_length = 256;
    options.collect_before_allocation = collect_before_allocation;
    return mmay_managed_heap_init(heap, allocator, options);
}

static void mmay_test_storage_descriptor_contract(void) {
    MarimayManagedHeap heap;
    MarimayFailAllocator state;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedArrayDescriptor invalid[] = {
        {MMAY_MANAGED_CELL_RAW64, MMAY_MANAGED_ARRAY_STORAGE_INVALID, 0,
            MMAY_MANAGED_OBJECT_INVALID, NULL},
        {MMAY_MANAGED_CELL_RAW64, MMAY_MANAGED_ARRAY_STORAGE_RAW32,
            sizeof(uint64_t), MMAY_MANAGED_OBJECT_INVALID, NULL},
        {MMAY_MANAGED_CELL_REFERENCE, MMAY_MANAGED_ARRAY_STORAGE_RAW64,
            sizeof(uint64_t), MMAY_MANAGED_OBJECT_ARRAY,
            &mmay_test_storage_recursive},
        {MMAY_MANAGED_CELL_RAW64, MMAY_MANAGED_ARRAY_STORAGE_CELL,
            sizeof(MarimayManagedCell), MMAY_MANAGED_OBJECT_INVALID, NULL},
        {MMAY_MANAGED_CELL_REFERENCE, MMAY_MANAGED_ARRAY_STORAGE_CELL,
            sizeof(MarimayManagedCell), MMAY_MANAGED_OBJECT_INVALID, NULL}
    };
    size_t index = 0;
    MMAY_EXPECT(sizeof(MarimayManagedCell) >= sizeof(MarimayManagedReference));
    if (sizeof(void *) == 8) {
        MMAY_EXPECT(sizeof(MarimayManagedCell) == 16);
    }
    MMAY_EXPECT(mmay_managed_array_f64_descriptor.storage_kind
        == MMAY_MANAGED_ARRAY_STORAGE_RAW64);
    MMAY_EXPECT(mmay_managed_array_f64_descriptor.storage_stride == 8);
    MMAY_EXPECT(mmay_managed_array_array_f64_descriptor.storage_kind
        == MMAY_MANAGED_ARRAY_STORAGE_CELL);
    MMAY_EXPECT(mmay_managed_array_array_f64_descriptor.storage_stride
        == sizeof(MarimayManagedCell));
    MMAY_EXPECT(mmay_managed_array_storage_alignment(
        &mmay_test_storage_raw8
    ) == 1);
    MMAY_EXPECT(mmay_managed_array_storage_alignment(
        &mmay_test_storage_recursive
    ) == _Alignof(MarimayManagedCell));
    mmay_fail_allocator_init(&state, mmay_system_allocator(), 0);
    MMAY_EXPECT(mmay_test_storage_heap_init(
        &heap, mmay_fail_allocator(&state), 64U * 1024U, false
    ));
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &invalid[index], NULL, 0, &array
        ) == MMAY_MANAGED_INVALID_ARGUMENT);
        MMAY_EXPECT(mmay_managed_reference_is_invalid(array));
    }
    MMAY_EXPECT(state.attempts == 0);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    {
        MarimayManagedArrayDescriptor mutable_descriptor = mmay_test_storage_raw64;
        MarimayManagedCell initial = mmay_test_storage_raw(UINT64_MAX);
        MarimayManagedCell observed;
        size_t length = 0;
        MMAY_EXPECT(mmay_test_storage_heap_init(
            &heap, mmay_system_allocator(), 64U * 1024U, false
        ));
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mutable_descriptor, &initial, 1, &array
        ) == MMAY_MANAGED_OK);
        mutable_descriptor.storage_stride = sizeof(uint32_t);
        MMAY_EXPECT(mmay_managed_array_access_get(
            &heap, array, &mutable_descriptor, 0,
            MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length
        ) == MMAY_MANAGED_INVALID_ARGUMENT);
        mutable_descriptor.storage_stride = sizeof(uint64_t);
        mmay_managed_heap_dispose(&heap);
    }
}

static void mmay_test_storage_round_trip_case(
    const MarimayManagedArrayDescriptor *descriptor,
    const uint64_t patterns[5]
) {
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedReference alias = mmay_managed_invalid_reference();
    MarimayManagedCell cells[5];
    MarimayManagedCell observed;
    MarimayManagedCell popped;
    MarimayManagedArray *private_array = NULL;
    MarimayManagedSlot *slot = NULL;
    bool did_pop = false;
    size_t length = 0;
    size_t index = 0;
    for (index = 0; index < 5; index += 1) {
        cells[index] = mmay_test_storage_raw(patterns[index]);
    }
    MMAY_EXPECT(mmay_test_storage_heap_init(
        &heap, mmay_system_allocator(), 64U * 1024U, false
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, descriptor, cells, 5, &array
    ) == MMAY_MANAGED_OK);
    alias = array;
    slot = mmay_managed_resolve(&heap, array);
    MMAY_EXPECT(slot != NULL && slot->as.array != NULL);
    private_array = slot->as.array;
    MMAY_EXPECT(private_array->capacity == 5);
    MMAY_EXPECT(mmay_managed_array_storage_bytes(private_array)
        == 5 * descriptor->storage_stride);
    MMAY_EXPECT(mmay_managed_slot_object_bytes(slot)
        == sizeof(MarimayManagedArray) + 5 * descriptor->storage_stride);
    for (index = 0; index < 5; index += 1) {
        MMAY_EXPECT(mmay_managed_array_get(
            &heap, alias, (int64_t)index, &observed, &length
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(length == 5 && observed.f64_bits == patterns[index]);
    }
    MMAY_EXPECT(mmay_managed_array_set(
        &heap, array, 1, cells[4], &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_get(
        &heap, alias, 1, &observed, &length
    ) == MMAY_MANAGED_OK && observed.f64_bits == patterns[4]);
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, 6)
        == MMAY_MANAGED_OK);
    private_array = mmay_managed_resolve(&heap, array)->as.array;
    for (index = private_array->length * descriptor->storage_stride;
        index < mmay_managed_array_storage_bytes(private_array); index += 1) {
        MMAY_EXPECT(private_array->storage[index] == 0);
    }
    MMAY_EXPECT(mmay_managed_array_pop(
        &heap, array, &popped, &did_pop
    ) == MMAY_MANAGED_OK && did_pop && popped.f64_bits == patterns[4]);
    private_array = mmay_managed_resolve(&heap, array)->as.array;
    for (index = private_array->length * descriptor->storage_stride;
        index < (private_array->length + 1) * descriptor->storage_stride;
        index += 1) {
        MMAY_EXPECT(private_array->storage[index] == 0);
    }
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_storage_all_raw_bits(void) {
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
    mmay_test_storage_round_trip_case(&mmay_test_storage_raw8, raw8);
    mmay_test_storage_round_trip_case(&mmay_test_storage_raw16, raw16);
    mmay_test_storage_round_trip_case(&mmay_test_storage_raw32, raw32);
    mmay_test_storage_round_trip_case(&mmay_test_storage_raw64, raw64);
}

static void mmay_test_storage_tracing_boundary(void) {
    MarimayManagedHeap heap;
    MarimayManagedReference raw = mmay_managed_invalid_reference();
    MarimayManagedReference outer = mmay_managed_invalid_reference();
    MarimayManagedReference retained = mmay_managed_invalid_reference();
    MarimayManagedReference hidden = mmay_managed_invalid_reference();
    MarimayManagedReference hidden_stale = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&raw, &outer};
    MarimayManagedRootFrame roots;
    MarimayManagedArray *private_outer = NULL;
    MarimayManagedCell raw_cell = mmay_test_storage_raw(UINT64_C(0x100000001));
    MarimayManagedCell reference_cell;
    size_t length = 0;
    memset(&roots, 0, sizeof(roots));
    MMAY_EXPECT(mmay_test_storage_heap_init(
        &heap, mmay_system_allocator(), 64U * 1024U, false
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_storage_raw64, &raw_cell, 1, &raw
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_storage_recursive, NULL, 0, &outer
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(&heap, &roots, locations, 2)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_reserve(&heap, outer, 2)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_storage_recursive, NULL, 0, &hidden
    ) == MMAY_MANAGED_OK);
    hidden_stale = hidden;
    private_outer = mmay_managed_resolve(&heap, outer)->as.array;
    MMAY_EXPECT(mmay_managed_array_storage_store(
        private_outer, 1, mmay_test_storage_reference(hidden)
    ) == MMAY_MANAGED_OK);
    hidden = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_length(&heap, hidden_stale, &length)
        == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_storage_recursive, NULL, 0, &retained
    ) == MMAY_MANAGED_OK);
    reference_cell = mmay_test_storage_reference(retained);
    MMAY_EXPECT(mmay_managed_array_push(&heap, outer, reference_cell)
        == MMAY_MANAGED_OK);
    retained = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_get(
        &heap, outer, 0, &reference_cell, &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_length(
        &heap, reference_cell.reference, &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(((uintptr_t)private_outer->storage
        % _Alignof(MarimayManagedCell)) == 0);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &roots) == MMAY_MANAGED_OK);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_storage_budget_density(void) {
    const MarimayManagedArrayDescriptor *descriptors[] = {
        &mmay_test_storage_raw8,
        &mmay_test_storage_raw16,
        &mmay_test_storage_raw32,
        &mmay_test_storage_raw64
    };
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedCell cells[65];
    size_t base = 0;
    size_t descriptor_index = 0;
    size_t index = 0;
    for (index = 0; index < 65; index += 1) {
        cells[index] = mmay_test_storage_raw((uint64_t)index);
    }
    MMAY_EXPECT(mmay_test_storage_heap_init(
        &heap, mmay_system_allocator(), 64U * 1024U, false
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &heap, &mmay_test_storage_raw64, NULL, 0, &array
    ) == MMAY_MANAGED_OK);
    base = mmay_managed_heap_live_bytes(&heap);
    mmay_managed_heap_dispose(&heap);
    for (descriptor_index = 0;
        descriptor_index < sizeof(descriptors) / sizeof(descriptors[0]);
        descriptor_index += 1) {
        size_t count = 64 / descriptors[descriptor_index]->storage_stride;
        MMAY_EXPECT(mmay_test_storage_heap_init(
            &heap, mmay_system_allocator(), base + 64, false
        ));
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, descriptors[descriptor_index], cells, count, &array
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_heap_live_bytes(&heap) == base + 64);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(mmay_test_storage_heap_init(
            &heap, mmay_system_allocator(), base + 64, false
        ));
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, descriptors[descriptor_index], cells, count + 1, &array
        ) == MMAY_MANAGED_LIMIT_REACHED);
        mmay_managed_heap_dispose(&heap);
    }
}

static void mmay_test_storage_failure_sweeps(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayManagedHeap heap;
        MarimayManagedReference array = mmay_managed_invalid_reference();
        MarimayManagedCell cells[] = {
            mmay_test_storage_raw(1),
            mmay_test_storage_raw(UINT32_MAX),
            mmay_test_storage_raw(UINT64_C(0x80000000))
        };
        MarimayManagedStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        MMAY_EXPECT(mmay_test_storage_heap_init(
            &heap, mmay_fail_allocator(&state), 64U * 1024U, true
        ));
        status = mmay_managed_array_new(
            &heap, &mmay_test_storage_raw32, cells, 3, &array
        );
        MMAY_EXPECT(status == MMAY_MANAGED_OK
            || status == MMAY_MANAGED_ALLOCATION_FAILED);
        reached_success = status == MMAY_MANAGED_OK;
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    {
        MarimayFailAllocator state;
        MarimayManagedHeap heap;
        MarimayManagedReference array = mmay_managed_invalid_reference();
        MarimayManagedCell initial = mmay_test_storage_raw(UINT8_MAX);
        MarimayManagedArray *private_array = NULL;
        size_t live = 0;
        size_t capacity = 0;
        size_t length = 0;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_storage_heap_init(
            &heap, mmay_fail_allocator(&state), 64U * 1024U, false
        ));
        MMAY_EXPECT(mmay_managed_array_new(
            &heap, &mmay_test_storage_raw8, &initial, 1, &array
        ) == MMAY_MANAGED_OK);
        private_array = mmay_managed_resolve(&heap, array)->as.array;
        live = mmay_managed_heap_live_bytes(&heap);
        capacity = private_array->capacity;
        state.failure_index = state.attempts;
        MMAY_EXPECT(mmay_managed_array_reserve(&heap, array, 8)
            == MMAY_MANAGED_ALLOCATION_FAILED);
        MMAY_EXPECT(mmay_managed_heap_live_bytes(&heap) == live);
        private_array = mmay_managed_resolve(&heap, array)->as.array;
        MMAY_EXPECT(private_array->capacity == capacity);
        MMAY_EXPECT(mmay_managed_array_length(&heap, array, &length)
            == MMAY_MANAGED_OK && length == 1);
        MMAY_EXPECT(private_array->storage[0] == UINT8_MAX);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}

void mmay_test_managed_array_storage(void) {
    mmay_test_storage_descriptor_contract();
    mmay_test_storage_all_raw_bits();
    mmay_test_storage_tracing_boundary();
    mmay_test_storage_budget_density();
    mmay_test_storage_failure_sweeps();
}
