#ifndef MMAY_RUNTIME_ENUM_H
#define MMAY_RUNTIME_ENUM_H

#include "mmay/runtime/heap.h"

typedef struct MarimayManagedEnumVariantDescriptor {
    size_t payload_count;
    const MarimayManagedFieldDescriptor *payloads;
} MarimayManagedEnumVariantDescriptor;

typedef struct MarimayManagedEnumDescriptor {
    size_t variant_count;
    const MarimayManagedEnumVariantDescriptor *variants;
} MarimayManagedEnumDescriptor;

/* Compiler runtime integration; source programs do not call this API. */
MarimayManagedStatus mmay_managed_enum_register_immediate_descriptor(
    MarimayManagedHeap *heap,
    const MarimayManagedEnumDescriptor *descriptor,
    uint32_t *descriptor_id
);

MarimayManagedStatus mmay_managed_enum_new(
    MarimayManagedHeap *heap,
    const MarimayManagedEnumDescriptor *descriptor,
    size_t variant,
    const MarimayManagedCell *payload,
    size_t payload_count,
    MarimayManagedReference *result
);
MarimayManagedStatus mmay_managed_enum_describe(
    const MarimayManagedHeap *heap,
    MarimayManagedReference value,
    const MarimayManagedEnumDescriptor **descriptor,
    size_t *variant
);
MarimayManagedStatus mmay_managed_enum_is_variant(
    const MarimayManagedHeap *heap,
    MarimayManagedReference value,
    size_t variant,
    bool *result
);
MarimayManagedStatus mmay_managed_enum_get_payload(
    const MarimayManagedHeap *heap,
    MarimayManagedReference value,
    size_t variant,
    size_t field,
    MarimayManagedCell *result
);

#endif
