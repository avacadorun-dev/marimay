#ifndef RUDA_RUNTIME_GENERATED_ROOTS_H
#define RUDA_RUNTIME_GENERATED_ROOTS_H

#include "ruda/runtime/heap.h"

/* These helpers are reserved for compiler-generated, statically typed C. */
static inline void ruda_managed_root_push_generated(
    RudaManagedHeap *heap,
    RudaManagedRootFrame *frame,
    RudaManagedReference *const *locations,
    size_t location_count
) {
    frame->locations = locations;
    frame->location_count = location_count;
    frame->previous = heap->roots;
    frame->heap_provenance = heap->provenance;
    frame->linked = true;
    heap->roots = frame;
}

static inline bool ruda_managed_root_pop_generated(
    RudaManagedHeap *heap,
    RudaManagedRootFrame *frame
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
