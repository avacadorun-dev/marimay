#include "../test_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"
#include "../../../runtime/common/managed_internal.h"

static const MarimayManagedArrayDescriptor mmay_edit_descriptors[] = {
    {MMAY_MANAGED_CELL_RAW64, MMAY_MANAGED_ARRAY_STORAGE_RAW8, 1,
        MMAY_MANAGED_OBJECT_INVALID, NULL},
    {MMAY_MANAGED_CELL_RAW64, MMAY_MANAGED_ARRAY_STORAGE_RAW16, 2,
        MMAY_MANAGED_OBJECT_INVALID, NULL},
    {MMAY_MANAGED_CELL_RAW64, MMAY_MANAGED_ARRAY_STORAGE_RAW32, 4,
        MMAY_MANAGED_OBJECT_INVALID, NULL},
    {MMAY_MANAGED_CELL_RAW64, MMAY_MANAGED_ARRAY_STORAGE_RAW64, 8,
        MMAY_MANAGED_OBJECT_INVALID, NULL},
    {MMAY_MANAGED_CELL_REFERENCE, MMAY_MANAGED_ARRAY_STORAGE_CELL, sizeof(MarimayManagedCell),
        MMAY_MANAGED_OBJECT_ARRAY, &mmay_managed_array_f64_descriptor}
};

static bool mmay_edit_heap(MarimayManagedHeap *heap, MarimayFailAllocator *state) {
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    options.max_objects = 64;
    options.max_array_length = 64;
    options.max_live_bytes = 65536;
    options.collect_before_allocation = true;
    return mmay_managed_heap_init(heap, mmay_fail_allocator(state), options);
}

static MarimayManagedCell mmay_edit_raw(uint64_t bits) {
    MarimayManagedCell result;
    memset(&result, 0, sizeof(result));
    result.f64_bits = bits;
    return result;
}

static void mmay_edit_expect_sequence(MarimayManagedHeap *heap,
    MarimayManagedReference reference, const MarimayManagedCell *cells,
    size_t count, bool references) {
    size_t index, length = 99;
    MMAY_EXPECT(mmay_managed_array_length(heap, reference, &length) == MMAY_MANAGED_OK);
    MMAY_EXPECT(length == count);
    for (index = 0; index < count; index += 1) {
        MarimayManagedCell cell;
        MMAY_EXPECT(mmay_managed_array_get(heap, reference, (int64_t)index,
            &cell, &length) == MMAY_MANAGED_OK);
        MMAY_EXPECT(references
            ? memcmp(&cell.reference, &cells[index].reference, sizeof(cell.reference)) == 0
            : cell.f64_bits == cells[index].f64_bits);
    }
}

static void mmay_edit_layout_case(size_t layout) {
    const MarimayManagedArrayDescriptor *descriptor = &mmay_edit_descriptors[layout];
    MarimayFailAllocator state;
    MarimayManagedHeap heap;
    MarimayManagedReference source, copied, child;
    MarimayManagedReference *locations[] = {&source, &copied};
    MarimayManagedRootFrame roots = {0};
    MarimayManagedCell cells[4], expected[3];
    MarimayManagedArray *original, *duplicate;
    size_t index, length, attempts;
    uint64_t generation;
    bool references = descriptor->cell_kind == MMAY_MANAGED_CELL_REFERENCE;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_edit_heap(&heap, &state));
    source = copied = child = mmay_managed_invalid_reference();
    MMAY_EXPECT(mmay_managed_array_new(&heap, &mmay_managed_array_f64_descriptor,
        NULL, 0, &child) == MMAY_MANAGED_OK);
    for (index = 0; index < 4; index += 1) {
        cells[index] = mmay_edit_raw(index == 1
            ? (layout == 0 ? UINT8_MAX : layout == 1 ? UINT16_MAX
                : layout == 2 ? UINT32_MAX : UINT64_C(0x8000000000000000))
            : (uint64_t)(index + 1));
        if (references) cells[index].reference = child;
    }
    MMAY_EXPECT(mmay_managed_array_new(&heap, descriptor, cells, 4, &source)
        == MMAY_MANAGED_OK);
    /* No caller root is present: copy must keep source and its children alive. */
    MMAY_EXPECT(mmay_managed_array_copy(&heap, source, &copied) == MMAY_MANAGED_OK);
    MMAY_EXPECT(source.slot != copied.slot);
    MMAY_EXPECT(mmay_managed_root_push(&heap, &roots, locations, 2) == MMAY_MANAGED_OK);
    original = mmay_managed_resolve(&heap, source)->as.array;
    duplicate = mmay_managed_resolve(&heap, copied)->as.array;
    MMAY_EXPECT(original != duplicate && original->storage != duplicate->storage);
    MMAY_EXPECT(duplicate->capacity == 4 && duplicate->mutation_generation == 0);
    mmay_edit_expect_sequence(&heap, copied, cells, 4, references);
    MMAY_EXPECT(mmay_managed_array_mutation_generation(&heap, source, &generation)
        == MMAY_MANAGED_OK && generation == 0);
    attempts = state.attempts;
    state.failure_index = attempts;
    MMAY_EXPECT(mmay_managed_array_remove_at(&heap, copied, 1, &length) == MMAY_MANAGED_OK);
    MMAY_EXPECT(length == 4 && state.attempts == attempts);
    expected[0] = cells[0]; expected[1] = cells[2]; expected[2] = cells[3];
    mmay_edit_expect_sequence(&heap, copied, expected, 3, references);
    mmay_edit_expect_sequence(&heap, source, cells, 4, references);
    MMAY_EXPECT(duplicate->capacity == 4 && duplicate->mutation_generation == 1);
    for (index = 3 * descriptor->storage_stride; index < 4 * descriptor->storage_stride; index += 1) {
        MMAY_EXPECT(duplicate->storage[index] == 0);
    }
    MMAY_EXPECT(mmay_managed_array_remove_at(&heap, copied, 2, &length) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_remove_at(&heap, copied, 0, &length) == MMAY_MANAGED_OK);
    mmay_edit_expect_sequence(&heap, copied, &cells[2], 1, references);
    MMAY_EXPECT(mmay_managed_array_remove_at(&heap, copied, 0, &length) == MMAY_MANAGED_OK);
    MMAY_EXPECT(duplicate->length == 0 && duplicate->mutation_generation == 4);
    MMAY_EXPECT(mmay_managed_array_remove_at(&heap, copied, 0, &length) == MMAY_MANAGED_BOUNDS);
    MMAY_EXPECT(length == 0 && duplicate->mutation_generation == 4);
    state.failure_index = SIZE_MAX;
    MMAY_EXPECT(mmay_managed_array_clear(&heap, source) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_resolve(&heap, child) == NULL);
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &roots) == MMAY_MANAGED_OK);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static void mmay_edit_failures(size_t layout, bool empty, bool slot_growth) {
    bool succeeded = false;
    size_t failure;
    for (failure = 0; failure < 16 && !succeeded; failure += 1) {
        MarimayFailAllocator state;
        MarimayManagedHeap heap;
        MarimayManagedReference source, result, child;
        MarimayManagedReference retained[64];
        MarimayManagedReference *locations[64];
        MarimayManagedRootFrame roots = {0};
        MarimayManagedCell cells[2] = {mmay_edit_raw(1), mmay_edit_raw(2)};
        MarimayManagedStatus status;
        MarimayManagedArray *original;
        size_t live, capacity, count = empty ? 0 : 2, i;
        bool references = layout == 4;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_edit_heap(&heap, &state));
        heap.options.collect_before_allocation = false;
        MMAY_EXPECT(mmay_managed_array_new(&heap, &mmay_managed_array_f64_descriptor,
            NULL, 0, &child) == MMAY_MANAGED_OK);
        if (references) { cells[0].reference = child; cells[1].reference = child; }
        MMAY_EXPECT(mmay_managed_array_new(&heap, &mmay_edit_descriptors[layout],
            cells, count, &source) == MMAY_MANAGED_OK);
        for (i = 0; i < 64; i += 1) {
            retained[i] = mmay_managed_invalid_reference();
            locations[i] = &retained[i];
        }
        retained[0] = child; retained[1] = source;
        if (slot_growth) {
            for (i = 2; heap.slot_count < heap.slot_capacity; i += 1) {
                MMAY_EXPECT(i < 64);
                if (i >= 64) break;
                MMAY_EXPECT(mmay_managed_array_new(&heap, &mmay_managed_array_f64_descriptor,
                    NULL, 0, &retained[i]) == MMAY_MANAGED_OK);
            }
        }
        MMAY_EXPECT(mmay_managed_root_push(&heap, &roots, locations, 64) == MMAY_MANAGED_OK);
        heap.options.collect_before_allocation = true;
        original = mmay_managed_resolve(&heap, source)->as.array;
        capacity = original->capacity;
        live = heap.live_byte_count;
        state.failure_index = state.attempts + failure;
        status = mmay_managed_array_copy(&heap, source, &result);
        succeeded = status == MMAY_MANAGED_OK;
        MMAY_EXPECT(succeeded || status == MMAY_MANAGED_ALLOCATION_FAILED);
        if (succeeded) {
            MarimayManagedArray *copy = mmay_managed_resolve(&heap, result)->as.array;
            MMAY_EXPECT(result.slot != source.slot && copy->capacity == count);
            MMAY_EXPECT(copy->mutation_generation == 0);
            mmay_edit_expect_sequence(&heap, result, cells, count, references);
        } else {
            MMAY_EXPECT(mmay_managed_reference_is_invalid(result));
            MMAY_EXPECT(heap.live_byte_count == live);
        }
        MMAY_EXPECT(original->capacity == capacity && original->mutation_generation == 0);
        mmay_edit_expect_sequence(&heap, source, cells, count, references);
        MMAY_EXPECT(mmay_managed_root_pop(&heap, &roots) == MMAY_MANAGED_OK);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(succeeded);
}

static void mmay_edit_boundaries(void) {
    static const int64_t invalid[] = {INT64_MIN, -1, 2, INT64_MAX};
    MarimayFailAllocator state;
    MarimayManagedHeap heap;
    MarimayManagedReference source, copied;
    MarimayManagedCell cells[] = {mmay_edit_raw(1), mmay_edit_raw(2)};
    MarimayManagedArray *original;
    size_t index, length, live, object_bytes, capacity;
    unsigned char *storage;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_edit_heap(&heap, &state));
    MMAY_EXPECT(mmay_managed_array_new(&heap, &mmay_edit_descriptors[3], cells, 2, &source)
        == MMAY_MANAGED_OK);
    original = mmay_managed_resolve(&heap, source)->as.array;
    original->mutation_generation = UINT64_MAX;
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        MMAY_EXPECT(mmay_managed_array_remove_at(&heap, source, invalid[index], &length)
            == MMAY_MANAGED_BOUNDS && length == 2);
    }
    MMAY_EXPECT(mmay_managed_array_remove_at(&heap, source, 0, &length) == MMAY_MANAGED_LIMIT_REACHED);
    mmay_edit_expect_sequence(&heap, source, cells, 2, false);
    MMAY_EXPECT(mmay_managed_array_copy(&heap, source, NULL) == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_array_copy(NULL, source, &copied) == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_array_copy(&heap, mmay_managed_invalid_reference(), &copied)
        == MMAY_MANAGED_INVALID_REFERENCE);
    {
        MarimayManagedReference forged = source;
        forged.generation += 1;
        MMAY_EXPECT(mmay_managed_array_copy(&heap, forged, &copied) == MMAY_MANAGED_INVALID_REFERENCE);
        MMAY_EXPECT(mmay_managed_array_remove_at(&heap, forged, 0, &length) == MMAY_MANAGED_INVALID_REFERENCE);
        forged = source;
        forged.heap_provenance += 1;
        MMAY_EXPECT(mmay_managed_array_copy(&heap, forged, &copied) == MMAY_MANAGED_INVALID_REFERENCE);
        heap.slots[source.slot].kind = MMAY_MANAGED_OBJECT_STRING;
        MMAY_EXPECT(mmay_managed_array_copy(&heap, source, &copied) == MMAY_MANAGED_TYPE_MISMATCH);
        MMAY_EXPECT(mmay_managed_array_remove_at(&heap, source, 0, &length) == MMAY_MANAGED_TYPE_MISMATCH);
        heap.slots[source.slot].kind = MMAY_MANAGED_OBJECT_ARRAY;
    }
    MMAY_EXPECT(mmay_managed_array_remove_at(&heap, source, 0, NULL) == MMAY_MANAGED_INVALID_ARGUMENT);
    storage = original->storage; capacity = original->capacity;
    original->capacity = SIZE_MAX;
    MMAY_EXPECT(mmay_managed_array_copy(&heap, source, &copied) == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(mmay_managed_array_remove_at(&heap, source, 0, &length) == MMAY_MANAGED_INVALID_REFERENCE);
    original->capacity = capacity;
    original->storage = NULL;
    MMAY_EXPECT(mmay_managed_array_copy(&heap, source, &copied) == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(mmay_managed_array_remove_at(&heap, source, 0, &length) == MMAY_MANAGED_INVALID_REFERENCE);
    original->storage = storage;
    original->length = capacity + 1;
    MMAY_EXPECT(mmay_managed_array_copy(&heap, source, &copied) == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(mmay_managed_array_remove_at(&heap, source, 0, &length) == MMAY_MANAGED_INVALID_REFERENCE);
    original->length = 2;
    heap.options.max_array_length = 1;
    MMAY_EXPECT(mmay_managed_array_copy(&heap, source, &copied) == MMAY_MANAGED_LIMIT_REACHED);
    heap.options.max_array_length = 64;
    heap.options.max_objects = heap.live_object_count;
    MMAY_EXPECT(mmay_managed_array_copy(&heap, source, &copied) == MMAY_MANAGED_LIMIT_REACHED);
    heap.options.max_objects = 64;
    live = heap.live_byte_count;
    object_bytes = sizeof(*original) + 2 * sizeof(uint64_t);
    heap.options.max_live_bytes = live + object_bytes - 1;
    MMAY_EXPECT(mmay_managed_array_copy(&heap, source, &copied) == MMAY_MANAGED_LIMIT_REACHED);
    MMAY_EXPECT(heap.live_byte_count == live && mmay_managed_reference_is_invalid(copied));
    heap.options.max_live_bytes += 1;
    MMAY_EXPECT(mmay_managed_array_copy(&heap, source, &copied) == MMAY_MANAGED_OK);
    MMAY_EXPECT(heap.live_byte_count == live + object_bytes);
    MMAY_EXPECT(mmay_managed_resolve(&heap, copied)->as.array->mutation_generation == 0);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static void mmay_edit_invalid_child(void) {
    MarimayFailAllocator state;
    MarimayManagedHeap heap;
    MarimayManagedReference child, source, copied;
    MarimayManagedCell value;
    MarimayManagedArray *array;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_edit_heap(&heap, &state));
    MMAY_EXPECT(mmay_managed_array_new(&heap, &mmay_managed_array_f64_descriptor,
        NULL, 0, &child) == MMAY_MANAGED_OK);
    value.reference = child;
    MMAY_EXPECT(mmay_managed_array_new(&heap, &mmay_edit_descriptors[4], &value, 1, &source) == MMAY_MANAGED_OK);
    array = mmay_managed_resolve(&heap, source)->as.array;
    value.reference = mmay_managed_invalid_reference();
    memcpy(array->storage, &value, sizeof(value));
    MMAY_EXPECT(mmay_managed_array_copy(&heap, source, &copied) == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(mmay_managed_reference_is_invalid(copied));
    value.reference = child;
    memcpy(array->storage, &value, sizeof(value));
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

void mmay_test_managed_array_edit(void) {
    size_t layout;
    for (layout = 0; layout < 5; layout += 1) {
        mmay_edit_layout_case(layout);
        mmay_edit_failures(layout, false, false);
        mmay_edit_failures(layout, true, false);
        mmay_edit_failures(layout, false, true);
    }
    mmay_edit_boundaries();
    mmay_edit_invalid_child();
}
