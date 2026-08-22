#ifndef RUDA_RUNTIME_GENERATED_ENUM_H
#define RUDA_RUNTIME_GENERATED_ENUM_H

#include "ruda/runtime/enum.h"

#define RUDA_MANAGED_IMMEDIATE_PROVENANCE_MASK UINT64_C(0xffffffff00000000)
#define RUDA_MANAGED_IMMEDIATE_TAG UINT32_C(1)
#define RUDA_MANAGED_IMMEDIATE_VARIANT_SHIFT 1U
#define RUDA_MANAGED_IMMEDIATE_DESCRIPTOR_SHIFT 2U
#define RUDA_MANAGED_IMMEDIATE_DESCRIPTOR_MAX UINT32_C(0x3fffffff)

/* These helpers are reserved for compiler-generated, statically typed C. */
static inline bool ruda_managed_enum_immediate_new_cached(
    RudaManagedHeap *heap,
    const RudaManagedEnumDescriptor *descriptor,
    size_t variant,
    uint64_t payload_bits,
    RudaManagedReference *result
) {
    uint32_t descriptor_id = 0;
    if (heap == NULL || !heap->initialized || result == NULL || variant > 1
        || heap->immediate_enum_last_descriptor != descriptor
        || heap->immediate_enum_last_id == 0) {
        return false;
    }
    descriptor_id = heap->immediate_enum_last_id;
    result->heap_provenance = heap->provenance
        | (uint64_t)(descriptor_id << RUDA_MANAGED_IMMEDIATE_DESCRIPTOR_SHIFT)
        | (uint64_t)((uint32_t)variant
            << RUDA_MANAGED_IMMEDIATE_VARIANT_SHIFT)
        | RUDA_MANAGED_IMMEDIATE_TAG;
    result->slot = (uint32_t)payload_bits;
    result->generation = (uint32_t)(payload_bits >> 32U);
    return true;
}

/* Descriptor IDs are preregistered before compiler-generated entry code. */
static inline void ruda_managed_enum_immediate_new_generated(
    const RudaManagedHeap *heap,
    uint32_t descriptor_id,
    size_t variant,
    uint64_t payload_bits,
    RudaManagedReference *result
) {
    result->heap_provenance = heap->provenance
        | (uint64_t)(descriptor_id << RUDA_MANAGED_IMMEDIATE_DESCRIPTOR_SHIFT)
        | (uint64_t)((uint32_t)variant
            << RUDA_MANAGED_IMMEDIATE_VARIANT_SHIFT)
        | RUDA_MANAGED_IMMEDIATE_TAG;
    result->slot = (uint32_t)payload_bits;
    result->generation = (uint32_t)(payload_bits >> 32U);
}

static inline bool ruda_managed_enum_immediate_decode_fast(
    const RudaManagedHeap *heap,
    RudaManagedReference reference,
    size_t *variant,
    uint64_t *payload_bits
) {
    uint32_t tag = (uint32_t)reference.heap_provenance;
    uint32_t descriptor_id = tag >> RUDA_MANAGED_IMMEDIATE_DESCRIPTOR_SHIFT;
    if (heap == NULL || !heap->initialized
        || (reference.heap_provenance
            & RUDA_MANAGED_IMMEDIATE_PROVENANCE_MASK) != heap->provenance
        || (tag & RUDA_MANAGED_IMMEDIATE_TAG) == 0 || descriptor_id == 0
        || descriptor_id > heap->immediate_enum_descriptor_count
        || heap->immediate_enum_descriptors == NULL) {
        return false;
    }
    if (variant != NULL) {
        *variant = (tag >> RUDA_MANAGED_IMMEDIATE_VARIANT_SHIFT) & UINT32_C(1);
    }
    if (payload_bits != NULL) {
        *payload_bits = (uint64_t)reference.slot
            | ((uint64_t)reference.generation << 32U);
    }
    return true;
}

#endif
