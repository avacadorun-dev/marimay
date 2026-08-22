#ifndef MMAY_BASE_CHECKED_INTEGER_H
#define MMAY_BASE_CHECKED_INTEGER_H

#include <stdbool.h>
#include <stdint.h>

/*
 * These statuses are the bootstrap spelling of the language's arithmetic
 * outcomes.  In particular, D035 invalid shifts and D042 division failures
 * are never folded into the generic overflow status.
 */
typedef enum MarimayCheckedIntegerStatus {
    MMAY_CHECKED_INTEGER_OK = 0,
    MMAY_CHECKED_INTEGER_OVERFLOW,
    MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO,
    MMAY_CHECKED_INTEGER_INVALID_SHIFT
} MarimayCheckedIntegerStatus;

typedef enum MarimayIntegerKind {
    MMAY_INTEGER_I8 = 0,
    MMAY_INTEGER_I16,
    MMAY_INTEGER_I32,
    MMAY_INTEGER_I64,
    MMAY_INTEGER_U8,
    MMAY_INTEGER_U16,
    MMAY_INTEGER_U32,
    MMAY_INTEGER_U64
} MarimayIntegerKind;

/* bits is the canonical low-width two's-complement bit pattern. */
typedef struct MarimayIntegerValue {
    MarimayIntegerKind kind;
    uint64_t bits;
} MarimayIntegerValue;

/* Operands must have the same kind. Wrapping and saturating helpers return
 * OK after handling arithmetic overflow, but preserve divide-by-zero and
 * invalid-shift statuses. */

unsigned int mmay_integer_bit_width(MarimayIntegerKind kind);
bool mmay_integer_kind_is_signed(MarimayIntegerKind kind);
bool mmay_integer_convert(
    MarimayIntegerValue value,
    MarimayIntegerKind target_kind,
    MarimayIntegerValue *result
);

MarimayIntegerValue mmay_integer_from_i8(int8_t value);
MarimayIntegerValue mmay_integer_from_i16(int16_t value);
MarimayIntegerValue mmay_integer_from_i32(int32_t value);
MarimayIntegerValue mmay_integer_from_i64(int64_t value);
MarimayIntegerValue mmay_integer_from_u8(uint8_t value);
MarimayIntegerValue mmay_integer_from_u16(uint16_t value);
MarimayIntegerValue mmay_integer_from_u32(uint32_t value);
MarimayIntegerValue mmay_integer_from_u64(uint64_t value);

bool mmay_integer_to_i8(MarimayIntegerValue value, int8_t *result);
bool mmay_integer_to_i16(MarimayIntegerValue value, int16_t *result);
bool mmay_integer_to_i32(MarimayIntegerValue value, int32_t *result);
bool mmay_integer_to_i64(MarimayIntegerValue value, int64_t *result);
bool mmay_integer_to_u8(MarimayIntegerValue value, uint8_t *result);
bool mmay_integer_to_u16(MarimayIntegerValue value, uint16_t *result);
bool mmay_integer_to_u32(MarimayIntegerValue value, uint32_t *result);
bool mmay_integer_to_u64(MarimayIntegerValue value, uint64_t *result);

MarimayCheckedIntegerStatus mmay_checked_integer_add(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_checked_integer_subtract(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_checked_integer_multiply(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_checked_integer_divide(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_checked_integer_remainder(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_checked_integer_negate(
    MarimayIntegerValue value, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_checked_integer_shift_left(
    MarimayIntegerValue value, MarimayIntegerValue count, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_checked_integer_shift_right(
    MarimayIntegerValue value, MarimayIntegerValue count, MarimayIntegerValue *result
);

MarimayCheckedIntegerStatus mmay_wrapping_integer_add(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_wrapping_integer_subtract(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_wrapping_integer_multiply(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_wrapping_integer_divide(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_wrapping_integer_remainder(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_wrapping_integer_negate(
    MarimayIntegerValue value, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_wrapping_integer_shift_left(
    MarimayIntegerValue value, MarimayIntegerValue count, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_wrapping_integer_shift_right(
    MarimayIntegerValue value, MarimayIntegerValue count, MarimayIntegerValue *result
);

MarimayCheckedIntegerStatus mmay_saturating_integer_add(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_saturating_integer_subtract(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_saturating_integer_multiply(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_saturating_integer_divide(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_saturating_integer_remainder(
    MarimayIntegerValue left, MarimayIntegerValue right, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_saturating_integer_negate(
    MarimayIntegerValue value, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_saturating_integer_shift_left(
    MarimayIntegerValue value, MarimayIntegerValue count, MarimayIntegerValue *result
);
MarimayCheckedIntegerStatus mmay_saturating_integer_shift_right(
    MarimayIntegerValue value, MarimayIntegerValue count, MarimayIntegerValue *result
);

#endif
