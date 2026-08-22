#ifndef MMAY_BASE_CHECKED_I64_H
#define MMAY_BASE_CHECKED_I64_H

#include <stdint.h>

typedef enum MarimayCheckedI64Status {
    MMAY_CHECKED_I64_OK,
    MMAY_CHECKED_I64_OVERFLOW,
    MMAY_CHECKED_I64_DIVISION_BY_ZERO,
    MMAY_CHECKED_I64_INVALID_SHIFT
} MarimayCheckedI64Status;

int64_t mmay_i64_from_twos_complement(uint64_t bits);
int64_t mmay_i64_bitwise_not(int64_t value);
int64_t mmay_i64_bitwise_and(int64_t left, int64_t right);
int64_t mmay_i64_bitwise_xor(int64_t left, int64_t right);
int64_t mmay_i64_bitwise_or(int64_t left, int64_t right);
int64_t mmay_i64_add_proven(int64_t left, int64_t right);
int64_t mmay_i64_subtract_proven(int64_t left, int64_t right);
int64_t mmay_i64_multiply_proven(int64_t left, int64_t right);

MarimayCheckedI64Status mmay_checked_i64_add(
    int64_t left,
    int64_t right,
    int64_t *result
);
MarimayCheckedI64Status mmay_checked_i64_subtract(
    int64_t left,
    int64_t right,
    int64_t *result
);
MarimayCheckedI64Status mmay_checked_i64_multiply(
    int64_t left,
    int64_t right,
    int64_t *result
);
MarimayCheckedI64Status mmay_checked_i64_divide(
    int64_t left,
    int64_t right,
    int64_t *result
);
MarimayCheckedI64Status mmay_checked_i64_remainder(
    int64_t left,
    int64_t right,
    int64_t *result
);
MarimayCheckedI64Status mmay_checked_i64_negate(
    int64_t value,
    int64_t *result
);
MarimayCheckedI64Status mmay_checked_i64_shift_left(
    int64_t value,
    int64_t count,
    int64_t *result
);
MarimayCheckedI64Status mmay_checked_i64_shift_right(
    int64_t value,
    int64_t count,
    int64_t *result
);

#endif
