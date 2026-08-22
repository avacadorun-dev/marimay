#ifndef RUDA_RUNTIME_GENERATED_RECORD_H
#define RUDA_RUNTIME_GENERATED_RECORD_H

#include "ruda/runtime/generated_array.h"
#include "ruda/runtime/record.h"

/* Exact non-moving record object layout reserved for compiler-generated C. */
typedef struct RudaManagedRecord {
    const RudaManagedRecordDescriptor *descriptor;
    RudaManagedCell *fields;
} RudaManagedRecord;

typedef struct RudaManagedRecordGeneratedView {
    RudaManagedReference reference;
    RudaManagedRecord *object;
} RudaManagedRecordGeneratedView;

#endif
