#ifndef RUDA_RUNTIME_INTERFACE_H
#define RUDA_RUNTIME_INTERFACE_H

#include "ruda/runtime/heap.h"

typedef struct RudaManagedInterfaceDescriptor {
    size_t method_count;
} RudaManagedInterfaceDescriptor;

/*
 * Interface values box one payload reference plus an immortal compiler-owned
 * method table. The table pointer is not a heap object and is never traced.
 */
RudaManagedStatus ruda_managed_interface_new(
    RudaManagedHeap *heap,
    const RudaManagedInterfaceDescriptor *descriptor,
    RudaManagedReference payload,
    const void *method_table,
    RudaManagedReference *result
);
RudaManagedStatus ruda_managed_interface_describe(
    const RudaManagedHeap *heap,
    RudaManagedReference value,
    const RudaManagedInterfaceDescriptor **descriptor,
    RudaManagedReference *payload,
    const void **method_table
);

#endif
