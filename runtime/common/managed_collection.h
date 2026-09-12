#ifndef MMAY_MANAGED_COLLECTION_H
#define MMAY_MANAGED_COLLECTION_H

#include "mmay/runtime/heap.h"

/* One allocation-free collection transaction. Begin validates/traces roots,
 * predicts exact post-sweep accounting and keeps unreachable bytes intact.
 * Finish reclaims them and clears all marks. A successful begin must have one
 * finish before any source execution or other heap mutation. The token owns
 * no allocations and must not be copied while pending. */
typedef struct MarimayManagedCollection {
    MarimayManagedHeap *heap;
    size_t live_objects;
    size_t live_bytes;
    uint32_t dead_head;
    bool reusable_slot;
    bool pending;
} MarimayManagedCollection;

MarimayManagedStatus mmay_managed_collection_begin(
    MarimayManagedHeap *heap,
    const MarimayManagedReference *temporary_roots,
    size_t temporary_root_count,
    const MarimayManagedCell *temporary_cells,
    size_t temporary_cell_count,
    MarimayManagedCollection *collection
);
void mmay_managed_collection_finish(MarimayManagedCollection *collection);

#endif
