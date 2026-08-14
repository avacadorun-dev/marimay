#ifndef RUDA_RUNTIME_STRING_H
#define RUDA_RUNTIME_STRING_H

#include "ruda/runtime/heap.h"

RudaManagedStatus ruda_managed_string_new(
    RudaManagedHeap *heap,
    const unsigned char *bytes,
    size_t length,
    RudaManagedReference *result
);
RudaManagedStatus ruda_managed_string_bytes(
    const RudaManagedHeap *heap,
    RudaManagedReference value,
    const unsigned char **bytes,
    size_t *length
);

#endif
