#include "managed_internal.h"

#include <limits.h>
#include <string.h>

static bool ruda_managed_enum_variant_is_immediate(
    const RudaManagedEnumDescriptor *descriptor,
    size_t variant
) {
    const RudaManagedEnumVariantDescriptor *selected = NULL;
    if (descriptor == NULL || descriptor->variant_count != 2
        || descriptor->variants == NULL || variant >= 2) {
        return false;
    }
    selected = &descriptor->variants[variant];
    if (selected->payload_count == 0) {
        return selected->payloads == NULL;
    }
    return selected->payload_count == 1 && selected->payloads != NULL
        && selected->payloads[0].cell_kind == RUDA_MANAGED_CELL_RAW64
        && !selected->payloads[0].is_mutable
        && selected->payloads[0].reference_kind == RUDA_MANAGED_OBJECT_INVALID
        && selected->payloads[0].reference_descriptor == NULL;
}

static RudaManagedStatus ruda_managed_immediate_enum_descriptor_id(
    RudaManagedHeap *heap,
    const RudaManagedEnumDescriptor *descriptor,
    uint32_t *result
) {
    size_t index = 0;
    size_t old_bytes = 0;
    size_t new_bytes = 0;
    size_t capacity = 0;
    const RudaManagedEnumDescriptor **resized = NULL;
    if (heap->immediate_enum_last_descriptor == descriptor
        && heap->immediate_enum_last_id != 0) {
        *result = heap->immediate_enum_last_id;
        return RUDA_MANAGED_OK;
    }
    for (index = 0; index < heap->immediate_enum_descriptor_count;
        index += 1) {
        if (heap->immediate_enum_descriptors[index] == descriptor) {
            *result = (uint32_t)index + UINT32_C(1);
            heap->immediate_enum_last_descriptor = descriptor;
            heap->immediate_enum_last_id = *result;
            return RUDA_MANAGED_OK;
        }
    }
    if (heap->immediate_enum_descriptor_count
        >= RUDA_MANAGED_IMMEDIATE_DESCRIPTOR_MAX) {
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    if (heap->immediate_enum_descriptor_count
        == heap->immediate_enum_descriptor_capacity) {
        capacity = heap->immediate_enum_descriptor_capacity == 0
            ? 8 : heap->immediate_enum_descriptor_capacity * 2;
        if (capacity < heap->immediate_enum_descriptor_capacity
            || capacity > RUDA_MANAGED_IMMEDIATE_DESCRIPTOR_MAX
            || !ruda_size_multiply(
                heap->immediate_enum_descriptor_capacity,
                sizeof(*heap->immediate_enum_descriptors),
                &old_bytes
            ) || !ruda_size_multiply(
                capacity,
                sizeof(*heap->immediate_enum_descriptors),
                &new_bytes
            )) {
            return RUDA_MANAGED_LIMIT_REACHED;
        }
        resized = ruda_resize(
            &heap->allocator,
            heap->immediate_enum_descriptors,
            old_bytes,
            new_bytes
        );
        if (resized == NULL) {
            return ruda_managed_resource_host_failure(heap, new_bytes);
        }
        heap->immediate_enum_descriptors = resized;
        heap->immediate_enum_descriptor_capacity = capacity;
    }
    index = heap->immediate_enum_descriptor_count;
    heap->immediate_enum_descriptors[index] = descriptor;
    heap->immediate_enum_descriptor_count += 1;
    *result = (uint32_t)index + UINT32_C(1);
    heap->immediate_enum_last_descriptor = descriptor;
    heap->immediate_enum_last_id = *result;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_enum_register_immediate_descriptor(
    RudaManagedHeap *heap,
    const RudaManagedEnumDescriptor *descriptor,
    uint32_t *descriptor_id
) {
    size_t variant = 0;
    bool eligible = false;
    if (!ruda_managed_heap_valid(heap) || descriptor == NULL
        || descriptor_id == NULL || descriptor->variant_count != 2
        || descriptor->variants == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    for (variant = 0; variant < descriptor->variant_count; variant += 1) {
        eligible = eligible
            || ruda_managed_enum_variant_is_immediate(descriptor, variant);
    }
    return eligible
        ? ruda_managed_immediate_enum_descriptor_id(
            heap, descriptor, descriptor_id
        )
        : RUDA_MANAGED_INVALID_ARGUMENT;
}

static RudaManagedStatus ruda_managed_immediate_enum_new(
    RudaManagedHeap *heap,
    const RudaManagedEnumDescriptor *descriptor,
    size_t variant,
    const RudaManagedCell *payload,
    size_t payload_count,
    RudaManagedReference *result
) {
    uint32_t descriptor_id = 0;
    uint64_t payload_bits = payload_count == 0 ? 0 : payload[0].f64_bits;
    RudaManagedStatus status = ruda_managed_immediate_enum_descriptor_id(
        heap, descriptor, &descriptor_id
    );
    if (status != RUDA_MANAGED_OK) {
        return status;
    }
    result->heap_provenance = heap->provenance
        | (uint64_t)(descriptor_id << RUDA_MANAGED_IMMEDIATE_DESCRIPTOR_SHIFT)
        | (uint64_t)((uint32_t)variant
            << RUDA_MANAGED_IMMEDIATE_VARIANT_SHIFT)
        | RUDA_MANAGED_IMMEDIATE_TAG;
    result->slot = (uint32_t)payload_bits;
    result->generation = (uint32_t)(payload_bits >> 32U);
    return RUDA_MANAGED_OK;
}

bool ruda_managed_immediate_enum_decode(
    const RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedEnumDescriptor **descriptor,
    size_t *variant,
    uint64_t *payload_bits
) {
    uint32_t tag = (uint32_t)reference.heap_provenance;
    uint32_t descriptor_id = tag >> RUDA_MANAGED_IMMEDIATE_DESCRIPTOR_SHIFT;
    if (!ruda_managed_heap_valid(heap)
        || (reference.heap_provenance
            & RUDA_MANAGED_IMMEDIATE_PROVENANCE_MASK) != heap->provenance
        || (tag & RUDA_MANAGED_IMMEDIATE_TAG) == 0
        || descriptor_id == 0
        || descriptor_id > heap->immediate_enum_descriptor_count
        || heap->immediate_enum_descriptors == NULL
        || heap->immediate_enum_descriptors[descriptor_id - 1] == NULL) {
        return false;
    }
    if (descriptor != NULL) {
        *descriptor = heap->immediate_enum_descriptors[descriptor_id - 1];
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

bool ruda_managed_reference_valid(
    const RudaManagedHeap *heap,
    RudaManagedReference reference
) {
    return ruda_managed_resolve_const(heap, reference) != NULL
        || ruda_managed_immediate_enum_decode(
            heap, reference, NULL, NULL, NULL
        );
}

RudaManagedStatus ruda_managed_enum_new(
    RudaManagedHeap *heap,
    const RudaManagedEnumDescriptor *descriptor,
    size_t variant,
    const RudaManagedCell *payload,
    size_t payload_count,
    RudaManagedReference *result
) {
    const RudaManagedEnumVariantDescriptor *selected = NULL;
    RudaManagedEnum *value = NULL;
    RudaManagedStatus status;
    size_t payload_bytes = 0;
    size_t object_bytes = 0;
    size_t index = 0;
    if (result != NULL) {
        *result = ruda_managed_invalid_reference();
    }
    if (!ruda_managed_heap_valid(heap) || result == NULL || descriptor == NULL
        || descriptor->variants == NULL
        || variant >= descriptor->variant_count) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    ruda_managed_resource_failure_clear(heap);
    selected = &descriptor->variants[variant];
    if (selected->payload_count != payload_count
        || (selected->payloads == NULL && payload_count != 0)
        || (payload == NULL && payload_count != 0)) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    for (index = 0; index < payload_count; index += 1) {
        status = ruda_managed_validate_field(
            heap,
            &selected->payloads[index],
            payload[index]
        );
        if (status != RUDA_MANAGED_OK) {
            return status;
        }
    }
    if (ruda_managed_enum_variant_is_immediate(descriptor, variant)) {
        return ruda_managed_immediate_enum_new(
            heap, descriptor, variant, payload, payload_count, result
        );
    }
    if (!ruda_size_multiply(
            payload_count,
            sizeof(RudaManagedCell),
            &payload_bytes
        ) || !ruda_size_add(sizeof(*value), payload_bytes, &object_bytes)) {
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    value = ruda_allocate(&heap->allocator, sizeof(*value));
    if (value == NULL) {
        return ruda_managed_resource_host_failure(heap, sizeof(*value));
    }
    memset(value, 0, sizeof(*value));
    if (payload_bytes != 0) {
        value->payload = ruda_allocate(&heap->allocator, payload_bytes);
        if (value->payload == NULL) {
            ruda_deallocate(&heap->allocator, value, sizeof(*value));
            return ruda_managed_resource_host_failure(heap, payload_bytes);
        }
        memcpy(value->payload, payload, payload_bytes);
    }
    value->descriptor = descriptor;
    value->variant = variant;
    {
        size_t root_count = ruda_managed_count_reference_fields(
            selected->payloads,
            payload_count
        );
        size_t root_bytes = 0;
        RudaManagedReference *roots = NULL;
        if (root_count != 0
            && ruda_size_multiply(
                root_count,
                sizeof(RudaManagedReference),
                &root_bytes
            )) {
            roots = ruda_allocate(&heap->allocator, root_bytes);
        }
        if (root_count != 0 && (roots == NULL
                || !ruda_managed_collect_reference_roots(
                    selected->payloads,
                    payload,
                    payload_count,
                    roots,
                    root_count,
                    &root_count
                ))) {
            ruda_deallocate(&heap->allocator, roots, root_bytes);
            ruda_managed_enum_dispose(heap, value);
            return ruda_managed_resource_host_failure(heap, root_bytes);
        }
        status = ruda_managed_publish_object(
            heap,
            RUDA_MANAGED_OBJECT_ENUM,
            value,
            object_bytes,
            roots,
            root_count,
            NULL,
            0,
            result
        );
        ruda_deallocate(&heap->allocator, roots, root_bytes);
    }
    if (status != RUDA_MANAGED_OK) {
        ruda_managed_enum_dispose(heap, value);
    }
    return status;
}
RudaManagedStatus ruda_managed_enum_describe(
    const RudaManagedHeap *heap,
    RudaManagedReference value,
    const RudaManagedEnumDescriptor **descriptor,
    size_t *variant
) {
    const RudaManagedSlot *slot = NULL;
    if (descriptor != NULL) {
        *descriptor = NULL;
    }
    if (variant != NULL) {
        *variant = 0;
    }
    if (!ruda_managed_heap_valid(heap) || descriptor == NULL
        || variant == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    if (ruda_managed_immediate_enum_decode(
            heap, value, descriptor, variant, NULL
        )) {
        return RUDA_MANAGED_OK;
    }
    slot = ruda_managed_resolve_const(heap, value);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_ENUM
        || slot->as.enumeration == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    *descriptor = slot->as.enumeration->descriptor;
    *variant = slot->as.enumeration->variant;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_enum_is_variant(
    const RudaManagedHeap *heap,
    RudaManagedReference value,
    size_t variant,
    bool *result
) {
    const RudaManagedSlot *slot = NULL;
    if (result != NULL) {
        *result = false;
    }
    if (!ruda_managed_heap_valid(heap) || result == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    {
        size_t actual_variant = 0;
        if (ruda_managed_immediate_enum_decode(
                heap, value, NULL, &actual_variant, NULL
            )) {
            *result = actual_variant == variant;
            return RUDA_MANAGED_OK;
        }
    }
    slot = ruda_managed_resolve_const(heap, value);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_ENUM
        || slot->as.enumeration == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    *result = slot->as.enumeration->variant == variant;
    return RUDA_MANAGED_OK;
}

RudaManagedStatus ruda_managed_enum_get_payload(
    const RudaManagedHeap *heap,
    RudaManagedReference value,
    size_t variant,
    size_t field,
    RudaManagedCell *result
) {
    const RudaManagedSlot *slot = NULL;
    const RudaManagedEnumVariantDescriptor *selected = NULL;
    if (result != NULL) {
        memset(result, 0, sizeof(*result));
    }
    if (!ruda_managed_heap_valid(heap) || result == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    {
        const RudaManagedEnumDescriptor *descriptor = NULL;
        size_t actual_variant = 0;
        uint64_t payload_bits = 0;
        if (ruda_managed_immediate_enum_decode(
                heap, value, &descriptor, &actual_variant, &payload_bits
            )) {
            if (actual_variant != variant
                || variant >= descriptor->variant_count) {
                return RUDA_MANAGED_TYPE_MISMATCH;
            }
            selected = &descriptor->variants[variant];
            if (field >= selected->payload_count) {
                return RUDA_MANAGED_BOUNDS;
            }
            result->f64_bits = payload_bits;
            return RUDA_MANAGED_OK;
        }
    }
    slot = ruda_managed_resolve_const(heap, value);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_ENUM
        || slot->as.enumeration == NULL
        || slot->as.enumeration->descriptor == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    if (slot->as.enumeration->variant != variant
        || variant >= slot->as.enumeration->descriptor->variant_count) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    selected = &slot->as.enumeration->descriptor->variants[variant];
    if (field >= selected->payload_count) {
        return RUDA_MANAGED_BOUNDS;
    }
    *result = slot->as.enumeration->payload[field];
    return RUDA_MANAGED_OK;
}
