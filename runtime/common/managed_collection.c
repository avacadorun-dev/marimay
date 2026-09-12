#include "managed_collection.h"
#include "managed_internal.h"

#include <limits.h>
#include <string.h>

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


MarimayManagedStatus mmay_managed_collection_begin(
    MarimayManagedHeap *heap,
    const MarimayManagedReference *temporary_roots,
    size_t temporary_root_count,
    const MarimayManagedCell *temporary_cells,
    size_t temporary_cell_count,
    MarimayManagedCollection *collection
) {
    MarimayManagedRootFrame *frame = NULL;
    uint32_t gray_head = UINT32_MAX;
    size_t index = 0;
    MarimayManagedStatus status = MMAY_MANAGED_OK;
    if (collection == NULL) return MMAY_MANAGED_INVALID_ARGUMENT;
    memset(collection, 0, sizeof(*collection));
    collection->dead_head = UINT32_MAX;
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
    collection->heap = heap;
    collection->live_objects = heap->live_object_count;
    collection->live_bytes = heap->live_byte_count;
    collection->reusable_slot = heap->free_slot != UINT32_MAX;
    uint32_t dead_tail = UINT32_MAX;
    for (index = 0; index < heap->slot_count; index += 1) {
        MarimayManagedSlot *slot = &heap->slots[index];
        if (!mmay_managed_slot_occupied(slot) || slot->marked) continue;
        size_t bytes = mmay_managed_slot_object_bytes(slot);
        if (collection->live_objects == 0 || bytes > collection->live_bytes) {
            status = MMAY_MANAGED_INVALID_REFERENCE;
            goto cleanup;
        }
        collection->live_objects -= 1;
        collection->live_bytes -= bytes;
        if (slot->generation != UINT32_MAX) collection->reusable_slot = true;
        if (dead_tail == UINT32_MAX) collection->dead_head = (uint32_t)index;
        else heap->slots[dead_tail].next_gray = (uint32_t)index;
        dead_tail = (uint32_t)index;
        slot->next_gray = UINT32_MAX;
    }
    collection->pending = true;
    return MMAY_MANAGED_OK;

cleanup:
    mmay_managed_clear_marks(heap);
    heap->collecting = false;
    memset(collection, 0, sizeof(*collection));
    return status;
}

void mmay_managed_collection_finish(MarimayManagedCollection *collection) {
    if (collection == NULL || !collection->pending) return;
    MarimayManagedHeap *heap = collection->heap;
    uint32_t index = collection->dead_head;
    while (index != UINT32_MAX) {
        MarimayManagedSlot *slot = &heap->slots[index];
        uint32_t next = slot->next_gray;
        size_t bytes = mmay_managed_slot_object_bytes(slot);
        mmay_managed_slot_dispose_object(heap, slot);
        heap->live_object_count -= 1;
        heap->live_byte_count -= bytes;
        if (slot->generation == UINT32_MAX) {
            slot->retired = true;
            slot->next_free = UINT32_MAX;
        } else {
            slot->generation += 1;
            slot->next_free = heap->free_slot;
            heap->free_slot = index;
        }
        index = next;
    }
    mmay_managed_clear_marks(heap);
    heap->collecting = false;
    collection->pending = false;
}

MarimayManagedStatus mmay_managed_collect_internal(
    MarimayManagedHeap *heap,
    const MarimayManagedReference *temporary_roots, size_t temporary_root_count,
    const MarimayManagedCell *temporary_cells, size_t temporary_cell_count
) {
    MarimayManagedCollection collection = {0};
    MarimayManagedStatus status = mmay_managed_collection_begin(heap,
        temporary_roots, temporary_root_count, temporary_cells,
        temporary_cell_count, &collection);
    if (status == MMAY_MANAGED_OK) mmay_managed_collection_finish(&collection);
    return status;
}

MarimayManagedStatus mmay_managed_heap_collect(MarimayManagedHeap *heap) {
    return mmay_managed_collect_internal(heap, NULL, 0, NULL, 0);
}
