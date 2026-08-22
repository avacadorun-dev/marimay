#include "mmay/base/checked_i64.h"

#include <limits.h>
#include <stddef.h>

int64_t mmay_i64_from_twos_complement(uint64_t bits) {
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

int64_t mmay_i64_bitwise_not(int64_t value) {
    return mmay_i64_from_twos_complement(~(uint64_t)value);
}

int64_t mmay_i64_bitwise_and(int64_t left, int64_t right) {
    return mmay_i64_from_twos_complement(
        (uint64_t)left & (uint64_t)right
    );
}

int64_t mmay_i64_bitwise_xor(int64_t left, int64_t right) {
    return mmay_i64_from_twos_complement(
        (uint64_t)left ^ (uint64_t)right
    );
}

int64_t mmay_i64_bitwise_or(int64_t left, int64_t right) {
    return mmay_i64_from_twos_complement(
        (uint64_t)left | (uint64_t)right
    );
}

int64_t mmay_i64_add_proven(int64_t left, int64_t right) {
    return mmay_i64_from_twos_complement((uint64_t)left + (uint64_t)right);
}

int64_t mmay_i64_subtract_proven(int64_t left, int64_t right) {
    return mmay_i64_from_twos_complement((uint64_t)left - (uint64_t)right);
}

int64_t mmay_i64_multiply_proven(int64_t left, int64_t right) {
    return mmay_i64_from_twos_complement((uint64_t)left * (uint64_t)right);
}

static int64_t mmay_i64_shift_right_valid(int64_t value, unsigned int count) {
    uint64_t bits = (uint64_t)value;
    if (count == 0) {
        return value;
    }
    bits >>= count;
    if (value < 0) {
        bits |= UINT64_MAX << (64U - count);
    }
    return mmay_i64_from_twos_complement(bits);
}

MarimayCheckedI64Status mmay_checked_i64_add(
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

MarimayCheckedI64Status mmay_checked_i64_subtract(
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

MarimayCheckedI64Status mmay_checked_i64_multiply(
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

MarimayCheckedI64Status mmay_checked_i64_divide(
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

MarimayCheckedI64Status mmay_checked_i64_remainder(
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

MarimayCheckedI64Status mmay_checked_i64_negate(
    int64_t value,
    int64_t *result
) {
    if (result == NULL || value == INT64_MIN) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    *result = -value;
    return MMAY_CHECKED_I64_OK;
}

MarimayCheckedI64Status mmay_checked_i64_shift_left(
    int64_t value,
    int64_t count,
    int64_t *result
) {
    int64_t shifted = 0;
    unsigned int unsigned_count = 0;
    if (result == NULL) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    if (count < 0 || count >= 64) {
        return MMAY_CHECKED_I64_INVALID_SHIFT;
    }
    unsigned_count = (unsigned int)count;
    shifted = mmay_i64_from_twos_complement(
        (uint64_t)value << unsigned_count
    );
    if (mmay_i64_shift_right_valid(shifted, unsigned_count) != value) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    *result = shifted;
    return MMAY_CHECKED_I64_OK;
}

MarimayCheckedI64Status mmay_checked_i64_shift_right(
    int64_t value,
    int64_t count,
    int64_t *result
) {
    if (result == NULL) {
        return MMAY_CHECKED_I64_OVERFLOW;
    }
    if (count < 0 || count >= 64) {
        return MMAY_CHECKED_I64_INVALID_SHIFT;
    }
    *result = mmay_i64_shift_right_valid(value, (unsigned int)count);
    return MMAY_CHECKED_I64_OK;
}
