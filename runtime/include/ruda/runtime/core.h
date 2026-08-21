#ifndef RUDA_RUNTIME_CORE_H
#define RUDA_RUNTIME_CORE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Managed references are context-bound values, never host pointers. The heap
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
    /* A managed object reference traced by the collector. */
    RUDA_MANAGED_CELL_REFERENCE
} RudaManagedCellKind;

typedef enum RudaManagedObjectKind {
    RUDA_MANAGED_OBJECT_INVALID,
    RUDA_MANAGED_OBJECT_ARRAY,
    RUDA_MANAGED_OBJECT_STRING,
    RUDA_MANAGED_OBJECT_RECORD,
    RUDA_MANAGED_OBJECT_ENUM,
    RUDA_MANAGED_OBJECT_INTERFACE
} RudaManagedObjectKind;

/*
 * Shared record/enum field layout. Nominal type identity is the descriptor
 * address, matching arrays. REFERENCE fields name the exact child object kind
 * and descriptor; strings use kind STRING and a null descriptor.
 */
typedef struct RudaManagedFieldDescriptor {
    RudaManagedCellKind cell_kind;
    bool is_mutable;
    RudaManagedObjectKind reference_kind;
    const void *reference_descriptor;
} RudaManagedFieldDescriptor;

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
    RUDA_MANAGED_BOUNDS,
    RUDA_MANAGED_TRUNCATE_INVALID_LENGTH,
    RUDA_MANAGED_NEGATIVE_ADDITIONAL,
    RUDA_MANAGED_ITERATOR_INVALIDATED
} RudaManagedStatus;

#endif
