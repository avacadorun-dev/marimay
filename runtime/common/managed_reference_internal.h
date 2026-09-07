#ifndef MMAY_MANAGED_REFERENCE_INTERNAL_H
#define MMAY_MANAGED_REFERENCE_INTERNAL_H

#include "mmay/runtime/heap.h"
#include "mmay/runtime/generated_slots.h"

/* Runtime-private, allocation-free validation. Keep the complete checks visible
 * to each separately compiled owner without requiring archive or program LTO.
 * Returned slots are borrowed only until the next operation that can grow or
 * collect the heap; this does not introduce a retained view or unchecked API. */
static inline bool mmay_managed_heap_valid(const MarimayManagedHeap *heap) {
    return heap != NULL && heap->initialized && heap->provenance != 0
        && heap->provenance != UINT64_MAX;
}

static inline bool mmay_managed_slot_occupied(const MarimayManagedSlot *slot) {
    if (slot == NULL) return false;
    switch (slot->kind) {
        case MMAY_MANAGED_OBJECT_ARRAY: return slot->as.array != NULL;
        case MMAY_MANAGED_OBJECT_STRING: return slot->as.string != NULL;
        case MMAY_MANAGED_OBJECT_RECORD: return slot->as.record != NULL;
        case MMAY_MANAGED_OBJECT_ENUM: return slot->as.enumeration != NULL;
        case MMAY_MANAGED_OBJECT_INTERFACE: return slot->as.iface != NULL;
        case MMAY_MANAGED_OBJECT_MAP: return slot->as.map != NULL;
        case MMAY_MANAGED_OBJECT_INVALID: break;
    }
    return false;
}

static inline const MarimayManagedSlot *mmay_managed_resolve_const(
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
        ? slot : NULL;
}

static inline MarimayManagedSlot *mmay_managed_resolve(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference
) {
    /* The mutable entry point receives a mutable heap and borrows its slot. */
    return (MarimayManagedSlot *)mmay_managed_resolve_const(heap, reference);
}

#endif
