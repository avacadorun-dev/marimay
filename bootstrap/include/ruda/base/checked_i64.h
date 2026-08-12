#ifndef RUDA_BASE_CHECKED_I64_H
#define RUDA_BASE_CHECKED_I64_H

#include <stdint.h>

typedef enum RudaCheckedI64Status {
    RUDA_CHECKED_I64_OK,
    RUDA_CHECKED_I64_OVERFLOW,
    RUDA_CHECKED_I64_DIVISION_BY_ZERO
} RudaCheckedI64Status;

RudaCheckedI64Status ruda_checked_i64_add(
    int64_t left,
    int64_t right,
    int64_t *result
);
RudaCheckedI64Status ruda_checked_i64_subtract(
    int64_t left,
    int64_t right,
    int64_t *result
);
RudaCheckedI64Status ruda_checked_i64_multiply(
    int64_t left,
    int64_t right,
    int64_t *result
);
RudaCheckedI64Status ruda_checked_i64_divide(
    int64_t left,
    int64_t right,
    int64_t *result
);
RudaCheckedI64Status ruda_checked_i64_remainder(
    int64_t left,
    int64_t right,
    int64_t *result
);
RudaCheckedI64Status ruda_checked_i64_negate(
    int64_t value,
    int64_t *result
);

#endif
