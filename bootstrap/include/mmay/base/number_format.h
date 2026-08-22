#ifndef MMAY_BASE_NUMBER_FORMAT_H
#define MMAY_BASE_NUMBER_FORMAT_H

#include <stddef.h>
#include <stdint.h>

/* Capacities include the terminating '\0'. */
#define MMAY_NUMBER_FORMAT_F32_CAPACITY 32u
#define MMAY_NUMBER_FORMAT_F64_CAPACITY 32u

typedef enum MarimayNumberFormatStatus {
    MMAY_NUMBER_FORMAT_OK,
    MMAY_NUMBER_FORMAT_INVALID_ARGUMENT,
    MMAY_NUMBER_FORMAT_BUFFER_TOO_SMALL,
    MMAY_NUMBER_FORMAT_INTERNAL_ERROR
} MarimayNumberFormatStatus;

/*
 * Formats raw IEEE 754 bits according to D046. The result is ASCII, does not
 * consult the host locale, and is written as a NUL-terminated string. The
 * length excludes that terminator. A NULL buffer with capacity zero may be
 * used to query the required capacity; it returns BUFFER_TOO_SMALL.
 */
MarimayNumberFormatStatus mmay_format_f32(
    uint32_t raw_bits,
    char *buffer,
    size_t capacity,
    size_t *length
);

MarimayNumberFormatStatus mmay_format_f64(
    uint64_t raw_bits,
    char *buffer,
    size_t capacity,
    size_t *length
);

#endif
