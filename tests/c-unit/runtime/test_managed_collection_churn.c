#include "../test_support.h"

#include <stdio.h>
#include <string.h>

#include "../../../runtime/common/managed_map_storage_internal.h"
#include "mmay/runtime/managed.h"

static MarimayManagedMapDescriptor mmay_churn_descriptor(size_t shape) {
    const MarimayManagedFieldDescriptor raw = {
        MMAY_MANAGED_CELL_RAW64, false, MMAY_MANAGED_OBJECT_INVALID, NULL
    };
    const MarimayManagedFieldDescriptor ref = {
        MMAY_MANAGED_CELL_REFERENCE, false, MMAY_MANAGED_OBJECT_STRING, NULL
    };
    const MarimayManagedFieldDescriptor absent = {0};
    MarimayManagedMapDescriptor result = {
        shape < 3 ? raw : ref,
        shape % 3 == 0 ? absent : shape % 3 == 1 ? raw : ref,
        shape % 3 == 0
    };
    return result;
}

static MarimayManagedStatus mmay_churn_cell(
    MarimayManagedHeap *heap, bool reference, size_t index,
    MarimayManagedReference *root, MarimayManagedCell *cell
) {
    char bytes[32];
    int length;
    memset(cell, 0, sizeof(*cell));
    if (!reference) {
        cell->f64_bits = (uint64_t)index;
        return MMAY_MANAGED_OK;
    }
    length = snprintf(bytes, sizeof(bytes), "churn-%zu", index);
    if (length < 0 || (size_t)length >= sizeof(bytes)) return MMAY_MANAGED_INVALID_ARGUMENT;
    {
        MarimayManagedStatus status = mmay_managed_string_new(
            heap, (const unsigned char *)bytes, (size_t)length, root
        );
        if (status == MMAY_MANAGED_OK) cell->reference = *root;
        return status;
    }
}

static void mmay_churn_equal(bool reference, MarimayManagedCell a, MarimayManagedCell b) {
    MMAY_EXPECT(reference ? mmay_managed_reference_equal(a.reference, b.reference)
        : a.f64_bits == b.f64_bits);
}

/* Repeated replacement in a full ordered collection must amortize rebuilding.
 * The allocation bound detects the old full-copy-per-replacement behavior
 * without relying on wall time or duplicating the planner's threshold. */
static bool mmay_churn_run(size_t shape, size_t n, size_t failure, bool budget,
    size_t *attempts) {
    MarimayManagedMapDescriptor descriptor = mmay_churn_descriptor(shape);
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    MarimayFailAllocator tracked;
    MarimayManagedHeap heap = {0};
    MarimayManagedRootFrame frame = {0};
    MarimayManagedReference collection = mmay_managed_invalid_reference();
    MarimayManagedReference key_root = mmay_managed_invalid_reference();
    MarimayManagedReference value_root = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {&collection, &key_root, &value_root};
    MarimayManagedCell keys[128] = {{0}}, values[128] = {{0}};
    MarimayManagedCell key = {0}, value = {0};
    const bool ref_key = shape >= 3, ref_value = shape % 3 == 2;
    const size_t steps = n * 8U;
    const MarimayManagedMap *map = NULL;
    size_t i, step, started = 0;
    bool changed = false, complete = false;
    MarimayManagedStatus status;
    options.collect_before_allocation = true;
    options.max_live_bytes = 1024U * 1024U;
    mmay_fail_allocator_init(&tracked, mmay_system_allocator(), failure);
    if (!mmay_managed_heap_init(&heap, mmay_fail_allocator(&tracked), options)) goto done;
    if (mmay_managed_root_push(&heap, &frame, locations, 3) != MMAY_MANAGED_OK) goto done;
    status = descriptor.is_set ? mmay_managed_set_new(&heap, &descriptor, &collection)
        : mmay_managed_map_new(&heap, &descriptor, &collection);
    if (status != MMAY_MANAGED_OK) goto done;
    for (i = 0; i < n; i += 1) {
        if (mmay_churn_cell(&heap, ref_key, i, &key_root, &keys[i]) != MMAY_MANAGED_OK) goto done;
        if (!descriptor.is_set && mmay_churn_cell(&heap, ref_value, i + n,
            &value_root, &values[i]) != MMAY_MANAGED_OK) goto done;
        status = descriptor.is_set ? mmay_managed_set_add(&heap, collection, keys[i], &changed)
            : mmay_managed_map_put(&heap, collection, keys[i], values[i], &changed);
        if (status != MMAY_MANAGED_OK) goto done;
        MMAY_EXPECT(changed);
    }
    started = tracked.attempts;
    for (step = 0; step < steps; step += 1) {
        const size_t selected = step % n;
        size_t live_before, bytes_before;
        uint64_t generation;
        if (ref_key) key_root = keys[selected].reference;
        if (ref_value) value_root = values[selected].reference;
        status = descriptor.is_set
            ? mmay_managed_set_remove(&heap, collection, keys[selected], &changed)
            : mmay_managed_map_remove(&heap, collection, keys[selected], &value, &changed);
        MMAY_EXPECT(status == MMAY_MANAGED_OK && changed);
        if (!descriptor.is_set) mmay_churn_equal(ref_value, value, values[selected]);
        MMAY_EXPECT(mmay_managed_map_resolve(&heap, collection, &map) == MMAY_MANAGED_OK);
        generation = map->mutation_generation;
        live_before = mmay_managed_heap_live_bytes(&heap);
        bytes_before = tracked.live_bytes;
        if (budget && step == 0) {
            MarimayManagedResourceFailure resource;
            /* At a full power-of-two entry reservation, one hole triggers
             * geometric entry growth. Bucket capacity stays sufficient. */
            const size_t added = n * map->entry_stride;
            const size_t allocation_before = tracked.attempts;
            heap.options.max_live_bytes = live_before + added - 1U;
            status = descriptor.is_set
                ? mmay_managed_set_add(&heap, collection, keys[selected], &changed)
                : mmay_managed_map_put(&heap, collection, keys[selected], values[selected], &changed);
            MMAY_EXPECT(status == MMAY_MANAGED_LIMIT_REACHED && !changed);
            MMAY_EXPECT(map->length == n - 1U && map->mutation_generation == generation);
            MMAY_EXPECT(tracked.attempts == allocation_before && tracked.live_bytes == bytes_before);
            MMAY_EXPECT(mmay_managed_heap_last_resource_failure(&heap, &resource));
            MMAY_EXPECT(resource.classification == MMAY_MANAGED_RESOURCE_FAILURE_BUDGET
                && resource.live_bytes == live_before && resource.requested_bytes == added);
            heap.options.max_live_bytes += 1U;
        }
        status = descriptor.is_set
            ? mmay_managed_set_add(&heap, collection, keys[selected], &changed)
            : mmay_managed_map_put(&heap, collection, keys[selected], values[selected], &changed);
        MMAY_EXPECT(mmay_managed_map_resolve(&heap, collection, &map) == MMAY_MANAGED_OK);
        if (status != MMAY_MANAGED_OK) {
            MMAY_EXPECT(!changed && map->length == n - 1U && map->mutation_generation == generation);
            MMAY_EXPECT(mmay_managed_heap_live_bytes(&heap) == live_before
                && tracked.live_bytes == bytes_before);
        } else {
            MMAY_EXPECT(changed && map->length == n && map->mutation_generation == generation + 1U);
            if (budget && step == 0) {
                MMAY_EXPECT(mmay_managed_heap_live_bytes(&heap) == heap.options.max_live_bytes);
                heap.options.max_live_bytes = options.max_live_bytes;
            }
        }
        /* Rotation is an independent insertion-order oracle, including the
         * state after a failed put (the preceding remove did commit). */
        for (i = 0; i < map->length; i += 1) {
            const size_t expected = (selected + 1U + i) % n;
            MMAY_EXPECT((descriptor.is_set
                ? mmay_managed_set_entry(&heap, collection, i, &key)
                : mmay_managed_map_entry(&heap, collection, i, &key, &value)) == MMAY_MANAGED_OK);
            mmay_churn_equal(ref_key, key, keys[expected]);
            if (!descriptor.is_set) mmay_churn_equal(ref_value, value, values[expected]);
        }
        if (status != MMAY_MANAGED_OK) goto done;
        if (step % 17U == 0) MMAY_EXPECT(mmay_managed_heap_collect(&heap) == MMAY_MANAGED_OK);
    }
    MMAY_EXPECT(tracked.attempts - started <= 2U + 4U * steps / n);
    if (n == 1) MMAY_EXPECT(tracked.attempts == started);
    complete = true;
done:
    if (frame.linked) MMAY_EXPECT(mmay_managed_root_pop(&heap, &frame) == MMAY_MANAGED_OK);
    if (heap.initialized) mmay_managed_heap_dispose(&heap);
    *attempts = tracked.attempts;
    MMAY_EXPECT(tracked.live_allocations == 0 && tracked.live_bytes == 0);
    return complete;
}

void mmay_test_managed_collection_churn(void) {
    size_t shape, failure, attempts = 0;
    for (shape = 0; shape < 6; shape += 1) {
        bool complete = false;
        for (failure = 0; failure < 1024; failure += 1) {
            complete = mmay_churn_run(shape, 8, failure, false, &attempts);
            if (complete) {
                MMAY_EXPECT(failure == attempts);
                break;
            }
        }
        MMAY_EXPECT(complete);
        MMAY_EXPECT(mmay_churn_run(shape, 1, SIZE_MAX, false, &attempts));
        MMAY_EXPECT(mmay_churn_run(shape, 8, SIZE_MAX, true, &attempts));
        MMAY_EXPECT(mmay_churn_run(shape, 32, SIZE_MAX, false, &attempts));
        MMAY_EXPECT(mmay_churn_run(shape, 128, SIZE_MAX, false, &attempts));
    }
}
