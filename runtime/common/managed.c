#include "managed_internal.h"

#include "ruda/base/provenance.h"

#include <limits.h>
#include <string.h>

size_t ruda_managed_language_max_array_length(void) {
#if SIZE_MAX > INT64_MAX
    return (size_t)INT64_MAX;
#else
    return SIZE_MAX;
#endif
}

RudaManagedReference ruda_managed_invalid_reference(void) {
    RudaManagedReference reference;
    reference.heap_provenance = UINT64_MAX;
    reference.slot = UINT32_MAX;
    reference.generation = UINT32_MAX;
    return reference;
}

bool ruda_managed_reference_is_invalid(RudaManagedReference reference) {
    return reference.heap_provenance == UINT64_MAX
        && reference.slot == UINT32_MAX
        && reference.generation == UINT32_MAX;
}

bool ruda_managed_reference_equal(
    RudaManagedReference left,
    RudaManagedReference right
) {
    return left.heap_provenance == right.heap_provenance
        && left.slot == right.slot && left.generation == right.generation;
}

RudaManagedHeapOptions ruda_managed_heap_default_options(void) {
    RudaManagedHeapOptions options;
    options.max_objects = 1000000;
    options.max_live_bytes = 64U * 1024U * 1024U;
    options.max_array_length = ruda_managed_language_max_array_length();
    options.collect_before_allocation = false;
    return options;
}

bool ruda_managed_heap_valid(const RudaManagedHeap *heap) {
    return heap != NULL && heap->initialized && heap->provenance != 0
        && heap->provenance != UINT64_MAX;
}

void ruda_managed_view_cache_clear(RudaManagedHeap *heap) {
    if (heap == NULL) {
        return;
    }
    heap->cached_record_reference = ruda_managed_invalid_reference();
    heap->cached_record_descriptor = NULL;
    heap->cached_record_object = NULL;
    heap->cached_record_fields = NULL;
    heap->cached_array_reference = ruda_managed_invalid_reference();
    heap->cached_array_descriptor = NULL;
    heap->cached_array_object = NULL;
    heap->cached_array_length = NULL;
    heap->cached_array_capacity = NULL;
    heap->cached_array_mutation_generation = NULL;
    heap->cached_array_storage = NULL;
    heap->cached_array_second_reference = ruda_managed_invalid_reference();
    heap->cached_array_second_descriptor = NULL;
    heap->cached_array_second_object = NULL;
}

bool ruda_managed_heap_init(
    RudaManagedHeap *heap,
    RudaAllocator allocator,
    RudaManagedHeapOptions options
) {
    uint64_t provenance = 0;
    if (heap == NULL || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL || options.max_objects == 0
        || options.max_objects > UINT32_MAX || options.max_live_bytes == 0
        || options.max_array_length > ruda_managed_language_max_array_length()
        || !ruda_new_provenance(&provenance)) {
        return false;
    }
    memset(heap, 0, sizeof(*heap));
    heap->allocator = allocator;
    heap->free_slot = UINT32_MAX;
    heap->provenance = provenance;
    heap->options = options;
    heap->initialized = true;
    ruda_managed_view_cache_clear(heap);
    return true;
}

void ruda_managed_heap_dispose(RudaManagedHeap *heap) {
    size_t index = 0;
    size_t slot_bytes = 0;
    size_t descriptor_bytes = 0;
    RudaManagedRootFrame *frame = NULL;
    if (!ruda_managed_heap_valid(heap)) {
        return;
    }
    frame = heap->roots;
    while (frame != NULL) {
        RudaManagedRootFrame *previous = frame->previous;
        memset(frame, 0, sizeof(*frame));
        frame = previous;
    }
    for (index = 0; index < heap->slot_count; index += 1) {
        ruda_managed_slot_dispose_object(heap, &heap->slots[index]);
    }
    (void)ruda_size_multiply(
        heap->slot_capacity,
        sizeof(RudaManagedSlot),
        &slot_bytes
    );
    ruda_deallocate(&heap->allocator, heap->slots, slot_bytes);
    (void)ruda_size_multiply(
        heap->immediate_enum_descriptor_capacity,
        sizeof(*heap->immediate_enum_descriptors),
        &descriptor_bytes
    );
    ruda_deallocate(
        &heap->allocator,
        heap->immediate_enum_descriptors,
        descriptor_bytes
    );
    memset(heap, 0, sizeof(*heap));
}

RudaManagedSlot *ruda_managed_resolve(
    RudaManagedHeap *heap,
    RudaManagedReference reference
) {
    RudaManagedSlot *slot = NULL;
    if (!ruda_managed_heap_valid(heap)
        || reference.heap_provenance != heap->provenance
        || reference.slot == UINT32_MAX
        || (size_t)reference.slot >= heap->slot_count) {
        return NULL;
    }
    slot = &heap->slots[reference.slot];
    return ruda_managed_slot_occupied(slot)
            && slot->generation == reference.generation
        ? slot
        : NULL;
}

const RudaManagedSlot *ruda_managed_resolve_const(
    const RudaManagedHeap *heap,
    RudaManagedReference reference
) {
    const RudaManagedSlot *slot = NULL;
    if (!ruda_managed_heap_valid(heap)
        || reference.heap_provenance != heap->provenance
        || reference.slot == UINT32_MAX
        || (size_t)reference.slot >= heap->slot_count) {
        return NULL;
    }
    slot = &heap->slots[reference.slot];
    return ruda_managed_slot_occupied(slot)
            && slot->generation == reference.generation
        ? slot
        : NULL;
}

RudaManagedStatus ruda_managed_reference_status(
    const RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedArrayDescriptor *expected
) {
    const RudaManagedSlot *slot = ruda_managed_resolve_const(heap, reference);
    if (slot == NULL) {
        return ruda_managed_immediate_enum_decode(
                heap, reference, NULL, NULL, NULL
            )
            ? RUDA_MANAGED_TYPE_MISMATCH
            : RUDA_MANAGED_INVALID_REFERENCE;
    }
    return expected == NULL
            || (slot->kind == RUDA_MANAGED_OBJECT_ARRAY
                && slot->as.array != NULL
                && slot->as.array->descriptor == expected)
        ? RUDA_MANAGED_OK
        : RUDA_MANAGED_TYPE_MISMATCH;
}

RudaManagedStatus ruda_managed_root_push(
    RudaManagedHeap *heap,
    RudaManagedRootFrame *frame,
    RudaManagedReference *const *locations,
    size_t location_count
) {
    size_t index = 0;
    if (!ruda_managed_heap_valid(heap) || frame == NULL
        || (locations == NULL && location_count != 0) || frame->linked) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    for (index = 0; index < location_count; index += 1) {
        if (locations[index] == NULL
            || (!ruda_managed_reference_is_invalid(*locations[index])
                && !ruda_managed_reference_valid(
                    heap, *locations[index]
                ))) {
            return locations[index] == NULL
                ? RUDA_MANAGED_INVALID_ARGUMENT
                : RUDA_MANAGED_INVALID_REFERENCE;
        }
    }
    frame->locations = locations;
    frame->location_count = location_count;
    frame->previous = heap->roots;
    frame->heap_provenance = heap->provenance;
    frame->linked = true;
    heap->roots = frame;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_root_pop(
    RudaManagedHeap *heap,
    RudaManagedRootFrame *frame
) {
    if (!ruda_managed_heap_valid(heap) || frame == NULL || !frame->linked
        || frame->heap_provenance != heap->provenance
        || heap->roots != frame) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    heap->roots = frame->previous;
    memset(frame, 0, sizeof(*frame));
    return RUDA_MANAGED_OK;
}

static void ruda_managed_clear_marks(RudaManagedHeap *heap) {
    size_t index = 0;
    for (index = 0; index < heap->slot_count; index += 1) {
        heap->slots[index].marked = false;
        heap->slots[index].next_gray = UINT32_MAX;
    }
}

RudaManagedStatus ruda_managed_mark_reference(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    uint32_t *gray_head
) {
    RudaManagedSlot *slot = NULL;
    if (ruda_managed_reference_is_invalid(reference)) {
        return RUDA_MANAGED_OK;
    }
    if (ruda_managed_immediate_enum_decode(
            heap, reference, NULL, NULL, NULL
        )) {
        return RUDA_MANAGED_OK;
    }
    slot = ruda_managed_resolve(heap, reference);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (!slot->marked) {
        slot->marked = true;
        slot->next_gray = *gray_head;
        *gray_head = reference.slot;
    }
    return RUDA_MANAGED_OK;
}


RudaManagedStatus ruda_managed_collect_internal(
    RudaManagedHeap *heap,
    const RudaManagedReference *temporary_roots,
    size_t temporary_root_count,
    const RudaManagedCell *temporary_cells,
    size_t temporary_cell_count
) {
    RudaManagedRootFrame *frame = NULL;
    uint32_t gray_head = UINT32_MAX;
    size_t index = 0;
    RudaManagedStatus status = RUDA_MANAGED_OK;
    if (!ruda_managed_heap_valid(heap) || heap->collecting
        || (temporary_roots == NULL && temporary_root_count != 0)
        || (temporary_cells == NULL && temporary_cell_count != 0)) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    heap->collecting = true;
    ruda_managed_view_cache_clear(heap);
    ruda_managed_clear_marks(heap);
    for (frame = heap->roots; frame != NULL; frame = frame->previous) {
        if (!frame->linked || frame->heap_provenance != heap->provenance
            || (frame->locations == NULL && frame->location_count != 0)) {
            status = RUDA_MANAGED_INVALID_ARGUMENT;
            goto cleanup;
        }
        for (index = 0; index < frame->location_count; index += 1) {
            if (frame->locations[index] == NULL) {
                status = RUDA_MANAGED_INVALID_ARGUMENT;
                goto cleanup;
            }
            status = ruda_managed_mark_reference(
                heap,
                *frame->locations[index],
                &gray_head
            );
            if (status != RUDA_MANAGED_OK) {
                goto cleanup;
            }
        }
    }
    for (index = 0; index < temporary_root_count; index += 1) {
        status = ruda_managed_mark_reference(
            heap,
            temporary_roots[index],
            &gray_head
        );
        if (status != RUDA_MANAGED_OK) {
            goto cleanup;
        }
    }
    for (index = 0; index < temporary_cell_count; index += 1) {
        status = ruda_managed_mark_reference(
            heap,
            temporary_cells[index].reference,
            &gray_head
        );
        if (status != RUDA_MANAGED_OK) {
            goto cleanup;
        }
    }
    while (gray_head != UINT32_MAX) {
        RudaManagedSlot *slot = &heap->slots[gray_head];
        gray_head = slot->next_gray;
        slot->next_gray = UINT32_MAX;
        if (!ruda_managed_slot_occupied(slot)) {
            status = RUDA_MANAGED_INVALID_REFERENCE;
            goto cleanup;
        }
        status = ruda_managed_trace_slot(heap, slot, &gray_head);
        if (status != RUDA_MANAGED_OK) {
            goto cleanup;
        }
    }
    for (index = 0; index < heap->slot_count; index += 1) {
        RudaManagedSlot *slot = &heap->slots[index];
        if (!ruda_managed_slot_occupied(slot) || slot->marked) {
            continue;
        }
        {
            size_t bytes = ruda_managed_slot_object_bytes(slot);
            ruda_managed_slot_dispose_object(heap, slot);
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
    ruda_managed_clear_marks(heap);
    heap->collecting = false;
    return status;
}

RudaManagedStatus ruda_managed_heap_collect(RudaManagedHeap *heap) {
    return ruda_managed_collect_internal(heap, NULL, 0, NULL, 0);
}

bool ruda_managed_add_within_limit(
    size_t current,
    size_t amount,
    size_t limit,
    size_t *result
) {
    return ruda_size_add(current, amount, result) && *result <= limit;
}

bool ruda_managed_new_slot_capacity(
    const RudaManagedHeap *heap,
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
        if (!ruda_size_multiply(next, 2, &doubled)
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
        || !ruda_size_multiply(
            heap->slot_capacity,
            sizeof(RudaManagedSlot),
            &old_bytes
        ) || !ruda_size_multiply(
            next,
            sizeof(RudaManagedSlot),
            &new_bytes
        )) {
        return false;
    }
    *capacity = next;
    *added_bytes = new_bytes - old_bytes;
    return true;
}

RudaManagedStatus ruda_managed_array_length(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    size_t *length
) {
    const RudaManagedSlot *slot = NULL;
    if (length != NULL) {
        *length = 0;
    }
    if (!ruda_managed_heap_valid(heap) || length == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve_const(heap, array);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_ARRAY || slot->as.array == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    *length = slot->as.array->length;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_describe(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    const RudaManagedArrayDescriptor **descriptor
) {
    const RudaManagedSlot *slot = NULL;
    if (descriptor != NULL) {
        *descriptor = NULL;
    }
    if (!ruda_managed_heap_valid(heap) || descriptor == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve_const(heap, array);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_ARRAY || slot->as.array == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    *descriptor = slot->as.array->descriptor;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_index(
    const RudaManagedHeap *heap,
    RudaManagedReference reference,
    int64_t index,
    const RudaManagedArray **array,
    size_t *current_length
) {
    const RudaManagedSlot *slot = NULL;
    if (array != NULL) {
        *array = NULL;
    }
    if (current_length != NULL) {
        *current_length = 0;
    }
    if (!ruda_managed_heap_valid(heap) || array == NULL
        || current_length == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve_const(heap, reference);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_ARRAY || slot->as.array == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    *current_length = slot->as.array->length;
    if (index < 0 || (uint64_t)index >= (uint64_t)slot->as.array->length) {
        return RUDA_MANAGED_BOUNDS;
    }
    *array = slot->as.array;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_array_get(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    int64_t index,
    RudaManagedCell *result,
    size_t *current_length
) {
    const RudaManagedArray *resolved = NULL;
    RudaManagedStatus status;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (result == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    status = ruda_managed_array_index(
        heap,
        array,
        index,
        &resolved,
        current_length
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    return ruda_managed_array_storage_load(
        resolved, (size_t)index, result
    );
}

size_t ruda_managed_heap_live_objects(const RudaManagedHeap *heap) {
    return ruda_managed_heap_valid(heap) ? heap->live_object_count : 0;
}

size_t ruda_managed_heap_live_bytes(const RudaManagedHeap *heap) {
    return ruda_managed_heap_valid(heap) ? heap->live_byte_count : 0;
}
