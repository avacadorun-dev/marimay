#include "../test_support.h"

#include <stdio.h>
#include <string.h>

#include "../../../runtime/common/managed_map_storage_internal.h"
#include "mmay/runtime/managed.h"

static void mmay_iteration_equal(
    MarimayManagedFieldDescriptor field, MarimayManagedCell a, MarimayManagedCell b
) {
    if (field.cell_kind == MMAY_MANAGED_CELL_REFERENCE) {
        MMAY_EXPECT(mmay_managed_reference_equal(a.reference, b.reference));
    } else {
        MMAY_EXPECT(a.f64_bits == b.f64_bits);
    }
}

static void mmay_iteration_compare(
    MarimayManagedHeap *heap, MarimayManagedReference reference,
    const MarimayManagedMapDescriptor *descriptor, size_t count
) {
    size_t outer = 0, inner = 0, ordinal, nested, next;
    MarimayManagedCell key, value, expected_key = {0}, expected_value = {0};
    for (ordinal = 0; ordinal < count; ordinal += 1U) {
        MMAY_EXPECT(mmay_managed_collection_next(heap, reference,
            descriptor->is_set, outer, &next) == MMAY_MANAGED_OK);
        MMAY_EXPECT(next > outer);
        outer = next;
        MMAY_EXPECT(mmay_managed_collection_cursor_entry(heap, reference,
            descriptor->is_set, outer, &key, &value) == MMAY_MANAGED_OK);
        MMAY_EXPECT((descriptor->is_set
            ? mmay_managed_set_entry(heap, reference, ordinal, &expected_key)
            : mmay_managed_map_entry(heap, reference, ordinal, &expected_key,
                &expected_value)) == MMAY_MANAGED_OK);
        mmay_iteration_equal(descriptor->key, key, expected_key);
        if (!descriptor->is_set) mmay_iteration_equal(descriptor->value, value, expected_value);
        else MMAY_EXPECT(value.f64_bits == 0);
        /* Every inner activation starts fresh; the outer cursor remains usable. */
        inner = 0;
        for (nested = 0; nested < count; nested += 1U) {
            MMAY_EXPECT(mmay_managed_collection_next(heap, reference,
                descriptor->is_set, inner, &inner) == MMAY_MANAGED_OK);
            MMAY_EXPECT(inner != 0);
        }
        MMAY_EXPECT(mmay_managed_collection_next(heap, reference,
            descriptor->is_set, inner, &inner) == MMAY_MANAGED_OK && inner == 0);
        MMAY_EXPECT(mmay_managed_collection_cursor_entry(heap, reference,
            descriptor->is_set, outer, &key, &value) == MMAY_MANAGED_OK);
        mmay_iteration_equal(descriptor->key, key, expected_key);
    }
    MMAY_EXPECT(mmay_managed_collection_next(heap, reference,
        descriptor->is_set, outer, &outer) == MMAY_MANAGED_OK && outer == 0);
}

static bool mmay_iteration_run(size_t shape, size_t failure, size_t *attempts) {
    const MarimayManagedFieldDescriptor raw = {
        MMAY_MANAGED_CELL_RAW64, false, MMAY_MANAGED_OBJECT_INVALID, NULL
    };
    const MarimayManagedFieldDescriptor ref = {
        MMAY_MANAGED_CELL_REFERENCE, false, MMAY_MANAGED_OBJECT_STRING, NULL
    };
    const MarimayManagedFieldDescriptor absent = {0};
    MarimayManagedMapDescriptor descriptor = {
        shape < 3 ? raw : ref,
        shape % 3 == 0 ? absent : shape % 3 == 1 ? raw : ref,
        shape % 3 == 0
    };
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    MarimayManagedHeap heap;
    MarimayFailAllocator allocation;
    MarimayManagedReference collection = mmay_managed_invalid_reference();
    MarimayManagedReference key_root = mmay_managed_invalid_reference();
    MarimayManagedReference value_root = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&collection, &key_root, &value_root};
    MarimayManagedRootFrame frame = {0};
    MarimayManagedCell key = {0}, value = {0}, removed = {0};
    MarimayManagedStatus status;
    size_t i, next = 99, count = 0, before;
    uint64_t generation = 0;
    bool changed = false, complete = false;
    options.collect_before_allocation = true;
    options.max_live_bytes = 1024U * 1024U;
    mmay_fail_allocator_init(&allocation, mmay_system_allocator(), failure);
    if (!mmay_managed_heap_init(&heap, mmay_fail_allocator(&allocation), options)) goto done;
    if (mmay_managed_root_push(&heap, &frame, locations, 3) != MMAY_MANAGED_OK) goto dispose;
    status = descriptor.is_set
        ? mmay_managed_set_new(&heap, &descriptor, &collection)
        : mmay_managed_map_new(&heap, &descriptor, &collection);
    if (status != MMAY_MANAGED_OK) goto pop;
    mmay_iteration_compare(&heap, collection, &descriptor, 0);
    for (i = 0; i < 24; i += 1U) {
        char text[24];
        int length = snprintf(text, sizeof(text), "key-%zu", i);
        if (descriptor.key.cell_kind == MMAY_MANAGED_CELL_REFERENCE) {
            status = mmay_managed_string_new(&heap, (const unsigned char *)text,
                (size_t)length, &key_root);
            if (status != MMAY_MANAGED_OK) goto pop;
            key.reference = key_root;
        } else key.f64_bits = UINT64_MAX - (uint64_t)i;
        if (descriptor.value.cell_kind == MMAY_MANAGED_CELL_REFERENCE) {
            status = mmay_managed_string_new(&heap, (const unsigned char *)text,
                (size_t)length, &value_root);
            if (status != MMAY_MANAGED_OK) goto pop;
            value.reference = value_root;
        } else value.f64_bits = (uint64_t)i;
        status = descriptor.is_set
            ? mmay_managed_set_add(&heap, collection, key, &changed)
            : mmay_managed_map_put(&heap, collection, key, value, &changed);
        if (status != MMAY_MANAGED_OK) goto pop;
        MMAY_EXPECT(changed);
        count += 1U;
        if (i % 3 != 0) {
            MMAY_EXPECT((descriptor.is_set
                ? mmay_managed_set_remove(&heap, collection, key, &changed)
                : mmay_managed_map_remove(&heap, collection, key, &removed, &changed))
                == MMAY_MANAGED_OK && changed);
            count -= 1U;
        }
        mmay_iteration_compare(&heap, collection, &descriptor, count);
    }
    /* Fail the very next allocation: traversal must still succeed with the
     * exact existing heap budget, including reference cells and collection. */
    before = allocation.attempts;
    allocation.failure_index = before;
    heap.options.max_live_bytes = mmay_managed_heap_live_bytes(&heap);
    MMAY_EXPECT(mmay_managed_map_mutation_generation(&heap, collection,
        &generation) == MMAY_MANAGED_OK);
    mmay_iteration_compare(&heap, collection, &descriptor, count);
    MMAY_EXPECT(allocation.attempts == before);
    MMAY_EXPECT(mmay_managed_map_check_mutation_generation(&heap, collection,
        generation) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_collection_next(&heap, collection,
        !descriptor.is_set, 0, &next) == MMAY_MANAGED_TYPE_MISMATCH && next == 0);
    MMAY_EXPECT(mmay_managed_collection_next(&heap, collection,
        descriptor.is_set, SIZE_MAX, &next) == MMAY_MANAGED_BOUNDS && next == 0);
    MMAY_EXPECT(mmay_managed_collection_cursor_entry(&heap, collection,
        descriptor.is_set, 0, &key, &value) == MMAY_MANAGED_BOUNDS);
    MMAY_EXPECT(mmay_managed_collection_cursor_entry(&heap, collection,
        descriptor.is_set, SIZE_MAX, &key, &value) == MMAY_MANAGED_BOUNDS);
    MMAY_EXPECT(mmay_managed_collection_next(&heap, collection,
        descriptor.is_set, 0, NULL) == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_collection_cursor_entry(&heap, collection,
        descriptor.is_set, 1, NULL, &value) == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_managed_collection_cursor_entry(&heap, collection,
        descriptor.is_set, 1, &key, NULL) == MMAY_MANAGED_INVALID_ARGUMENT);
    {
        const MarimayManagedMap *map = NULL;
        MarimayManagedReference wrong = collection;
        MMAY_EXPECT(mmay_managed_map_resolve(&heap, collection, &map) == MMAY_MANAGED_OK);
        for (i = 0; i < map->entry_count; i += 1U) {
            if (!mmay_managed_map_entry_prefix(map, i)->active) {
                MMAY_EXPECT(mmay_managed_collection_cursor_entry(&heap, collection,
                    descriptor.is_set, i + 1U, &key, &value) == MMAY_MANAGED_BOUNDS);
            }
        }
        wrong.generation += 1U;
        MMAY_EXPECT(mmay_managed_collection_next(&heap, wrong,
            descriptor.is_set, 0, &next) == MMAY_MANAGED_INVALID_REFERENCE);
        MMAY_EXPECT(mmay_managed_collection_cursor_entry(&heap, wrong,
            descriptor.is_set, 1, &key, &value) == MMAY_MANAGED_INVALID_REFERENCE);
    }
    complete = true;
pop:
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
dispose:
    mmay_managed_heap_dispose(&heap);
done:
    *attempts = allocation.attempts;
    MMAY_EXPECT(allocation.live_allocations == 0);
    MMAY_EXPECT(allocation.live_bytes == 0);
    return complete;
}

void mmay_test_managed_collection_iteration(void) {
    size_t shape, failure, attempts = 0;
    for (shape = 0; shape < 6; shape += 1U) {
        bool complete = false;
        for (failure = 0; failure < 1024; failure += 1U) {
            complete = mmay_iteration_run(shape, failure, &attempts);
            if (complete) {
                MMAY_EXPECT(attempts == failure);
                break;
            }
        }
        MMAY_EXPECT(complete);
    }
}
