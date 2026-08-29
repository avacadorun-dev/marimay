#ifndef MMAY_RUNTIME_CORE_H
#define MMAY_RUNTIME_CORE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Managed references are context-bound values, never host pointers. The heap
 * provenance rejects cross-heap values, while a per-slot generation rejects a
 * value after its slot has been reclaimed and reused.
 */
typedef struct MarimayManagedReference {
    uint64_t heap_provenance;
    uint32_t slot;
    uint32_t generation;
} MarimayManagedReference;

MarimayManagedReference mmay_managed_invalid_reference(void);
bool mmay_managed_reference_is_invalid(MarimayManagedReference reference);
bool mmay_managed_reference_equal(
    MarimayManagedReference left,
    MarimayManagedReference right
);

typedef enum MarimayManagedCellKind {
    MMAY_MANAGED_CELL_INVALID,
    /* An exact scalar value represented by raw 64-bit payload bits. */
    MMAY_MANAGED_CELL_RAW64,
    /* A managed object reference traced by the collector. */
    MMAY_MANAGED_CELL_REFERENCE
} MarimayManagedCellKind;

typedef enum MarimayManagedObjectKind {
    MMAY_MANAGED_OBJECT_INVALID,
    MMAY_MANAGED_OBJECT_ARRAY,
    MMAY_MANAGED_OBJECT_STRING,
    MMAY_MANAGED_OBJECT_RECORD,
    MMAY_MANAGED_OBJECT_ENUM,
    MMAY_MANAGED_OBJECT_INTERFACE,
    MMAY_MANAGED_OBJECT_MAP
} MarimayManagedObjectKind;

/*
 * Shared record/enum field layout. Nominal type identity is the descriptor
 * address, matching arrays. REFERENCE fields name the exact child object kind
 * and descriptor; strings use kind STRING and a null descriptor.
 */
typedef struct MarimayManagedFieldDescriptor {
    MarimayManagedCellKind cell_kind;
    bool is_mutable;
    MarimayManagedObjectKind reference_kind;
    const void *reference_descriptor;
} MarimayManagedFieldDescriptor;

typedef union MarimayManagedCell {
    uint64_t f64_bits;
    MarimayManagedReference reference;
} MarimayManagedCell;

typedef enum MarimayManagedStatus {
    MMAY_MANAGED_OK,
    MMAY_MANAGED_INVALID_ARGUMENT,
    MMAY_MANAGED_ALLOCATION_FAILED,
    MMAY_MANAGED_LIMIT_REACHED,
    /* Reserved for a length not representable by the language's i64 index. */
    MMAY_MANAGED_LENGTH_LIMIT,
    MMAY_MANAGED_INVALID_REFERENCE,
    MMAY_MANAGED_TYPE_MISMATCH,
    MMAY_MANAGED_BOUNDS,
    MMAY_MANAGED_TRUNCATE_INVALID_LENGTH,
    MMAY_MANAGED_NEGATIVE_ADDITIONAL,
    MMAY_MANAGED_ITERATOR_INVALIDATED
} MarimayManagedStatus;

#endif
