#include "managed_internal.h"
#include "managed_publication.h"

#include <limits.h>
#include <string.h>

/* A single allocation retains the unchanged logical header + payload charge.
 * Collection tracing precedes allocation, while physical sweep waits until
 * borrowed bytes (including an aliased output's old value) have been copied. */
static MarimayManagedStatus mmay_managed_string_join(
    MarimayManagedHeap *heap,
    const unsigned char *left, size_t left_length,
    const unsigned char *right, size_t right_length,
    MarimayManagedReference *result
) {
    size_t length = 0, object_bytes = 0;
    if (result != NULL) *result = mmay_managed_invalid_reference();
    if (!mmay_managed_heap_valid(heap) || heap->collecting || result == NULL
        || (left == NULL && left_length != 0)
        || (right == NULL && right_length != 0)
        || !mmay_size_add(left_length, right_length, &length)
        || !mmay_size_add(sizeof(MarimayManagedString), length, &object_bytes))
        return MMAY_MANAGED_INVALID_ARGUMENT;
    mmay_managed_resource_failure_clear(heap);
    MarimayManagedPublication publication = {0};
    MarimayManagedStatus status = mmay_managed_publication_prepare(heap,
        object_bytes, NULL, 0, NULL, 0, true, &publication);
    if (status != MMAY_MANAGED_OK) return status;
    MarimayManagedString *value = mmay_allocate(&heap->allocator, object_bytes);
    if (value == NULL) {
        mmay_managed_publication_cancel(&publication);
        return mmay_managed_resource_host_failure(heap, object_bytes);
    }
    value->length = length;
    value->bytes = length == 0 ? NULL : (unsigned char *)value + sizeof(*value);
    if (left_length != 0) memcpy(value->bytes, left, left_length);
    if (right_length != 0) memcpy(value->bytes + left_length, right, right_length);
    status = mmay_managed_publication_commit(&publication,
        MMAY_MANAGED_OBJECT_STRING, value, result);
    if (status != MMAY_MANAGED_OK) mmay_managed_string_dispose(heap, value);
    return status;
}

MarimayManagedStatus mmay_managed_string_new(
    MarimayManagedHeap *heap, const unsigned char *bytes, size_t length,
    MarimayManagedReference *result
) {
    return mmay_managed_string_join(heap, bytes, length, NULL, 0, result);
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
    const unsigned char *left, size_t left_length,
    const unsigned char *right, size_t right_length,
    MarimayManagedReference *result
) {
    return mmay_managed_string_join(heap, left, left_length,
        right, right_length, result);
}
