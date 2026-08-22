#ifndef MMAY_BASE_CHECKED_I64_FAST_H
#define MMAY_BASE_CHECKED_I64_FAST_H

#include "mmay/base/checked_i64.h"

#include <limits.h>

static inline int64_t mmay_i64_from_twos_complement_fast(uint64_t bits) {
    const uint64_t sign = UINT64_C(1) << 63;
    uint64_t magnitude = 0;
    if (bits <= (uint64_t)INT64_MAX) {
        return (int64_t)bits;
    }
    if (bits == sign) {
        return INT64_MIN;
    }
    magnitude = (~bits) + UINT64_C(1);
    return -(int64_t)magnitude;
}

static inline int64_t mmay_i64_add_proven_fast(
    int64_t left,
    int64_t right
) {
    return mmay_i64_from_twos_complement_fast(
        (uint64_t)left + (uint64_t)right
    );
}

static inline int64_t mmay_i64_subtract_proven_fast(
    int64_t left,
    int64_t right
) {
    return mmay_i64_from_twos_complement_fast(
        (uint64_t)left - (uint64_t)right
    );
}

static inline int64_t mmay_i64_multiply_proven_fast(
    int64_t left,
    int64_t right
) {
    return mmay_i64_from_twos_complement_fast(
        (uint64_t)left * (uint64_t)right
    );
}

static inline MarimayCheckedI64Status mmay_checked_i64_add_fast(
    int64_t left,
    int64_t right,
    int64_t *result
) {
    if (result == NULL) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    if ((right > 0 && left > INT64_MAX - right)
        || (right < 0 && left < INT64_MIN - right)) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    *result = left + right;
    return MMAY_CHECKED_I64_OK;
}

static inline MarimayCheckedI64Status mmay_checked_i64_subtract_fast(
    int64_t left,
    int64_t right,
    int64_t *result
) {
    if (result == NULL) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    if ((right < 0 && left > INT64_MAX + right)
        || (right > 0 && left < INT64_MIN + right)) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    *result = left - right;
    return MMAY_CHECKED_I64_OK;
}

static inline MarimayCheckedI64Status mmay_checked_i64_multiply_fast(
    int64_t left,
    int64_t right,
    int64_t *result
) {
    if (result == NULL) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    if (left == 0 || right == 0) {
        *result = 0;
        return MMAY_CHECKED_I64_OK;
    }
    if ((left == INT64_MIN && right == -1)
        || (right == INT64_MIN && left == -1)) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    if (left > 0) {
        if ((right > 0 && left > INT64_MAX / right)
            || (right < 0 && right < INT64_MIN / left)) {
            return MMAY_CHECKED_I64_OVERFLOW;
        }
    } else if ((right > 0 && left < INT64_MIN / right)
        || (right < 0 && left < INT64_MAX / right)) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    *result = left * right;
    return MMAY_CHECKED_I64_OK;
}

static inline MarimayCheckedI64Status mmay_checked_i64_divide_fast(
    int64_t left,
    int64_t right,
    int64_t *result
) {
    if (result == NULL) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    if (right == 0) {
        return MMAY_CHECKED_I64_DIVISION_BY_ZERO;
    }
    if (left == INT64_MIN && right == -1) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    *result = left / right;
    return MMAY_CHECKED_I64_OK;
}

static inline MarimayCheckedI64Status mmay_checked_i64_remainder_fast(
    int64_t left,
    int64_t right,
    int64_t *result
) {
    if (result == NULL) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    if (right == 0) {
        return MMAY_CHECKED_I64_DIVISION_BY_ZERO;
    }
    if (left == INT64_MIN && right == -1) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    *result = left % right;
    return MMAY_CHECKED_I64_OK;
}

static inline MarimayCheckedI64Status mmay_checked_i64_negate_fast(
    int64_t value,
    int64_t *result
) {
    if (result == NULL || value == INT64_MIN) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    *result = -value;
    return MMAY_CHECKED_I64_OK;
}

#endif
