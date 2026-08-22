#ifndef RUDA_MANAGED_ARRAY_STORAGE_INTERNAL_H
#define RUDA_MANAGED_ARRAY_STORAGE_INTERNAL_H

#include <string.h>

#include "managed_internal.h"

/* Callers must first validate the immutable descriptor and the index. */
static inline void ruda_managed_array_storage_read_unchecked(
    const RudaManagedArray *array,
    size_t index,
    RudaManagedCell *result
) {
    const unsigned char *source =
        array->storage + index * array->descriptor->storage_stride;
    memset(result, 0, sizeof(*result));
    switch (array->descriptor->storage_kind) {
        case RUDA_MANAGED_ARRAY_STORAGE_RAW8: {
            uint8_t value = 0;
            memcpy(&value, source, sizeof(value));
            result->f64_bits = value;
            return;
        }
        case RUDA_MANAGED_ARRAY_STORAGE_RAW16: {
            uint16_t value = 0;
            memcpy(&value, source, sizeof(value));
            result->f64_bits = value;
            return;
        }
        case RUDA_MANAGED_ARRAY_STORAGE_RAW32: {
            uint32_t value = 0;
            memcpy(&value, source, sizeof(value));
            result->f64_bits = value;
            return;
        }
        case RUDA_MANAGED_ARRAY_STORAGE_RAW64:
            memcpy(&result->f64_bits, source, sizeof(result->f64_bits));
            return;
        case RUDA_MANAGED_ARRAY_STORAGE_CELL:
            memcpy(result, source, sizeof(*result));
            return;
        case RUDA_MANAGED_ARRAY_STORAGE_INVALID:
            return;
    }
}

/* The value must already satisfy the descriptor's scalar/reference contract. */
static inline void ruda_managed_array_storage_write_unchecked(
    RudaManagedArray *array,
    size_t index,
    RudaManagedCell value
) {
    unsigned char *target =
        array->storage + index * array->descriptor->storage_stride;
    switch (array->descriptor->storage_kind) {
        case RUDA_MANAGED_ARRAY_STORAGE_RAW8: {
            uint8_t raw = (uint8_t)value.f64_bits;
            memcpy(target, &raw, sizeof(raw));
            return;
        }
        case RUDA_MANAGED_ARRAY_STORAGE_RAW16: {
            uint16_t raw = (uint16_t)value.f64_bits;
            memcpy(target, &raw, sizeof(raw));
            return;
        }
        case RUDA_MANAGED_ARRAY_STORAGE_RAW32: {
            uint32_t raw = (uint32_t)value.f64_bits;
            memcpy(target, &raw, sizeof(raw));
            return;
        }
        case RUDA_MANAGED_ARRAY_STORAGE_RAW64:
            memcpy(target, &value.f64_bits, sizeof(value.f64_bits));
            return;
        case RUDA_MANAGED_ARRAY_STORAGE_CELL:
            memcpy(target, &value, sizeof(value));
            return;
        case RUDA_MANAGED_ARRAY_STORAGE_INVALID:
            return;
    }
}

#endif
