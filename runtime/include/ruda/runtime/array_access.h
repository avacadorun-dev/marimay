#ifndef RUDA_RUNTIME_ARRAY_ACCESS_H
#define RUDA_RUNTIME_ARRAY_ACCESS_H

#include "ruda/runtime/array_view.h"

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
RudaManagedStatus ruda_managed_array_view_length(
    const RudaManagedArrayView *view,
    size_t *length
);
RudaManagedStatus ruda_managed_array_view_require(
    const RudaManagedArrayView *view,
    int64_t index,
    size_t *current_length
);
RudaManagedStatus ruda_managed_array_view_get(
    const RudaManagedArrayView *view,
    int64_t index,
    RudaManagedArrayAccessMode mode,
    RudaManagedCell *result,
    size_t *current_length
);
RudaManagedStatus ruda_managed_array_view_set(
    RudaManagedHeap *heap,
    const RudaManagedArrayView *view,
    int64_t index,
    RudaManagedArrayAccessMode mode,
    RudaManagedCell value,
    size_t *current_length
);
RudaManagedStatus ruda_managed_array_view_push(
    RudaManagedHeap *heap,
    const RudaManagedArrayView *view,
    RudaManagedCell value
);
RudaManagedStatus ruda_managed_array_view_pop(
    const RudaManagedArrayView *view,
    RudaManagedCell *result,
    bool *popped
);

RudaManagedStatus ruda_managed_array_access_length(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    const RudaManagedArrayDescriptor *expected,
    size_t *length
);
RudaManagedStatus ruda_managed_array_access_require(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    const RudaManagedArrayDescriptor *expected,
    int64_t index,
    size_t *current_length
);
RudaManagedStatus ruda_managed_array_access_get(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    const RudaManagedArrayDescriptor *expected,
    int64_t index,
    RudaManagedArrayAccessMode mode,
    RudaManagedCell *result,
    size_t *current_length
);
RudaManagedStatus ruda_managed_array_access_set(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    const RudaManagedArrayDescriptor *expected,
    int64_t index,
    RudaManagedArrayAccessMode mode,
    RudaManagedCell value,
    size_t *current_length
);

#endif
