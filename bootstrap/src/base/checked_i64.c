#include "ruda/base/checked_i64.h"

#include <limits.h>
#include <stddef.h>

int64_t ruda_i64_from_twos_complement(uint64_t bits) {
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

int64_t ruda_i64_bitwise_not(int64_t value) {
    return ruda_i64_from_twos_complement(~(uint64_t)value);
}

int64_t ruda_i64_bitwise_and(int64_t left, int64_t right) {
    return ruda_i64_from_twos_complement(
        (uint64_t)left & (uint64_t)right
    );
}

int64_t ruda_i64_bitwise_xor(int64_t left, int64_t right) {
    return ruda_i64_from_twos_complement(
        (uint64_t)left ^ (uint64_t)right
    );
}

int64_t ruda_i64_bitwise_or(int64_t left, int64_t right) {
    return ruda_i64_from_twos_complement(
        (uint64_t)left | (uint64_t)right
    );
}

int64_t ruda_i64_add_proven(int64_t left, int64_t right) {
    return ruda_i64_from_twos_complement((uint64_t)left + (uint64_t)right);
}

int64_t ruda_i64_subtract_proven(int64_t left, int64_t right) {
    return ruda_i64_from_twos_complement((uint64_t)left - (uint64_t)right);
}

int64_t ruda_i64_multiply_proven(int64_t left, int64_t right) {
    return ruda_i64_from_twos_complement((uint64_t)left * (uint64_t)right);
}

static int64_t ruda_i64_shift_right_valid(int64_t value, unsigned int count) {
    uint64_t bits = (uint64_t)value;
    if (count == 0) {
        return value;
    }
    bits >>= count;
    if (value < 0) {
        bits |= UINT64_MAX << (64U - count);
    }
    return ruda_i64_from_twos_complement(bits);
}

RudaCheckedI64Status ruda_checked_i64_add(
    int64_t left,
    int64_t right,
    int64_t *result
) {
    if (result == NULL) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    if ((right > 0 && left > INT64_MAX - right)
        || (right < 0 && left < INT64_MIN - right)) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    *result = left + right;
    return RUDA_CHECKED_I64_OK;
}

RudaCheckedI64Status ruda_checked_i64_subtract(
    int64_t left,
    int64_t right,
    int64_t *result
) {
    if (result == NULL) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    if ((right < 0 && left > INT64_MAX + right)
        || (right > 0 && left < INT64_MIN + right)) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    *result = left - right;
    return RUDA_CHECKED_I64_OK;
}

RudaCheckedI64Status ruda_checked_i64_multiply(
    int64_t left,
    int64_t right,
    int64_t *result
) {
    if (result == NULL) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    if (left == 0 || right == 0) {
        *result = 0;
        return RUDA_CHECKED_I64_OK;
    }
    if ((left == INT64_MIN && right == -1)
        || (right == INT64_MIN && left == -1)) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    if (left > 0) {
        if ((right > 0 && left > INT64_MAX / right)
            || (right < 0 && right < INT64_MIN / left)) {
            return RUDA_CHECKED_I64_OVERFLOW;
        }
    } else if ((right > 0 && left < INT64_MIN / right)
        || (right < 0 && left < INT64_MAX / right)) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    *result = left * right;
    return RUDA_CHECKED_I64_OK;
}

RudaCheckedI64Status ruda_checked_i64_divide(
    int64_t left,
    int64_t right,
    int64_t *result
) {
    if (result == NULL) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    if (right == 0) {
        return RUDA_CHECKED_I64_DIVISION_BY_ZERO;
    }
    if (left == INT64_MIN && right == -1) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    *result = left / right;
    return RUDA_CHECKED_I64_OK;
}

RudaCheckedI64Status ruda_checked_i64_remainder(
    int64_t left,
    int64_t right,
    int64_t *result
) {
    if (result == NULL) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    if (right == 0) {
        return RUDA_CHECKED_I64_DIVISION_BY_ZERO;
    }
    if (left == INT64_MIN && right == -1) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    *result = left % right;
    return RUDA_CHECKED_I64_OK;
}

RudaCheckedI64Status ruda_checked_i64_negate(
    int64_t value,
    int64_t *result
) {
    if (result == NULL || value == INT64_MIN) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    *result = -value;
    return RUDA_CHECKED_I64_OK;
}

RudaCheckedI64Status ruda_checked_i64_shift_left(
    int64_t value,
    int64_t count,
    int64_t *result
) {
    int64_t shifted = 0;
    unsigned int unsigned_count = 0;
    if (result == NULL) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    if (count < 0 || count >= 64) {
        return RUDA_CHECKED_I64_INVALID_SHIFT;
    }
    unsigned_count = (unsigned int)count;
    shifted = ruda_i64_from_twos_complement(
        (uint64_t)value << unsigned_count
    );
    if (ruda_i64_shift_right_valid(shifted, unsigned_count) != value) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    *result = shifted;
    return RUDA_CHECKED_I64_OK;
}

RudaCheckedI64Status ruda_checked_i64_shift_right(
    int64_t value,
    int64_t count,
    int64_t *result
) {
    if (result == NULL) {
        return RUDA_CHECKED_I64_OVERFLOW;
    }
    if (count < 0 || count >= 64) {
        return RUDA_CHECKED_I64_INVALID_SHIFT;
    }
    *result = ruda_i64_shift_right_valid(value, (unsigned int)count);
    return RUDA_CHECKED_I64_OK;
}
