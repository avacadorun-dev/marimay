#include "managed_internal.h"

#include <limits.h>
#include <string.h>

RudaManagedStatus ruda_managed_string_new(
    RudaManagedHeap *heap,
    const unsigned char *bytes,
    size_t length,
    RudaManagedReference *result
) {
    RudaManagedString *value = NULL;
    RudaManagedStatus status;
    size_t object_bytes = 0;
    if (result != NULL) {
        *result = ruda_managed_invalid_reference();
    }
    if (!ruda_managed_heap_valid(heap) || result == NULL
        || (bytes == NULL && length != 0)
        || !ruda_size_add(sizeof(*value), length, &object_bytes)) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    ruda_managed_resource_failure_clear(heap);
    value = ruda_allocate(&heap->allocator, sizeof(*value));
    if (value == NULL) {
        return ruda_managed_resource_host_failure(heap, sizeof(*value));
    }
    memset(value, 0, sizeof(*value));
    if (length != 0) {
        value->bytes = ruda_allocate(&heap->allocator, length);
        if (value->bytes == NULL) {
            ruda_deallocate(&heap->allocator, value, sizeof(*value));
            return ruda_managed_resource_host_failure(heap, length);
        }
        memcpy(value->bytes, bytes, length);
        value->length = length;
    }
    status = ruda_managed_publish_object(
        heap,
        RUDA_MANAGED_OBJECT_STRING,
        value,
        object_bytes,
        NULL,
        0,
        NULL,
        0,
        result
    );
    if (status != RUDA_MANAGED_OK) {
        ruda_managed_string_dispose(heap, value);
    }
    return status;
}
RudaManagedStatus ruda_managed_string_bytes(
    const RudaManagedHeap *heap,
    RudaManagedReference value,
    const unsigned char **bytes,
    size_t *length
) {
    const RudaManagedSlot *slot = NULL;
    if (bytes != NULL) {
        *bytes = NULL;
    }
    if (length != NULL) {
        *length = 0;
    }
    if (!ruda_managed_heap_valid(heap) || bytes == NULL || length == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve_const(heap, value);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_STRING || slot->as.string == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    *bytes = slot->as.string->bytes;
    *length = slot->as.string->length;
    return RUDA_MANAGED_OK;
}
