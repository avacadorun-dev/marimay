#include "ruda/base/number_format.h"

#include <stdbool.h>
#include <string.h>


/* 5^324 plus the largest binary shift used by either IEEE width fits here. */
enum {
    RUDA_NUMBER_FORMAT_LIMBS = 128,
    RUDA_NUMBER_FORMAT_OUTPUT = 32
};

typedef struct RudaNumberBig {
    uint32_t limbs[RUDA_NUMBER_FORMAT_LIMBS];
    size_t length;
} RudaNumberBig;

typedef struct RudaNumberProfile {
    unsigned fraction_bits;
    unsigned exponent_bits;
    unsigned precision;
    int exponent_bias;
    int minimum_binary_exponent;
    int maximum_binary_exponent;
    unsigned maximum_digits;
} RudaNumberProfile;

typedef struct RudaNumberDecoded {
    uint64_t significand;
    int exponent;
} RudaNumberDecoded;

typedef struct RudaNumberBoundary {
    RudaNumberBig numerator;
    int exponent;
} RudaNumberBoundary;

typedef struct RudaNumberBounds {
    RudaNumberBoundary lower;
    RudaNumberBoundary upper;
} RudaNumberBounds;

typedef struct RudaNumberCandidate {
    uint64_t digits;
    int exponent;
} RudaNumberCandidate;

static const RudaNumberProfile ruda_number_f32_profile = {
    23u, 8u, 24u, 127, -149, 128, 9u
};

static const RudaNumberProfile ruda_number_f64_profile = {
    52u, 11u, 53u, 1023, -1074, 1024, 17u
};

static void ruda_number_big_zero(RudaNumberBig *value) {
    if (value != NULL) {
        value->length = 0;
    }
}

static void ruda_number_big_normalize(RudaNumberBig *value) {
    if (value == NULL) {
        return;
    }
    while (value->length != 0
        && value->limbs[value->length - 1] == 0) {
        value->length -= 1;
    }
}

static bool ruda_number_big_from_u64(
    RudaNumberBig *value,
    uint64_t input
) {
    if (value == NULL) {
        return false;
    }
    ruda_number_big_zero(value);
    if (input == 0) {
        return true;
    }
    value->limbs[0] = (uint32_t)input;
    value->length = 1;
    if (input > UINT32_MAX) {
        value->limbs[1] = (uint32_t)(input >> 32);
        value->length = 2;
    }
    return true;
}

static bool ruda_number_big_copy(
    RudaNumberBig *destination,
    const RudaNumberBig *source
) {
    size_t index = 0;
    if (destination == NULL || source == NULL) {
        return false;
    }
    destination->length = source->length;
    for (index = 0; index < source->length; index += 1) {
        destination->limbs[index] = source->limbs[index];
    }
    return true;
}

static bool ruda_number_big_shift_left(
    RudaNumberBig *value,
    unsigned shift
) {
    size_t word_shift = shift / 32u;
    unsigned bit_shift = shift % 32u;
    size_t old_length = 0;
    size_t index = 0;
    uint32_t carry = 0;
    RudaNumberBig shifted_value;
    if (value == NULL || value->length == 0) {
        return true;
    }
    old_length = value->length;
    if (word_shift >= RUDA_NUMBER_FORMAT_LIMBS
        || old_length + word_shift > RUDA_NUMBER_FORMAT_LIMBS) {
        return false;
    }
    if (bit_shift == 0) {
        for (index = old_length; index != 0; index -= 1) {
            value->limbs[index - 1 + word_shift] = value->limbs[index - 1];
        }
        for (index = 0; index < word_shift; index += 1) {
            value->limbs[index] = 0;
        }
        value->length = old_length + word_shift;
        return true;
    }
    if (old_length + word_shift + 1 > RUDA_NUMBER_FORMAT_LIMBS) {
        return false;
    }
    ruda_number_big_zero(&shifted_value);
    shifted_value.length = old_length + word_shift;
    for (index = 0; index < shifted_value.length; index += 1) {
        shifted_value.limbs[index] = 0;
    }
    for (index = 0; index < old_length; index += 1) {
        uint64_t shifted = ((uint64_t)value->limbs[index] << bit_shift)
            | carry;
        shifted_value.limbs[index + word_shift] = (uint32_t)shifted;
        carry = (uint32_t)(shifted >> 32);
    }
    if (carry != 0) {
        shifted_value.limbs[shifted_value.length] = carry;
        shifted_value.length += 1;
    }
    value->length = shifted_value.length;
    for (index = 0; index < value->length; index += 1) {
        value->limbs[index] = shifted_value.limbs[index];
    }
    return true;
}

static bool ruda_number_big_add(
    RudaNumberBig *destination,
    const RudaNumberBig *source
) {
    size_t index = 0;
    size_t maximum = 0;
    uint64_t carry = 0;
    if (destination == NULL || source == NULL) {
        return false;
    }
    maximum = destination->length > source->length
        ? destination->length : source->length;
    for (index = 0; index < maximum; index += 1) {
        uint64_t left = index < destination->length
            ? destination->limbs[index] : 0;
        uint64_t right = index < source->length ? source->limbs[index] : 0;
        uint64_t sum = left + right + carry;
        destination->limbs[index] = (uint32_t)sum;
        carry = sum >> 32;
    }
    destination->length = maximum;
    if (carry != 0) {
        if (destination->length == RUDA_NUMBER_FORMAT_LIMBS) {
            return false;
        }
        destination->limbs[destination->length] = (uint32_t)carry;
        destination->length += 1;
    }
    return true;
}

static bool ruda_number_big_add_small(
    RudaNumberBig *value,
    uint32_t addend
) {
    size_t index = 0;
    uint64_t carry = addend;
    if (value == NULL) {
        return false;
    }
    while (carry != 0 && index < value->length) {
        uint64_t sum = (uint64_t)value->limbs[index] + carry;
        value->limbs[index] = (uint32_t)sum;
        carry = sum >> 32;
        index += 1;
    }
    if (carry != 0) {
        if (value->length == RUDA_NUMBER_FORMAT_LIMBS) {
            return false;
        }
        value->limbs[value->length] = (uint32_t)carry;
        value->length += 1;
    }
    return true;
}

static int ruda_number_big_compare(
    const RudaNumberBig *left,
    const RudaNumberBig *right
) {
    size_t index = 0;
    if (left->length != right->length) {
        return left->length < right->length ? -1 : 1;
    }
    index = left->length;
    while (index != 0) {
        index -= 1;
        if (left->limbs[index] != right->limbs[index]) {
            return left->limbs[index] < right->limbs[index] ? -1 : 1;
        }
    }
    return 0;
}

static bool ruda_number_big_subtract(
    RudaNumberBig *destination,
    const RudaNumberBig *source
) {
    size_t index = 0;
    uint64_t borrow = 0;
    if (destination == NULL || source == NULL
        || ruda_number_big_compare(destination, source) < 0) {
        return false;
    }
    for (index = 0; index < destination->length; index += 1) {
        uint64_t left = destination->limbs[index];
        uint64_t right = index < source->length ? source->limbs[index] : 0;
        uint64_t subtrahend = right + borrow;
        destination->limbs[index] = (uint32_t)(left - subtrahend);
        borrow = left < subtrahend ? 1u : 0u;
    }
    ruda_number_big_normalize(destination);
    return borrow == 0;
}

static bool ruda_number_big_multiply_small(
    RudaNumberBig *value,
    uint32_t multiplier
) {
    size_t index = 0;
    uint64_t carry = 0;
    if (value == NULL) {
        return false;
    }
    for (index = 0; index < value->length; index += 1) {
        uint64_t product = (uint64_t)value->limbs[index] * multiplier + carry;
        value->limbs[index] = (uint32_t)product;
        carry = product >> 32;
    }
    if (carry != 0) {
        if (value->length == RUDA_NUMBER_FORMAT_LIMBS) {
            return false;
        }
        value->limbs[value->length] = (uint32_t)carry;
        value->length += 1;
    }
    return true;
}

static unsigned ruda_number_big_bit_length(const RudaNumberBig *value) {
    uint32_t high = 0;
    unsigned bits = 0;
    if (value == NULL || value->length == 0) {
        return 0;
    }
    high = value->limbs[value->length - 1];
    bits = 32u;
    while ((high & UINT32_C(0x80000000)) == 0) {
        high <<= 1;
        bits -= 1;
    }
    return (unsigned)((value->length - 1) * 32u) + bits;
}

static unsigned ruda_number_big_get_bit(
    const RudaNumberBig *value,
    unsigned bit
) {
    size_t limb = bit / 32u;
    if (value == NULL || limb >= value->length) {
        return 0;
    }
    return (value->limbs[limb] >> (bit % 32u)) & 1u;
}

static bool ruda_number_big_power5(
    RudaNumberBig *value,
    unsigned exponent
) {
    unsigned index = 0;
    if (!ruda_number_big_from_u64(value, 1)) {
        return false;
    }
    for (index = 0; index < exponent; index += 1) {
        if (!ruda_number_big_multiply_small(value, 5)) {
            return false;
        }
    }
    return true;
}

static bool ruda_number_big_multiply_power5(
    RudaNumberBig *value,
    unsigned exponent
) {
    unsigned index = 0;
    if (value == NULL) {
        return false;
    }
    for (index = 0; index < exponent; index += 1) {
        if (!ruda_number_big_multiply_small(value, 5)) {
            return false;
        }
    }
    return true;
}

static int ruda_number_big_compare_scaled(
    const RudaNumberBig *left,
    int left_exponent,
    const RudaNumberBig *right,
    int right_exponent
) {
    RudaNumberBig left_scaled;
    RudaNumberBig right_scaled;
    int common_exponent = left_exponent < right_exponent
        ? left_exponent : right_exponent;
    if (!ruda_number_big_copy(&left_scaled, left)
        || !ruda_number_big_copy(&right_scaled, right)) {
        return 0;
    }
    if (!ruda_number_big_shift_left(
        &left_scaled,
        (unsigned)(left_exponent - common_exponent)
    ) || !ruda_number_big_shift_left(
        &right_scaled,
        (unsigned)(right_exponent - common_exponent)
    )) {
        return 0;
    }
    return ruda_number_big_compare(&left_scaled, &right_scaled);
}

static bool ruda_number_big_divide(
    const RudaNumberBig *numerator,
    const RudaNumberBig *denominator,
    RudaNumberBig *quotient,
    RudaNumberBig *remainder
) {
    unsigned bit = 0;
    unsigned bits = 0;
    if (numerator == NULL || denominator == NULL || quotient == NULL
        || remainder == NULL || denominator->length == 0) {
        return false;
    }
    bits = ruda_number_big_bit_length(numerator);
    ruda_number_big_zero(quotient);
    ruda_number_big_zero(remainder);
    while (bits != 0) {
        bit = bits - 1;
        if (!ruda_number_big_shift_left(remainder, 1)
            || !ruda_number_big_add_small(
                remainder,
                ruda_number_big_get_bit(numerator, bit)
            ) || !ruda_number_big_shift_left(quotient, 1)) {
            return false;
        }
        if (ruda_number_big_compare(remainder, denominator) >= 0) {
            if (!ruda_number_big_subtract(remainder, denominator)
                || !ruda_number_big_add_small(quotient, 1)) {
                return false;
            }
        }
        bits -= 1;
    }
    return true;
}

static bool ruda_number_big_to_u64(
    const RudaNumberBig *value,
    uint64_t *result
) {
    if (value == NULL || result == NULL || value->length > 2) {
        return false;
    }
    *result = value->length == 0 ? 0 : value->limbs[0];
    if (value->length == 2) {
        *result |= (uint64_t)value->limbs[1] << 32;
    }
    return true;
}

static bool ruda_number_decode(
    uint64_t raw_bits,
    const RudaNumberProfile *profile,
    RudaNumberDecoded *decoded,
    bool *negative,
    bool *infinity,
    bool *nan
) {
    uint64_t fraction_mask = (UINT64_C(1) << profile->fraction_bits) - 1;
    uint64_t exponent_mask = (UINT64_C(1) << profile->exponent_bits) - 1;
    uint64_t fraction = raw_bits & fraction_mask;
    uint64_t exponent = (raw_bits >> profile->fraction_bits) & exponent_mask;
    if (decoded == NULL || negative == NULL || infinity == NULL || nan == NULL) {
        return false;
    }
    *negative = (raw_bits >> (profile->fraction_bits + profile->exponent_bits))
        != 0;
    *infinity = exponent == exponent_mask && fraction == 0;
    *nan = exponent == exponent_mask && fraction != 0;
    decoded->significand = fraction;
    decoded->exponent = profile->minimum_binary_exponent;
    if (exponent != 0) {
        decoded->significand |= UINT64_C(1) << profile->fraction_bits;
        decoded->exponent = (int)exponent - profile->exponent_bias
            - (int)profile->fraction_bits;
    }
    return true;
}

static bool ruda_number_value_compare_power10(
    const RudaNumberDecoded *value,
    int power,
    int *comparison
) {
    RudaNumberBig left;
    RudaNumberBig right;
    if (value == NULL || comparison == NULL) {
        return false;
    }
    if (power >= 0) {
        if (!ruda_number_big_from_u64(&left, value->significand)
            || !ruda_number_big_power5(&right, (unsigned)power)) {
            return false;
        }
        *comparison = ruda_number_big_compare_scaled(
            &left,
            value->exponent,
            &right,
            power
        );
        return true;
    }
    if (!ruda_number_big_from_u64(&left, value->significand)
        || !ruda_number_big_multiply_power5(&left, (unsigned)(-power))
        || !ruda_number_big_from_u64(&right, 1)) {
        return false;
    }
    *comparison = ruda_number_big_compare_scaled(
        &left,
        value->exponent - power,
        &right,
        0
    );
    return true;
}

static bool ruda_number_find_decimal_exponent(
    const RudaNumberDecoded *value,
    int *decimal_exponent
) {
    RudaNumberBig significand;
    int binary_exponent = 0;
    int estimate = 0;
    int comparison = 0;
    if (value == NULL || decimal_exponent == NULL
        || value->significand == 0
        || !ruda_number_big_from_u64(&significand, value->significand)) {
        return false;
    }
    binary_exponent = value->exponent
        + (int)ruda_number_big_bit_length(&significand) - 1;
    if (binary_exponent >= 0) {
        estimate = (binary_exponent * 1233) / 4096;
    } else {
        estimate = -(((-binary_exponent) * 1233 + 4095) / 4096);
    }
    if (!ruda_number_value_compare_power10(value, estimate, &comparison)) {
        return false;
    }
    while (comparison < 0) {
        estimate -= 1;
        if (!ruda_number_value_compare_power10(value, estimate, &comparison)) {
            return false;
        }
    }
    while (true) {
        int next_comparison = 0;
        if (!ruda_number_value_compare_power10(
            value, estimate + 1, &next_comparison
        )) {
            return false;
        }
        if (next_comparison < 0) {
            break;
        }
        estimate += 1;
    }
    *decimal_exponent = estimate;
    return true;
}

static bool ruda_number_make_midpoint(
    RudaNumberBoundary *boundary,
    const RudaNumberDecoded *left,
    const RudaNumberDecoded *right
) {
    RudaNumberBig left_value;
    RudaNumberBig right_value;
    int common_exponent = 0;
    if (boundary == NULL || left == NULL || right == NULL) {
        return false;
    }
    common_exponent = left->exponent < right->exponent
        ? left->exponent : right->exponent;
    if (!ruda_number_big_from_u64(&left_value, left->significand)
        || !ruda_number_big_from_u64(&right_value, right->significand)
        || !ruda_number_big_shift_left(
            &left_value,
            (unsigned)(left->exponent - common_exponent)
        )
        || !ruda_number_big_shift_left(
            &right_value,
            (unsigned)(right->exponent - common_exponent)
        )
        || !ruda_number_big_add(&left_value, &right_value)) {
        return false;
    }
    boundary->numerator = left_value;
    boundary->exponent = common_exponent - 1;
    return true;
}

static bool ruda_number_make_bounds(
    uint64_t raw_bits,
    const RudaNumberProfile *profile,
    const RudaNumberDecoded *value,
    RudaNumberBounds *bounds
) {
    RudaNumberDecoded previous;
    RudaNumberDecoded next;
    uint64_t magnitude_mask = 0;
    uint64_t magnitude = 0;
    uint64_t previous_bits = 0;
    uint64_t next_bits = 0;
    uint64_t maximum = 0;
    bool ignored_negative = false;
    bool ignored_infinity = false;
    bool ignored_nan = false;
    if (value == NULL || profile == NULL || bounds == NULL) {
        return false;
    }
    if (profile->fraction_bits + profile->exponent_bits == 64) {
        magnitude_mask = UINT64_MAX;
    } else {
        magnitude_mask = (UINT64_C(1)
            << (profile->fraction_bits + profile->exponent_bits)) - 1;
    }
    magnitude = raw_bits & magnitude_mask;
    maximum = (((UINT64_C(1) << profile->exponent_bits) - 2)
        << profile->fraction_bits)
        | ((UINT64_C(1) << profile->fraction_bits) - 1);
    previous_bits = magnitude - 1;
    next_bits = magnitude + 1;
    if (value->significand == 1
        && value->exponent == profile->minimum_binary_exponent) {
        previous.significand = 0;
        previous.exponent = value->exponent;
    } else if (!ruda_number_decode(
        previous_bits,
        profile,
        &previous,
        &ignored_negative,
        &ignored_infinity,
        &ignored_nan
    )) {
        return false;
    }
    if (magnitude == maximum) {
        next.significand = 1;
        next.exponent = profile->maximum_binary_exponent;
    } else if (!ruda_number_decode(
        next_bits,
        profile,
        &next,
        &ignored_negative,
        &ignored_infinity,
        &ignored_nan
    )) {
        return false;
    }
    return ruda_number_make_midpoint(&bounds->lower, &previous, value)
        && ruda_number_make_midpoint(&bounds->upper, value, &next);
}

static bool ruda_number_build_scaled_quotient(
    const RudaNumberDecoded *value,
    int decimal_power,
    RudaNumberBig *numerator,
    RudaNumberBig *denominator
) {
    RudaNumberBig power5;
    int shifted_exponent = 0;
    if (value == NULL || numerator == NULL || denominator == NULL) {
        return false;
    }
    if (decimal_power >= 0) {
        if (!ruda_number_big_from_u64(numerator, value->significand)
            || !ruda_number_big_power5(&power5, (unsigned)decimal_power)
            || !ruda_number_big_copy(denominator, &power5)) {
            return false;
        }
        if (value->exponent >= decimal_power) {
            return ruda_number_big_shift_left(
                numerator,
                (unsigned)(value->exponent - decimal_power)
            );
        }
        return ruda_number_big_shift_left(
            denominator,
            (unsigned)(decimal_power - value->exponent)
        );
    }
    if (!ruda_number_big_from_u64(numerator, value->significand)
        || !ruda_number_big_multiply_power5(
            numerator, (unsigned)(-decimal_power)
        )) {
        return false;
    }
    shifted_exponent = value->exponent - decimal_power;
    if (shifted_exponent >= 0) {
        ruda_number_big_zero(denominator);
        return ruda_number_big_shift_left(
            numerator,
            (unsigned)shifted_exponent
        );
    }
    if (!ruda_number_big_from_u64(denominator, 1)) {
        return false;
    }
    return ruda_number_big_shift_left(
        denominator,
        (unsigned)(-shifted_exponent)
    );
}

static bool ruda_number_decimal_floor(
    const RudaNumberDecoded *value,
    int decimal_power,
    uint64_t *floor_value
) {
    RudaNumberBig numerator;
    RudaNumberBig denominator;
    RudaNumberBig quotient;
    RudaNumberBig remainder;
    if (!ruda_number_build_scaled_quotient(
        value, decimal_power, &numerator, &denominator
    ) || !ruda_number_big_divide(
        &numerator, &denominator, &quotient, &remainder
    )) {
        return false;
    }
    return ruda_number_big_to_u64(&quotient, floor_value);
}

static int ruda_number_decimal_compare_value(
    const RudaNumberDecoded *value,
    uint64_t digits,
    int decimal_power
) {
    RudaNumberBig left;
    RudaNumberBig right;
    if (decimal_power >= 0) {
        if (!ruda_number_big_from_u64(&left, digits)
            || !ruda_number_big_multiply_power5(
                &left, (unsigned)decimal_power
            )
            || !ruda_number_big_from_u64(&right, value->significand)) {
            return 0;
        }
        /* Return exact value versus digits * 10^q. */
        return ruda_number_big_compare_scaled(
            &right, value->exponent, &left, decimal_power
        );
    }
    if (!ruda_number_big_from_u64(&left, digits)
        || !ruda_number_big_from_u64(&right, value->significand)
        || !ruda_number_big_multiply_power5(
            &right, (unsigned)(-decimal_power)
        )) {
        return 0;
    }
    /* Return value * 5^(-q) versus digits * 2^q. */
    return ruda_number_big_compare_scaled(
        &right,
        value->exponent,
        &left,
        decimal_power
    );
}

static int ruda_number_decimal_compare_boundary(
    uint64_t digits,
    int decimal_power,
    const RudaNumberBoundary *boundary
) {
    RudaNumberBig decimal;
    RudaNumberBig boundary_value;
    if (boundary == NULL) {
        return 0;
    }
    if (decimal_power >= 0) {
        if (!ruda_number_big_from_u64(&decimal, digits)
            || !ruda_number_big_multiply_power5(
                &decimal, (unsigned)decimal_power
            )
            || !ruda_number_big_copy(&boundary_value, &boundary->numerator)) {
            return 0;
        }
        return ruda_number_big_compare_scaled(
            &decimal,
            decimal_power,
            &boundary_value,
            boundary->exponent
        );
    }
    if (!ruda_number_big_from_u64(&decimal, digits)
        || !ruda_number_big_copy(&boundary_value, &boundary->numerator)
        || !ruda_number_big_multiply_power5(
            &boundary_value, (unsigned)(-decimal_power)
        )) {
        return 0;
    }
    /* Multiply both sides by 5^(-q). */
    return ruda_number_big_compare_scaled(
        &decimal,
        decimal_power,
        &boundary_value,
        boundary->exponent
    );
}

static bool ruda_number_candidate_valid(
    uint64_t digits,
    int decimal_power,
    const RudaNumberBounds *bounds,
    bool odd
) {
    int lower_comparison = 0;
    int upper_comparison = 0;
    if (digits == 0 || bounds == NULL) {
        return false;
    }
    lower_comparison = ruda_number_decimal_compare_boundary(
        digits, decimal_power, &bounds->lower
    );
    upper_comparison = ruda_number_decimal_compare_boundary(
        digits, decimal_power, &bounds->upper
    );
    if (lower_comparison < 0 || upper_comparison > 0) {
        return false;
    }
    if (odd && (lower_comparison == 0 || upper_comparison == 0)) {
        return false;
    }
    return true;
}

static unsigned ruda_number_decimal_digits(uint64_t value) {
    unsigned digits = 1;
    while (value >= 10) {
        value /= 10;
        digits += 1;
    }
    return digits;
}

static uint64_t ruda_number_power10_u64(unsigned exponent) {
    uint64_t value = 1;
    unsigned index = 0;
    for (index = 0; index < exponent; index += 1) {
        value *= 10;
    }
    return value;
}

static bool ruda_number_normalize_candidate(
    uint64_t digits,
    int decimal_power,
    unsigned requested_digits,
    RudaNumberCandidate *candidate
) {
    uint64_t lower = ruda_number_power10_u64(requested_digits - 1);
    uint64_t upper = ruda_number_power10_u64(requested_digits);
    if (candidate == NULL || digits < lower || digits > upper) {
        return false;
    }
    if (digits == upper) {
        digits /= 10;
        decimal_power += 1;
    }
    while (digits != 0 && digits % 10 == 0) {
        digits /= 10;
        decimal_power += 1;
    }
    if (digits == 0) {
        return false;
    }
    candidate->digits = digits;
    candidate->exponent = decimal_power;
    return true;
}

static bool ruda_number_find_candidate(
    const RudaNumberDecoded *value,
    const RudaNumberProfile *profile,
    const RudaNumberBounds *bounds,
    bool target_odd,
    RudaNumberCandidate *candidate
) {
    int decimal_exponent = 0;
    unsigned requested_digits = 0;
    if (!ruda_number_find_decimal_exponent(value, &decimal_exponent)) {
        return false;
    }
    for (requested_digits = 1;
        requested_digits <= profile->maximum_digits;
        requested_digits += 1) {
        int decimal_power = decimal_exponent
            - (int)requested_digits + 1;
        uint64_t floor_value = 0;
        RudaNumberCandidate floor_candidate = {0, 0};
        RudaNumberCandidate ceiling_candidate = {0, 0};
        bool floor_valid = false;
        bool ceiling_valid = false;
        if (!ruda_number_decimal_floor(
            value, decimal_power, &floor_value
        )) {
            return false;
        }
        floor_valid = ruda_number_candidate_valid(
            floor_value,
            decimal_power,
            bounds,
            target_odd
        ) && ruda_number_normalize_candidate(
            floor_value,
            decimal_power,
            requested_digits,
            &floor_candidate
        );
        ceiling_candidate = floor_candidate;
        if (floor_value != UINT64_MAX) {
            ceiling_candidate.exponent = decimal_power;
            ceiling_valid = ruda_number_candidate_valid(
                floor_value + 1,
                decimal_power,
                bounds,
                target_odd
            ) && ruda_number_normalize_candidate(
                floor_value + 1,
                decimal_power,
                requested_digits,
                &ceiling_candidate
            );
        }
        if (floor_valid && ceiling_valid) {
            int midpoint = ruda_number_decimal_compare_value(
                value,
                (floor_value * 2 + 1) * 5,
                decimal_power - 1
            );
            if (midpoint < 0 || (midpoint == 0 && (floor_value & 1u) == 0)) {
                *candidate = floor_candidate;
            } else {
                *candidate = ceiling_candidate;
            }
            return true;
        }
        if (floor_valid) {
            *candidate = floor_candidate;
            return true;
        }
        if (ceiling_valid) {
            *candidate = ceiling_candidate;
            return true;
        }
    }
    return false;
}

static bool ruda_number_append_char(
    char *output,
    size_t *length,
    char character
) {
    if (output == NULL || length == NULL || *length >= RUDA_NUMBER_FORMAT_OUTPUT - 1) {
        return false;
    }
    output[*length] = character;
    *length += 1;
    return true;
}

static bool ruda_number_append_digits(
    char *output,
    size_t *length,
    uint64_t digits,
    unsigned count
) {
    char reversed[20];
    unsigned index = 0;
    if (count == 0 || count >= sizeof(reversed)) {
        return false;
    }
    for (index = 0; index < count; index += 1) {
        reversed[index] = (char)('0' + digits % 10);
        digits /= 10;
    }
    while (index != 0) {
        index -= 1;
        if (!ruda_number_append_char(output, length, reversed[index])) {
            return false;
        }
    }
    return true;
}

static bool ruda_number_emit_finite(
    bool negative,
    const RudaNumberCandidate *candidate,
    char *output,
    size_t *length
) {
    unsigned digits_count = 0;
    int scientific_exponent = 0;
    int index = 0;
    uint64_t divisor = 1;
    if (candidate == NULL || output == NULL || length == NULL) {
        return false;
    }
    *length = 0;
    digits_count = ruda_number_decimal_digits(candidate->digits);
    scientific_exponent = candidate->exponent + (int)digits_count - 1;
    if (negative && !ruda_number_append_char(output, length, '-')) {
        return false;
    }
    if (scientific_exponent >= -4 && scientific_exponent <= 15) {
        if (scientific_exponent >= 0) {
            unsigned before = (unsigned)scientific_exponent + 1;
            if (digits_count <= before) {
                if (!ruda_number_append_digits(
                    output, length, candidate->digits, digits_count
                )) {
                    return false;
                }
                for (index = (int)digits_count; index < (int)before; index += 1) {
                    if (!ruda_number_append_char(output, length, '0')) {
                        return false;
                    }
                }
                if (!ruda_number_append_char(output, length, '.')
                    || !ruda_number_append_char(output, length, '0')) {
                    return false;
                }
                return true;
            }
            divisor = ruda_number_power10_u64(digits_count - before);
            if (!ruda_number_append_digits(
                output, length, candidate->digits / divisor, before
            ) || !ruda_number_append_char(output, length, '.')) {
                return false;
            }
            return ruda_number_append_digits(
                output,
                length,
                candidate->digits % divisor,
                digits_count - before
            );
        }
        if (!ruda_number_append_char(output, length, '0')
            || !ruda_number_append_char(output, length, '.')) {
            return false;
        }
        for (index = -scientific_exponent - 1; index > 0; index -= 1) {
            if (!ruda_number_append_char(output, length, '0')) {
                return false;
            }
        }
        return ruda_number_append_digits(
            output, length, candidate->digits, digits_count
        );
    }
    if (!ruda_number_append_char(
        output, length, (char)('0' + candidate->digits
            / ruda_number_power10_u64(digits_count - 1))
    ) || !ruda_number_append_char(output, length, '.')) {
        return false;
    }
    if (digits_count == 1) {
        if (!ruda_number_append_char(output, length, '0')) {
            return false;
        }
    } else if (!ruda_number_append_digits(
        output,
        length,
        candidate->digits % ruda_number_power10_u64(digits_count - 1),
        digits_count - 1
    )) {
        return false;
    }
    if (!ruda_number_append_char(output, length, 'e')) {
        return false;
    }
    if (scientific_exponent < 0) {
        if (!ruda_number_append_char(output, length, '-')) {
            return false;
        }
        scientific_exponent = -scientific_exponent;
    }
    return ruda_number_append_digits(
        output,
        length,
        (uint64_t)scientific_exponent,
        ruda_number_decimal_digits((uint64_t)scientific_exponent)
    );
}

static RudaNumberFormatStatus ruda_number_finish(
    const char *output,
    size_t length,
    char *buffer,
    size_t capacity,
    size_t *result_length
) {
    if (result_length == NULL) {
        return RUDA_NUMBER_FORMAT_INVALID_ARGUMENT;
    }
    *result_length = length;
    if (buffer == NULL || capacity <= length) {
        return RUDA_NUMBER_FORMAT_BUFFER_TOO_SMALL;
    }
    memcpy(buffer, output, length);
    buffer[length] = '\0';
    return RUDA_NUMBER_FORMAT_OK;
}

static RudaNumberFormatStatus ruda_number_format(
    uint64_t raw_bits,
    const RudaNumberProfile *profile,
    char *buffer,
    size_t capacity,
    size_t *length
) {
    RudaNumberDecoded value;
    RudaNumberBounds bounds;
    RudaNumberCandidate candidate;
    char output[RUDA_NUMBER_FORMAT_OUTPUT];
    size_t output_length = 0;
    bool negative = false;
    bool infinity = false;
    bool nan = false;
    uint64_t magnitude_mask = 0;
    if (profile == NULL || length == NULL) {
        return RUDA_NUMBER_FORMAT_INVALID_ARGUMENT;
    }
    if (profile->fraction_bits + profile->exponent_bits == 64) {
        magnitude_mask = UINT64_MAX;
    } else {
        magnitude_mask = (UINT64_C(1)
            << (profile->fraction_bits + profile->exponent_bits)) - 1;
    }
    if (!ruda_number_decode(
        raw_bits, profile, &value, &negative, &infinity, &nan
    )) {
        return RUDA_NUMBER_FORMAT_INTERNAL_ERROR;
    }
    output[0] = '\0';
    if (nan || infinity) {
        const char *text = nan ? "nan" : "inf";
        output_length = 0;
        if (negative && !ruda_number_append_char(output, &output_length, '-')) {
            return RUDA_NUMBER_FORMAT_INTERNAL_ERROR;
        }
        if (!ruda_number_append_char(output, &output_length, text[0])
            || !ruda_number_append_char(output, &output_length, text[1])
            || !ruda_number_append_char(output, &output_length, text[2])) {
            return RUDA_NUMBER_FORMAT_INTERNAL_ERROR;
        }
        return ruda_number_finish(
            output, output_length, buffer, capacity, length
        );
    }
    if (value.significand == 0) {
        output_length = 0;
        if (negative && !ruda_number_append_char(output, &output_length, '-')) {
            return RUDA_NUMBER_FORMAT_INTERNAL_ERROR;
        }
        if (!ruda_number_append_char(output, &output_length, '0')
            || !ruda_number_append_char(output, &output_length, '.')
            || !ruda_number_append_char(output, &output_length, '0')) {
            return RUDA_NUMBER_FORMAT_INTERNAL_ERROR;
        }
        return ruda_number_finish(
            output, output_length, buffer, capacity, length
        );
    }
    if (!ruda_number_make_bounds(raw_bits, profile, &value, &bounds)
        || !ruda_number_find_candidate(
            &value,
            profile,
            &bounds,
            (raw_bits & magnitude_mask & UINT64_C(1)) != 0,
            &candidate
        )
        || !ruda_number_emit_finite(
            negative, &candidate, output, &output_length
        )) {
        return RUDA_NUMBER_FORMAT_INTERNAL_ERROR;
    }
    return ruda_number_finish(output, output_length, buffer, capacity, length);
}

RudaNumberFormatStatus ruda_format_f32(
    uint32_t raw_bits,
    char *buffer,
    size_t capacity,
    size_t *length
) {
    return ruda_number_format(
        raw_bits,
        &ruda_number_f32_profile,
        buffer,
        capacity,
        length
    );
}

RudaNumberFormatStatus ruda_format_f64(
    uint64_t raw_bits,
    char *buffer,
    size_t capacity,
    size_t *length
) {
    return ruda_number_format(
        raw_bits,
        &ruda_number_f64_profile,
        buffer,
        capacity,
        length
    );
}
