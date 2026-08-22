#ifndef RUDA_RUNTIME_RECORD_H
#define RUDA_RUNTIME_RECORD_H

#include "ruda/runtime/heap.h"

typedef struct RudaManagedRecordDescriptor {
    size_t field_count;
    const RudaManagedFieldDescriptor *fields;
} RudaManagedRecordDescriptor;

/* Non-escaping exact record view; see D063's rooting/safepoint lifetime. */
typedef struct RudaManagedRecordView {
    RudaManagedReference reference;
    const RudaManagedRecordDescriptor *descriptor;
    void *object;
    RudaManagedCell *fields;
} RudaManagedRecordView;

RudaManagedStatus ruda_managed_record_view_bind(
    RudaManagedHeap *heap,
    RudaManagedReference record,
    const RudaManagedRecordDescriptor *expected,
    RudaManagedRecordView *view
);

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
