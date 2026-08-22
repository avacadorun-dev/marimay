#ifndef MMAY_RUNTIME_RECORD_H
#define MMAY_RUNTIME_RECORD_H

#include "mmay/runtime/heap.h"

typedef struct MarimayManagedRecordDescriptor {
    size_t field_count;
    const MarimayManagedFieldDescriptor *fields;
} MarimayManagedRecordDescriptor;

/* Non-escaping exact record view; see D063's rooting/safepoint lifetime. */
typedef struct MarimayManagedRecordView {
    MarimayManagedReference reference;
    const MarimayManagedRecordDescriptor *descriptor;
    void *object;
    MarimayManagedCell *fields;
} MarimayManagedRecordView;

MarimayManagedStatus mmay_managed_record_view_bind(
    MarimayManagedHeap *heap,
    MarimayManagedReference record,
    const MarimayManagedRecordDescriptor *expected,
    MarimayManagedRecordView *view
);

MarimayManagedStatus mmay_managed_record_new(
    MarimayManagedHeap *heap,
    const MarimayManagedRecordDescriptor *descriptor,
    const MarimayManagedCell *fields,
    size_t field_count,
    MarimayManagedReference *result
);
MarimayManagedStatus mmay_managed_record_describe(
    const MarimayManagedHeap *heap,
    MarimayManagedReference record,
    const MarimayManagedRecordDescriptor **descriptor
);
MarimayManagedStatus mmay_managed_record_get(
    const MarimayManagedHeap *heap,
    MarimayManagedReference record,
    size_t field,
    MarimayManagedCell *result
);
MarimayManagedStatus mmay_managed_record_set(
    MarimayManagedHeap *heap,
    MarimayManagedReference record,
    size_t field,
    MarimayManagedCell value
);

#endif
