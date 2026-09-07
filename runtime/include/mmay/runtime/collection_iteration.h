#ifndef MMAY_RUNTIME_COLLECTION_ITERATION_H
#define MMAY_RUNTIME_COLLECTION_ITERATION_H

#include "mmay/runtime/map.h"

/* D089 allocation-free checked traversal. Zero starts NEXT; a positive
 * result is one past an active storage position, zero means exhaustion.
 * Callers own independent cursors and check mutation generation before NEXT.
 * No cursor grants an unchecked capability or retains an object/address. */
MarimayManagedStatus mmay_managed_collection_next(
    const MarimayManagedHeap *heap, MarimayManagedReference collection,
    bool is_set, size_t cursor, size_t *next
);

/* Rejects zero, out-of-range and removed positions. Both outputs are required;
 * Set produces a zero value cell. Output cells borrow the receiver's roots. */
MarimayManagedStatus mmay_managed_collection_cursor_entry(
    const MarimayManagedHeap *heap, MarimayManagedReference collection,
    bool is_set, size_t cursor, MarimayManagedCell *key, MarimayManagedCell *value
);

#endif
