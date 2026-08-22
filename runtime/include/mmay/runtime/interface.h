#ifndef MMAY_RUNTIME_INTERFACE_H
#define MMAY_RUNTIME_INTERFACE_H

#include "mmay/runtime/heap.h"

typedef struct MarimayManagedInterfaceDescriptor {
    size_t method_count;
} MarimayManagedInterfaceDescriptor;

/*
 * Interface values box one payload reference plus an immortal compiler-owned
 * method table. The table pointer is not a heap object and is never traced.
 */
MarimayManagedStatus mmay_managed_interface_new(
    MarimayManagedHeap *heap,
    const MarimayManagedInterfaceDescriptor *descriptor,
    MarimayManagedReference payload,
    const void *method_table,
    MarimayManagedReference *result
);
MarimayManagedStatus mmay_managed_interface_describe(
    const MarimayManagedHeap *heap,
    MarimayManagedReference value,
    const MarimayManagedInterfaceDescriptor **descriptor,
    MarimayManagedReference *payload,
    const void **method_table
);

#endif
