#include "ruda/base/checked_integer.h"

#include <stddef.h>

typedef struct RudaIntegerSpec {
    unsigned int width;
    bool is_signed;
    uint64_t mask;
    uint64_t sign_bit;
    int64_t signed_min;
    int64_t signed_max;
} RudaIntegerSpec;

static bool ruda_integer_spec(
    RudaIntegerKind kind,
    RudaIntegerSpec *spec
) {
    unsigned int width = 0;
    bool is_signed = false;
    if (spec == NULL) {
        return false;
    }
    switch (kind) {
        case RUDA_INTEGER_I8:
            width = 8;
            is_signed = true;
            break;
        case RUDA_INTEGER_I16:
            width = 16;
            is_signed = true;
            break;
        case RUDA_INTEGER_I32:
            width = 32;
            is_signed = true;
            break;
        case RUDA_INTEGER_I64:
            width = 64;
            is_signed = true;
            break;
        case RUDA_INTEGER_U8:
            width = 8;
            break;
        case RUDA_INTEGER_U16:
            width = 16;
            break;
        case RUDA_INTEGER_U32:
            width = 32;
            break;
        case RUDA_INTEGER_U64:
            width = 64;
            break;
        default:
            return false;
    }
    spec->width = width;
    spec->is_signed = is_signed;
    spec->sign_bit = UINT64_C(1) << (width - 1);
    spec->mask = width == 64
        ? UINT64_MAX
        : (UINT64_C(1) << width) - UINT64_C(1);
    spec->signed_max = (int64_t)(spec->sign_bit - UINT64_C(1));
    spec->signed_min = -spec->signed_max - INT64_C(1);
    return true;
}

static uint64_t ruda_signed_to_bits(
    int64_t value,
    const RudaIntegerSpec *spec
) {
    uint64_t magnitude = 0;
    if (value >= 0) {
        return (uint64_t)value & spec->mask;
    }
    /* The +1 keeps the negation in range even for INT64_MIN. */
    magnitude = (uint64_t)(-(value + INT64_C(1))) + UINT64_C(1);
    return (UINT64_MAX - magnitude + UINT64_C(1)) & spec->mask;
}

static int64_t ruda_bits_to_signed(
    uint64_t bits,
    const RudaIntegerSpec *spec
) {
    uint64_t magnitude = 0;
    bits &= spec->mask;
    if ((bits & spec->sign_bit) == 0) {
        return (int64_t)bits;
    }
    if (bits == spec->sign_bit) {
        return spec->signed_min;
    }
    magnitude = spec->mask - bits + UINT64_C(1);
    return -(int64_t)magnitude;
}

static RudaIntegerValue ruda_integer_make(
    RudaIntegerKind kind,
    uint64_t bits
) {
    RudaIntegerSpec spec;
    RudaIntegerValue result;
    result.kind = kind;
    result.bits = bits;
    if (ruda_integer_spec(kind, &spec)) {
        result.bits &= spec.mask;
    }
    return result;
}

static bool ruda_value_is_valid(
    RudaIntegerValue value,
    RudaIntegerSpec *spec
) {
    return ruda_integer_spec(value.kind, spec) && value.bits <= spec->mask;
}

static bool ruda_prepare_binary(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result,
    RudaIntegerSpec *spec
) {
    if (result == NULL || left.kind != right.kind
        || !ruda_value_is_valid(left, spec)
        || right.bits > spec->mask) {
        return false;
    }
    return true;
}

static bool ruda_prepare_unary(
    RudaIntegerValue value,
    RudaIntegerValue *result,
    RudaIntegerSpec *spec
) {
    return result != NULL && ruda_value_is_valid(value, spec);
}

static void ruda_set_bits(
    RudaIntegerValue *result,
    RudaIntegerKind kind,
    uint64_t bits,
    const RudaIntegerSpec *spec
) {
    result->kind = kind;
    result->bits = bits & spec->mask;
}

static void ruda_set_signed(
    RudaIntegerValue *result,
    RudaIntegerValue input,
    int64_t value,
    const RudaIntegerSpec *spec
) {
    ruda_set_bits(result, input.kind, ruda_signed_to_bits(value, spec), spec);
}

static uint64_t ruda_arithmetic_shift_right(
    uint64_t bits,
    unsigned int count,
    const RudaIntegerSpec *spec
) {
    uint64_t shifted = bits >> count;
    if (spec->is_signed && count != 0 && (bits & spec->sign_bit) != 0) {
        shifted |= spec->mask ^ (spec->mask >> count);
    }
    return shifted & spec->mask;
}

static RudaCheckedIntegerStatus ruda_shift_count(
    RudaIntegerValue count_value,
    const RudaIntegerSpec *spec,
    unsigned int *count
) {
    int64_t signed_count = 0;
    if (count == NULL) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    if (spec->is_signed) {
        signed_count = ruda_bits_to_signed(count_value.bits, spec);
        if (signed_count < 0 || signed_count >= (int64_t)spec->width) {
            return RUDA_CHECKED_INTEGER_INVALID_SHIFT;
        }
        *count = (unsigned int)signed_count;
    } else {
        if (count_value.bits >= (uint64_t)spec->width) {
            return RUDA_CHECKED_INTEGER_INVALID_SHIFT;
        }
        *count = (unsigned int)count_value.bits;
    }
    return RUDA_CHECKED_INTEGER_OK;
}

unsigned int ruda_integer_bit_width(RudaIntegerKind kind) {
    RudaIntegerSpec spec;
    return ruda_integer_spec(kind, &spec) ? spec.width : 0;
}

bool ruda_integer_kind_is_signed(RudaIntegerKind kind) {
    RudaIntegerSpec spec;
    return ruda_integer_spec(kind, &spec) && spec.is_signed;
}

bool ruda_integer_convert(
    RudaIntegerValue value,
    RudaIntegerKind target_kind,
    RudaIntegerValue *result
) {
    RudaIntegerSpec source;
    RudaIntegerSpec target;
    if (result == NULL || !ruda_value_is_valid(value, &source)
        || !ruda_integer_spec(target_kind, &target)) {
        return false;
    }
    if (source.is_signed) {
        int64_t signed_value = ruda_bits_to_signed(value.bits, &source);
        if (target.is_signed) {
            if (signed_value < target.signed_min
                || signed_value > target.signed_max) {
                return false;
            }
            *result = ruda_integer_make(
                target_kind, ruda_signed_to_bits(signed_value, &target)
            );
            return true;
        }
        if (signed_value < 0 || (uint64_t)signed_value > target.mask) {
            return false;
        }
        *result = ruda_integer_make(target_kind, (uint64_t)signed_value);
        return true;
    }
    if (target.is_signed) {
        if (value.bits > (uint64_t)target.signed_max) {
            return false;
        }
        *result = ruda_integer_make(target_kind, value.bits);
        return true;
    }
    if (value.bits > target.mask) {
        return false;
    }
    *result = ruda_integer_make(target_kind, value.bits);
    return true;
}

RudaIntegerValue ruda_integer_from_i8(int8_t value) {
    RudaIntegerSpec spec;
    (void)ruda_integer_spec(RUDA_INTEGER_I8, &spec);
    return ruda_integer_make(
        RUDA_INTEGER_I8,
        ruda_signed_to_bits((int64_t)value, &spec)
    );
}

RudaIntegerValue ruda_integer_from_i16(int16_t value) {
    RudaIntegerSpec spec;
    (void)ruda_integer_spec(RUDA_INTEGER_I16, &spec);
    return ruda_integer_make(
        RUDA_INTEGER_I16,
        ruda_signed_to_bits((int64_t)value, &spec)
    );
}

RudaIntegerValue ruda_integer_from_i32(int32_t value) {
    RudaIntegerSpec spec;
    (void)ruda_integer_spec(RUDA_INTEGER_I32, &spec);
    return ruda_integer_make(
        RUDA_INTEGER_I32,
        ruda_signed_to_bits((int64_t)value, &spec)
    );
}

RudaIntegerValue ruda_integer_from_i64(int64_t value) {
    RudaIntegerSpec spec;
    (void)ruda_integer_spec(RUDA_INTEGER_I64, &spec);
    return ruda_integer_make(
        RUDA_INTEGER_I64,
        ruda_signed_to_bits(value, &spec)
    );
}

RudaIntegerValue ruda_integer_from_u8(uint8_t value) {
    return ruda_integer_make(RUDA_INTEGER_U8, (uint64_t)value);
}

RudaIntegerValue ruda_integer_from_u16(uint16_t value) {
    return ruda_integer_make(RUDA_INTEGER_U16, (uint64_t)value);
}

RudaIntegerValue ruda_integer_from_u32(uint32_t value) {
    return ruda_integer_make(RUDA_INTEGER_U32, (uint64_t)value);
}

RudaIntegerValue ruda_integer_from_u64(uint64_t value) {
    return ruda_integer_make(RUDA_INTEGER_U64, value);
}

static bool ruda_value_as_i64(RudaIntegerValue value, int64_t *result) {
    RudaIntegerSpec spec;
    if (result == NULL || !ruda_value_is_valid(value, &spec)) {
        return false;
    }
    if (spec.is_signed) {
        *result = ruda_bits_to_signed(value.bits, &spec);
        return true;
    }
    if (value.bits > (uint64_t)INT64_MAX) {
        return false;
    }
    *result = (int64_t)value.bits;
    return true;
}

static bool ruda_value_as_u64(RudaIntegerValue value, uint64_t *result) {
    RudaIntegerSpec spec;
    int64_t signed_value = 0;
    if (result == NULL || !ruda_value_is_valid(value, &spec)) {
        return false;
    }
    if (!spec.is_signed) {
        *result = value.bits;
        return true;
    }
    signed_value = ruda_bits_to_signed(value.bits, &spec);
    if (signed_value < 0) {
        return false;
    }
    *result = (uint64_t)signed_value;
    return true;
}

bool ruda_integer_to_i8(RudaIntegerValue value, int8_t *result) {
    int64_t converted = 0;
    if (result == NULL || !ruda_value_as_i64(value, &converted)
        || converted < INT8_MIN || converted > INT8_MAX) {
        return false;
    }
    *result = (int8_t)converted;
    return true;
}

bool ruda_integer_to_i16(RudaIntegerValue value, int16_t *result) {
    int64_t converted = 0;
    if (result == NULL || !ruda_value_as_i64(value, &converted)
        || converted < INT16_MIN || converted > INT16_MAX) {
        return false;
    }
    *result = (int16_t)converted;
    return true;
}

bool ruda_integer_to_i32(RudaIntegerValue value, int32_t *result) {
    int64_t converted = 0;
    if (result == NULL || !ruda_value_as_i64(value, &converted)
        || converted < INT32_MIN || converted > INT32_MAX) {
        return false;
    }
    *result = (int32_t)converted;
    return true;
}

bool ruda_integer_to_i64(RudaIntegerValue value, int64_t *result) {
    return ruda_value_as_i64(value, result);
}

bool ruda_integer_to_u8(RudaIntegerValue value, uint8_t *result) {
    uint64_t converted = 0;
    if (result == NULL || !ruda_value_as_u64(value, &converted)
        || converted > UINT8_MAX) {
        return false;
    }
    *result = (uint8_t)converted;
    return true;
}

bool ruda_integer_to_u16(RudaIntegerValue value, uint16_t *result) {
    uint64_t converted = 0;
    if (result == NULL || !ruda_value_as_u64(value, &converted)
        || converted > UINT16_MAX) {
        return false;
    }
    *result = (uint16_t)converted;
    return true;
}

bool ruda_integer_to_u32(RudaIntegerValue value, uint32_t *result) {
    uint64_t converted = 0;
    if (result == NULL || !ruda_value_as_u64(value, &converted)
        || converted > UINT32_MAX) {
        return false;
    }
    *result = (uint32_t)converted;
    return true;
}

bool ruda_integer_to_u64(RudaIntegerValue value, uint64_t *result) {
    return ruda_value_as_u64(value, result);
}

RudaCheckedIntegerStatus ruda_checked_integer_add(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    if (spec.is_signed) {
        int64_t left_value = ruda_bits_to_signed(left.bits, &spec);
        int64_t right_value = ruda_bits_to_signed(right.bits, &spec);
        if ((right_value > 0 && left_value > spec.signed_max - right_value)
            || (right_value < 0 && left_value < spec.signed_min - right_value)) {
            return RUDA_CHECKED_INTEGER_OVERFLOW;
        }
        ruda_set_signed(result, left, left_value + right_value, &spec);
    } else {
        if (left.bits > spec.mask - right.bits) {
            return RUDA_CHECKED_INTEGER_OVERFLOW;
        }
        ruda_set_bits(result, left.kind, left.bits + right.bits, &spec);
    }
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_checked_integer_subtract(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    if (spec.is_signed) {
        int64_t left_value = ruda_bits_to_signed(left.bits, &spec);
        int64_t right_value = ruda_bits_to_signed(right.bits, &spec);
        if ((right_value < 0 && left_value > spec.signed_max + right_value)
            || (right_value > 0 && left_value < spec.signed_min + right_value)) {
            return RUDA_CHECKED_INTEGER_OVERFLOW;
        }
        ruda_set_signed(result, left, left_value - right_value, &spec);
    } else {
        if (left.bits < right.bits) {
            return RUDA_CHECKED_INTEGER_OVERFLOW;
        }
        ruda_set_bits(result, left.kind, left.bits - right.bits, &spec);
    }
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_checked_integer_multiply(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    if (spec.is_signed) {
        int64_t left_value = ruda_bits_to_signed(left.bits, &spec);
        int64_t right_value = ruda_bits_to_signed(right.bits, &spec);
        if (left_value != 0 && right_value != 0) {
            if ((left_value == spec.signed_min && right_value == -1)
                || (right_value == spec.signed_min && left_value == -1)) {
                return RUDA_CHECKED_INTEGER_OVERFLOW;
            }
            if (left_value > 0) {
                if ((right_value > 0
                        && left_value > spec.signed_max / right_value)
                    || (right_value < 0
                        && right_value < spec.signed_min / left_value)) {
                    return RUDA_CHECKED_INTEGER_OVERFLOW;
                }
            } else if ((right_value > 0
                    && left_value < spec.signed_min / right_value)
                || (right_value < 0
                    && left_value < spec.signed_max / right_value)) {
                return RUDA_CHECKED_INTEGER_OVERFLOW;
            }
        }
        ruda_set_signed(result, left, left_value * right_value, &spec);
    } else {
        if (left.bits != 0 && right.bits > spec.mask / left.bits) {
            return RUDA_CHECKED_INTEGER_OVERFLOW;
        }
        ruda_set_bits(result, left.kind, left.bits * right.bits, &spec);
    }
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_checked_integer_divide(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    if (right.bits == 0) {
        return RUDA_CHECKED_INTEGER_DIVISION_BY_ZERO;
    }
    if (spec.is_signed) {
        int64_t left_value = ruda_bits_to_signed(left.bits, &spec);
        int64_t right_value = ruda_bits_to_signed(right.bits, &spec);
        if (left_value == spec.signed_min && right_value == -1) {
            return RUDA_CHECKED_INTEGER_OVERFLOW;
        }
        ruda_set_signed(result, left, left_value / right_value, &spec);
    } else {
        ruda_set_bits(result, left.kind, left.bits / right.bits, &spec);
    }
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_checked_integer_remainder(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    if (right.bits == 0) {
        return RUDA_CHECKED_INTEGER_DIVISION_BY_ZERO;
    }
    if (spec.is_signed) {
        int64_t left_value = ruda_bits_to_signed(left.bits, &spec);
        int64_t right_value = ruda_bits_to_signed(right.bits, &spec);
        if (left_value == spec.signed_min && right_value == -1) {
            return RUDA_CHECKED_INTEGER_OVERFLOW;
        }
        ruda_set_signed(result, left, left_value % right_value, &spec);
    } else {
        ruda_set_bits(result, left.kind, left.bits % right.bits, &spec);
    }
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_checked_integer_negate(
    RudaIntegerValue value,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    if (!ruda_prepare_unary(value, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    if (spec.is_signed) {
        int64_t signed_value = ruda_bits_to_signed(value.bits, &spec);
        if (signed_value == spec.signed_min) {
            return RUDA_CHECKED_INTEGER_OVERFLOW;
        }
        ruda_set_signed(result, value, -signed_value, &spec);
    } else {
        if (value.bits != 0) {
            return RUDA_CHECKED_INTEGER_OVERFLOW;
        }
        ruda_set_bits(result, value.kind, 0, &spec);
    }
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_checked_integer_shift_left(
    RudaIntegerValue value,
    RudaIntegerValue count_value,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus count_status = RUDA_CHECKED_INTEGER_OK;
    unsigned int count = 0;
    uint64_t shifted = 0;
    if (!ruda_prepare_binary(value, count_value, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    count_status = ruda_shift_count(count_value, &spec, &count);
    if (count_status != RUDA_CHECKED_INTEGER_OK) {
        return count_status;
    }
    shifted = (value.bits << count) & spec.mask;
    if (spec.is_signed) {
        if (ruda_arithmetic_shift_right(shifted, count, &spec)
            != value.bits) {
            return RUDA_CHECKED_INTEGER_OVERFLOW;
        }
    } else if (value.bits > (spec.mask >> count)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    ruda_set_bits(result, value.kind, shifted, &spec);
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_checked_integer_shift_right(
    RudaIntegerValue value,
    RudaIntegerValue count_value,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus count_status = RUDA_CHECKED_INTEGER_OK;
    unsigned int count = 0;
    if (!ruda_prepare_binary(value, count_value, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    count_status = ruda_shift_count(count_value, &spec, &count);
    if (count_status != RUDA_CHECKED_INTEGER_OK) {
        return count_status;
    }
    ruda_set_bits(
        result,
        value.kind,
        ruda_arithmetic_shift_right(value.bits, count, &spec),
        &spec
    );
    return RUDA_CHECKED_INTEGER_OK;
}

static RudaCheckedIntegerStatus ruda_wrapping_shift_left_impl(
    RudaIntegerValue value,
    RudaIntegerValue count_value,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus count_status = RUDA_CHECKED_INTEGER_OK;
    unsigned int count = 0;
    if (!ruda_prepare_binary(value, count_value, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    count_status = ruda_shift_count(count_value, &spec, &count);
    if (count_status != RUDA_CHECKED_INTEGER_OK) {
        return count_status;
    }
    ruda_set_bits(result, value.kind, value.bits << count, &spec);
    return RUDA_CHECKED_INTEGER_OK;
}

static RudaCheckedIntegerStatus ruda_wrapping_shift_right_impl(
    RudaIntegerValue value,
    RudaIntegerValue count_value,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus count_status = RUDA_CHECKED_INTEGER_OK;
    unsigned int count = 0;
    if (!ruda_prepare_binary(value, count_value, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    count_status = ruda_shift_count(count_value, &spec, &count);
    if (count_status != RUDA_CHECKED_INTEGER_OK) {
        return count_status;
    }
    ruda_set_bits(
        result,
        value.kind,
        ruda_arithmetic_shift_right(value.bits, count, &spec),
        &spec
    );
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_wrapping_integer_add(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    ruda_set_bits(result, left.kind, left.bits + right.bits, &spec);
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_wrapping_integer_subtract(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    ruda_set_bits(result, left.kind, left.bits - right.bits, &spec);
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_wrapping_integer_multiply(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    ruda_set_bits(result, left.kind, left.bits * right.bits, &spec);
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_wrapping_integer_divide(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus status = RUDA_CHECKED_INTEGER_OK;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    status = ruda_checked_integer_divide(left, right, result);
    if (status != RUDA_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    ruda_set_bits(result, left.kind, left.bits, &spec);
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_wrapping_integer_remainder(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus status = RUDA_CHECKED_INTEGER_OK;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    status = ruda_checked_integer_remainder(left, right, result);
    if (status != RUDA_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    ruda_set_bits(result, left.kind, 0, &spec);
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_wrapping_integer_negate(
    RudaIntegerValue value,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    if (!ruda_prepare_unary(value, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    ruda_set_bits(result, value.kind, UINT64_C(0) - value.bits, &spec);
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_wrapping_integer_shift_left(
    RudaIntegerValue value,
    RudaIntegerValue count,
    RudaIntegerValue *result
) {
    return ruda_wrapping_shift_left_impl(value, count, result);
}

RudaCheckedIntegerStatus ruda_wrapping_integer_shift_right(
    RudaIntegerValue value,
    RudaIntegerValue count,
    RudaIntegerValue *result
) {
    return ruda_wrapping_shift_right_impl(value, count, result);
}

static void ruda_set_saturating_max(
    RudaIntegerValue *result,
    RudaIntegerValue input,
    const RudaIntegerSpec *spec
) {
    ruda_set_bits(
        result,
        input.kind,
        spec->is_signed ? spec->sign_bit - UINT64_C(1) : spec->mask,
        spec
    );
}

static void ruda_set_saturating_min(
    RudaIntegerValue *result,
    RudaIntegerValue input,
    const RudaIntegerSpec *spec
) {
    ruda_set_bits(
        result,
        input.kind,
        spec->is_signed ? spec->sign_bit : UINT64_C(0),
        spec
    );
}

RudaCheckedIntegerStatus ruda_saturating_integer_add(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus status = RUDA_CHECKED_INTEGER_OK;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    status = ruda_checked_integer_add(left, right, result);
    if (status != RUDA_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    if (!spec.is_signed
        || ruda_bits_to_signed(left.bits, &spec) >= 0) {
        ruda_set_saturating_max(result, left, &spec);
    } else {
        ruda_set_saturating_min(result, left, &spec);
    }
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_saturating_integer_subtract(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus status = RUDA_CHECKED_INTEGER_OK;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    status = ruda_checked_integer_subtract(left, right, result);
    if (status != RUDA_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    if (!spec.is_signed) {
        ruda_set_saturating_min(result, left, &spec);
    } else if (ruda_bits_to_signed(left.bits, &spec) >= 0
        && ruda_bits_to_signed(right.bits, &spec) < 0) {
        ruda_set_saturating_max(result, left, &spec);
    } else {
        ruda_set_saturating_min(result, left, &spec);
    }
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_saturating_integer_multiply(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus status = RUDA_CHECKED_INTEGER_OK;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    status = ruda_checked_integer_multiply(left, right, result);
    if (status != RUDA_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    if (!spec.is_signed) {
        ruda_set_saturating_max(result, left, &spec);
    } else {
        int64_t left_value = ruda_bits_to_signed(left.bits, &spec);
        int64_t right_value = ruda_bits_to_signed(right.bits, &spec);
        if ((left_value < 0) != (right_value < 0)) {
            ruda_set_saturating_min(result, left, &spec);
        } else {
            ruda_set_saturating_max(result, left, &spec);
        }
    }
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_saturating_integer_divide(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus status = RUDA_CHECKED_INTEGER_OK;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    status = ruda_checked_integer_divide(left, right, result);
    if (status != RUDA_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    ruda_set_saturating_max(result, left, &spec);
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_saturating_integer_remainder(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus status = RUDA_CHECKED_INTEGER_OK;
    if (!ruda_prepare_binary(left, right, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    status = ruda_checked_integer_remainder(left, right, result);
    if (status != RUDA_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    ruda_set_bits(result, left.kind, 0, &spec);
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_saturating_integer_negate(
    RudaIntegerValue value,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus status = RUDA_CHECKED_INTEGER_OK;
    if (!ruda_prepare_unary(value, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    status = ruda_checked_integer_negate(value, result);
    if (status != RUDA_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    ruda_set_saturating_min(result, value, &spec);
    if (spec.is_signed) {
        ruda_set_saturating_max(result, value, &spec);
    }
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_saturating_integer_shift_left(
    RudaIntegerValue value,
    RudaIntegerValue count,
    RudaIntegerValue *result
) {
    RudaIntegerSpec spec;
    RudaCheckedIntegerStatus status = RUDA_CHECKED_INTEGER_OK;
    if (!ruda_prepare_binary(value, count, result, &spec)) {
        return RUDA_CHECKED_INTEGER_OVERFLOW;
    }
    status = ruda_checked_integer_shift_left(value, count, result);
    if (status != RUDA_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    if (spec.is_signed && (value.bits & spec.sign_bit) != 0) {
        ruda_set_saturating_min(result, value, &spec);
    } else {
        ruda_set_saturating_max(result, value, &spec);
    }
    return RUDA_CHECKED_INTEGER_OK;
}

RudaCheckedIntegerStatus ruda_saturating_integer_shift_right(
    RudaIntegerValue value,
    RudaIntegerValue count,
    RudaIntegerValue *result
) {
    return ruda_checked_integer_shift_right(value, count, result);
}
