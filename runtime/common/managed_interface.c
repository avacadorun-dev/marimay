#include "managed_internal.h"

#include <limits.h>
#include <string.h>

RudaManagedStatus ruda_managed_interface_new(
    RudaManagedHeap *heap,
    const RudaManagedInterfaceDescriptor *descriptor,
    RudaManagedReference payload,
    const void *method_table,
    RudaManagedReference *result
) {
    RudaManagedInterface *value = NULL;
    RudaManagedStatus status;
    if (result != NULL) {
        *result = ruda_managed_invalid_reference();
    }
    if (!ruda_managed_heap_valid(heap) || result == NULL
        || descriptor == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    if (ruda_managed_resolve_const(heap, payload) == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    value = ruda_allocate(&heap->allocator, sizeof(*value));
    if (value == NULL) {
        return RUDA_MANAGED_ALLOCATION_FAILED;
    }
    memset(value, 0, sizeof(*value));
    value->descriptor = descriptor;
    value->payload = payload;
    value->method_table = method_table;
    status = ruda_managed_publish_object(
        heap,
        RUDA_MANAGED_OBJECT_INTERFACE,
        value,
        sizeof(*value),
        &payload,
        1,
        NULL,
        0,
        result
    );
    if (status != RUDA_MANAGED_OK) {
        ruda_managed_interface_dispose(heap, value);
    }
    return status;
}

RudaManagedStatus ruda_managed_interface_describe(
    const RudaManagedHeap *heap,
    RudaManagedReference value,
    const RudaManagedInterfaceDescriptor **descriptor,
    RudaManagedReference *payload,
    const void **method_table
) {
    const RudaManagedSlot *slot = NULL;
    if (descriptor != NULL) {
        *descriptor = NULL;
    }
    if (payload != NULL) {
        *payload = ruda_managed_invalid_reference();
    }
    if (method_table != NULL) {
        *method_table = NULL;
    }
    if (!ruda_managed_heap_valid(heap) || descriptor == NULL
        || payload == NULL || method_table == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    slot = ruda_managed_resolve_const(heap, value);
    if (slot == NULL) {
        return RUDA_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != RUDA_MANAGED_OBJECT_INTERFACE
        || slot->as.iface == NULL) {
        return RUDA_MANAGED_TYPE_MISMATCH;
    }
    *descriptor = slot->as.iface->descriptor;
    *payload = slot->as.iface->payload;
    *method_table = slot->as.iface->method_table;
    return RUDA_MANAGED_OK;
}
