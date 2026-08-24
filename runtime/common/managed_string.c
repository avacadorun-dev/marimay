#include "managed_internal.h"

#include <limits.h>
#include <string.h>

MarimayManagedStatus mmay_managed_string_new(
    MarimayManagedHeap *heap,
    const unsigned char *bytes,
    size_t length,
    MarimayManagedReference *result
) {
    MarimayManagedString *value = NULL;
    MarimayManagedStatus status;
    size_t object_bytes = 0;
    if (result != NULL) {
        *result = mmay_managed_invalid_reference();
    }
    if (!mmay_managed_heap_valid(heap) || result == NULL
        || (bytes == NULL && length != 0)
        || !mmay_size_add(sizeof(*value), length, &object_bytes)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    mmay_managed_resource_failure_clear(heap);
    value = mmay_allocate(&heap->allocator, sizeof(*value));
    if (value == NULL) {
        return mmay_managed_resource_host_failure(heap, sizeof(*value));
    }
    memset(value, 0, sizeof(*value));
    if (length != 0) {
        value->bytes = mmay_allocate(&heap->allocator, length);
        if (value->bytes == NULL) {
            mmay_deallocate(&heap->allocator, value, sizeof(*value));
            return mmay_managed_resource_host_failure(heap, length);
        }
        memcpy(value->bytes, bytes, length);
        value->length = length;
    }
    status = mmay_managed_publish_object(
        heap,
        MMAY_MANAGED_OBJECT_STRING,
        value,
        object_bytes,
        NULL,
        0,
        NULL,
        0,
        result
    );
    if (status != MMAY_MANAGED_OK) {
        mmay_managed_string_dispose(heap, value);
    }
    return status;
}
MarimayManagedStatus mmay_managed_string_bytes(
    const MarimayManagedHeap *heap,
    MarimayManagedReference value,
    const unsigned char **bytes,
    size_t *length
) {
    const MarimayManagedSlot *slot = NULL;
    if (bytes != NULL) {
        *bytes = NULL;
    }
    if (length != NULL) {
        *length = 0;
    }
    if (!mmay_managed_heap_valid(heap) || bytes == NULL || length == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve_const(heap, value);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_STRING || slot->as.string == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *bytes = slot->as.string->bytes;
    *length = slot->as.string->length;
    return MMAY_MANAGED_OK;
}

MarimayManagedStatus mmay_managed_string_concat(
    MarimayManagedHeap *heap,
    const unsigned char *left,
    size_t left_length,
    const unsigned char *right,
    size_t right_length,
    MarimayManagedReference *result
) {
    size_t length = 0;
    unsigned char *joined = NULL;
    MarimayManagedStatus status;
    if (result != NULL) {
        *result = mmay_managed_invalid_reference();
    }
    if (!mmay_managed_heap_valid(heap) || result == NULL
        || (left == NULL && left_length != 0)
        || (right == NULL && right_length != 0)
        || !mmay_size_add(left_length, right_length, &length)) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (length != 0) {
        joined = mmay_allocate(&heap->allocator, length);
        if (joined == NULL) {
            return mmay_managed_resource_host_failure(heap, length);
        }
        if (left_length != 0) {
            memcpy(joined, left, left_length);
        }
        if (right_length != 0) {
            memcpy(joined + left_length, right, right_length);
        }
    }
    status = mmay_managed_string_new(heap, joined, length, result);
    if (joined != NULL) {
        mmay_deallocate(&heap->allocator, joined, length);
    }
    return status;
}
