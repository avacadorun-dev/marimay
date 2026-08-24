#ifndef MMAY_RUNTIME_STRING_H
#define MMAY_RUNTIME_STRING_H

#include "mmay/runtime/heap.h"

MarimayManagedStatus mmay_managed_string_new(
    MarimayManagedHeap *heap,
    const unsigned char *bytes,
    size_t length,
    MarimayManagedReference *result
);
MarimayManagedStatus mmay_managed_string_bytes(
    const MarimayManagedHeap *heap,
    MarimayManagedReference value,
    const unsigned char **bytes,
    size_t *length
);
MarimayManagedStatus mmay_managed_string_concat(
    MarimayManagedHeap *heap,
    const unsigned char *left,
    size_t left_length,
    const unsigned char *right,
    size_t right_length,
    MarimayManagedReference *result
);

#endif
