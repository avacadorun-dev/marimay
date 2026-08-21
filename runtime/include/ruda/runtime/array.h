#ifndef RUDA_RUNTIME_ARRAY_H
#define RUDA_RUNTIME_ARRAY_H

#include "ruda/runtime/heap.h"

/*
 * Exact array type identity is the descriptor address. Descriptors are
 * immutable and must outlive every heap object created from them. Reference
 * descriptors name the one exact managed object type accepted by each
 * element. Strings use kind STRING and a null descriptor; every other
 * reference kind carries the exact child descriptor address. This represents
 * arbitrary nested and recursive []T graphs without teaching the
 * collector one enum value per source type.
 */
typedef struct RudaManagedArrayDescriptor {
    RudaManagedCellKind cell_kind;
    RudaManagedObjectKind reference_kind;
    const void *reference_descriptor;
} RudaManagedArrayDescriptor;

/* Canonical descriptors used by the f64 preview surface. */
extern const RudaManagedArrayDescriptor ruda_managed_array_f64_descriptor;
extern const RudaManagedArrayDescriptor
    ruda_managed_array_array_f64_descriptor;

/*
 * Creates one array transactionally. On failure, result is invalid and no
 * object is published. Reference-valued initial cells are temporary roots if
 * collection is attempted.
 */
RudaManagedStatus ruda_managed_array_new(
    RudaManagedHeap *heap,
    const RudaManagedArrayDescriptor *descriptor,
    const RudaManagedCell *initial_cells,
    size_t initial_length,
    RudaManagedReference *result
);
RudaManagedStatus ruda_managed_array_length(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    size_t *length
);
RudaManagedStatus ruda_managed_array_describe(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    const RudaManagedArrayDescriptor **descriptor
);
/*
 * Indexing validates the signed index before conversion or address arithmetic.
 * current_length receives the observed length on both OK and BOUNDS.
 */
RudaManagedStatus ruda_managed_array_get(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    int64_t index,
    RudaManagedCell *result,
    size_t *current_length
);
RudaManagedStatus ruda_managed_array_set(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    int64_t index,
    RudaManagedCell value,
    size_t *current_length
);
/*
 * Push is failure-atomic. For reference elements, value.reference is a
 * temporary root during any collection or allocation performed by this call.
 */
RudaManagedStatus ruda_managed_array_push(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    RudaManagedCell value
);
/* Successful initialized-sequence mutations advance this non-wrapping value. */
RudaManagedStatus ruda_managed_array_mutation_generation(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    uint64_t *generation
);
RudaManagedStatus ruda_managed_array_check_mutation_generation(
    const RudaManagedHeap *heap,
    RudaManagedReference array,
    uint64_t expected_generation
);
/* On an empty array, popped is false and result is zeroed. */
RudaManagedStatus ruda_managed_array_pop(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    RudaManagedCell *result,
    bool *popped
);
/* new_length is signed so callers can preserve the source error distinction. */
RudaManagedStatus ruda_managed_array_truncate(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    int64_t new_length
);
RudaManagedStatus ruda_managed_array_clear(
    RudaManagedHeap *heap,
    RudaManagedReference array
);
/* additional is signed so a negative request is not silently converted. */
RudaManagedStatus ruda_managed_array_reserve(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    int64_t additional
);

#endif
