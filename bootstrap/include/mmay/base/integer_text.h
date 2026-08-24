#ifndef MMAY_BASE_INTEGER_TEXT_H
#define MMAY_BASE_INTEGER_TEXT_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/checked_integer.h"

/* Includes a possible sign and the terminating NUL. */
#define MMAY_INTEGER_TEXT_CAPACITY 66u
#define MMAY_BOOL_TEXT_CAPACITY 6u

typedef enum MarimayIntegerTextStatus {
    MMAY_INTEGER_TEXT_OK,
    MMAY_INTEGER_TEXT_INVALID_ARGUMENT,
    MMAY_INTEGER_TEXT_BUFFER_TOO_SMALL
} MarimayIntegerTextStatus;

typedef enum MarimayIntegerParseStatus {
    MMAY_INTEGER_PARSE_OK = 0,
    MMAY_INTEGER_PARSE_EMPTY,
    MMAY_INTEGER_PARSE_INVALID_DIGIT,
    MMAY_INTEGER_PARSE_OUT_OF_RANGE,
    MMAY_INTEGER_PARSE_INVALID_RADIX
} MarimayIntegerParseStatus;

MarimayIntegerTextStatus mmay_format_bool(
    bool value, char *buffer, size_t capacity, size_t *length
);

/* Canonical lower-case radix formatting. minimum_digits excludes the sign. */
MarimayIntegerTextStatus mmay_format_integer(
    MarimayIntegerValue value,
    unsigned int radix,
    size_t minimum_digits,
    char *buffer,
    size_t capacity,
    size_t *length
);
MarimayIntegerParseStatus mmay_parse_i64(
    const unsigned char *text,
    size_t length,
    int64_t radix,
    int64_t *result,
    size_t *error_offset
);

#endif
