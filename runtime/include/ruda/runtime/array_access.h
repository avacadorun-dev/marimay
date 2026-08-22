#ifndef RUDA_RUNTIME_ARRAY_ACCESS_H
#define RUDA_RUNTIME_ARRAY_ACCESS_H

#include "ruda/runtime/array.h"

/*
 * Internal generated-C/VM boundary for exact array access. CHECKED validates
 * the signed index and publishes the observed length for G4002. IN_BOUNDS is
 * accepted only after the MIR or bytecode verifier has established the exact
 * array/index capability with no intervening invalidation.
 *
 * Every operation resolves the managed handle exactly once, validates the
 * immutable descriptor at that resolve, and retains no runtime pointer after
 * returning. The caller keeps the array reference rooted for the whole call.
 */
typedef enum RudaManagedArrayAccessMode {
    RUDA_MANAGED_ARRAY_ACCESS_CHECKED,
    RUDA_MANAGED_ARRAY_ACCESS_IN_BOUNDS
} RudaManagedArrayAccessMode;

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
