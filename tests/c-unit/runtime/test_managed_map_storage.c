#include "../test_support.h"

#include <stdio.h>
#include <string.h>

#include "../../../runtime/common/managed_internal.h"
#include "../../../runtime/common/managed_map_storage_internal.h"

static MarimayManagedMapDescriptor mmay_test_storage_descriptor(size_t shape) {
    const MarimayManagedFieldDescriptor raw = {
        MMAY_MANAGED_CELL_RAW64, false, MMAY_MANAGED_OBJECT_INVALID, NULL
    };
    const MarimayManagedFieldDescriptor ref = {
        MMAY_MANAGED_CELL_REFERENCE, false, MMAY_MANAGED_OBJECT_STRING, NULL
    };
    const MarimayManagedFieldDescriptor absent = {0};
    MarimayManagedMapDescriptor descriptor;
    descriptor.key = shape < 3 ? raw : ref;
    descriptor.is_set = shape % 3 == 0;
    descriptor.value = descriptor.is_set ? absent : shape % 3 == 1 ? raw : ref;
    return descriptor;
}

static MarimayManagedStatus mmay_test_storage_cell(
    MarimayManagedHeap *heap, MarimayManagedCellKind kind, size_t index,
    MarimayManagedReference *root, MarimayManagedCell *cell
) {
    char text[32];
    int length;
    MarimayManagedStatus status;
    memset(cell, 0, sizeof(*cell));
    if (kind == MMAY_MANAGED_CELL_RAW64) {
        /* Poison the inactive union tail. RAW64 consumes exactly its word,
         * not an assumed canonical representation of the enclosing union. */
        memset(cell, 0xa5, sizeof(*cell));
        cell->f64_bits = UINT64_MAX - (uint64_t)index;
        return MMAY_MANAGED_OK;
    }
    length = snprintf(text, sizeof(text), "cell-%zu", index);
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(text));
    status = mmay_managed_string_new(heap, (const unsigned char *)text,
        (size_t)length, root);
    if (status == MMAY_MANAGED_OK) cell->reference = *root;
    return status;
}

static void mmay_test_storage_cell_equal(
    MarimayManagedCellKind kind, MarimayManagedCell left, MarimayManagedCell right
) {
    if (kind == MMAY_MANAGED_CELL_RAW64) {
        MMAY_EXPECT(left.f64_bits == right.f64_bits);
        /* Inactive union bytes are not a scalar value and are not compared. */
    } else {
        MMAY_EXPECT(mmay_managed_reference_equal(left.reference, right.reference));
    }
}

static MarimayManagedStatus mmay_test_storage_entry(
    MarimayManagedHeap *heap, MarimayManagedReference reference, bool is_set,
    size_t index, MarimayManagedCell *key, MarimayManagedCell *value
) {
    return is_set ? mmay_managed_set_entry(heap, reference, index, key)
        : mmay_managed_map_entry(heap, reference, index, key, value);
}

/* Every layout executes growth, deletion, same-capacity compaction, tracing
 * and teardown at every reachable deterministic allocation failure index. */
static bool mmay_test_storage_failure_run(size_t shape, size_t failure, size_t *attempts) {
    MarimayManagedMapDescriptor descriptor = mmay_test_storage_descriptor(shape);
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    MarimayFailAllocator allocation;
    MarimayManagedHeap heap;
    MarimayManagedReference reference = mmay_managed_invalid_reference();
    MarimayManagedReference key_root = mmay_managed_invalid_reference();
    MarimayManagedReference value_root = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&reference, &key_root, &value_root};
    MarimayManagedRootFrame frame = {0};
    MarimayManagedCell keys[48] = {{0}}, values[48] = {{0}};
    MarimayManagedCell key = {0}, value = {0};
    MarimayManagedStatus status;
    bool complete = false, inserted = false;
    size_t index, count = 0;
    options.max_objects = 256;
    options.max_live_bytes = 1024U * 1024U;
    options.max_collection_length = 256;
    options.collect_before_allocation = true;
    mmay_fail_allocator_init(&allocation, mmay_system_allocator(), failure);
    if (!mmay_managed_heap_init(&heap, mmay_fail_allocator(&allocation), options)) goto finished;
    if (mmay_managed_root_push(&heap, &frame, locations, 3) != MMAY_MANAGED_OK) goto teardown;
    status = descriptor.is_set
        ? mmay_managed_set_new(&heap, &descriptor, &reference)
        : mmay_managed_map_new(&heap, &descriptor, &reference);
    if (status != MMAY_MANAGED_OK) goto pop;
    for (index = 0; index < 48; index += 1) {
        const MarimayManagedMap *map = NULL;
        size_t bytes_before, objects_before;
        uint64_t generation;
        if (index == 32) {
            size_t removed;
            for (removed = 0; removed < 16; removed += 1) {
                MMAY_EXPECT(mmay_test_storage_entry(&heap, reference, descriptor.is_set,
                    removed, &key, &value) == MMAY_MANAGED_OK);
                mmay_test_storage_cell_equal(descriptor.key.cell_kind, key, keys[removed * 2U]);
                MMAY_EXPECT((descriptor.is_set
                    ? mmay_managed_set_remove(&heap, reference, key, &inserted)
                    : mmay_managed_map_remove(&heap, reference, key, &value, &inserted))
                    == MMAY_MANAGED_OK && inserted);
            }
            count = 16;
        }
        status = mmay_test_storage_cell(&heap, descriptor.key.cell_kind, index,
            &key_root, &keys[index]);
        if (status != MMAY_MANAGED_OK) goto pop;
        if (!descriptor.is_set) {
            status = mmay_test_storage_cell(&heap, descriptor.value.cell_kind, index + 100U,
                &value_root, &values[index]);
            if (status != MMAY_MANAGED_OK) goto pop;
        }
        MMAY_EXPECT(mmay_managed_map_resolve(&heap, reference, &map) == MMAY_MANAGED_OK);
        generation = map->mutation_generation;
        bytes_before = allocation.live_bytes;
        objects_before = allocation.live_allocations;
        inserted = true;
        status = descriptor.is_set
            ? mmay_managed_set_add(&heap, reference, keys[index], &inserted)
            : mmay_managed_map_put(&heap, reference, keys[index], values[index], &inserted);
        MMAY_EXPECT(mmay_managed_map_resolve(&heap, reference, &map) == MMAY_MANAGED_OK);
        if (status != MMAY_MANAGED_OK) {
            size_t retained;
            MMAY_EXPECT(!inserted && map->length == count
                && map->mutation_generation == generation);
            /* Collection may release removed strings, never leak an unpublished
             * replacement buffer or advance logical state on failure. */
            MMAY_EXPECT(allocation.live_bytes <= bytes_before);
            MMAY_EXPECT(allocation.live_allocations <= objects_before);
            for (retained = 0; retained < count; retained += 1) {
                size_t source = index < 32 ? retained
                    : retained < 16 ? retained * 2U + 1U : retained + 16U;
                MMAY_EXPECT(mmay_test_storage_entry(&heap, reference, descriptor.is_set,
                    retained, &key, &value) == MMAY_MANAGED_OK);
                mmay_test_storage_cell_equal(descriptor.key.cell_kind, key, keys[source]);
                if (!descriptor.is_set) {
                    mmay_test_storage_cell_equal(descriptor.value.cell_kind, value, values[source]);
                }
            }
            goto pop;
        }
        count += 1U;
        MMAY_EXPECT(inserted && map->length == count
            && map->mutation_generation == generation + 1U);
        MMAY_EXPECT(map->entry_stride == sizeof(MarimayManagedMapEntry)
            + (shape < 3 ? sizeof(uint64_t) : sizeof(MarimayManagedReference))
            + (shape % 3 == 0 ? 0 : shape % 3 == 1
                ? sizeof(uint64_t) : sizeof(MarimayManagedReference)));
        MMAY_EXPECT(mmay_managed_map_storage_bytes(map)
            == map->entry_capacity * map->entry_stride + map->bucket_capacity * sizeof(size_t));
        if (index == 32) MMAY_EXPECT(map->entry_count == 17 && map->entry_capacity == 32);
    }
    for (index = 0; index < 32; index += 1) {
        size_t source = index < 16 ? index * 2U + 1U : index + 16U;
        MMAY_EXPECT(mmay_test_storage_entry(&heap, reference, descriptor.is_set,
            index, &key, &value) == MMAY_MANAGED_OK);
        mmay_test_storage_cell_equal(descriptor.key.cell_kind, key, keys[source]);
        if (!descriptor.is_set) {
            mmay_test_storage_cell_equal(descriptor.value.cell_kind, value, values[source]);
        }
    }
    status = mmay_managed_heap_collect(&heap);
    if (status != MMAY_MANAGED_OK) goto pop;
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 1U
        + (shape < 3 ? 0U : 32U) + (shape % 3 == 2 ? 32U : 0U));
    MMAY_EXPECT((descriptor.is_set ? mmay_managed_set_clear(&heap, reference)
        : mmay_managed_map_clear(&heap, reference)) == MMAY_MANAGED_OK);
    key_root = mmay_managed_invalid_reference();
    value_root = mmay_managed_invalid_reference();
    status = mmay_managed_heap_collect(&heap);
    if (status != MMAY_MANAGED_OK) goto pop;
    MMAY_EXPECT(mmay_managed_heap_live_objects(&heap) == 1U);
    complete = true;
pop:
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
teardown:
    mmay_managed_heap_dispose(&heap);
finished:
    *attempts = allocation.attempts;
    MMAY_EXPECT(allocation.live_allocations == 0);
    MMAY_EXPECT(allocation.live_bytes == 0);
    return complete;
}

static void mmay_test_storage_budget_boundary(size_t shape) {
    MarimayManagedMapDescriptor descriptor = mmay_test_storage_descriptor(shape);
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    MarimayFailAllocator allocation;
    MarimayManagedHeap heap;
    MarimayManagedReference reference = mmay_managed_invalid_reference();
    MarimayManagedReference key_root = mmay_managed_invalid_reference();
    MarimayManagedReference value_root = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&reference, &key_root, &value_root};
    MarimayManagedRootFrame frame = {0};
    MarimayManagedCell key = {0}, value = {0};
    MarimayManagedResourceFailure failure;
    const MarimayManagedMap *map = NULL;
    size_t managed_before, allocated_before, required, index;
    bool inserted = false;
    options.max_live_bytes = 1024U * 1024U;
    mmay_fail_allocator_init(&allocation, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_managed_heap_init(&heap, mmay_fail_allocator(&allocation), options));
    MMAY_EXPECT(mmay_managed_root_push(&heap, &frame, locations, 3) == MMAY_MANAGED_OK);
    MMAY_EXPECT((descriptor.is_set ? mmay_managed_set_new(&heap, &descriptor, &reference)
        : mmay_managed_map_new(&heap, &descriptor, &reference)) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_test_storage_cell(&heap, descriptor.key.cell_kind, 0,
        &key_root, &key) == MMAY_MANAGED_OK);
    if (!descriptor.is_set) {
        MMAY_EXPECT(mmay_test_storage_cell(&heap, descriptor.value.cell_kind, 100,
            &value_root, &value) == MMAY_MANAGED_OK);
    }
    MMAY_EXPECT(mmay_managed_map_resolve(&heap, reference, &map) == MMAY_MANAGED_OK);
    required = 4U * map->entry_stride + 8U * sizeof(size_t);
    managed_before = mmay_managed_heap_live_bytes(&heap);
    allocated_before = allocation.live_bytes;
    heap.options.max_live_bytes = managed_before + required - 1U;
    inserted = true;
    MMAY_EXPECT((descriptor.is_set ? mmay_managed_set_add(&heap, reference, key, &inserted)
        : mmay_managed_map_put(&heap, reference, key, value, &inserted))
        == MMAY_MANAGED_LIMIT_REACHED);
    MMAY_EXPECT(!inserted && map->length == 0 && map->mutation_generation == 0);
    MMAY_EXPECT(mmay_managed_heap_last_resource_failure(&heap, &failure));
    MMAY_EXPECT(failure.classification == MMAY_MANAGED_RESOURCE_FAILURE_BUDGET
        && failure.live_bytes == managed_before && failure.requested_bytes == required
        && failure.limit_bytes == heap.options.max_live_bytes);
    MMAY_EXPECT(allocation.live_bytes == allocated_before);
    heap.options.max_live_bytes += 1U;
    MMAY_EXPECT((descriptor.is_set ? mmay_managed_set_add(&heap, reference, key, &inserted)
        : mmay_managed_map_put(&heap, reference, key, value, &inserted)) == MMAY_MANAGED_OK);
    MMAY_EXPECT(inserted && mmay_managed_heap_live_bytes(&heap) == heap.options.max_live_bytes);
    MMAY_EXPECT(allocation.live_bytes == allocated_before + required);
    MMAY_EXPECT(mmay_managed_map_storage_bytes(map) == required);
    for (index = map->entry_stride; index < 4U * map->entry_stride; index += 1) {
        MMAY_EXPECT(map->entries[index] == 0);
    }
    MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    mmay_managed_heap_dispose(&heap);
    MMAY_EXPECT(allocation.live_allocations == 0);
    MMAY_EXPECT(allocation.live_bytes == 0);
}

void mmay_test_managed_map_storage(void) {
    size_t shape, failure, attempts = 0;
    for (shape = 0; shape < 6; shape += 1) {
        bool complete = false;
        mmay_test_storage_budget_boundary(shape);
        for (failure = 0; failure < 2048; failure += 1) {
            complete = mmay_test_storage_failure_run(shape, failure, &attempts);
            if (complete) {
                MMAY_EXPECT(failure == attempts);
                break;
            }
        }
        MMAY_EXPECT(complete);
    }
}
