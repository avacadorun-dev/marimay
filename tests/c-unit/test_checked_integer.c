#include "test_support.h"

#include <stddef.h>
#include <stdint.h>

#include "ruda/base/checked_integer.h"

typedef struct RudaIntegerFixture {
    RudaIntegerKind kind;
    unsigned int width;
    bool is_signed;
    uint64_t mask;
    uint64_t sign_bit;
    uint64_t minimum;
    uint64_t maximum;
} RudaIntegerFixture;

typedef RudaCheckedIntegerStatus (*RudaIntegerBinaryOperation)(
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaIntegerValue *result
);

typedef RudaCheckedIntegerStatus (*RudaIntegerUnaryOperation)(
    RudaIntegerValue value,
    RudaIntegerValue *result
);

typedef RudaCheckedIntegerStatus (*RudaIntegerShiftOperation)(
    RudaIntegerValue value,
    RudaIntegerValue count,
    RudaIntegerValue *result
);

static RudaIntegerFixture ruda_integer_fixture(RudaIntegerKind kind) {
    RudaIntegerFixture fixture;
    fixture.kind = kind;
    fixture.width = ruda_integer_bit_width(kind);
    fixture.is_signed = ruda_integer_kind_is_signed(kind);
    fixture.mask = fixture.width == 64
        ? UINT64_MAX
        : (UINT64_C(1) << fixture.width) - UINT64_C(1);
    fixture.sign_bit = UINT64_C(1) << (fixture.width - 1);
    fixture.minimum = fixture.is_signed ? fixture.sign_bit : UINT64_C(0);
    fixture.maximum = fixture.is_signed
        ? fixture.sign_bit - UINT64_C(1)
        : fixture.mask;
    return fixture;
}

static RudaIntegerValue ruda_bits_value(
    const RudaIntegerFixture *fixture,
    uint64_t bits
) {
    RudaIntegerValue value;
    value.kind = fixture->kind;
    value.bits = bits & fixture->mask;
    return value;
}

static uint64_t ruda_signed_bits(
    const RudaIntegerFixture *fixture,
    int64_t value
) {
    uint64_t magnitude = 0;
    if (value >= 0) {
        return (uint64_t)value & fixture->mask;
    }
    magnitude = (uint64_t)(-(value + INT64_C(1))) + UINT64_C(1);
    return (UINT64_MAX - magnitude + UINT64_C(1)) & fixture->mask;
}

static RudaIntegerValue ruda_signed_value(
    const RudaIntegerFixture *fixture,
    int64_t value
) {
    return ruda_bits_value(fixture, ruda_signed_bits(fixture, value));
}

static RudaIntegerValue ruda_unsigned_value(
    const RudaIntegerFixture *fixture,
    uint64_t value
) {
    return ruda_bits_value(fixture, value);
}

static RudaIntegerValue ruda_small_value(
    const RudaIntegerFixture *fixture,
    int64_t signed_value,
    uint64_t unsigned_value
) {
    return fixture->is_signed
        ? ruda_signed_value(fixture, signed_value)
        : ruda_unsigned_value(fixture, unsigned_value);
}

static void ruda_expect_value(
    RudaIntegerValue actual,
    RudaIntegerKind kind,
    uint64_t expected_bits
) {
    RUDA_EXPECT(actual.kind == kind);
    RUDA_EXPECT(actual.bits == expected_bits);
}

static void ruda_expect_binary(
    RudaIntegerBinaryOperation operation,
    RudaIntegerValue left,
    RudaIntegerValue right,
    RudaCheckedIntegerStatus expected_status,
    uint64_t expected_bits
) {
    RudaIntegerFixture fixture = ruda_integer_fixture(left.kind);
    RudaIntegerValue result = ruda_bits_value(&fixture, UINT64_MAX);
    RudaCheckedIntegerStatus status = operation(left, right, &result);
    RUDA_EXPECT(status == expected_status);
    if (expected_status == RUDA_CHECKED_INTEGER_OK) {
        ruda_expect_value(result, left.kind, expected_bits);
    } else {
        RUDA_EXPECT(result.bits == fixture.mask);
    }
}

static void ruda_expect_unary(
    RudaIntegerUnaryOperation operation,
    RudaIntegerValue value,
    RudaCheckedIntegerStatus expected_status,
    uint64_t expected_bits
) {
    RudaIntegerFixture fixture = ruda_integer_fixture(value.kind);
    RudaIntegerValue result = ruda_bits_value(&fixture, UINT64_MAX);
    RudaCheckedIntegerStatus status = operation(value, &result);
    RUDA_EXPECT(status == expected_status);
    if (expected_status == RUDA_CHECKED_INTEGER_OK) {
        ruda_expect_value(result, value.kind, expected_bits);
    } else {
        RUDA_EXPECT(result.bits == fixture.mask);
    }
}

static void ruda_expect_shift(
    RudaIntegerShiftOperation operation,
    RudaIntegerValue value,
    RudaIntegerValue count,
    RudaCheckedIntegerStatus expected_status,
    uint64_t expected_bits
) {
    RudaIntegerFixture fixture = ruda_integer_fixture(value.kind);
    RudaIntegerValue result = ruda_bits_value(&fixture, UINT64_MAX);
    RudaCheckedIntegerStatus status = operation(value, count, &result);
    RUDA_EXPECT(status == expected_status);
    if (expected_status == RUDA_CHECKED_INTEGER_OK) {
        ruda_expect_value(result, value.kind, expected_bits);
    } else {
        RUDA_EXPECT(result.bits == fixture.mask);
    }
}

static void ruda_test_checked_integer_shape(void) {
    static const RudaIntegerKind kinds[] = {
        RUDA_INTEGER_I8, RUDA_INTEGER_I16, RUDA_INTEGER_I32, RUDA_INTEGER_I64,
        RUDA_INTEGER_U8, RUDA_INTEGER_U16, RUDA_INTEGER_U32, RUDA_INTEGER_U64
    };
    size_t index = 0;
    RUDA_EXPECT(RUDA_CHECKED_INTEGER_OK == 0);
    RUDA_EXPECT(RUDA_CHECKED_INTEGER_OVERFLOW == 1);
    RUDA_EXPECT(RUDA_CHECKED_INTEGER_DIVISION_BY_ZERO == 2);
    RUDA_EXPECT(RUDA_CHECKED_INTEGER_INVALID_SHIFT == 3);
    for (index = 0; index < sizeof(kinds) / sizeof(kinds[0]); index += 1) {
        RudaIntegerFixture fixture = ruda_integer_fixture(kinds[index]);
        RUDA_EXPECT(fixture.width == (index < 4 ? 8U << index : 8U << (index - 4)));
        RUDA_EXPECT(fixture.is_signed == (index < 4));
        RUDA_EXPECT(ruda_integer_bit_width((RudaIntegerKind)99) == 0);
        RUDA_EXPECT(!ruda_integer_kind_is_signed((RudaIntegerKind)99));
    }
}

static void ruda_test_checked_integer_conversions(void) {
    int8_t i8 = 0;
    int16_t i16 = 0;
    int32_t i32 = 0;
    int64_t i64 = 0;
    uint8_t u8 = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    uint64_t u64 = 0;
    RudaIntegerValue invalid = { (RudaIntegerKind)99, UINT64_MAX };
    RUDA_EXPECT(ruda_integer_to_i8(
        ruda_integer_from_i8(INT8_MIN), &i8
    ) && i8 == INT8_MIN);
    RUDA_EXPECT(ruda_integer_to_i16(
        ruda_integer_from_i16(INT16_MIN), &i16
    ) && i16 == INT16_MIN);
    RUDA_EXPECT(ruda_integer_to_i32(
        ruda_integer_from_i32(INT32_MIN), &i32
    ) && i32 == INT32_MIN);
    RUDA_EXPECT(ruda_integer_to_i64(
        ruda_integer_from_i64(INT64_MIN), &i64
    ) && i64 == INT64_MIN);
    RUDA_EXPECT(ruda_integer_to_u8(
        ruda_integer_from_u8(UINT8_MAX), &u8
    ) && u8 == UINT8_MAX);
    RUDA_EXPECT(ruda_integer_to_u16(
        ruda_integer_from_u16(UINT16_MAX), &u16
    ) && u16 == UINT16_MAX);
    RUDA_EXPECT(ruda_integer_to_u32(
        ruda_integer_from_u32(UINT32_MAX), &u32
    ) && u32 == UINT32_MAX);
    RUDA_EXPECT(ruda_integer_to_u64(
        ruda_integer_from_u64(UINT64_MAX), &u64
    ) && u64 == UINT64_MAX);
    RUDA_EXPECT(!ruda_integer_to_i8(invalid, &i8));
    RUDA_EXPECT(ruda_integer_to_i8(ruda_integer_from_u8(1), &i8)
        && i8 == 1);
    RUDA_EXPECT(!ruda_integer_to_i8(ruda_integer_from_i8(1), NULL));
    RUDA_EXPECT(ruda_integer_to_i16(ruda_integer_from_i8(-7), &i16)
        && i16 == -7);
    RUDA_EXPECT(ruda_integer_to_i64(ruda_integer_from_u32(UINT32_MAX), &i64)
        && i64 == (int64_t)UINT32_MAX);
    RUDA_EXPECT(ruda_integer_to_u64(ruda_integer_from_i32(INT32_MIN), &u64)
        == false);
    RUDA_EXPECT(ruda_integer_to_u16(ruda_integer_from_i32(65535), &u16)
        && u16 == UINT16_MAX);
    RUDA_EXPECT(ruda_integer_to_i8(ruda_integer_from_i16(128), &i8)
        == false);
    RUDA_EXPECT(ruda_integer_to_u8(ruda_integer_from_u16(256), &u8)
        == false);
    RUDA_EXPECT(ruda_integer_to_i64(ruda_integer_from_u64(UINT64_MAX), &i64)
        == false);
}

static void ruda_test_checked_integer_arithmetic(
    const RudaIntegerFixture *fixture
) {
    RudaIntegerValue zero = ruda_bits_value(fixture, 0);
    RudaIntegerValue one = ruda_bits_value(fixture, 1);
    RudaIntegerValue two = ruda_bits_value(fixture, 2);
    RudaIntegerValue minimum = ruda_bits_value(fixture, fixture->minimum);
    RudaIntegerValue maximum = ruda_bits_value(fixture, fixture->maximum);
    RudaIntegerValue negative_one = ruda_bits_value(fixture, fixture->mask);
    RudaIntegerValue seven = ruda_small_value(fixture, 7, 7);
    RudaIntegerValue minus_seven = ruda_small_value(fixture, -7, 0);
    uint64_t expected_one_plus_one = 2;

    ruda_expect_binary(
        ruda_checked_integer_add, zero, one,
        RUDA_CHECKED_INTEGER_OK, one.bits
    );
    ruda_expect_binary(
        ruda_checked_integer_add, maximum, one,
        RUDA_CHECKED_INTEGER_OVERFLOW, 0
    );
    ruda_expect_binary(
        ruda_checked_integer_subtract, one, one,
        RUDA_CHECKED_INTEGER_OK, zero.bits
    );
    ruda_expect_binary(
        ruda_checked_integer_subtract, zero, one,
        fixture->is_signed ? RUDA_CHECKED_INTEGER_OK
                           : RUDA_CHECKED_INTEGER_OVERFLOW,
        fixture->is_signed ? negative_one.bits : 0
    );
    ruda_expect_binary(
        ruda_checked_integer_multiply, one, one,
        RUDA_CHECKED_INTEGER_OK, one.bits
    );
    ruda_expect_binary(
        ruda_checked_integer_multiply, maximum, two,
        RUDA_CHECKED_INTEGER_OVERFLOW, 0
    );
    ruda_expect_binary(
        ruda_checked_integer_divide, seven, two,
        RUDA_CHECKED_INTEGER_OK, fixture->is_signed ? ruda_signed_bits(fixture, 3) : 3
    );
    ruda_expect_binary(
        ruda_checked_integer_remainder, seven, two,
        RUDA_CHECKED_INTEGER_OK, one.bits
    );
    ruda_expect_binary(
        ruda_checked_integer_divide, zero, zero,
        RUDA_CHECKED_INTEGER_DIVISION_BY_ZERO, 0
    );
    ruda_expect_binary(
        ruda_checked_integer_remainder, zero, zero,
        RUDA_CHECKED_INTEGER_DIVISION_BY_ZERO, 0
    );
    ruda_expect_unary(
        ruda_checked_integer_negate, zero,
        RUDA_CHECKED_INTEGER_OK, zero.bits
    );
    ruda_expect_unary(
        ruda_checked_integer_negate, one,
        fixture->is_signed ? RUDA_CHECKED_INTEGER_OK : RUDA_CHECKED_INTEGER_OVERFLOW,
        fixture->is_signed ? negative_one.bits : 0
    );
    ruda_expect_unary(
        ruda_checked_integer_negate, minimum,
        fixture->is_signed ? RUDA_CHECKED_INTEGER_OVERFLOW
                           : RUDA_CHECKED_INTEGER_OK,
        0
    );
    ruda_expect_binary(
        ruda_checked_integer_add, one, one,
        RUDA_CHECKED_INTEGER_OK, expected_one_plus_one
    );
    if (fixture->is_signed) {
        ruda_expect_binary(
            ruda_checked_integer_subtract, minimum, one,
            RUDA_CHECKED_INTEGER_OVERFLOW, 0
        );
        ruda_expect_binary(
            ruda_checked_integer_multiply, minimum, negative_one,
            RUDA_CHECKED_INTEGER_OVERFLOW, 0
        );
        ruda_expect_binary(
            ruda_checked_integer_divide, minimum, negative_one,
            RUDA_CHECKED_INTEGER_OVERFLOW, 0
        );
        ruda_expect_binary(
            ruda_checked_integer_remainder, minimum, negative_one,
            RUDA_CHECKED_INTEGER_OVERFLOW, 0
        );
        ruda_expect_binary(
            ruda_checked_integer_divide, minus_seven, two,
            RUDA_CHECKED_INTEGER_OK, ruda_signed_bits(fixture, -3)
        );
        ruda_expect_binary(
            ruda_checked_integer_remainder, minus_seven, two,
            RUDA_CHECKED_INTEGER_OK, ruda_signed_bits(fixture, -1)
        );
    }
}

static void ruda_test_checked_integer_shifts(
    const RudaIntegerFixture *fixture
) {
    RudaIntegerValue one = ruda_bits_value(fixture, 1);
    RudaIntegerValue minimum = ruda_bits_value(fixture, fixture->minimum);
    RudaIntegerValue maximum = ruda_bits_value(fixture, fixture->maximum);
    RudaIntegerValue count_zero = ruda_bits_value(fixture, 0);
    RudaIntegerValue count_one = ruda_bits_value(fixture, 1);
    RudaIntegerValue count_width = ruda_bits_value(fixture, fixture->width);
    RudaIntegerValue negative_count = ruda_signed_value(fixture, -1);
    uint64_t expected_left = UINT64_C(2) & fixture->mask;
    uint64_t expected_min_right = fixture->is_signed
        ? fixture->sign_bit | (fixture->sign_bit >> 1)
        : fixture->minimum;

    ruda_expect_shift(
        ruda_checked_integer_shift_left, one, count_zero,
        RUDA_CHECKED_INTEGER_OK, one.bits
    );
    ruda_expect_shift(
        ruda_checked_integer_shift_left, one, count_one,
        RUDA_CHECKED_INTEGER_OK, expected_left
    );
    ruda_expect_shift(
        ruda_checked_integer_shift_right, maximum, count_one,
        RUDA_CHECKED_INTEGER_OK, fixture->is_signed
            ? fixture->maximum >> 1 : fixture->maximum >> 1
    );
    ruda_expect_shift(
        ruda_checked_integer_shift_right, minimum, count_one,
        RUDA_CHECKED_INTEGER_OK, expected_min_right
    );
    ruda_expect_shift(
        ruda_checked_integer_shift_left, one, count_width,
        RUDA_CHECKED_INTEGER_INVALID_SHIFT, 0
    );
    ruda_expect_shift(
        ruda_checked_integer_shift_right, one, count_width,
        RUDA_CHECKED_INTEGER_INVALID_SHIFT, 0
    );
    if (fixture->is_signed) {
        ruda_expect_shift(
            ruda_checked_integer_shift_left, one, negative_count,
            RUDA_CHECKED_INTEGER_INVALID_SHIFT, 0
        );
        ruda_expect_shift(
            ruda_checked_integer_shift_right, one, negative_count,
            RUDA_CHECKED_INTEGER_INVALID_SHIFT, 0
        );
        ruda_expect_shift(
            ruda_checked_integer_shift_left, maximum, count_one,
            RUDA_CHECKED_INTEGER_OVERFLOW, 0
        );
        ruda_expect_shift(
            ruda_checked_integer_shift_left, minimum, count_one,
            RUDA_CHECKED_INTEGER_OVERFLOW, 0
        );
    } else {
        ruda_expect_shift(
            ruda_checked_integer_shift_left, maximum, count_one,
            RUDA_CHECKED_INTEGER_OVERFLOW, 0
        );
    }
}

static void ruda_test_wrapping_integer_operations(
    const RudaIntegerFixture *fixture
) {
    RudaIntegerValue zero = ruda_bits_value(fixture, 0);
    RudaIntegerValue one = ruda_bits_value(fixture, 1);
    RudaIntegerValue two = ruda_bits_value(fixture, 2);
    RudaIntegerValue minimum = ruda_bits_value(fixture, fixture->minimum);
    RudaIntegerValue maximum = ruda_bits_value(fixture, fixture->maximum);
    RudaIntegerValue negative_one = ruda_bits_value(fixture, fixture->mask);
    RudaIntegerValue count_one = ruda_bits_value(fixture, 1);

    ruda_expect_binary(
        ruda_wrapping_integer_add, maximum, one,
        RUDA_CHECKED_INTEGER_OK, fixture->is_signed
            ? fixture->minimum : 0
    );
    ruda_expect_binary(
        ruda_wrapping_integer_subtract, zero, one,
        RUDA_CHECKED_INTEGER_OK, fixture->mask
    );
    ruda_expect_binary(
        ruda_wrapping_integer_multiply, maximum, two,
        RUDA_CHECKED_INTEGER_OK, (maximum.bits << 1) & fixture->mask
    );
    ruda_expect_unary(
        ruda_wrapping_integer_negate, minimum,
        RUDA_CHECKED_INTEGER_OK, minimum.bits
    );
    ruda_expect_unary(
        ruda_wrapping_integer_negate, one,
        RUDA_CHECKED_INTEGER_OK, fixture->is_signed
            ? negative_one.bits : fixture->mask
    );
    ruda_expect_binary(
        ruda_wrapping_integer_divide, zero, zero,
        RUDA_CHECKED_INTEGER_DIVISION_BY_ZERO, 0
    );
    ruda_expect_binary(
        ruda_wrapping_integer_remainder, zero, zero,
        RUDA_CHECKED_INTEGER_DIVISION_BY_ZERO, 0
    );
    if (fixture->is_signed) {
        ruda_expect_binary(
            ruda_wrapping_integer_divide, minimum, negative_one,
            RUDA_CHECKED_INTEGER_OK, minimum.bits
        );
        ruda_expect_binary(
            ruda_wrapping_integer_remainder, minimum, negative_one,
            RUDA_CHECKED_INTEGER_OK, 0
        );
    }
    ruda_expect_shift(
        ruda_wrapping_integer_shift_left, maximum, count_one,
        RUDA_CHECKED_INTEGER_OK, (maximum.bits << 1) & fixture->mask
    );
    ruda_expect_shift(
        ruda_wrapping_integer_shift_right, minimum, count_one,
        RUDA_CHECKED_INTEGER_OK, fixture->is_signed
            ? fixture->sign_bit | (fixture->sign_bit >> 1) : fixture->minimum
    );
    ruda_expect_shift(
        ruda_wrapping_integer_shift_left, one,
        ruda_bits_value(fixture, fixture->width),
        RUDA_CHECKED_INTEGER_INVALID_SHIFT, 0
    );
}

static void ruda_test_saturating_integer_operations(
    const RudaIntegerFixture *fixture
) {
    RudaIntegerValue zero = ruda_bits_value(fixture, 0);
    RudaIntegerValue one = ruda_bits_value(fixture, 1);
    RudaIntegerValue two = ruda_bits_value(fixture, 2);
    RudaIntegerValue minimum = ruda_bits_value(fixture, fixture->minimum);
    RudaIntegerValue maximum = ruda_bits_value(fixture, fixture->maximum);
    RudaIntegerValue negative_one = ruda_bits_value(fixture, fixture->mask);
    RudaIntegerValue count_one = ruda_bits_value(fixture, 1);
    RudaIntegerValue count_width = ruda_bits_value(fixture, fixture->width);
    uint64_t expected_min_right = fixture->is_signed
        ? fixture->sign_bit | (fixture->sign_bit >> 1)
        : fixture->minimum;

    ruda_expect_binary(
        ruda_saturating_integer_add, maximum, one,
        RUDA_CHECKED_INTEGER_OK, maximum.bits
    );
    ruda_expect_binary(
        ruda_saturating_integer_subtract, zero, one,
        RUDA_CHECKED_INTEGER_OK, fixture->is_signed
            ? negative_one.bits : 0
    );
    ruda_expect_binary(
        ruda_saturating_integer_multiply, maximum, two,
        RUDA_CHECKED_INTEGER_OK, maximum.bits
    );
    ruda_expect_unary(
        ruda_saturating_integer_negate, one,
        RUDA_CHECKED_INTEGER_OK, fixture->is_signed ? negative_one.bits : 0
    );
    ruda_expect_binary(
        ruda_saturating_integer_divide, zero, zero,
        RUDA_CHECKED_INTEGER_DIVISION_BY_ZERO, 0
    );
    ruda_expect_binary(
        ruda_saturating_integer_remainder, zero, zero,
        RUDA_CHECKED_INTEGER_DIVISION_BY_ZERO, 0
    );
    ruda_expect_shift(
        ruda_saturating_integer_shift_left, maximum, count_one,
        RUDA_CHECKED_INTEGER_OK, maximum.bits
    );
    ruda_expect_shift(
        ruda_saturating_integer_shift_left, one, count_width,
        RUDA_CHECKED_INTEGER_INVALID_SHIFT, 0
    );
    ruda_expect_shift(
        ruda_saturating_integer_shift_right, minimum, count_one,
        RUDA_CHECKED_INTEGER_OK, expected_min_right
    );
    if (fixture->is_signed) {
        ruda_expect_binary(
            ruda_saturating_integer_add, minimum, negative_one,
            RUDA_CHECKED_INTEGER_OK, minimum.bits
        );
        ruda_expect_binary(
            ruda_saturating_integer_subtract, maximum, negative_one,
            RUDA_CHECKED_INTEGER_OK, maximum.bits
        );
        ruda_expect_binary(
            ruda_saturating_integer_multiply, minimum, negative_one,
            RUDA_CHECKED_INTEGER_OK, maximum.bits
        );
        ruda_expect_binary(
            ruda_saturating_integer_multiply, minimum, two,
            RUDA_CHECKED_INTEGER_OK, minimum.bits
        );
        ruda_expect_binary(
            ruda_saturating_integer_divide, minimum, negative_one,
            RUDA_CHECKED_INTEGER_OK, maximum.bits
        );
        ruda_expect_binary(
            ruda_saturating_integer_remainder, minimum, negative_one,
            RUDA_CHECKED_INTEGER_OK, 0
        );
        ruda_expect_unary(
            ruda_saturating_integer_negate, minimum,
            RUDA_CHECKED_INTEGER_OK, maximum.bits
        );
        ruda_expect_shift(
            ruda_saturating_integer_shift_left, minimum, count_one,
            RUDA_CHECKED_INTEGER_OK, minimum.bits
        );
    }
}

static void ruda_test_checked_integer_invalid_inputs(void) {
    RudaIntegerValue left = ruda_integer_from_i8(1);
    RudaIntegerValue right = ruda_integer_from_i16(1);
    RudaIntegerValue malformed = { RUDA_INTEGER_I8, UINT64_C(0x100) };
    RudaIntegerValue result = ruda_integer_from_i8(99);
    RUDA_EXPECT(ruda_checked_integer_add(left, right, &result)
        == RUDA_CHECKED_INTEGER_OVERFLOW);
    RUDA_EXPECT(result.bits == UINT64_C(99));
    RUDA_EXPECT(ruda_checked_integer_add(malformed, left, &result)
        == RUDA_CHECKED_INTEGER_OVERFLOW);
    RUDA_EXPECT(result.bits == UINT64_C(99));
    RUDA_EXPECT(ruda_checked_integer_add(left, right, NULL)
        == RUDA_CHECKED_INTEGER_OVERFLOW);
}

void ruda_test_checked_integer(void) {
    static const RudaIntegerKind kinds[] = {
        RUDA_INTEGER_I8, RUDA_INTEGER_I16, RUDA_INTEGER_I32, RUDA_INTEGER_I64,
        RUDA_INTEGER_U8, RUDA_INTEGER_U16, RUDA_INTEGER_U32, RUDA_INTEGER_U64
    };
    size_t index = 0;
    ruda_test_checked_integer_shape();
    ruda_test_checked_integer_conversions();
    ruda_test_checked_integer_invalid_inputs();
    for (index = 0; index < sizeof(kinds) / sizeof(kinds[0]); index += 1) {
        RudaIntegerFixture fixture = ruda_integer_fixture(kinds[index]);
        ruda_test_checked_integer_arithmetic(&fixture);
        ruda_test_checked_integer_shifts(&fixture);
        ruda_test_wrapping_integer_operations(&fixture);
        ruda_test_saturating_integer_operations(&fixture);
    }
}
