#ifndef MMAY_RUNTIME_GENERATED_ROOTS_H
#define MMAY_RUNTIME_GENERATED_ROOTS_H

#include "mmay/runtime/heap.h"

/* These helpers are reserved for compiler-generated, statically typed C. */
static inline void mmay_managed_root_push_generated(
    MarimayManagedHeap *heap,
    MarimayManagedRootFrame *frame,
    MarimayManagedReference *const *locations,
    size_t location_count
) {
    frame->locations = locations;
    frame->location_count = location_count;
    frame->previous = heap->roots;
    frame->heap_provenance = heap->provenance;
    frame->linked = true;
    heap->roots = frame;
}

static inline bool mmay_managed_root_pop_generated(
    MarimayManagedHeap *heap,
    MarimayManagedRootFrame *frame
) {
    if (heap->roots != frame || !frame->linked
        || frame->heap_provenance != heap->provenance) {
        return false;
    }
    heap->roots = frame->previous;
    frame->linked = false;
    return true;
}

#endif
