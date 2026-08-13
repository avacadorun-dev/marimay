#ifndef RUDA_RUNTIME_MANAGED_H
#define RUDA_RUNTIME_MANAGED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"

/*
 * Managed references are context-bound values, never host pointers.  The heap
 * provenance rejects cross-heap values, while a per-slot generation rejects a
 * value after its slot has been reclaimed and reused.
 */
typedef struct RudaManagedReference {
    uint64_t heap_provenance;
    uint32_t slot;
    uint32_t generation;
} RudaManagedReference;

RudaManagedReference ruda_managed_invalid_reference(void);
bool ruda_managed_reference_is_invalid(RudaManagedReference reference);
bool ruda_managed_reference_equal(
    RudaManagedReference left,
    RudaManagedReference right
);

typedef enum RudaManagedCellKind {
    RUDA_MANAGED_CELL_INVALID,
    /* An exact scalar value represented by raw 64-bit payload bits. */
    RUDA_MANAGED_CELL_RAW64,
    /* A managed array reference traced by the collector. */
    RUDA_MANAGED_CELL_REFERENCE
} RudaManagedCellKind;

/*
 * Exact array type identity is the descriptor address.  Descriptors are
 * immutable and must outlive every heap object created from them.  Reference
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

typedef union RudaManagedCell {
    uint64_t f64_bits;
    RudaManagedReference reference;
} RudaManagedCell;

typedef enum RudaManagedStatus {
    RUDA_MANAGED_OK,
    RUDA_MANAGED_INVALID_ARGUMENT,
    RUDA_MANAGED_ALLOCATION_FAILED,
    RUDA_MANAGED_LIMIT_REACHED,
    /* Reserved for a length not representable by the language's i64 index. */
    RUDA_MANAGED_LENGTH_LIMIT,
    RUDA_MANAGED_INVALID_REFERENCE,
    RUDA_MANAGED_TYPE_MISMATCH,
    RUDA_MANAGED_BOUNDS
} RudaManagedStatus;

typedef struct RudaManagedHeapOptions {
    size_t max_objects;
    size_t max_live_bytes;
    /* A configured resource ceiling; reaching it reports LIMIT_REACHED. */
    size_t max_array_length;
    /* Collect before every operation that may allocate or grow storage. */
    bool collect_before_allocation;
} RudaManagedHeapOptions;

struct RudaManagedSlot;
struct RudaManagedRootFrame;

/*
 * Caller-owned exact root frame.  Each entry points to one live reference
 * location; scalar locations must not be included.  Frames must be popped in
 * strict reverse push order and their locations must outlive the linked frame.
 * A frame is zero-initialized before its first push.
 */
typedef struct RudaManagedRootFrame {
    RudaManagedReference *const *locations;
    size_t location_count;
    struct RudaManagedRootFrame *previous;
    uint64_t heap_provenance;
    bool linked;
} RudaManagedRootFrame;

/*
 * Stack-allocatable heap owner.  Slot contents are private to managed.c; users
 * must not inspect or modify these bookkeeping fields directly.
 */
typedef struct RudaManagedHeap {
    RudaAllocator allocator;
    struct RudaManagedSlot *slots;
    size_t slot_count;
    size_t slot_capacity;
    size_t live_object_count;
    size_t live_byte_count;
    uint32_t free_slot;
    uint64_t provenance;
    RudaManagedRootFrame *roots;
    RudaManagedHeapOptions options;
    bool collecting;
    bool initialized;
} RudaManagedHeap;

RudaManagedHeapOptions ruda_managed_heap_default_options(void);
bool ruda_managed_heap_init(
    RudaManagedHeap *heap,
    RudaAllocator allocator,
    RudaManagedHeapOptions options
);
/* Invalidates all references and releases every heap-owned allocation. */
void ruda_managed_heap_dispose(RudaManagedHeap *heap);

RudaManagedStatus ruda_managed_root_push(
    RudaManagedHeap *heap,
    RudaManagedRootFrame *frame,
    RudaManagedReference *const *locations,
    size_t location_count
);
RudaManagedStatus ruda_managed_root_pop(
    RudaManagedHeap *heap,
    RudaManagedRootFrame *frame
);

/*
 * Creates one array transactionally.  On failure, result is invalid and no
 * object is published.  Reference-valued initial cells are treated as
 * temporary roots if collection is attempted.
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
 * Indexing validates the signed index before any conversion or address
 * arithmetic.  current_length receives the observed length on both OK and
 * BOUNDS, allowing the caller to construct the stable G4002 diagnostic.
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
 * Push is failure-atomic.  For reference elements, value.reference is a
 * temporary root during any collection or allocation performed by this call.
 */
RudaManagedStatus ruda_managed_array_push(
    RudaManagedHeap *heap,
    RudaManagedReference array,
    RudaManagedCell value
);

/* Traces exact roots and managed array edges, then sweeps unreachable slots. */
RudaManagedStatus ruda_managed_heap_collect(RudaManagedHeap *heap);

/* Read-only deterministic inspection for limits, diagnostics, and tests. */
size_t ruda_managed_heap_live_objects(const RudaManagedHeap *heap);
size_t ruda_managed_heap_live_bytes(const RudaManagedHeap *heap);

#endif
