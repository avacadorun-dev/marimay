#ifndef RUDA_RUNTIME_ARRAY_H
#define RUDA_RUNTIME_ARRAY_H

#include "ruda/runtime/heap.h"

/*
 * Exact array type identity is the descriptor address. Descriptors are
 * immutable and must outlive every heap object created from them. Reference
 * descriptors name the one exact array type accepted by each element; this
 * represents arbitrary nested and recursive Array<T> graphs without teaching
 * the collector one enum value per source type.
 */
typedef struct RudaManagedArrayDescriptor {
    RudaManagedCellKind cell_kind;
    const struct RudaManagedArrayDescriptor *reference_type;
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

#endif
