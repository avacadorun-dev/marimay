#ifndef MMAY_RUNTIME_ARRAY_ACCESS_H
#define MMAY_RUNTIME_ARRAY_ACCESS_H

#include "mmay/runtime/array_view.h"

/*
 * Internal generated-C/VM boundary for exact array access. CHECKED validates
 * the signed index and publishes the observed length for G4002. IN_BOUNDS is
 * accepted only after the MIR or bytecode verifier has established the exact
 * array/index capability with no intervening invalidation.
 *
 * Legacy access operations resolve the managed handle exactly once per call.
 * A bound view validates the same descriptor once and may retain non-moving
 * object-field locations for one generated activation while the exact array
 * remains precisely reachable.
 */
MarimayManagedStatus mmay_managed_array_view_length(
    const MarimayManagedArrayView *view,
    size_t *length
);
MarimayManagedStatus mmay_managed_array_view_require(
    const MarimayManagedArrayView *view,
    int64_t index,
    size_t *current_length
);
MarimayManagedStatus mmay_managed_array_view_get(
    const MarimayManagedArrayView *view,
    int64_t index,
    MarimayManagedArrayAccessMode mode,
    MarimayManagedCell *result,
    size_t *current_length
);
MarimayManagedStatus mmay_managed_array_view_set(
    MarimayManagedHeap *heap,
    const MarimayManagedArrayView *view,
    int64_t index,
    MarimayManagedArrayAccessMode mode,
    MarimayManagedCell value,
    size_t *current_length
);
MarimayManagedStatus mmay_managed_array_view_push(
    MarimayManagedHeap *heap,
    const MarimayManagedArrayView *view,
    MarimayManagedCell value
);
MarimayManagedStatus mmay_managed_array_view_pop(
    const MarimayManagedArrayView *view,
    MarimayManagedCell *result,
    bool *popped
);

MarimayManagedStatus mmay_managed_array_access_length(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    const MarimayManagedArrayDescriptor *expected,
    size_t *length
);
MarimayManagedStatus mmay_managed_array_access_require(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    const MarimayManagedArrayDescriptor *expected,
    int64_t index,
    size_t *current_length
);
MarimayManagedStatus mmay_managed_array_access_get(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    const MarimayManagedArrayDescriptor *expected,
    int64_t index,
    MarimayManagedArrayAccessMode mode,
    MarimayManagedCell *result,
    size_t *current_length
);
MarimayManagedStatus mmay_managed_array_access_set(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    const MarimayManagedArrayDescriptor *expected,
    int64_t index,
    MarimayManagedArrayAccessMode mode,
    MarimayManagedCell value,
    size_t *current_length
);

#endif
