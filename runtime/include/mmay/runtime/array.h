#ifndef MMAY_RUNTIME_ARRAY_H
#define MMAY_RUNTIME_ARRAY_H

#include "mmay/runtime/heap.h"

/*
 * Exact array type identity is the descriptor address. Descriptors are
 * immutable and must outlive every heap object created from them. Reference
 * descriptors name the one exact managed object type accepted by each
 * element. Strings use kind STRING and a null descriptor; every other
 * reference kind carries the exact child descriptor address. This represents
 * arbitrary nested and recursive []T graphs without teaching the
 * collector one enum value per source type.
 */
typedef enum MarimayManagedArrayStorageKind {
    MMAY_MANAGED_ARRAY_STORAGE_INVALID,
    MMAY_MANAGED_ARRAY_STORAGE_RAW8,
    MMAY_MANAGED_ARRAY_STORAGE_RAW16,
    MMAY_MANAGED_ARRAY_STORAGE_RAW32,
    MMAY_MANAGED_ARRAY_STORAGE_RAW64,
    MMAY_MANAGED_ARRAY_STORAGE_CELL
} MarimayManagedArrayStorageKind;

typedef struct MarimayManagedArrayDescriptor {
    MarimayManagedCellKind cell_kind;
    MarimayManagedArrayStorageKind storage_kind;
    size_t storage_stride;
    MarimayManagedObjectKind reference_kind;
    const void *reference_descriptor;
} MarimayManagedArrayDescriptor;

/* Canonical descriptors used by the f64 preview surface. */
extern const MarimayManagedArrayDescriptor mmay_managed_array_f64_descriptor;
extern const MarimayManagedArrayDescriptor
    mmay_managed_array_array_f64_descriptor;

/*
 * Creates one array transactionally. On failure, result is invalid and no
 * object is published. Reference-valued initial cells are temporary roots if
 * collection is attempted.
 */
MarimayManagedStatus mmay_managed_array_new(
    MarimayManagedHeap *heap,
    const MarimayManagedArrayDescriptor *descriptor,
    const MarimayManagedCell *initial_cells,
    size_t initial_length,
    MarimayManagedReference *result
);
MarimayManagedStatus mmay_managed_array_length(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    size_t *length
);
MarimayManagedStatus mmay_managed_array_describe(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    const MarimayManagedArrayDescriptor **descriptor
);
/*
 * Indexing validates the signed index before conversion or address arithmetic.
 * current_length receives the observed length on both OK and BOUNDS.
 */
MarimayManagedStatus mmay_managed_array_get(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    int64_t index,
    MarimayManagedCell *result,
    size_t *current_length
);
MarimayManagedStatus mmay_managed_array_set(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    int64_t index,
    MarimayManagedCell value,
    size_t *current_length
);
/*
 * Push is failure-atomic. For reference elements, value.reference is a
 * temporary root during any collection or allocation performed by this call.
 */
MarimayManagedStatus mmay_managed_array_push(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    MarimayManagedCell value
);
/* Successful initialized-sequence mutations advance this non-wrapping value. */
MarimayManagedStatus mmay_managed_array_mutation_generation(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    uint64_t *generation
);
MarimayManagedStatus mmay_managed_array_check_mutation_generation(
    const MarimayManagedHeap *heap,
    MarimayManagedReference array,
    uint64_t expected_generation
);
/* On an empty array, popped is false and result is zeroed. */
MarimayManagedStatus mmay_managed_array_pop(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    MarimayManagedCell *result,
    bool *popped
);
/* new_length is signed so callers can preserve the source error distinction. */
MarimayManagedStatus mmay_managed_array_truncate(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    int64_t new_length
);
MarimayManagedStatus mmay_managed_array_clear(
    MarimayManagedHeap *heap,
    MarimayManagedReference array
);
/* additional is signed so a negative request is not silently converted. */
MarimayManagedStatus mmay_managed_array_reserve(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    int64_t additional
);
/* Internal generated-code primitive for a verified repeated RAW64 push loop. */
MarimayManagedStatus mmay_managed_array_fill_raw64_pushes(
    MarimayManagedHeap *heap,
    MarimayManagedReference array,
    size_t count,
    uint64_t bits
);

#endif
