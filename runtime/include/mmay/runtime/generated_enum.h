#ifndef MMAY_RUNTIME_GENERATED_ENUM_H
#define MMAY_RUNTIME_GENERATED_ENUM_H

#include "mmay/runtime/enum.h"

#define MMAY_MANAGED_IMMEDIATE_PROVENANCE_MASK UINT64_C(0xffffffff00000000)
#define MMAY_MANAGED_IMMEDIATE_TAG UINT32_C(1)
#define MMAY_MANAGED_IMMEDIATE_VARIANT_SHIFT 1U
#define MMAY_MANAGED_IMMEDIATE_DESCRIPTOR_SHIFT 2U
#define MMAY_MANAGED_IMMEDIATE_DESCRIPTOR_MAX UINT32_C(0x3fffffff)

/* These helpers are reserved for compiler-generated, statically typed C. */
static inline bool mmay_managed_enum_immediate_new_cached(
    MarimayManagedHeap *heap,
    const MarimayManagedEnumDescriptor *descriptor,
    size_t variant,
    uint64_t payload_bits,
    MarimayManagedReference *result
) {
    uint32_t descriptor_id = 0;
    if (heap == NULL || !heap->initialized || result == NULL || variant > 1
        || heap->immediate_enum_last_descriptor != descriptor
        || heap->immediate_enum_last_id == 0) {
        return false;
    }
    descriptor_id = heap->immediate_enum_last_id;
    result->heap_provenance = heap->provenance
        | (uint64_t)(descriptor_id << MMAY_MANAGED_IMMEDIATE_DESCRIPTOR_SHIFT)
        | (uint64_t)((uint32_t)variant
            << MMAY_MANAGED_IMMEDIATE_VARIANT_SHIFT)
        | MMAY_MANAGED_IMMEDIATE_TAG;
    result->slot = (uint32_t)payload_bits;
    result->generation = (uint32_t)(payload_bits >> 32U);
    return true;
}

/* Descriptor IDs are preregistered before compiler-generated entry code. */
static inline void mmay_managed_enum_immediate_new_generated(
    const MarimayManagedHeap *heap,
    uint32_t descriptor_id,
    size_t variant,
    uint64_t payload_bits,
    MarimayManagedReference *result
) {
    result->heap_provenance = heap->provenance
        | (uint64_t)(descriptor_id << MMAY_MANAGED_IMMEDIATE_DESCRIPTOR_SHIFT)
        | (uint64_t)((uint32_t)variant
            << MMAY_MANAGED_IMMEDIATE_VARIANT_SHIFT)
        | MMAY_MANAGED_IMMEDIATE_TAG;
    result->slot = (uint32_t)payload_bits;
    result->generation = (uint32_t)(payload_bits >> 32U);
}

static inline bool mmay_managed_enum_immediate_decode_fast(
    const MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    size_t *variant,
    uint64_t *payload_bits
) {
    uint32_t tag = (uint32_t)reference.heap_provenance;
    uint32_t descriptor_id = tag >> MMAY_MANAGED_IMMEDIATE_DESCRIPTOR_SHIFT;
    if (heap == NULL || !heap->initialized
        || (reference.heap_provenance
            & MMAY_MANAGED_IMMEDIATE_PROVENANCE_MASK) != heap->provenance
        || (tag & MMAY_MANAGED_IMMEDIATE_TAG) == 0 || descriptor_id == 0
        || descriptor_id > heap->immediate_enum_descriptor_count
        || heap->immediate_enum_descriptors == NULL) {
        return false;
    }
    if (variant != NULL) {
        *variant = (tag >> MMAY_MANAGED_IMMEDIATE_VARIANT_SHIFT) & UINT32_C(1);
    }
    if (payload_bits != NULL) {
        *payload_bits = (uint64_t)reference.slot
            | ((uint64_t)reference.generation << 32U);
    }
    return true;
}

#endif
