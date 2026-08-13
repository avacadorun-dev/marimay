#ifndef RUDA_BASE_F64_H
#define RUDA_BASE_F64_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"

/* The Stage 0 f64 ABI is the raw IEEE 754 binary64 interchange bit pattern. */
bool ruda_f64_binary64_supported(void);

double ruda_f64_to_double(uint64_t bits);
uint64_t ruda_f64_bits(double value);

uint64_t ruda_f64_add(uint64_t left, uint64_t right);
uint64_t ruda_f64_subtract(uint64_t left, uint64_t right);
uint64_t ruda_f64_multiply(uint64_t left, uint64_t right);
uint64_t ruda_f64_divide(uint64_t left, uint64_t right);
uint64_t ruda_f64_negate(uint64_t value);

bool ruda_f64_equal(uint64_t left, uint64_t right);
bool ruda_f64_less(uint64_t left, uint64_t right);
bool ruda_f64_less_equal(uint64_t left, uint64_t right);

typedef enum RudaF64ParseStatus {
    RUDA_F64_PARSE_OK,
    RUDA_F64_PARSE_INVALID_ARGUMENT,
    RUDA_F64_PARSE_INVALID_SYNTAX,
    RUDA_F64_PARSE_ALLOCATION_FAILED,
    RUDA_F64_PARSE_LIMIT_REACHED
} RudaF64ParseStatus;

/*
 * Converts the closed D024 decimal-literal grammar to binary64 without using
 * the host locale or floating-point parser. The input excludes a unary sign.
 */
RudaF64ParseStatus ruda_f64_parse_decimal(
    const unsigned char *bytes,
    size_t length,
    RudaAllocator allocator,
    size_t max_significant_digits,
    uint64_t *result
);

#endif
