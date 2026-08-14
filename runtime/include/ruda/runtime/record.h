#ifndef RUDA_RUNTIME_RECORD_H
#define RUDA_RUNTIME_RECORD_H

#include "ruda/runtime/heap.h"

typedef struct RudaManagedRecordDescriptor {
    size_t field_count;
    const RudaManagedFieldDescriptor *fields;
} RudaManagedRecordDescriptor;

RudaManagedStatus ruda_managed_record_new(
    RudaManagedHeap *heap,
    const RudaManagedRecordDescriptor *descriptor,
    const RudaManagedCell *fields,
    size_t field_count,
    RudaManagedReference *result
);
RudaManagedStatus ruda_managed_record_describe(
    const RudaManagedHeap *heap,
    RudaManagedReference record,
    const RudaManagedRecordDescriptor **descriptor
);
RudaManagedStatus ruda_managed_record_get(
    const RudaManagedHeap *heap,
    RudaManagedReference record,
    size_t field,
    RudaManagedCell *result
);
RudaManagedStatus ruda_managed_record_set(
    RudaManagedHeap *heap,
    RudaManagedReference record,
    size_t field,
    RudaManagedCell value
);

#endif
