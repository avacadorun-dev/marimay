#include "managed_internal.h"

#include <limits.h>
#include <string.h>

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
