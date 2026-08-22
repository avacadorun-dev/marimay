#ifndef MMAY_BASE_F64_H
#define MMAY_BASE_F64_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"

/* The Stage 0 f64 ABI is the raw IEEE 754 binary64 interchange bit pattern. */
bool mmay_f64_binary64_supported(void);

double mmay_f64_to_double(uint64_t bits);
uint64_t mmay_f64_bits(double value);

uint64_t mmay_f64_add(uint64_t left, uint64_t right);
uint64_t mmay_f64_subtract(uint64_t left, uint64_t right);
uint64_t mmay_f64_multiply(uint64_t left, uint64_t right);
uint64_t mmay_f64_divide(uint64_t left, uint64_t right);
uint64_t mmay_f64_negate(uint64_t value);

bool mmay_f64_equal(uint64_t left, uint64_t right);
bool mmay_f64_less(uint64_t left, uint64_t right);
bool mmay_f64_less_equal(uint64_t left, uint64_t right);

typedef enum MarimayF64ParseStatus {
    MMAY_F64_PARSE_OK,
    MMAY_F64_PARSE_INVALID_ARGUMENT,
    MMAY_F64_PARSE_INVALID_SYNTAX,
    MMAY_F64_PARSE_ALLOCATION_FAILED,
    MMAY_F64_PARSE_LIMIT_REACHED
} MarimayF64ParseStatus;

/*
 * Converts the closed D024 decimal-literal grammar to binary64 without using
 * the host locale or floating-point parser. The input excludes a unary sign.
 */
MarimayF64ParseStatus mmay_f64_parse_decimal(
    const unsigned char *bytes,
    size_t length,
    MarimayAllocator allocator,
    size_t max_significant_digits,
    uint64_t *result
);

#endif
