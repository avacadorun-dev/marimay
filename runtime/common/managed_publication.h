#ifndef MMAY_MANAGED_PUBLICATION_H
#define MMAY_MANAGED_PUBLICATION_H

#include "managed_collection.h"

/* Private single-operation reservation. No source call, recursive heap use or
 * unrelated mutation may separate preparation from commit/cancel. Deferred
 * collection preserves borrowed source bytes until the owner has copied them;
 * all paths must finish that collection before returning to their caller. */
typedef struct MarimayManagedPublication {
    MarimayManagedHeap *heap;
    MarimayManagedCollection collection;
    size_t slot_capacity;
    size_t added_bytes;
    size_t next_bytes;
    bool ready;
} MarimayManagedPublication;

MarimayManagedStatus mmay_managed_publication_prepare(
    MarimayManagedHeap *heap, size_t object_bytes,
    const MarimayManagedReference *temporary_roots, size_t temporary_root_count,
    const MarimayManagedCell *temporary_cells, size_t temporary_cell_count,
    bool defer_sweep, MarimayManagedPublication *publication
);
MarimayManagedStatus mmay_managed_publication_commit(
    MarimayManagedPublication *publication, MarimayManagedObjectKind kind,
    void *object, MarimayManagedReference *result
);
void mmay_managed_publication_cancel(MarimayManagedPublication *publication);

#endif
