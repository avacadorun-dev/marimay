#ifndef MMAY_RUNTIME_GENERATED_RECORD_H
#define MMAY_RUNTIME_GENERATED_RECORD_H

#include "mmay/runtime/generated_array.h"
#include "mmay/runtime/record.h"

/* Exact non-moving record object layout reserved for compiler-generated C. */
typedef struct MarimayManagedRecord {
    const MarimayManagedRecordDescriptor *descriptor;
    MarimayManagedCell *fields;
} MarimayManagedRecord;

typedef struct MarimayManagedRecordGeneratedView {
    MarimayManagedReference reference;
    MarimayManagedRecord *object;
} MarimayManagedRecordGeneratedView;

#endif
