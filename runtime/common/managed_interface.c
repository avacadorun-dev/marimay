#include "managed_internal.h"

#include <limits.h>
#include <string.h>

MarimayManagedStatus mmay_managed_interface_new(
    MarimayManagedHeap *heap,
    const MarimayManagedInterfaceDescriptor *descriptor,
    MarimayManagedReference payload,
    const void *method_table,
    MarimayManagedReference *result
) {
    MarimayManagedInterface *value = NULL;
    MarimayManagedStatus status;
    if (result != NULL) {
        *result = mmay_managed_invalid_reference();
    }
    if (!mmay_managed_heap_valid(heap) || result == NULL
        || descriptor == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    mmay_managed_resource_failure_clear(heap);
    if (!mmay_managed_reference_valid(heap, payload)) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    value = mmay_allocate(&heap->allocator, sizeof(*value));
    if (value == NULL) {
        return mmay_managed_resource_host_failure(heap, sizeof(*value));
    }
    memset(value, 0, sizeof(*value));
    value->descriptor = descriptor;
    value->payload = payload;
    value->method_table = method_table;
    status = mmay_managed_publish_object(
        heap,
        MMAY_MANAGED_OBJECT_INTERFACE,
        value,
        sizeof(*value),
        &payload,
        1,
        NULL,
        0,
        result
    );
    if (status != MMAY_MANAGED_OK) {
        mmay_managed_interface_dispose(heap, value);
    }
    return status;
}

MarimayManagedStatus mmay_managed_interface_describe(
    const MarimayManagedHeap *heap,
    MarimayManagedReference value,
    const MarimayManagedInterfaceDescriptor **descriptor,
    MarimayManagedReference *payload,
    const void **method_table
) {
    const MarimayManagedSlot *slot = NULL;
    if (descriptor != NULL) {
        *descriptor = NULL;
    }
    if (payload != NULL) {
        *payload = mmay_managed_invalid_reference();
    }
    if (method_table != NULL) {
        *method_table = NULL;
    }
    if (!mmay_managed_heap_valid(heap) || descriptor == NULL
        || payload == NULL || method_table == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    slot = mmay_managed_resolve_const(heap, value);
    if (slot == NULL) {
        return MMAY_MANAGED_INVALID_REFERENCE;
    }
    if (slot->kind != MMAY_MANAGED_OBJECT_INTERFACE
        || slot->as.iface == NULL) {
        return MMAY_MANAGED_TYPE_MISMATCH;
    }
    *descriptor = slot->as.iface->descriptor;
    *payload = slot->as.iface->payload;
    *method_table = slot->as.iface->method_table;
    return MMAY_MANAGED_OK;
}
