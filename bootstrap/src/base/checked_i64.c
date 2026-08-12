#include "ruda/base/checked_i64.h"

#include <limits.h>
#include <stddef.h>

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
