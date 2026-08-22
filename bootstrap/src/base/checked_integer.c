#include "mmay/base/checked_integer.h"

#include <stddef.h>

typedef struct MarimayIntegerSpec {
    unsigned int width;
    bool is_signed;
    uint64_t mask;
    uint64_t sign_bit;
    int64_t signed_min;
    int64_t signed_max;
} MarimayIntegerSpec;

static bool mmay_integer_spec(
    MarimayIntegerKind kind,
    MarimayIntegerSpec *spec
) {
    unsigned int width = 0;
    bool is_signed = false;
    if (spec == NULL) {
        return false;
    }
    switch (kind) {
        case MMAY_INTEGER_I8:
            width = 8;
            is_signed = true;
            break;
        case MMAY_INTEGER_I16:
            width = 16;
            is_signed = true;
            break;
        case MMAY_INTEGER_I32:
            width = 32;
            is_signed = true;
            break;
        case MMAY_INTEGER_I64:
            width = 64;
            is_signed = true;
            break;
        case MMAY_INTEGER_U8:
            width = 8;
            break;
        case MMAY_INTEGER_U16:
            width = 16;
            break;
        case MMAY_INTEGER_U32:
            width = 32;
            break;
        case MMAY_INTEGER_U64:
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

static uint64_t mmay_signed_to_bits(
    int64_t value,
    const MarimayIntegerSpec *spec
) {
    uint64_t magnitude = 0;
    if (value >= 0) {
        return (uint64_t)value & spec->mask;
    }
    /* The +1 keeps the negation in range even for INT64_MIN. */
    magnitude = (uint64_t)(-(value + INT64_C(1))) + UINT64_C(1);
    return (UINT64_MAX - magnitude + UINT64_C(1)) & spec->mask;
}

static int64_t mmay_bits_to_signed(
    uint64_t bits,
    const MarimayIntegerSpec *spec
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

static MarimayIntegerValue mmay_integer_make(
    MarimayIntegerKind kind,
    uint64_t bits
) {
    MarimayIntegerSpec spec;
    MarimayIntegerValue result;
    result.kind = kind;
    result.bits = bits;
    if (mmay_integer_spec(kind, &spec)) {
        result.bits &= spec.mask;
    }
    return result;
}

static bool mmay_value_is_valid(
    MarimayIntegerValue value,
    MarimayIntegerSpec *spec
) {
    return mmay_integer_spec(value.kind, spec) && value.bits <= spec->mask;
}

static bool mmay_prepare_binary(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result,
    MarimayIntegerSpec *spec
) {
    if (result == NULL || left.kind != right.kind
        || !mmay_value_is_valid(left, spec)
        || right.bits > spec->mask) {
        return false;
    }
    return true;
}

static bool mmay_prepare_unary(
    MarimayIntegerValue value,
    MarimayIntegerValue *result,
    MarimayIntegerSpec *spec
) {
    return result != NULL && mmay_value_is_valid(value, spec);
}

static void mmay_set_bits(
    MarimayIntegerValue *result,
    MarimayIntegerKind kind,
    uint64_t bits,
    const MarimayIntegerSpec *spec
) {
    result->kind = kind;
    result->bits = bits & spec->mask;
}

static void mmay_set_signed(
    MarimayIntegerValue *result,
    MarimayIntegerValue input,
    int64_t value,
    const MarimayIntegerSpec *spec
) {
    mmay_set_bits(result, input.kind, mmay_signed_to_bits(value, spec), spec);
}

static uint64_t mmay_arithmetic_shift_right(
    uint64_t bits,
    unsigned int count,
    const MarimayIntegerSpec *spec
) {
    uint64_t shifted = bits >> count;
    if (spec->is_signed && count != 0 && (bits & spec->sign_bit) != 0) {
        shifted |= spec->mask ^ (spec->mask >> count);
    }
    return shifted & spec->mask;
}

static MarimayCheckedIntegerStatus mmay_shift_count(
    MarimayIntegerValue count_value,
    const MarimayIntegerSpec *spec,
    unsigned int *count
) {
    int64_t signed_count = 0;
    if (count == NULL) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    if (spec->is_signed) {
        signed_count = mmay_bits_to_signed(count_value.bits, spec);
        if (signed_count < 0 || signed_count >= (int64_t)spec->width) {
            return MMAY_CHECKED_INTEGER_INVALID_SHIFT;
        }
        *count = (unsigned int)signed_count;
    } else {
        if (count_value.bits >= (uint64_t)spec->width) {
            return MMAY_CHECKED_INTEGER_INVALID_SHIFT;
        }
        *count = (unsigned int)count_value.bits;
    }
    return MMAY_CHECKED_INTEGER_OK;
}

unsigned int mmay_integer_bit_width(MarimayIntegerKind kind) {
    MarimayIntegerSpec spec;
    return mmay_integer_spec(kind, &spec) ? spec.width : 0;
}

bool mmay_integer_kind_is_signed(MarimayIntegerKind kind) {
    MarimayIntegerSpec spec;
    return mmay_integer_spec(kind, &spec) && spec.is_signed;
}

bool mmay_integer_convert(
    MarimayIntegerValue value,
    MarimayIntegerKind target_kind,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec source;
    MarimayIntegerSpec target;
    if (result == NULL || !mmay_value_is_valid(value, &source)
        || !mmay_integer_spec(target_kind, &target)) {
        return false;
    }
    if (source.is_signed) {
        int64_t signed_value = mmay_bits_to_signed(value.bits, &source);
        if (target.is_signed) {
            if (signed_value < target.signed_min
                || signed_value > target.signed_max) {
                return false;
            }
            *result = mmay_integer_make(
                target_kind, mmay_signed_to_bits(signed_value, &target)
            );
            return true;
        }
        if (signed_value < 0 || (uint64_t)signed_value > target.mask) {
            return false;
        }
        *result = mmay_integer_make(target_kind, (uint64_t)signed_value);
        return true;
    }
    if (target.is_signed) {
        if (value.bits > (uint64_t)target.signed_max) {
            return false;
        }
        *result = mmay_integer_make(target_kind, value.bits);
        return true;
    }
    if (value.bits > target.mask) {
        return false;
    }
    *result = mmay_integer_make(target_kind, value.bits);
    return true;
}

MarimayIntegerValue mmay_integer_from_i8(int8_t value) {
    MarimayIntegerSpec spec;
    (void)mmay_integer_spec(MMAY_INTEGER_I8, &spec);
    return mmay_integer_make(
        MMAY_INTEGER_I8,
        mmay_signed_to_bits((int64_t)value, &spec)
    );
}

MarimayIntegerValue mmay_integer_from_i16(int16_t value) {
    MarimayIntegerSpec spec;
    (void)mmay_integer_spec(MMAY_INTEGER_I16, &spec);
    return mmay_integer_make(
        MMAY_INTEGER_I16,
        mmay_signed_to_bits((int64_t)value, &spec)
    );
}

MarimayIntegerValue mmay_integer_from_i32(int32_t value) {
    MarimayIntegerSpec spec;
    (void)mmay_integer_spec(MMAY_INTEGER_I32, &spec);
    return mmay_integer_make(
        MMAY_INTEGER_I32,
        mmay_signed_to_bits((int64_t)value, &spec)
    );
}

MarimayIntegerValue mmay_integer_from_i64(int64_t value) {
    MarimayIntegerSpec spec;
    (void)mmay_integer_spec(MMAY_INTEGER_I64, &spec);
    return mmay_integer_make(
        MMAY_INTEGER_I64,
        mmay_signed_to_bits(value, &spec)
    );
}

MarimayIntegerValue mmay_integer_from_u8(uint8_t value) {
    return mmay_integer_make(MMAY_INTEGER_U8, (uint64_t)value);
}

MarimayIntegerValue mmay_integer_from_u16(uint16_t value) {
    return mmay_integer_make(MMAY_INTEGER_U16, (uint64_t)value);
}

MarimayIntegerValue mmay_integer_from_u32(uint32_t value) {
    return mmay_integer_make(MMAY_INTEGER_U32, (uint64_t)value);
}

MarimayIntegerValue mmay_integer_from_u64(uint64_t value) {
    return mmay_integer_make(MMAY_INTEGER_U64, value);
}

static bool mmay_value_as_i64(MarimayIntegerValue value, int64_t *result) {
    MarimayIntegerSpec spec;
    if (result == NULL || !mmay_value_is_valid(value, &spec)) {
        return false;
    }
    if (spec.is_signed) {
        *result = mmay_bits_to_signed(value.bits, &spec);
        return true;
    }
    if (value.bits > (uint64_t)INT64_MAX) {
        return false;
    }
    *result = (int64_t)value.bits;
    return true;
}

static bool mmay_value_as_u64(MarimayIntegerValue value, uint64_t *result) {
    MarimayIntegerSpec spec;
    int64_t signed_value = 0;
    if (result == NULL || !mmay_value_is_valid(value, &spec)) {
        return false;
    }
    if (!spec.is_signed) {
        *result = value.bits;
        return true;
    }
    signed_value = mmay_bits_to_signed(value.bits, &spec);
    if (signed_value < 0) {
        return false;
    }
    *result = (uint64_t)signed_value;
    return true;
}

bool mmay_integer_to_i8(MarimayIntegerValue value, int8_t *result) {
    int64_t converted = 0;
    if (result == NULL || !mmay_value_as_i64(value, &converted)
        || converted < INT8_MIN || converted > INT8_MAX) {
        return false;
    }
    *result = (int8_t)converted;
    return true;
}

bool mmay_integer_to_i16(MarimayIntegerValue value, int16_t *result) {
    int64_t converted = 0;
    if (result == NULL || !mmay_value_as_i64(value, &converted)
        || converted < INT16_MIN || converted > INT16_MAX) {
        return false;
    }
    *result = (int16_t)converted;
    return true;
}

bool mmay_integer_to_i32(MarimayIntegerValue value, int32_t *result) {
    int64_t converted = 0;
    if (result == NULL || !mmay_value_as_i64(value, &converted)
        || converted < INT32_MIN || converted > INT32_MAX) {
        return false;
    }
    *result = (int32_t)converted;
    return true;
}

bool mmay_integer_to_i64(MarimayIntegerValue value, int64_t *result) {
    return mmay_value_as_i64(value, result);
}

bool mmay_integer_to_u8(MarimayIntegerValue value, uint8_t *result) {
    uint64_t converted = 0;
    if (result == NULL || !mmay_value_as_u64(value, &converted)
        || converted > UINT8_MAX) {
        return false;
    }
    *result = (uint8_t)converted;
    return true;
}

bool mmay_integer_to_u16(MarimayIntegerValue value, uint16_t *result) {
    uint64_t converted = 0;
    if (result == NULL || !mmay_value_as_u64(value, &converted)
        || converted > UINT16_MAX) {
        return false;
    }
    *result = (uint16_t)converted;
    return true;
}

bool mmay_integer_to_u32(MarimayIntegerValue value, uint32_t *result) {
    uint64_t converted = 0;
    if (result == NULL || !mmay_value_as_u64(value, &converted)
        || converted > UINT32_MAX) {
        return false;
    }
    *result = (uint32_t)converted;
    return true;
}

bool mmay_integer_to_u64(MarimayIntegerValue value, uint64_t *result) {
    return mmay_value_as_u64(value, result);
}

MarimayCheckedIntegerStatus mmay_checked_integer_add(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    if (spec.is_signed) {
        int64_t left_value = mmay_bits_to_signed(left.bits, &spec);
        int64_t right_value = mmay_bits_to_signed(right.bits, &spec);
        if ((right_value > 0 && left_value > spec.signed_max - right_value)
            || (right_value < 0 && left_value < spec.signed_min - right_value)) {
            return MMAY_CHECKED_INTEGER_OVERFLOW;
        }
        mmay_set_signed(result, left, left_value + right_value, &spec);
    } else {
        if (left.bits > spec.mask - right.bits) {
            return MMAY_CHECKED_INTEGER_OVERFLOW;
        }
        mmay_set_bits(result, left.kind, left.bits + right.bits, &spec);
    }
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_checked_integer_subtract(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    if (spec.is_signed) {
        int64_t left_value = mmay_bits_to_signed(left.bits, &spec);
        int64_t right_value = mmay_bits_to_signed(right.bits, &spec);
        if ((right_value < 0 && left_value > spec.signed_max + right_value)
            || (right_value > 0 && left_value < spec.signed_min + right_value)) {
            return MMAY_CHECKED_INTEGER_OVERFLOW;
        }
        mmay_set_signed(result, left, left_value - right_value, &spec);
    } else {
        if (left.bits < right.bits) {
            return MMAY_CHECKED_INTEGER_OVERFLOW;
        }
        mmay_set_bits(result, left.kind, left.bits - right.bits, &spec);
    }
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_checked_integer_multiply(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    if (spec.is_signed) {
        int64_t left_value = mmay_bits_to_signed(left.bits, &spec);
        int64_t right_value = mmay_bits_to_signed(right.bits, &spec);
        if (left_value != 0 && right_value != 0) {
            if ((left_value == spec.signed_min && right_value == -1)
                || (right_value == spec.signed_min && left_value == -1)) {
                return MMAY_CHECKED_INTEGER_OVERFLOW;
            }
            if (left_value > 0) {
                if ((right_value > 0
                        && left_value > spec.signed_max / right_value)
                    || (right_value < 0
                        && right_value < spec.signed_min / left_value)) {
                    return MMAY_CHECKED_INTEGER_OVERFLOW;
                }
            } else if ((right_value > 0
                    && left_value < spec.signed_min / right_value)
                || (right_value < 0
                    && left_value < spec.signed_max / right_value)) {
                return MMAY_CHECKED_INTEGER_OVERFLOW;
            }
        }
        mmay_set_signed(result, left, left_value * right_value, &spec);
    } else {
        if (left.bits != 0 && right.bits > spec.mask / left.bits) {
            return MMAY_CHECKED_INTEGER_OVERFLOW;
        }
        mmay_set_bits(result, left.kind, left.bits * right.bits, &spec);
    }
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_checked_integer_divide(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    if (right.bits == 0) {
        return MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO;
    }
    if (spec.is_signed) {
        int64_t left_value = mmay_bits_to_signed(left.bits, &spec);
        int64_t right_value = mmay_bits_to_signed(right.bits, &spec);
        if (left_value == spec.signed_min && right_value == -1) {
            return MMAY_CHECKED_INTEGER_OVERFLOW;
        }
        mmay_set_signed(result, left, left_value / right_value, &spec);
    } else {
        mmay_set_bits(result, left.kind, left.bits / right.bits, &spec);
    }
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_checked_integer_remainder(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    if (right.bits == 0) {
        return MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO;
    }
    if (spec.is_signed) {
        int64_t left_value = mmay_bits_to_signed(left.bits, &spec);
        int64_t right_value = mmay_bits_to_signed(right.bits, &spec);
        if (left_value == spec.signed_min && right_value == -1) {
            return MMAY_CHECKED_INTEGER_OVERFLOW;
        }
        mmay_set_signed(result, left, left_value % right_value, &spec);
    } else {
        mmay_set_bits(result, left.kind, left.bits % right.bits, &spec);
    }
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_checked_integer_negate(
    MarimayIntegerValue value,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    if (!mmay_prepare_unary(value, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    if (spec.is_signed) {
        int64_t signed_value = mmay_bits_to_signed(value.bits, &spec);
        if (signed_value == spec.signed_min) {
            return MMAY_CHECKED_INTEGER_OVERFLOW;
        }
        mmay_set_signed(result, value, -signed_value, &spec);
    } else {
        if (value.bits != 0) {
            return MMAY_CHECKED_INTEGER_OVERFLOW;
        }
        mmay_set_bits(result, value.kind, 0, &spec);
    }
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_checked_integer_shift_left(
    MarimayIntegerValue value,
    MarimayIntegerValue count_value,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus count_status = MMAY_CHECKED_INTEGER_OK;
    unsigned int count = 0;
    uint64_t shifted = 0;
    if (!mmay_prepare_binary(value, count_value, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    count_status = mmay_shift_count(count_value, &spec, &count);
    if (count_status != MMAY_CHECKED_INTEGER_OK) {
        return count_status;
    }
    shifted = (value.bits << count) & spec.mask;
    if (spec.is_signed) {
        if (mmay_arithmetic_shift_right(shifted, count, &spec)
            != value.bits) {
            return MMAY_CHECKED_INTEGER_OVERFLOW;
        }
    } else if (value.bits > (spec.mask >> count)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    mmay_set_bits(result, value.kind, shifted, &spec);
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_checked_integer_shift_right(
    MarimayIntegerValue value,
    MarimayIntegerValue count_value,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus count_status = MMAY_CHECKED_INTEGER_OK;
    unsigned int count = 0;
    if (!mmay_prepare_binary(value, count_value, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    count_status = mmay_shift_count(count_value, &spec, &count);
    if (count_status != MMAY_CHECKED_INTEGER_OK) {
        return count_status;
    }
    mmay_set_bits(
        result,
        value.kind,
        mmay_arithmetic_shift_right(value.bits, count, &spec),
        &spec
    );
    return MMAY_CHECKED_INTEGER_OK;
}

static MarimayCheckedIntegerStatus mmay_wrapping_shift_left_impl(
    MarimayIntegerValue value,
    MarimayIntegerValue count_value,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus count_status = MMAY_CHECKED_INTEGER_OK;
    unsigned int count = 0;
    if (!mmay_prepare_binary(value, count_value, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    count_status = mmay_shift_count(count_value, &spec, &count);
    if (count_status != MMAY_CHECKED_INTEGER_OK) {
        return count_status;
    }
    mmay_set_bits(result, value.kind, value.bits << count, &spec);
    return MMAY_CHECKED_INTEGER_OK;
}

static MarimayCheckedIntegerStatus mmay_wrapping_shift_right_impl(
    MarimayIntegerValue value,
    MarimayIntegerValue count_value,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus count_status = MMAY_CHECKED_INTEGER_OK;
    unsigned int count = 0;
    if (!mmay_prepare_binary(value, count_value, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    count_status = mmay_shift_count(count_value, &spec, &count);
    if (count_status != MMAY_CHECKED_INTEGER_OK) {
        return count_status;
    }
    mmay_set_bits(
        result,
        value.kind,
        mmay_arithmetic_shift_right(value.bits, count, &spec),
        &spec
    );
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_wrapping_integer_add(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    mmay_set_bits(result, left.kind, left.bits + right.bits, &spec);
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_wrapping_integer_subtract(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    mmay_set_bits(result, left.kind, left.bits - right.bits, &spec);
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_wrapping_integer_multiply(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    mmay_set_bits(result, left.kind, left.bits * right.bits, &spec);
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_wrapping_integer_divide(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus status = MMAY_CHECKED_INTEGER_OK;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    status = mmay_checked_integer_divide(left, right, result);
    if (status != MMAY_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    mmay_set_bits(result, left.kind, left.bits, &spec);
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_wrapping_integer_remainder(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus status = MMAY_CHECKED_INTEGER_OK;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    status = mmay_checked_integer_remainder(left, right, result);
    if (status != MMAY_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    mmay_set_bits(result, left.kind, 0, &spec);
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_wrapping_integer_negate(
    MarimayIntegerValue value,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    if (!mmay_prepare_unary(value, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    mmay_set_bits(result, value.kind, UINT64_C(0) - value.bits, &spec);
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_wrapping_integer_shift_left(
    MarimayIntegerValue value,
    MarimayIntegerValue count,
    MarimayIntegerValue *result
) {
    return mmay_wrapping_shift_left_impl(value, count, result);
}

MarimayCheckedIntegerStatus mmay_wrapping_integer_shift_right(
    MarimayIntegerValue value,
    MarimayIntegerValue count,
    MarimayIntegerValue *result
) {
    return mmay_wrapping_shift_right_impl(value, count, result);
}

static void mmay_set_saturating_max(
    MarimayIntegerValue *result,
    MarimayIntegerValue input,
    const MarimayIntegerSpec *spec
) {
    mmay_set_bits(
        result,
        input.kind,
        spec->is_signed ? spec->sign_bit - UINT64_C(1) : spec->mask,
        spec
    );
}

static void mmay_set_saturating_min(
    MarimayIntegerValue *result,
    MarimayIntegerValue input,
    const MarimayIntegerSpec *spec
) {
    mmay_set_bits(
        result,
        input.kind,
        spec->is_signed ? spec->sign_bit : UINT64_C(0),
        spec
    );
}

MarimayCheckedIntegerStatus mmay_saturating_integer_add(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus status = MMAY_CHECKED_INTEGER_OK;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    status = mmay_checked_integer_add(left, right, result);
    if (status != MMAY_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    if (!spec.is_signed
        || mmay_bits_to_signed(left.bits, &spec) >= 0) {
        mmay_set_saturating_max(result, left, &spec);
    } else {
        mmay_set_saturating_min(result, left, &spec);
    }
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_saturating_integer_subtract(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus status = MMAY_CHECKED_INTEGER_OK;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    status = mmay_checked_integer_subtract(left, right, result);
    if (status != MMAY_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    if (!spec.is_signed) {
        mmay_set_saturating_min(result, left, &spec);
    } else if (mmay_bits_to_signed(left.bits, &spec) >= 0
        && mmay_bits_to_signed(right.bits, &spec) < 0) {
        mmay_set_saturating_max(result, left, &spec);
    } else {
        mmay_set_saturating_min(result, left, &spec);
    }
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_saturating_integer_multiply(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus status = MMAY_CHECKED_INTEGER_OK;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    status = mmay_checked_integer_multiply(left, right, result);
    if (status != MMAY_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    if (!spec.is_signed) {
        mmay_set_saturating_max(result, left, &spec);
    } else {
        int64_t left_value = mmay_bits_to_signed(left.bits, &spec);
        int64_t right_value = mmay_bits_to_signed(right.bits, &spec);
        if ((left_value < 0) != (right_value < 0)) {
            mmay_set_saturating_min(result, left, &spec);
        } else {
            mmay_set_saturating_max(result, left, &spec);
        }
    }
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_saturating_integer_divide(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus status = MMAY_CHECKED_INTEGER_OK;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    status = mmay_checked_integer_divide(left, right, result);
    if (status != MMAY_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    mmay_set_saturating_max(result, left, &spec);
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_saturating_integer_remainder(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus status = MMAY_CHECKED_INTEGER_OK;
    if (!mmay_prepare_binary(left, right, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    status = mmay_checked_integer_remainder(left, right, result);
    if (status != MMAY_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    mmay_set_bits(result, left.kind, 0, &spec);
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_saturating_integer_negate(
    MarimayIntegerValue value,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus status = MMAY_CHECKED_INTEGER_OK;
    if (!mmay_prepare_unary(value, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    status = mmay_checked_integer_negate(value, result);
    if (status != MMAY_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    mmay_set_saturating_min(result, value, &spec);
    if (spec.is_signed) {
        mmay_set_saturating_max(result, value, &spec);
    }
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_saturating_integer_shift_left(
    MarimayIntegerValue value,
    MarimayIntegerValue count,
    MarimayIntegerValue *result
) {
    MarimayIntegerSpec spec;
    MarimayCheckedIntegerStatus status = MMAY_CHECKED_INTEGER_OK;
    if (!mmay_prepare_binary(value, count, result, &spec)) {
        return MMAY_CHECKED_INTEGER_OVERFLOW;
    }
    status = mmay_checked_integer_shift_left(value, count, result);
    if (status != MMAY_CHECKED_INTEGER_OVERFLOW) {
        return status;
    }
    if (spec.is_signed && (value.bits & spec.sign_bit) != 0) {
        mmay_set_saturating_min(result, value, &spec);
    } else {
        mmay_set_saturating_max(result, value, &spec);
    }
    return MMAY_CHECKED_INTEGER_OK;
}

MarimayCheckedIntegerStatus mmay_saturating_integer_shift_right(
    MarimayIntegerValue value,
    MarimayIntegerValue count,
    MarimayIntegerValue *result
) {
    return mmay_checked_integer_shift_right(value, count, result);
}
