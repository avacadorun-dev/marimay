#include "managed_publication.h"
#include "managed_internal.h"

#include <limits.h>
#include <string.h>

void mmay_managed_publication_cancel(MarimayManagedPublication *publication) {
    if (publication == NULL) return;
    mmay_managed_collection_finish(&publication->collection);
    publication->ready = false;
}

static MarimayManagedStatus publication_prepare(
    MarimayManagedHeap *heap, size_t object_bytes,
    const MarimayManagedReference *temporary_roots, size_t temporary_root_count,
    const MarimayManagedCell *temporary_cells, size_t temporary_cell_count,
    bool defer_sweep, MarimayManagedPublication *publication
) {
    publication->heap = heap;
    publication->ready = false;
    publication->collection.pending = false;
    bool collected = false;
    MarimayManagedStatus status;
    for (;;) {
        if (!collected && (heap->options.collect_before_allocation
                || heap->live_object_count >= heap->options.max_objects)) {
collect:
            status = mmay_managed_collection_begin(heap,
                temporary_roots, temporary_root_count, temporary_cells,
                temporary_cell_count, &publication->collection);
            if (status != MMAY_MANAGED_OK) return status;
            collected = true;
            if (!defer_sweep)
                mmay_managed_collection_finish(&publication->collection);
        }
        size_t live_objects = collected ? publication->collection.live_objects
            : heap->live_object_count;
        size_t live_bytes = collected ? publication->collection.live_bytes
            : heap->live_byte_count;
        size_t slot_added_bytes = 0;
        bool slot_ready;
        if (collected && publication->collection.reusable_slot) {
            publication->slot_capacity = heap->slot_capacity;
            slot_ready = true;
        } else {
            slot_ready = mmay_managed_new_slot_capacity(heap,
                &publication->slot_capacity, &slot_added_bytes);
        }
        if (live_objects >= heap->options.max_objects || !slot_ready
            || !mmay_size_add(object_bytes, slot_added_bytes,
                &publication->added_bytes)) {
            if (!collected) goto collect;
            mmay_managed_publication_cancel(publication);
            return MMAY_MANAGED_LIMIT_REACHED;
        }
        if (!mmay_managed_add_within_limit(live_bytes, publication->added_bytes,
                heap->options.max_live_bytes, &publication->next_bytes)) {
            if (!collected) goto collect;
            mmay_managed_publication_cancel(publication);
            return mmay_managed_resource_budget_failure(heap,
                publication->added_bytes);
        }
        publication->ready = true;
        return MMAY_MANAGED_OK;
    }
}

MarimayManagedStatus mmay_managed_publication_prepare(
    MarimayManagedHeap *heap, size_t object_bytes,
    const MarimayManagedReference *temporary_roots, size_t temporary_root_count,
    const MarimayManagedCell *temporary_cells, size_t temporary_cell_count,
    bool defer_sweep, MarimayManagedPublication *publication
) {
    if (publication == NULL) return MMAY_MANAGED_INVALID_ARGUMENT;
    publication->heap = NULL;
    publication->ready = false;
    publication->collection.pending = false;
    if (!mmay_managed_heap_valid(heap) || heap->collecting)
        return MMAY_MANAGED_INVALID_ARGUMENT;
    return publication_prepare(heap, object_bytes, temporary_roots,
        temporary_root_count, temporary_cells, temporary_cell_count,
        defer_sweep, publication);
}

static bool mmay_managed_publication_kind(MarimayManagedObjectKind kind) {
    return kind == MMAY_MANAGED_OBJECT_STRING
        || kind == MMAY_MANAGED_OBJECT_RECORD
        || kind == MMAY_MANAGED_OBJECT_ENUM
        || kind == MMAY_MANAGED_OBJECT_INTERFACE
        || kind == MMAY_MANAGED_OBJECT_MAP;
}

/* Both entry paths use one slot installer. An already staged object has no
 * intervening allocation or observer between preparation and this call. */
static MarimayManagedStatus publication_install(
    MarimayManagedHeap *heap, MarimayManagedObjectKind kind, void *object,
    size_t slot_capacity, size_t next_bytes, MarimayManagedReference *result
) {
    if (slot_capacity != heap->slot_capacity) {
        size_t old_bytes = heap->slot_capacity * sizeof(MarimayManagedSlot);
        size_t new_bytes = slot_capacity * sizeof(MarimayManagedSlot);
        MarimayManagedSlot *resized = mmay_resize(&heap->allocator,
            heap->slots, old_bytes, new_bytes);
        if (resized == NULL)
            return mmay_managed_resource_host_failure(heap, new_bytes);
        memset(resized + heap->slot_capacity, 0,
            (slot_capacity - heap->slot_capacity) * sizeof(*resized));
        heap->slots = resized;
        heap->slot_capacity = slot_capacity;
    }
    uint32_t slot_index;
    if (heap->free_slot != UINT32_MAX) {
        MarimayManagedSlot *slot = &heap->slots[heap->free_slot];
        slot_index = heap->free_slot;
        heap->free_slot = slot->next_free;
        slot->next_free = UINT32_MAX;
    } else {
        slot_index = (uint32_t)heap->slot_count;
        heap->slot_count += 1;
        heap->slots[slot_index].generation = 1;
        heap->slots[slot_index].next_free = UINT32_MAX;
    }
    heap->slots[slot_index].kind = kind;
    switch (kind) {
        case MMAY_MANAGED_OBJECT_STRING:
            heap->slots[slot_index].as.string = object;
            break;
        case MMAY_MANAGED_OBJECT_RECORD:
            heap->slots[slot_index].as.record = object;
            break;
        case MMAY_MANAGED_OBJECT_ENUM:
            heap->slots[slot_index].as.enumeration = object;
            break;
        case MMAY_MANAGED_OBJECT_INTERFACE:
            heap->slots[slot_index].as.iface = object;
            break;
        case MMAY_MANAGED_OBJECT_MAP:
            heap->slots[slot_index].as.map = object;
            break;
        case MMAY_MANAGED_OBJECT_ARRAY:
        case MMAY_MANAGED_OBJECT_INVALID:
            return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    heap->slots[slot_index].marked = false;
    heap->slots[slot_index].retired = false;
    heap->live_object_count += 1;
    heap->live_byte_count = next_bytes;
    result->heap_provenance = heap->provenance;
    result->slot = slot_index;
    result->generation = heap->slots[slot_index].generation;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_publication_commit(
    MarimayManagedPublication *publication, MarimayManagedObjectKind kind,
    void *object, MarimayManagedReference *result
) {
    if (result != NULL) *result = mmay_managed_invalid_reference();
    if (publication == NULL || !publication->ready
        || !mmay_managed_heap_valid(publication->heap) || object == NULL
        || result == NULL || !mmay_managed_publication_kind(kind)) {
        mmay_managed_publication_cancel(publication);
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    MarimayManagedHeap *heap = publication->heap;
    mmay_managed_collection_finish(&publication->collection);
    publication->ready = false;
    size_t next_bytes = 0;
    if (!mmay_size_add(heap->live_byte_count, publication->added_bytes,
            &next_bytes) || next_bytes != publication->next_bytes)
        return MMAY_MANAGED_INVALID_ARGUMENT;
    return publication_install(heap, kind, object, publication->slot_capacity,
        next_bytes, result);
}

MarimayManagedStatus mmay_managed_publish_object(
    MarimayManagedHeap *heap, MarimayManagedObjectKind kind,
    void *object, size_t object_bytes,
    const MarimayManagedReference *temporary_roots, size_t temporary_root_count,
    const MarimayManagedCell *temporary_cells, size_t temporary_cell_count,
    MarimayManagedReference *result
) {
    if (result != NULL) *result = mmay_managed_invalid_reference();
    if (!mmay_managed_heap_valid(heap) || heap->collecting
        || result == NULL || object == NULL
        || !mmay_managed_publication_kind(kind)) return MMAY_MANAGED_INVALID_ARGUMENT;
    MarimayManagedPublication publication;
    MarimayManagedStatus status = publication_prepare(heap,
        object_bytes, temporary_roots, temporary_root_count, temporary_cells,
        temporary_cell_count, false, &publication);
    if (status != MMAY_MANAGED_OK) return status;
    return publication_install(heap, kind, object, publication.slot_capacity,
        publication.next_bytes, result);
}
