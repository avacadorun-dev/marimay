#ifndef MMAY_RUNTIME_ARRAY_EDIT_H
#define MMAY_RUNTIME_ARRAY_EDIT_H

#include "mmay/runtime/array.h"

/* A distinct, exact-length shallow copy. Source is rooted during collection.
   Failure leaves source unchanged and result invalid; generation starts at 0. */
MarimayManagedStatus mmay_managed_array_copy(
    MarimayManagedHeap *heap, MarimayManagedReference source,
    MarimayManagedReference *result
);

/* Allocation-free stable removal after the caller prepares its source result.
   BOUNDS leaves the array unchanged. current_length is the pre-operation length
   on OK/BOUNDS. Every failure precedes the single mutation-generation commit. */
MarimayManagedStatus mmay_managed_array_remove_at(
    MarimayManagedHeap *heap, MarimayManagedReference array,
    int64_t index, size_t *current_length
);

#endif
