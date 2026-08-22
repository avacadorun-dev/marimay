#include "mmay/base/f64.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/vector.h"

enum {
    MMAY_F64_MAX_DECIMAL_DIGITS = 1000000
};

typedef struct MarimayF64BigUInt {
    MarimayVector limbs;
    bool initialized;
} MarimayF64BigUInt;

static bool mmay_f64_big_init(MarimayF64BigUInt *value, MarimayAllocator allocator) {
    if (value == NULL) {
        return false;
    }
    memset(value, 0, sizeof(*value));
    value->initialized = mmay_vector_init(
        &value->limbs,
        allocator,
        sizeof(uint32_t)
    );
    return value->initialized;
}

static void mmay_f64_big_dispose(MarimayF64BigUInt *value) {
    if (value != NULL && value->initialized) {
        mmay_vector_dispose(&value->limbs);
    }
    if (value != NULL) {
        memset(value, 0, sizeof(*value));
    }
}

static void mmay_f64_big_normalize(MarimayF64BigUInt *value) {
    while (value != NULL && value->limbs.length != 0) {
        const uint32_t *high = mmay_vector_at_const(
            &value->limbs,
            value->limbs.length - 1
        );
        if (high != NULL && *high != 0) {
            break;
        }
        value->limbs.length -= 1;
    }
}

static bool mmay_f64_big_set_u32(MarimayF64BigUInt *value, uint32_t input) {
    if (value == NULL || !value->initialized) {
        return false;
    }
    value->limbs.length = 0;
    return input == 0 || mmay_vector_push(&value->limbs, &input);
}

static bool mmay_f64_big_copy(
    MarimayF64BigUInt *output,
    const MarimayF64BigUInt *input
) {
    if (output == NULL || input == NULL || !output->initialized
        || !input->initialized) {
        return false;
    }
    output->limbs.length = 0;
    return mmay_vector_append(
        &output->limbs,
        input->limbs.data,
        input->limbs.length
    );
}

static bool mmay_f64_big_multiply_small(
    MarimayF64BigUInt *value,
    uint32_t multiplier
) {
    uint64_t carry = 0;
    size_t index = 0;
    if (value == NULL || !value->initialized) {
        return false;
    }
    for (index = 0; index < value->limbs.length; index += 1) {
        uint32_t *limb = mmay_vector_at(&value->limbs, index);
        uint64_t product = (uint64_t)*limb * multiplier + carry;
        *limb = (uint32_t)product;
        carry = product >> 32;
    }
    if (carry != 0) {
        uint32_t high = (uint32_t)carry;
        return mmay_vector_push(&value->limbs, &high);
    }
    return true;
}

static bool mmay_f64_big_add_small(MarimayF64BigUInt *value, uint32_t addend) {
    uint64_t carry = addend;
    size_t index = 0;
    if (value == NULL || !value->initialized) {
        return false;
    }
    while (carry != 0 && index < value->limbs.length) {
        uint32_t *limb = mmay_vector_at(&value->limbs, index);
        uint64_t sum = (uint64_t)*limb + carry;
        *limb = (uint32_t)sum;
        carry = sum >> 32;
        index += 1;
    }
    if (carry != 0) {
        uint32_t high = (uint32_t)carry;
        return mmay_vector_push(&value->limbs, &high);
    }
    return true;
}

static bool mmay_f64_big_shift_left(
    MarimayF64BigUInt *value,
    size_t count
) {
    MarimayF64BigUInt shifted;
    size_t word_count = count / 32;
    unsigned int bit_count = (unsigned int)(count % 32);
    size_t new_length = 0;
    size_t index = 0;
    bool ok = false;
    memset(&shifted, 0, sizeof(shifted));
    if (value == NULL || !value->initialized || value->limbs.length == 0
        || count == 0) {
        return value != NULL && value->initialized;
    }
    if (!mmay_size_add(value->limbs.length, word_count, &new_length)
        || !mmay_size_add(new_length, bit_count == 0 ? 0 : 1, &new_length)
        || !mmay_f64_big_init(&shifted, value->limbs.allocator)
        || !mmay_vector_resize_zeroed(&shifted.limbs, new_length)) {
        mmay_f64_big_dispose(&shifted);
        return false;
    }
    for (index = 0; index < value->limbs.length; index += 1) {
        const uint32_t *source = mmay_vector_at_const(&value->limbs, index);
        uint32_t *low = mmay_vector_at(&shifted.limbs, index + word_count);
        uint64_t part = (uint64_t)*source << bit_count;
        *low |= (uint32_t)part;
        if (bit_count != 0) {
            uint32_t *high = mmay_vector_at(
                &shifted.limbs,
                index + word_count + 1
            );
            *high |= (uint32_t)(part >> 32);
        }
    }
    mmay_f64_big_normalize(&shifted);
    mmay_vector_dispose(&value->limbs);
    value->limbs = shifted.limbs;
    shifted.initialized = false;
    ok = true;
    mmay_f64_big_dispose(&shifted);
    return ok;
}

static void mmay_f64_big_shift_right_one(MarimayF64BigUInt *value) {
    uint32_t carry = 0;
    size_t index = value->limbs.length;
    while (index != 0) {
        uint32_t *limb = mmay_vector_at(&value->limbs, index - 1);
        uint32_t next = *limb & 1U;
        *limb = (*limb >> 1) | (carry << 31);
        carry = next;
        index -= 1;
    }
    mmay_f64_big_normalize(value);
}

static int mmay_f64_big_compare(
    const MarimayF64BigUInt *left,
    const MarimayF64BigUInt *right
) {
    size_t index = 0;
    if (left->limbs.length != right->limbs.length) {
        return left->limbs.length < right->limbs.length ? -1 : 1;
    }
    index = left->limbs.length;
    while (index != 0) {
        const uint32_t *left_limb = mmay_vector_at_const(
            &left->limbs,
            index - 1
        );
        const uint32_t *right_limb = mmay_vector_at_const(
            &right->limbs,
            index - 1
        );
        if (*left_limb != *right_limb) {
            return *left_limb < *right_limb ? -1 : 1;
        }
        index -= 1;
    }
    return 0;
}

static void mmay_f64_big_subtract(
    MarimayF64BigUInt *left,
    const MarimayF64BigUInt *right
) {
    uint64_t borrow = 0;
    size_t index = 0;
    for (index = 0; index < left->limbs.length; index += 1) {
        uint32_t *left_limb = mmay_vector_at(&left->limbs, index);
        const uint32_t *right_limb = index < right->limbs.length
            ? mmay_vector_at_const(&right->limbs, index)
            : NULL;
        uint64_t subtrahend = (uint64_t)(right_limb == NULL ? 0 : *right_limb)
            + borrow;
        uint64_t original = *left_limb;
        *left_limb = (uint32_t)(original - subtrahend);
        borrow = original < subtrahend ? 1 : 0;
    }
    mmay_f64_big_normalize(left);
}

static size_t mmay_f64_big_bit_length(const MarimayF64BigUInt *value) {
    const uint32_t *high = NULL;
    uint32_t bits = 0;
    if (value == NULL || value->limbs.length == 0) {
        return 0;
    }
    high = mmay_vector_at_const(&value->limbs, value->limbs.length - 1);
    bits = *high;
    {
        size_t result = 0;
        if (!mmay_size_multiply(value->limbs.length - 1, 32, &result)) {
            return SIZE_MAX;
        }
        while (bits != 0) {
            if (result == SIZE_MAX) {
                return SIZE_MAX;
            }
            result += 1;
            bits >>= 1;
        }
        return result;
    }
}

static bool mmay_f64_negative_magnitude(int64_t value, size_t *result) {
    uint64_t magnitude = 0;
    if (value >= 0 || result == NULL) {
        return false;
    }
    magnitude = (uint64_t)(-(value + 1)) + UINT64_C(1);
    if (magnitude > SIZE_MAX) {
        return false;
    }
    *result = (size_t)magnitude;
    return true;
}

static bool mmay_f64_big_compare_shifted(
    const MarimayF64BigUInt *left,
    const MarimayF64BigUInt *right,
    int64_t shift,
    int *result
) {
    MarimayF64BigUInt shifted;
    bool ok = false;
    memset(&shifted, 0, sizeof(shifted));
    if (!mmay_f64_big_init(&shifted, left->limbs.allocator)) {
        return false;
    }
    if (shift >= 0) {
        ok = mmay_f64_big_copy(&shifted, right)
            && mmay_f64_big_shift_left(&shifted, (size_t)shift);
        if (ok) {
            *result = mmay_f64_big_compare(left, &shifted);
        }
    } else {
        size_t magnitude = 0;
        if (!mmay_f64_negative_magnitude(shift, &magnitude)) {
            mmay_f64_big_dispose(&shifted);
            return false;
        }
        ok = mmay_f64_big_copy(&shifted, left)
            && mmay_f64_big_shift_left(&shifted, magnitude);
        if (ok) {
            *result = mmay_f64_big_compare(&shifted, right);
        }
    }
    mmay_f64_big_dispose(&shifted);
    return ok;
}

static bool mmay_f64_big_rounded_quotient(
    const MarimayF64BigUInt *numerator,
    const MarimayF64BigUInt *denominator,
    int64_t binary_shift,
    uint64_t *result
) {
    MarimayF64BigUInt remainder;
    MarimayF64BigUInt divisor;
    MarimayF64BigUInt trial;
    size_t numerator_bits = 0;
    size_t denominator_bits = 0;
    size_t quotient_bits = 0;
    uint64_t quotient = 0;
    bool ok = false;
    memset(&remainder, 0, sizeof(remainder));
    memset(&divisor, 0, sizeof(divisor));
    memset(&trial, 0, sizeof(trial));
    if (!mmay_f64_big_init(&remainder, numerator->limbs.allocator)
        || !mmay_f64_big_init(&divisor, numerator->limbs.allocator)
        || !mmay_f64_big_init(&trial, numerator->limbs.allocator)
        || !mmay_f64_big_copy(&remainder, numerator)
        || !mmay_f64_big_copy(&divisor, denominator)) {
        goto cleanup;
    }
    if (binary_shift >= 0) {
        if (!mmay_f64_big_shift_left(&remainder, (size_t)binary_shift)) {
            goto cleanup;
        }
    } else {
        size_t magnitude = 0;
        if (!mmay_f64_negative_magnitude(binary_shift, &magnitude)
            || !mmay_f64_big_shift_left(&divisor, magnitude)) {
            goto cleanup;
        }
    }
    numerator_bits = mmay_f64_big_bit_length(&remainder);
    denominator_bits = mmay_f64_big_bit_length(&divisor);
    quotient_bits = numerator_bits > denominator_bits
        ? numerator_bits - denominator_bits
        : 0;
    if (numerator_bits == SIZE_MAX || denominator_bits == SIZE_MAX
        || quotient_bits >= 64 || !mmay_f64_big_copy(&trial, &divisor)
        || !mmay_f64_big_shift_left(&trial, quotient_bits)) {
        goto cleanup;
    }
    for (;;) {
        if (mmay_f64_big_compare(&remainder, &trial) >= 0) {
            mmay_f64_big_subtract(&remainder, &trial);
            quotient |= UINT64_C(1) << quotient_bits;
        }
        if (quotient_bits == 0) {
            break;
        }
        mmay_f64_big_shift_right_one(&trial);
        quotient_bits -= 1;
    }
    if (!mmay_f64_big_shift_left(&remainder, 1)) {
        goto cleanup;
    }
    {
        int half = mmay_f64_big_compare(&remainder, &divisor);
        if (half > 0 || (half == 0 && (quotient & UINT64_C(1)) != 0)) {
            quotient += UINT64_C(1);
        }
    }
    *result = quotient;
    ok = true;
cleanup:
    mmay_f64_big_dispose(&trial);
    mmay_f64_big_dispose(&divisor);
    mmay_f64_big_dispose(&remainder);
    return ok;
}

static bool mmay_f64_decimal_digit(unsigned char byte) {
    return byte >= '0' && byte <= '9';
}

MarimayF64ParseStatus mmay_f64_parse_decimal(
    const unsigned char *bytes,
    size_t length,
    MarimayAllocator allocator,
    size_t max_significant_digits,
    uint64_t *result
) {
    MarimayF64BigUInt significand;
    MarimayF64BigUInt numerator;
    MarimayF64BigUInt denominator;
    size_t offset = 0;
    size_t fraction_digits = 0;
    size_t stored_digits = 0;
    size_t decimal_digits = 0;
    size_t trailing_zeros = 0;
    size_t effective_max_digits = 0;
    int64_t explicit_exponent = 0;
    int exponent_sign = 1;
    int64_t decimal_exponent = 0;
    int64_t decimal_order = 0;
    int64_t binary_exponent = 0;
    uint64_t rounded = 0;
    bool seen_dot = false;
    bool seen_exponent = false;
    bool seen_nonzero = false;
    bool previous_digit = false;
    MarimayF64ParseStatus status = MMAY_F64_PARSE_INVALID_SYNTAX;
    memset(&significand, 0, sizeof(significand));
    memset(&numerator, 0, sizeof(numerator));
    memset(&denominator, 0, sizeof(denominator));
    if (bytes == NULL || length == 0 || result == NULL
        || max_significant_digits == 0 || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL) {
        return MMAY_F64_PARSE_INVALID_ARGUMENT;
    }
    effective_max_digits = max_significant_digits;
    if (effective_max_digits > MMAY_F64_MAX_DECIMAL_DIGITS) {
        effective_max_digits = MMAY_F64_MAX_DECIMAL_DIGITS;
    }
    if (!mmay_f64_big_init(&significand, allocator)
        || !mmay_f64_big_init(&numerator, allocator)
        || !mmay_f64_big_init(&denominator, allocator)
        || !mmay_f64_big_set_u32(&significand, 0)) {
        status = MMAY_F64_PARSE_ALLOCATION_FAILED;
        goto cleanup;
    }
    while (offset < length) {
        unsigned char byte = bytes[offset];
        if (mmay_f64_decimal_digit(byte)) {
            uint32_t digit = (uint32_t)(byte - '0');
            previous_digit = true;
            if (seen_exponent) {
                if (explicit_exponent < INT64_C(1000000)) {
                    explicit_exponent = explicit_exponent * 10 + digit;
                    if (explicit_exponent > INT64_C(1000000)) {
                        explicit_exponent = INT64_C(1000000);
                    }
                }
            } else {
                if (seen_dot) {
                    if (fraction_digits == SIZE_MAX) {
                        status = MMAY_F64_PARSE_LIMIT_REACHED;
                        goto cleanup;
                    }
                    fraction_digits += 1;
                }
                if (seen_nonzero || digit != 0) {
                    seen_nonzero = true;
                    if (decimal_digits == SIZE_MAX
                        || stored_digits == SIZE_MAX) {
                        status = MMAY_F64_PARSE_LIMIT_REACHED;
                        goto cleanup;
                    }
                    decimal_digits += 1;
                    stored_digits += 1;
                    if (stored_digits > effective_max_digits) {
                        status = MMAY_F64_PARSE_LIMIT_REACHED;
                        goto cleanup;
                    }
                    if (!mmay_f64_big_multiply_small(&significand, 10)
                        || !mmay_f64_big_add_small(&significand, digit)) {
                        status = MMAY_F64_PARSE_ALLOCATION_FAILED;
                        goto cleanup;
                    }
                    if (digit == 0) {
                        if (trailing_zeros == SIZE_MAX) {
                            status = MMAY_F64_PARSE_LIMIT_REACHED;
                            goto cleanup;
                        }
                        trailing_zeros += 1;
                    } else {
                        trailing_zeros = 0;
                    }
                }
            }
            offset += 1;
            continue;
        }
        if (byte == '_') {
            if (!previous_digit || offset + 1 >= length
                || !mmay_f64_decimal_digit(bytes[offset + 1])) {
                goto cleanup;
            }
            previous_digit = false;
            offset += 1;
            continue;
        }
        if (byte == '.' && !seen_dot && !seen_exponent && previous_digit
            && offset + 1 < length
            && mmay_f64_decimal_digit(bytes[offset + 1])) {
            seen_dot = true;
            previous_digit = false;
            offset += 1;
            continue;
        }
        if ((byte == 'e' || byte == 'E') && !seen_exponent
            && previous_digit) {
            seen_exponent = true;
            previous_digit = false;
            offset += 1;
            if (offset < length && (bytes[offset] == '+' || bytes[offset] == '-')) {
                exponent_sign = bytes[offset] == '-' ? -1 : 1;
                offset += 1;
            }
            continue;
        }
        goto cleanup;
    }
    if (!previous_digit || (!seen_dot && !seen_exponent)) {
        goto cleanup;
    }
    if (!seen_nonzero) {
        *result = 0;
        status = MMAY_F64_PARSE_OK;
        goto cleanup;
    }
    explicit_exponent *= exponent_sign;
    if (fraction_digits > (size_t)INT64_MAX) {
        status = MMAY_F64_PARSE_LIMIT_REACHED;
        goto cleanup;
    }
    if (explicit_exponent
        <= INT64_MIN + (int64_t)fraction_digits) {
        status = MMAY_F64_PARSE_LIMIT_REACHED;
        goto cleanup;
    }
    decimal_exponent = explicit_exponent - (int64_t)fraction_digits;
    if (trailing_zeros != 0) {
        size_t index = 0;
        if (trailing_zeros > (size_t)INT64_MAX
            || decimal_exponent > INT64_MAX - (int64_t)trailing_zeros) {
            status = MMAY_F64_PARSE_LIMIT_REACHED;
            goto cleanup;
        }
        decimal_exponent += (int64_t)trailing_zeros;
        decimal_digits -= trailing_zeros;
        if (!mmay_f64_big_set_u32(&significand, 0)) {
            status = MMAY_F64_PARSE_ALLOCATION_FAILED;
            goto cleanup;
        }
        seen_nonzero = false;
        stored_digits = decimal_digits;
        for (index = 0; index < length && stored_digits != 0; index += 1) {
            unsigned char byte = bytes[index];
            if (byte == 'e' || byte == 'E') {
                break;
            }
            if (mmay_f64_decimal_digit(byte)) {
                uint32_t digit = (uint32_t)(byte - '0');
                if (seen_nonzero || digit != 0) {
                    seen_nonzero = true;
                    if (!mmay_f64_big_multiply_small(&significand, 10)
                        || !mmay_f64_big_add_small(&significand, digit)) {
                        status = MMAY_F64_PARSE_ALLOCATION_FAILED;
                        goto cleanup;
                    }
                    stored_digits -= 1;
                }
            }
        }
    }
    if (decimal_digits == 0 || decimal_digits > (size_t)INT64_MAX
        || decimal_exponent > INT64_MAX - ((int64_t)decimal_digits - 1)) {
        status = MMAY_F64_PARSE_LIMIT_REACHED;
        goto cleanup;
    }
    decimal_order = decimal_exponent + ((int64_t)decimal_digits - 1);
    if (decimal_order > 308) {
        *result = UINT64_C(0x7ff0000000000000);
        status = MMAY_F64_PARSE_OK;
        goto cleanup;
    }
    if (decimal_order < -324) {
        *result = 0;
        status = MMAY_F64_PARSE_OK;
        goto cleanup;
    }
    if (!mmay_f64_big_copy(&numerator, &significand)
        || !mmay_f64_big_set_u32(&denominator, 1)) {
        status = MMAY_F64_PARSE_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (decimal_exponent >= 0) {
        int64_t index = 0;
        for (index = 0; index < decimal_exponent; index += 1) {
            if (!mmay_f64_big_multiply_small(&numerator, 5)) {
                status = MMAY_F64_PARSE_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    } else {
        int64_t index = 0;
        for (index = 0; index < -decimal_exponent; index += 1) {
            if (!mmay_f64_big_multiply_small(&denominator, 5)) {
                status = MMAY_F64_PARSE_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    }
    {
        size_t numerator_bits = mmay_f64_big_bit_length(&numerator);
        size_t denominator_bits = mmay_f64_big_bit_length(&denominator);
        int64_t ratio_exponent = 0;
        int comparison = 0;
        if (numerator_bits == SIZE_MAX || denominator_bits == SIZE_MAX
            || numerator_bits > (size_t)INT64_MAX
            || denominator_bits > (size_t)INT64_MAX) {
            status = MMAY_F64_PARSE_LIMIT_REACHED;
            goto cleanup;
        }
        ratio_exponent = (int64_t)numerator_bits
            - (int64_t)denominator_bits;
        if (!mmay_f64_big_compare_shifted(
                &numerator,
                &denominator,
                ratio_exponent,
                &comparison
            )) {
            status = MMAY_F64_PARSE_ALLOCATION_FAILED;
            goto cleanup;
        }
        if (comparison < 0) {
            if (ratio_exponent == INT64_MIN) {
                status = MMAY_F64_PARSE_LIMIT_REACHED;
                goto cleanup;
            }
            ratio_exponent -= 1;
        }
        if ((decimal_exponent > 0
                && ratio_exponent > INT64_MAX - decimal_exponent)
            || (decimal_exponent < 0
                && ratio_exponent < INT64_MIN - decimal_exponent)) {
            status = MMAY_F64_PARSE_LIMIT_REACHED;
            goto cleanup;
        }
        binary_exponent = ratio_exponent + decimal_exponent;
    }
    if (binary_exponent > 1023) {
        *result = UINT64_C(0x7ff0000000000000);
        status = MMAY_F64_PARSE_OK;
        goto cleanup;
    }
    if (binary_exponent < -1075) {
        *result = 0;
        status = MMAY_F64_PARSE_OK;
        goto cleanup;
    }
    if (binary_exponent >= -1022) {
        int64_t shift = 0;
        if ((binary_exponent > 0
                && decimal_exponent < INT64_MIN + binary_exponent)
            || (binary_exponent < 0
                && decimal_exponent > INT64_MAX + binary_exponent)) {
            status = MMAY_F64_PARSE_LIMIT_REACHED;
            goto cleanup;
        }
        shift = decimal_exponent - binary_exponent;
        if (shift > INT64_MAX - 52) {
            status = MMAY_F64_PARSE_LIMIT_REACHED;
            goto cleanup;
        }
        shift += 52;
        if (!mmay_f64_big_rounded_quotient(
                &numerator,
                &denominator,
                shift,
                &rounded
            )) {
            status = MMAY_F64_PARSE_ALLOCATION_FAILED;
            goto cleanup;
        }
        if (rounded == (UINT64_C(1) << 53)) {
            rounded >>= 1;
            binary_exponent += 1;
        }
        if (binary_exponent > 1023) {
            *result = UINT64_C(0x7ff0000000000000);
        } else {
            *result = ((uint64_t)(binary_exponent + 1023) << 52)
                | (rounded & ((UINT64_C(1) << 52) - 1));
        }
    } else {
        int64_t shift = 0;
        if (decimal_exponent > INT64_MAX - 1074) {
            status = MMAY_F64_PARSE_LIMIT_REACHED;
            goto cleanup;
        }
        shift = decimal_exponent + 1074;
        if (!mmay_f64_big_rounded_quotient(
                &numerator,
                &denominator,
                shift,
                &rounded
            )) {
            status = MMAY_F64_PARSE_ALLOCATION_FAILED;
            goto cleanup;
        }
        *result = rounded;
    }
    status = MMAY_F64_PARSE_OK;
cleanup:
    mmay_f64_big_dispose(&denominator);
    mmay_f64_big_dispose(&numerator);
    mmay_f64_big_dispose(&significand);
    return status;
}
