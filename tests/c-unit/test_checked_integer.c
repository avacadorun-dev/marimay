#include "test_support.h"

#include <stddef.h>
#include <stdint.h>

#include "mmay/base/checked_integer.h"

typedef struct MarimayIntegerFixture {
    MarimayIntegerKind kind;
    unsigned int width;
    bool is_signed;
    uint64_t mask;
    uint64_t sign_bit;
    uint64_t minimum;
    uint64_t maximum;
} MarimayIntegerFixture;

typedef MarimayCheckedIntegerStatus (*MarimayIntegerBinaryOperation)(
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayIntegerValue *result
);

typedef MarimayCheckedIntegerStatus (*MarimayIntegerUnaryOperation)(
    MarimayIntegerValue value,
    MarimayIntegerValue *result
);

typedef MarimayCheckedIntegerStatus (*MarimayIntegerShiftOperation)(
    MarimayIntegerValue value,
    MarimayIntegerValue count,
    MarimayIntegerValue *result
);

static MarimayIntegerFixture mmay_integer_fixture(MarimayIntegerKind kind) {
    MarimayIntegerFixture fixture;
    fixture.kind = kind;
    fixture.width = mmay_integer_bit_width(kind);
    fixture.is_signed = mmay_integer_kind_is_signed(kind);
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

static MarimayIntegerValue mmay_bits_value(
    const MarimayIntegerFixture *fixture,
    uint64_t bits
) {
    MarimayIntegerValue value;
    value.kind = fixture->kind;
    value.bits = bits & fixture->mask;
    return value;
}

static uint64_t mmay_signed_bits(
    const MarimayIntegerFixture *fixture,
    int64_t value
) {
    uint64_t magnitude = 0;
    if (value >= 0) {
        return (uint64_t)value & fixture->mask;
    }
    magnitude = (uint64_t)(-(value + INT64_C(1))) + UINT64_C(1);
    return (UINT64_MAX - magnitude + UINT64_C(1)) & fixture->mask;
}

static MarimayIntegerValue mmay_signed_value(
    const MarimayIntegerFixture *fixture,
    int64_t value
) {
    return mmay_bits_value(fixture, mmay_signed_bits(fixture, value));
}

static MarimayIntegerValue mmay_unsigned_value(
    const MarimayIntegerFixture *fixture,
    uint64_t value
) {
    return mmay_bits_value(fixture, value);
}

static MarimayIntegerValue mmay_small_value(
    const MarimayIntegerFixture *fixture,
    int64_t signed_value,
    uint64_t unsigned_value
) {
    return fixture->is_signed
        ? mmay_signed_value(fixture, signed_value)
        : mmay_unsigned_value(fixture, unsigned_value);
}

static void mmay_expect_value(
    MarimayIntegerValue actual,
    MarimayIntegerKind kind,
    uint64_t expected_bits
) {
    MMAY_EXPECT(actual.kind == kind);
    MMAY_EXPECT(actual.bits == expected_bits);
}

static void mmay_expect_binary(
    MarimayIntegerBinaryOperation operation,
    MarimayIntegerValue left,
    MarimayIntegerValue right,
    MarimayCheckedIntegerStatus expected_status,
    uint64_t expected_bits
) {
    MarimayIntegerFixture fixture = mmay_integer_fixture(left.kind);
    MarimayIntegerValue result = mmay_bits_value(&fixture, UINT64_MAX);
    MarimayCheckedIntegerStatus status = operation(left, right, &result);
    MMAY_EXPECT(status == expected_status);
    if (expected_status == MMAY_CHECKED_INTEGER_OK) {
        mmay_expect_value(result, left.kind, expected_bits);
    } else {
        MMAY_EXPECT(result.bits == fixture.mask);
    }
}

static void mmay_expect_unary(
    MarimayIntegerUnaryOperation operation,
    MarimayIntegerValue value,
    MarimayCheckedIntegerStatus expected_status,
    uint64_t expected_bits
) {
    MarimayIntegerFixture fixture = mmay_integer_fixture(value.kind);
    MarimayIntegerValue result = mmay_bits_value(&fixture, UINT64_MAX);
    MarimayCheckedIntegerStatus status = operation(value, &result);
    MMAY_EXPECT(status == expected_status);
    if (expected_status == MMAY_CHECKED_INTEGER_OK) {
        mmay_expect_value(result, value.kind, expected_bits);
    } else {
        MMAY_EXPECT(result.bits == fixture.mask);
    }
}

static void mmay_expect_shift(
    MarimayIntegerShiftOperation operation,
    MarimayIntegerValue value,
    MarimayIntegerValue count,
    MarimayCheckedIntegerStatus expected_status,
    uint64_t expected_bits
) {
    MarimayIntegerFixture fixture = mmay_integer_fixture(value.kind);
    MarimayIntegerValue result = mmay_bits_value(&fixture, UINT64_MAX);
    MarimayCheckedIntegerStatus status = operation(value, count, &result);
    MMAY_EXPECT(status == expected_status);
    if (expected_status == MMAY_CHECKED_INTEGER_OK) {
        mmay_expect_value(result, value.kind, expected_bits);
    } else {
        MMAY_EXPECT(result.bits == fixture.mask);
    }
}

static void mmay_test_checked_integer_shape(void) {
    static const MarimayIntegerKind kinds[] = {
        MMAY_INTEGER_I8, MMAY_INTEGER_I16, MMAY_INTEGER_I32, MMAY_INTEGER_I64,
        MMAY_INTEGER_U8, MMAY_INTEGER_U16, MMAY_INTEGER_U32, MMAY_INTEGER_U64
    };
    size_t index = 0;
    MMAY_EXPECT(MMAY_CHECKED_INTEGER_OK == 0);
    MMAY_EXPECT(MMAY_CHECKED_INTEGER_OVERFLOW == 1);
    MMAY_EXPECT(MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO == 2);
    MMAY_EXPECT(MMAY_CHECKED_INTEGER_INVALID_SHIFT == 3);
    for (index = 0; index < sizeof(kinds) / sizeof(kinds[0]); index += 1) {
        MarimayIntegerFixture fixture = mmay_integer_fixture(kinds[index]);
        MMAY_EXPECT(fixture.width == (index < 4 ? 8U << index : 8U << (index - 4)));
        MMAY_EXPECT(fixture.is_signed == (index < 4));
        MMAY_EXPECT(mmay_integer_bit_width((MarimayIntegerKind)99) == 0);
        MMAY_EXPECT(!mmay_integer_kind_is_signed((MarimayIntegerKind)99));
    }
}

static void mmay_test_checked_integer_conversions(void) {
    int8_t i8 = 0;
    int16_t i16 = 0;
    int32_t i32 = 0;
    int64_t i64 = 0;
    uint8_t u8 = 0;
    uint16_t u16 = 0;
    uint32_t u32 = 0;
    uint64_t u64 = 0;
    MarimayIntegerValue invalid = { (MarimayIntegerKind)99, UINT64_MAX };
    MMAY_EXPECT(mmay_integer_to_i8(
        mmay_integer_from_i8(INT8_MIN), &i8
    ) && i8 == INT8_MIN);
    MMAY_EXPECT(mmay_integer_to_i16(
        mmay_integer_from_i16(INT16_MIN), &i16
    ) && i16 == INT16_MIN);
    MMAY_EXPECT(mmay_integer_to_i32(
        mmay_integer_from_i32(INT32_MIN), &i32
    ) && i32 == INT32_MIN);
    MMAY_EXPECT(mmay_integer_to_i64(
        mmay_integer_from_i64(INT64_MIN), &i64
    ) && i64 == INT64_MIN);
    MMAY_EXPECT(mmay_integer_to_u8(
        mmay_integer_from_u8(UINT8_MAX), &u8
    ) && u8 == UINT8_MAX);
    MMAY_EXPECT(mmay_integer_to_u16(
        mmay_integer_from_u16(UINT16_MAX), &u16
    ) && u16 == UINT16_MAX);
    MMAY_EXPECT(mmay_integer_to_u32(
        mmay_integer_from_u32(UINT32_MAX), &u32
    ) && u32 == UINT32_MAX);
    MMAY_EXPECT(mmay_integer_to_u64(
        mmay_integer_from_u64(UINT64_MAX), &u64
    ) && u64 == UINT64_MAX);
    MMAY_EXPECT(!mmay_integer_to_i8(invalid, &i8));
    MMAY_EXPECT(mmay_integer_to_i8(mmay_integer_from_u8(1), &i8)
        && i8 == 1);
    MMAY_EXPECT(!mmay_integer_to_i8(mmay_integer_from_i8(1), NULL));
    MMAY_EXPECT(mmay_integer_to_i16(mmay_integer_from_i8(-7), &i16)
        && i16 == -7);
    MMAY_EXPECT(mmay_integer_to_i64(mmay_integer_from_u32(UINT32_MAX), &i64)
        && i64 == (int64_t)UINT32_MAX);
    MMAY_EXPECT(mmay_integer_to_u64(mmay_integer_from_i32(INT32_MIN), &u64)
        == false);
    MMAY_EXPECT(mmay_integer_to_u16(mmay_integer_from_i32(65535), &u16)
        && u16 == UINT16_MAX);
    MMAY_EXPECT(mmay_integer_to_i8(mmay_integer_from_i16(128), &i8)
        == false);
    MMAY_EXPECT(mmay_integer_to_u8(mmay_integer_from_u16(256), &u8)
        == false);
    MMAY_EXPECT(mmay_integer_to_i64(mmay_integer_from_u64(UINT64_MAX), &i64)
        == false);
}

static void mmay_test_checked_integer_arithmetic(
    const MarimayIntegerFixture *fixture
) {
    MarimayIntegerValue zero = mmay_bits_value(fixture, 0);
    MarimayIntegerValue one = mmay_bits_value(fixture, 1);
    MarimayIntegerValue two = mmay_bits_value(fixture, 2);
    MarimayIntegerValue minimum = mmay_bits_value(fixture, fixture->minimum);
    MarimayIntegerValue maximum = mmay_bits_value(fixture, fixture->maximum);
    MarimayIntegerValue negative_one = mmay_bits_value(fixture, fixture->mask);
    MarimayIntegerValue seven = mmay_small_value(fixture, 7, 7);
    MarimayIntegerValue minus_seven = mmay_small_value(fixture, -7, 0);
    uint64_t expected_one_plus_one = 2;

    mmay_expect_binary(
        mmay_checked_integer_add, zero, one,
        MMAY_CHECKED_INTEGER_OK, one.bits
    );
    mmay_expect_binary(
        mmay_checked_integer_add, maximum, one,
        MMAY_CHECKED_INTEGER_OVERFLOW, 0
    );
    mmay_expect_binary(
        mmay_checked_integer_subtract, one, one,
        MMAY_CHECKED_INTEGER_OK, zero.bits
    );
    mmay_expect_binary(
        mmay_checked_integer_subtract, zero, one,
        fixture->is_signed ? MMAY_CHECKED_INTEGER_OK
                           : MMAY_CHECKED_INTEGER_OVERFLOW,
        fixture->is_signed ? negative_one.bits : 0
    );
    mmay_expect_binary(
        mmay_checked_integer_multiply, one, one,
        MMAY_CHECKED_INTEGER_OK, one.bits
    );
    mmay_expect_binary(
        mmay_checked_integer_multiply, maximum, two,
        MMAY_CHECKED_INTEGER_OVERFLOW, 0
    );
    mmay_expect_binary(
        mmay_checked_integer_divide, seven, two,
        MMAY_CHECKED_INTEGER_OK, fixture->is_signed ? mmay_signed_bits(fixture, 3) : 3
    );
    mmay_expect_binary(
        mmay_checked_integer_remainder, seven, two,
        MMAY_CHECKED_INTEGER_OK, one.bits
    );
    mmay_expect_binary(
        mmay_checked_integer_divide, zero, zero,
        MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO, 0
    );
    mmay_expect_binary(
        mmay_checked_integer_remainder, zero, zero,
        MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO, 0
    );
    mmay_expect_unary(
        mmay_checked_integer_negate, zero,
        MMAY_CHECKED_INTEGER_OK, zero.bits
    );
    mmay_expect_unary(
        mmay_checked_integer_negate, one,
        fixture->is_signed ? MMAY_CHECKED_INTEGER_OK : MMAY_CHECKED_INTEGER_OVERFLOW,
        fixture->is_signed ? negative_one.bits : 0
    );
    mmay_expect_unary(
        mmay_checked_integer_negate, minimum,
        fixture->is_signed ? MMAY_CHECKED_INTEGER_OVERFLOW
                           : MMAY_CHECKED_INTEGER_OK,
        0
    );
    mmay_expect_binary(
        mmay_checked_integer_add, one, one,
        MMAY_CHECKED_INTEGER_OK, expected_one_plus_one
    );
    if (fixture->is_signed) {
        mmay_expect_binary(
            mmay_checked_integer_subtract, minimum, one,
            MMAY_CHECKED_INTEGER_OVERFLOW, 0
        );
        mmay_expect_binary(
            mmay_checked_integer_multiply, minimum, negative_one,
            MMAY_CHECKED_INTEGER_OVERFLOW, 0
        );
        mmay_expect_binary(
            mmay_checked_integer_divide, minimum, negative_one,
            MMAY_CHECKED_INTEGER_OVERFLOW, 0
        );
        mmay_expect_binary(
            mmay_checked_integer_remainder, minimum, negative_one,
            MMAY_CHECKED_INTEGER_OVERFLOW, 0
        );
        mmay_expect_binary(
            mmay_checked_integer_divide, minus_seven, two,
            MMAY_CHECKED_INTEGER_OK, mmay_signed_bits(fixture, -3)
        );
        mmay_expect_binary(
            mmay_checked_integer_remainder, minus_seven, two,
            MMAY_CHECKED_INTEGER_OK, mmay_signed_bits(fixture, -1)
        );
    }
}

static void mmay_test_checked_integer_shifts(
    const MarimayIntegerFixture *fixture
) {
    MarimayIntegerValue one = mmay_bits_value(fixture, 1);
    MarimayIntegerValue minimum = mmay_bits_value(fixture, fixture->minimum);
    MarimayIntegerValue maximum = mmay_bits_value(fixture, fixture->maximum);
    MarimayIntegerValue count_zero = mmay_bits_value(fixture, 0);
    MarimayIntegerValue count_one = mmay_bits_value(fixture, 1);
    MarimayIntegerValue count_width = mmay_bits_value(fixture, fixture->width);
    MarimayIntegerValue negative_count = mmay_signed_value(fixture, -1);
    uint64_t expected_left = UINT64_C(2) & fixture->mask;
    uint64_t expected_min_right = fixture->is_signed
        ? fixture->sign_bit | (fixture->sign_bit >> 1)
        : fixture->minimum;

    mmay_expect_shift(
        mmay_checked_integer_shift_left, one, count_zero,
        MMAY_CHECKED_INTEGER_OK, one.bits
    );
    mmay_expect_shift(
        mmay_checked_integer_shift_left, one, count_one,
        MMAY_CHECKED_INTEGER_OK, expected_left
    );
    mmay_expect_shift(
        mmay_checked_integer_shift_right, maximum, count_one,
        MMAY_CHECKED_INTEGER_OK, fixture->is_signed
            ? fixture->maximum >> 1 : fixture->maximum >> 1
    );
    mmay_expect_shift(
        mmay_checked_integer_shift_right, minimum, count_one,
        MMAY_CHECKED_INTEGER_OK, expected_min_right
    );
    mmay_expect_shift(
        mmay_checked_integer_shift_left, one, count_width,
        MMAY_CHECKED_INTEGER_INVALID_SHIFT, 0
    );
    mmay_expect_shift(
        mmay_checked_integer_shift_right, one, count_width,
        MMAY_CHECKED_INTEGER_INVALID_SHIFT, 0
    );
    if (fixture->is_signed) {
        mmay_expect_shift(
            mmay_checked_integer_shift_left, one, negative_count,
            MMAY_CHECKED_INTEGER_INVALID_SHIFT, 0
        );
        mmay_expect_shift(
            mmay_checked_integer_shift_right, one, negative_count,
            MMAY_CHECKED_INTEGER_INVALID_SHIFT, 0
        );
        mmay_expect_shift(
            mmay_checked_integer_shift_left, maximum, count_one,
            MMAY_CHECKED_INTEGER_OVERFLOW, 0
        );
        mmay_expect_shift(
            mmay_checked_integer_shift_left, minimum, count_one,
            MMAY_CHECKED_INTEGER_OVERFLOW, 0
        );
    } else {
        mmay_expect_shift(
            mmay_checked_integer_shift_left, maximum, count_one,
            MMAY_CHECKED_INTEGER_OVERFLOW, 0
        );
    }
}

static void mmay_test_wrapping_integer_operations(
    const MarimayIntegerFixture *fixture
) {
    MarimayIntegerValue zero = mmay_bits_value(fixture, 0);
    MarimayIntegerValue one = mmay_bits_value(fixture, 1);
    MarimayIntegerValue two = mmay_bits_value(fixture, 2);
    MarimayIntegerValue minimum = mmay_bits_value(fixture, fixture->minimum);
    MarimayIntegerValue maximum = mmay_bits_value(fixture, fixture->maximum);
    MarimayIntegerValue negative_one = mmay_bits_value(fixture, fixture->mask);
    MarimayIntegerValue count_one = mmay_bits_value(fixture, 1);

    mmay_expect_binary(
        mmay_wrapping_integer_add, maximum, one,
        MMAY_CHECKED_INTEGER_OK, fixture->is_signed
            ? fixture->minimum : 0
    );
    mmay_expect_binary(
        mmay_wrapping_integer_subtract, zero, one,
        MMAY_CHECKED_INTEGER_OK, fixture->mask
    );
    mmay_expect_binary(
        mmay_wrapping_integer_multiply, maximum, two,
        MMAY_CHECKED_INTEGER_OK, (maximum.bits << 1) & fixture->mask
    );
    mmay_expect_unary(
        mmay_wrapping_integer_negate, minimum,
        MMAY_CHECKED_INTEGER_OK, minimum.bits
    );
    mmay_expect_unary(
        mmay_wrapping_integer_negate, one,
        MMAY_CHECKED_INTEGER_OK, fixture->is_signed
            ? negative_one.bits : fixture->mask
    );
    mmay_expect_binary(
        mmay_wrapping_integer_divide, zero, zero,
        MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO, 0
    );
    mmay_expect_binary(
        mmay_wrapping_integer_remainder, zero, zero,
        MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO, 0
    );
    if (fixture->is_signed) {
        mmay_expect_binary(
            mmay_wrapping_integer_divide, minimum, negative_one,
            MMAY_CHECKED_INTEGER_OK, minimum.bits
        );
        mmay_expect_binary(
            mmay_wrapping_integer_remainder, minimum, negative_one,
            MMAY_CHECKED_INTEGER_OK, 0
        );
    }
    mmay_expect_shift(
        mmay_wrapping_integer_shift_left, maximum, count_one,
        MMAY_CHECKED_INTEGER_OK, (maximum.bits << 1) & fixture->mask
    );
    mmay_expect_shift(
        mmay_wrapping_integer_shift_right, minimum, count_one,
        MMAY_CHECKED_INTEGER_OK, fixture->is_signed
            ? fixture->sign_bit | (fixture->sign_bit >> 1) : fixture->minimum
    );
    mmay_expect_shift(
        mmay_wrapping_integer_shift_left, one,
        mmay_bits_value(fixture, fixture->width),
        MMAY_CHECKED_INTEGER_INVALID_SHIFT, 0
    );
}

static void mmay_test_saturating_integer_operations(
    const MarimayIntegerFixture *fixture
) {
    MarimayIntegerValue zero = mmay_bits_value(fixture, 0);
    MarimayIntegerValue one = mmay_bits_value(fixture, 1);
    MarimayIntegerValue two = mmay_bits_value(fixture, 2);
    MarimayIntegerValue minimum = mmay_bits_value(fixture, fixture->minimum);
    MarimayIntegerValue maximum = mmay_bits_value(fixture, fixture->maximum);
    MarimayIntegerValue negative_one = mmay_bits_value(fixture, fixture->mask);
    MarimayIntegerValue count_one = mmay_bits_value(fixture, 1);
    MarimayIntegerValue count_width = mmay_bits_value(fixture, fixture->width);
    uint64_t expected_min_right = fixture->is_signed
        ? fixture->sign_bit | (fixture->sign_bit >> 1)
        : fixture->minimum;

    mmay_expect_binary(
        mmay_saturating_integer_add, maximum, one,
        MMAY_CHECKED_INTEGER_OK, maximum.bits
    );
    mmay_expect_binary(
        mmay_saturating_integer_subtract, zero, one,
        MMAY_CHECKED_INTEGER_OK, fixture->is_signed
            ? negative_one.bits : 0
    );
    mmay_expect_binary(
        mmay_saturating_integer_multiply, maximum, two,
        MMAY_CHECKED_INTEGER_OK, maximum.bits
    );
    mmay_expect_unary(
        mmay_saturating_integer_negate, one,
        MMAY_CHECKED_INTEGER_OK, fixture->is_signed ? negative_one.bits : 0
    );
    mmay_expect_binary(
        mmay_saturating_integer_divide, zero, zero,
        MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO, 0
    );
    mmay_expect_binary(
        mmay_saturating_integer_remainder, zero, zero,
        MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO, 0
    );
    mmay_expect_shift(
        mmay_saturating_integer_shift_left, maximum, count_one,
        MMAY_CHECKED_INTEGER_OK, maximum.bits
    );
    mmay_expect_shift(
        mmay_saturating_integer_shift_left, one, count_width,
        MMAY_CHECKED_INTEGER_INVALID_SHIFT, 0
    );
    mmay_expect_shift(
        mmay_saturating_integer_shift_right, minimum, count_one,
        MMAY_CHECKED_INTEGER_OK, expected_min_right
    );
    if (fixture->is_signed) {
        mmay_expect_binary(
            mmay_saturating_integer_add, minimum, negative_one,
            MMAY_CHECKED_INTEGER_OK, minimum.bits
        );
        mmay_expect_binary(
            mmay_saturating_integer_subtract, maximum, negative_one,
            MMAY_CHECKED_INTEGER_OK, maximum.bits
        );
        mmay_expect_binary(
            mmay_saturating_integer_multiply, minimum, negative_one,
            MMAY_CHECKED_INTEGER_OK, maximum.bits
        );
        mmay_expect_binary(
            mmay_saturating_integer_multiply, minimum, two,
            MMAY_CHECKED_INTEGER_OK, minimum.bits
        );
        mmay_expect_binary(
            mmay_saturating_integer_divide, minimum, negative_one,
            MMAY_CHECKED_INTEGER_OK, maximum.bits
        );
        mmay_expect_binary(
            mmay_saturating_integer_remainder, minimum, negative_one,
            MMAY_CHECKED_INTEGER_OK, 0
        );
        mmay_expect_unary(
            mmay_saturating_integer_negate, minimum,
            MMAY_CHECKED_INTEGER_OK, maximum.bits
        );
        mmay_expect_shift(
            mmay_saturating_integer_shift_left, minimum, count_one,
            MMAY_CHECKED_INTEGER_OK, minimum.bits
        );
    }
}

static void mmay_test_checked_integer_invalid_inputs(void) {
    MarimayIntegerValue left = mmay_integer_from_i8(1);
    MarimayIntegerValue right = mmay_integer_from_i16(1);
    MarimayIntegerValue malformed = { MMAY_INTEGER_I8, UINT64_C(0x100) };
    MarimayIntegerValue result = mmay_integer_from_i8(99);
    MMAY_EXPECT(mmay_checked_integer_add(left, right, &result)
        == MMAY_CHECKED_INTEGER_OVERFLOW);
    MMAY_EXPECT(result.bits == UINT64_C(99));
    MMAY_EXPECT(mmay_checked_integer_add(malformed, left, &result)
        == MMAY_CHECKED_INTEGER_OVERFLOW);
    MMAY_EXPECT(result.bits == UINT64_C(99));
    MMAY_EXPECT(mmay_checked_integer_add(left, right, NULL)
        == MMAY_CHECKED_INTEGER_OVERFLOW);
}

void mmay_test_checked_integer(void) {
    static const MarimayIntegerKind kinds[] = {
        MMAY_INTEGER_I8, MMAY_INTEGER_I16, MMAY_INTEGER_I32, MMAY_INTEGER_I64,
        MMAY_INTEGER_U8, MMAY_INTEGER_U16, MMAY_INTEGER_U32, MMAY_INTEGER_U64
    };
    size_t index = 0;
    mmay_test_checked_integer_shape();
    mmay_test_checked_integer_conversions();
    mmay_test_checked_integer_invalid_inputs();
    for (index = 0; index < sizeof(kinds) / sizeof(kinds[0]); index += 1) {
        MarimayIntegerFixture fixture = mmay_integer_fixture(kinds[index]);
        mmay_test_checked_integer_arithmetic(&fixture);
        mmay_test_checked_integer_shifts(&fixture);
        mmay_test_wrapping_integer_operations(&fixture);
        mmay_test_saturating_integer_operations(&fixture);
    }
}
