#include "managed_internal.h"

#include "mmay/base/provenance.h"

#include <limits.h>
#include <string.h>

size_t mmay_managed_language_max_array_length(void) {
#if SIZE_MAX > INT64_MAX
    return (size_t)INT64_MAX;
#else
    return SIZE_MAX;
#endif
}

MarimayManagedReference mmay_managed_invalid_reference(void) {
    MarimayManagedReference reference;
    reference.heap_provenance = UINT64_MAX;
    reference.slot = UINT32_MAX;
    reference.generation = UINT32_MAX;
    return reference;
}

bool mmay_managed_reference_is_invalid(MarimayManagedReference reference) {
    return reference.heap_provenance == UINT64_MAX
        && reference.slot == UINT32_MAX
        && reference.generation == UINT32_MAX;
}

bool mmay_managed_reference_equal(
    MarimayManagedReference left,
    MarimayManagedReference right
) {
    return left.heap_provenance == right.heap_provenance
        && left.slot == right.slot && left.generation == right.generation;
}

MarimayManagedHeapOptions mmay_managed_heap_default_options(void) {
    MarimayManagedHeapOptions options;
    options.max_objects = 1000000;
    options.max_live_bytes = 64U * 1024U * 1024U;
    options.max_array_length = mmay_managed_language_max_array_length();
    options.collect_before_allocation = false;
    return options;
}

bool mmay_managed_heap_valid(const MarimayManagedHeap *heap) {
    return heap != NULL && heap->initialized && heap->provenance != 0
        && heap->provenance != UINT64_MAX;
}

void mmay_managed_view_cache_clear(MarimayManagedHeap *heap) {
    if (heap == NULL) {
        return;
    }
    heap->cached_record_reference = mmay_managed_invalid_reference();
    heap->cached_record_descriptor = NULL;
    heap->cached_record_object = NULL;
    heap->cached_record_fields = NULL;
    heap->cached_array_reference = mmay_managed_invalid_reference();
    heap->cached_array_descriptor = NULL;
    heap->cached_array_object = NULL;
    heap->cached_array_length = NULL;
    heap->cached_array_capacity = NULL;
    heap->cached_array_mutation_generation = NULL;
    heap->cached_array_storage = NULL;
    heap->cached_array_second_reference = mmay_managed_invalid_reference();
    heap->cached_array_second_descriptor = NULL;
    heap->cached_array_second_object = NULL;
}

bool mmay_managed_heap_init(
    MarimayManagedHeap *heap,
    MarimayAllocator allocator,
    MarimayManagedHeapOptions options
) {
    uint64_t provenance = 0;
    if (heap == NULL || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL || options.max_objects == 0
        || options.max_objects > UINT32_MAX || options.max_live_bytes == 0
        || options.max_array_length > mmay_managed_language_max_array_length()
        || !mmay_new_provenance(&provenance)) {
        return false;
    }
    memset(heap, 0, sizeof(*heap));
    heap->allocator = allocator;
    heap->free_slot = UINT32_MAX;
    heap->provenance = provenance;
    heap->options = options;
    heap->initialized = true;
    mmay_managed_view_cache_clear(heap);
    return true;
}

void mmay_managed_heap_dispose(MarimayManagedHeap *heap) {
    size_t index = 0;
    size_t slot_bytes = 0;
    size_t descriptor_bytes = 0;
    MarimayManagedRootFrame *frame = NULL;
    if (!mmay_managed_heap_valid(heap)) {
        return;
    }
    frame = heap->roots;
    while (frame != NULL) {
        MarimayManagedRootFrame *previous = frame->previous;
        memset(frame, 0, sizeof(*frame));
        frame = previous;
    }
    for (index = 0; index < heap->slot_count; index += 1) {
        mmay_managed_slot_dispose_object(heap, &heap->slots[index]);
    }
    (void)mmay_size_multiply(
        heap->slot_capacity,
        sizeof(MarimayManagedSlot),
        &slot_bytes
    );
    mmay_deallocate(&heap->allocator, heap->slots, slot_bytes);
    (void)mmay_size_multiply(
        heap->immediate_enum_descriptor_capacity,
        sizeof(*heap->immediate_enum_descriptors),
        &descriptor_bytes
    );
    mmay_deallocate(
        &heap->allocator,
        heap->immediate_enum_descriptors,
        descriptor_bytes
    );
    memset(heap, 0, sizeof(*heap));
}

MarimayManagedSlot *mmay_managed_resolve(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference
) {
    MarimayManagedSlot *slot = NULL;
    if (!mmay_managed_heap_valid(heap)
        || reference.heap_provenance != heap->provenance
        || reference.slot == UINT32_MAX
        || (size_t)reference.slot >= heap->slot_count) {
        return NULL;
    }
    slot = &heap->slots[reference.slot];
    return mmay_managed_slot_occupied(slot)
            && slot->generation == reference.generation
        ? slot
        : NULL;
}

const MarimayManagedSlot *mmay_managed_resolve_const(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference
) {
    const MarimayManagedSlot *slot = NULL;
    if (!mmay_managed_heap_valid(heap)
        || reference.heap_provenance != heap->provenance
        || reference.slot == UINT32_MAX
        || (size_t)reference.slot >= heap->slot_count) {
        return NULL;
    }
    slot = &heap->slots[reference.slot];
    return mmay_managed_slot_occupied(slot)
            && slot->generation == reference.generation
        ? slot
        : NULL;
}

MarimayManagedStatus mmay_managed_reference_status(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedArrayDescriptor *expected
) {
    const MarimayManagedSlot *slot = mmay_managed_resolve_const(heap, reference);
    if (slot == NULL) {
        return mmay_managed_immediate_enum_decode(
                heap, reference, NULL, NULL, NULL
            )
            ? MMAY_MANAGED_TYPE_MISMATCH
            : MMAY_MANAGED_INVALID_REFERENCE;
    }
    return expected == NULL
            || (slot->kind == MMAY_MANAGED_OBJECT_ARRAY
                && slot->as.array != NULL
                && slot->as.array->descriptor == expected)
        ? MMAY_MANAGED_OK
        : MMAY_MANAGED_TYPE_MISMATCH;
}

MarimayManagedStatus mmay_managed_root_push(
    MarimayManagedHeap *heap,
    MarimayManagedRootFrame *frame,
    MarimayManagedReference *const *locations,
    size_t location_count
) {
    size_t index = 0;
    if (!mmay_managed_heap_valid(heap) || frame == NULL
        || (locations == NULL && location_count != 0) || frame->linked) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    for (index = 0; index < location_count; index += 1) {
        if (locations[index] == NULL
            || (!mmay_managed_reference_is_invalid(*locations[index])
                && !mmay_managed_reference_valid(
                    heap, *locations[index]
                ))) {
            return locations[index] == NULL
                ? MMAY_MANAGED_INVALID_ARGUMENT
                : MMAY_MANAGED_INVALID_REFERENCE;
        }
    }
    frame->locations = locations;
    frame->location_count = location_count;
    frame->previous = heap->roots;
    frame->heap_provenance = heap->provenance;
    frame->linked = true;
    heap->roots = frame;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_root_pop(
    MarimayManagedHeap *heap,
    MarimayManagedRootFrame *frame
) {
    if (!mmay_managed_heap_valid(heap) || frame == NULL || !frame->linked
        || frame->heap_provenance != heap->provenance
        || heap->roots != frame) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    heap->roots = frame->previous;
    memset(frame, 0, sizeof(*frame));
    return MMAY_MANAGED_OK;
}

static void mmay_managed_clear_marks(MarimayManagedHeap *heap) {
    size_t index = 0;
    for (index = 0; index < heap->slot_count; index += 1) {
        heap->slots[index].marked = false;
        heap->slots[index].next_gray = UINT32_MAX;
    }
}

MarimayManagedStatus mmay_managed_mark_reference(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    uint32_t *gray_head
) {
    MarimayManagedSlot *slot = NULL;
    if (mmay_managed_reference_is_invalid(reference)) {
        return MMAY_MANAGED_OK;
    }
    if (mmay_managed_immediate_enum_decode(
            heap, reference, NULL, NULL, NULL
        )) {
        return MMAY_MANAGED_OK;
    }
    slot = mmay_managed_resolve(heap, reference);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (!slot->marked) {
        slot->marked = true;
        slot->next_gray = *gray_head;
        *gray_head = reference.slot;
    }
    return MMAY_MANAGED_OK;
}


MarimayManagedStatus mmay_managed_collect_internal(
    MarimayManagedHeap *heap,
    const MarimayManagedReference *temporary_roots,
    size_t temporary_root_count,
    const MarimayManagedCell *temporary_cells,
    size_t temporary_cell_count
) {
    MarimayManagedRootFrame *frame = NULL;
    uint32_t gray_head = UINT32_MAX;
    size_t index = 0;
    MarimayManagedStatus status = MMAY_MANAGED_OK;
    if (!mmay_managed_heap_valid(heap) || heap->collecting
        || (temporary_roots == NULL && temporary_root_count != 0)
        || (temporary_cells == NULL && temporary_cell_count != 0)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    heap->collecting = true;
    mmay_managed_view_cache_clear(heap);
    mmay_managed_clear_marks(heap);
    for (frame = heap->roots; frame != NULL; frame = frame->previous) {
        if (!frame->linked || frame->heap_provenance != heap->provenance
            || (frame->locations == NULL && frame->location_count != 0)) {
            status = MMAY_MANAGED_INVALID_ARGUMENT;
            goto cleanup;
        }
        for (index = 0; index < frame->location_count; index += 1) {
            if (frame->locations[index] == NULL) {
                status = MMAY_MANAGED_INVALID_ARGUMENT;
                goto cleanup;
            }
            status = mmay_managed_mark_reference(
                heap,
                *frame->locations[index],
                &gray_head
            );
            if (status != MMAY_MANAGED_OK) {
                goto cleanup;
            }
        }
    }
    for (index = 0; index < temporary_root_count; index += 1) {
        status = mmay_managed_mark_reference(
            heap,
            temporary_roots[index],
            &gray_head
        );
        if (status != MMAY_MANAGED_OK) {
            goto cleanup;
        }
    }
    for (index = 0; index < temporary_cell_count; index += 1) {
        status = mmay_managed_mark_reference(
            heap,
            temporary_cells[index].reference,
            &gray_head
        );
        if (status != MMAY_MANAGED_OK) {
            goto cleanup;
        }
    }
    while (gray_head != UINT32_MAX) {
        MarimayManagedSlot *slot = &heap->slots[gray_head];
        gray_head = slot->next_gray;
        slot->next_gray = UINT32_MAX;
        if (!mmay_managed_slot_occupied(slot)) {
            status = MMAY_MANAGED_INVALID_REFERENCE;
            goto cleanup;
        }
        status = mmay_managed_trace_slot(heap, slot, &gray_head);
        if (status != MMAY_MANAGED_OK) {
            goto cleanup;
        }
    }
    for (index = 0; index < heap->slot_count; index += 1) {
        MarimayManagedSlot *slot = &heap->slots[index];
        if (!mmay_managed_slot_occupied(slot) || slot->marked) {
            continue;
        }
        {
            size_t bytes = mmay_managed_slot_object_bytes(slot);
            mmay_managed_slot_dispose_object(heap, slot);
            heap->live_object_count -= 1;
            heap->live_byte_count -= bytes;
        }
        if (slot->generation == UINT32_MAX) {
            slot->retired = true;
            slot->next_free = UINT32_MAX;
        } else {
            slot->generation += 1;
            slot->next_free = heap->free_slot;
            heap->free_slot = (uint32_t)index;
        }
    }

cleanup:
    mmay_managed_clear_marks(heap);
    heap->collecting = false;
    return status;
}

MarimayManagedStatus mmay_managed_heap_collect(MarimayManagedHeap *heap) {
    return mmay_managed_collect_internal(heap, NULL, 0, NULL, 0);
}

bool mmay_managed_add_within_limit(
    size_t current,
    size_t amount,
    size_t limit,
    size_t *result
) {
    return mmay_size_add(current, amount, result) && *result <= limit;
}

bool mmay_managed_new_slot_capacity(
    const MarimayManagedHeap *heap,
    size_t *capacity,
    size_t *added_bytes
) {
    size_t next = heap->slot_capacity == 0 ? 8 : heap->slot_capacity;
    size_t old_bytes = 0;
    size_t new_bytes = 0;
    if (heap->free_slot != UINT32_MAX) {
        *capacity = heap->slot_capacity;
        *added_bytes = 0;
        return true;
    }
    if (heap->slot_count >= heap->options.max_objects) {
        return false;
    }
    while (next <= heap->slot_count) {
        size_t doubled = 0;
        if (!mmay_size_multiply(next, 2, &doubled)
            || doubled > heap->options.max_objects) {
            next = heap->options.max_objects;
            break;
        }
        next = doubled;
    }
    if (next > heap->options.max_objects) {
        next = heap->options.max_objects;
    }
    if (next <= heap->slot_count
        || !mmay_size_multiply(
            heap->slot_capacity,
            sizeof(MarimayManagedSlot),
            &old_bytes
        ) || !mmay_size_multiply(
            next,
            sizeof(MarimayManagedSlot),
            &new_bytes
        )) {
        return false;
    }
    *capacity = next;
    *added_bytes = new_bytes - old_bytes;
    return true;
}

MarimayManagedStatus mmay_managed_array_length(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    size_t *length
) {
    const MarimayManagedSlot *slot = NULL;
    if (length != NULL) {
        *length = 0;
    }
    if (!mmay_managed_heap_valid(heap) || length == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve_const(heap, array);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_ARRAY || slot->as.array == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *length = slot->as.array->length;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_describe(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    const MarimayManagedArrayDescriptor **descriptor
) {
    const MarimayManagedSlot *slot = NULL;
    if (descriptor != NULL) {
        *descriptor = NULL;
    }
    if (!mmay_managed_heap_valid(heap) || descriptor == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve_const(heap, array);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_ARRAY || slot->as.array == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *descriptor = slot->as.array->descriptor;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_index(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    int64_t index,
    const MarimayManagedArray **array,
    size_t *current_length
) {
    const MarimayManagedSlot *slot = NULL;
    if (array != NULL) {
        *array = NULL;
    }
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (!mmay_managed_heap_valid(heap) || array == NULL
        || current_length == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve_const(heap, reference);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_ARRAY || slot->as.array == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *current_length = slot->as.array->length;
    if (index < 0 || (uint64_t)index >= (uint64_t)slot->as.array->length) {
        return MMAY_MANAGED_BOUNDS;
    }
    *array = slot->as.array;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_array_get(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    int64_t index,
    MarimayManagedCell *result,
    size_t *current_length
) {
    const MarimayManagedArray *resolved = NULL;
    MarimayManagedStatus status;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (result == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    status = mmay_managed_array_index(
        heap,
        array,
        index,
        &resolved,
        current_length
    );
    if (status != MMAY_MANAGED_OK) {
        return status;
    }
    return mmay_managed_array_storage_load(
        resolved, (size_t)index, result
    );
}

size_t mmay_managed_heap_live_objects(const MarimayManagedHeap *heap) {
    return mmay_managed_heap_valid(heap) ? heap->live_object_count : 0;
}

size_t mmay_managed_heap_live_bytes(const MarimayManagedHeap *heap) {
    return mmay_managed_heap_valid(heap) ? heap->live_byte_count : 0;
}
