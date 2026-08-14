#ifndef RUDA_RUNTIME_ENUM_H
#define RUDA_RUNTIME_ENUM_H

#include "ruda/runtime/heap.h"

typedef struct RudaManagedEnumVariantDescriptor {
    size_t payload_count;
    const RudaManagedFieldDescriptor *payloads;
} RudaManagedEnumVariantDescriptor;

typedef struct RudaManagedEnumDescriptor {
    size_t variant_count;
    const RudaManagedEnumVariantDescriptor *variants;
} RudaManagedEnumDescriptor;

RudaManagedStatus ruda_managed_enum_new(
    RudaManagedHeap *heap,
    const RudaManagedEnumDescriptor *descriptor,
    size_t variant,
    const RudaManagedCell *payload,
    size_t payload_count,
    RudaManagedReference *result
);
RudaManagedStatus ruda_managed_enum_describe(
    const RudaManagedHeap *heap,
    RudaManagedReference value,
    const RudaManagedEnumDescriptor **descriptor,
    size_t *variant
);
RudaManagedStatus ruda_managed_enum_is_variant(
    const RudaManagedHeap *heap,
    RudaManagedReference value,
    size_t variant,
    bool *result
);
RudaManagedStatus ruda_managed_enum_get_payload(
    const RudaManagedHeap *heap,
    RudaManagedReference value,
    size_t variant,
    size_t field,
    RudaManagedCell *result
);

#endif
