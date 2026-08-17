#ifndef RUDA_BASE_CHECKED_INTEGER_H
#define RUDA_BASE_CHECKED_INTEGER_H

#include <stdbool.h>
#include <stdint.h>

/*
 * These statuses are the bootstrap spelling of the language's arithmetic
 * outcomes.  In particular, D035 invalid shifts and D042 division failures
 * are never folded into the generic overflow status.
 */
typedef enum RudaCheckedIntegerStatus {
    RUDA_CHECKED_INTEGER_OK = 0,
    RUDA_CHECKED_INTEGER_OVERFLOW,
    RUDA_CHECKED_INTEGER_DIVISION_BY_ZERO,
    RUDA_CHECKED_INTEGER_INVALID_SHIFT
} RudaCheckedIntegerStatus;

typedef enum RudaIntegerKind {
    RUDA_INTEGER_I8 = 0,
    RUDA_INTEGER_I16,
    RUDA_INTEGER_I32,
    RUDA_INTEGER_I64,
    RUDA_INTEGER_U8,
    RUDA_INTEGER_U16,
    RUDA_INTEGER_U32,
    RUDA_INTEGER_U64
} RudaIntegerKind;

/* bits is the canonical low-width two's-complement bit pattern. */
typedef struct RudaIntegerValue {
    RudaIntegerKind kind;
    uint64_t bits;
} RudaIntegerValue;

/* Operands must have the same kind. Wrapping and saturating helpers return
 * OK after handling arithmetic overflow, but preserve divide-by-zero and
 * invalid-shift statuses. */

unsigned int ruda_integer_bit_width(RudaIntegerKind kind);
bool ruda_integer_kind_is_signed(RudaIntegerKind kind);

RudaIntegerValue ruda_integer_from_i8(int8_t value);
RudaIntegerValue ruda_integer_from_i16(int16_t value);
RudaIntegerValue ruda_integer_from_i32(int32_t value);
RudaIntegerValue ruda_integer_from_i64(int64_t value);
RudaIntegerValue ruda_integer_from_u8(uint8_t value);
RudaIntegerValue ruda_integer_from_u16(uint16_t value);
RudaIntegerValue ruda_integer_from_u32(uint32_t value);
RudaIntegerValue ruda_integer_from_u64(uint64_t value);

bool ruda_integer_to_i8(RudaIntegerValue value, int8_t *result);
bool ruda_integer_to_i16(RudaIntegerValue value, int16_t *result);
bool ruda_integer_to_i32(RudaIntegerValue value, int32_t *result);
bool ruda_integer_to_i64(RudaIntegerValue value, int64_t *result);
bool ruda_integer_to_u8(RudaIntegerValue value, uint8_t *result);
bool ruda_integer_to_u16(RudaIntegerValue value, uint16_t *result);
bool ruda_integer_to_u32(RudaIntegerValue value, uint32_t *result);
bool ruda_integer_to_u64(RudaIntegerValue value, uint64_t *result);

RudaCheckedIntegerStatus ruda_checked_integer_add(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_checked_integer_subtract(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_checked_integer_multiply(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_checked_integer_divide(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_checked_integer_remainder(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_checked_integer_negate(
    RudaIntegerValue value, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_checked_integer_shift_left(
    RudaIntegerValue value, RudaIntegerValue count, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_checked_integer_shift_right(
    RudaIntegerValue value, RudaIntegerValue count, RudaIntegerValue *result
);

RudaCheckedIntegerStatus ruda_wrapping_integer_add(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_wrapping_integer_subtract(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_wrapping_integer_multiply(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_wrapping_integer_divide(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_wrapping_integer_remainder(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_wrapping_integer_negate(
    RudaIntegerValue value, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_wrapping_integer_shift_left(
    RudaIntegerValue value, RudaIntegerValue count, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_wrapping_integer_shift_right(
    RudaIntegerValue value, RudaIntegerValue count, RudaIntegerValue *result
);

RudaCheckedIntegerStatus ruda_saturating_integer_add(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_saturating_integer_subtract(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_saturating_integer_multiply(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_saturating_integer_divide(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_saturating_integer_remainder(
    RudaIntegerValue left, RudaIntegerValue right, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_saturating_integer_negate(
    RudaIntegerValue value, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_saturating_integer_shift_left(
    RudaIntegerValue value, RudaIntegerValue count, RudaIntegerValue *result
);
RudaCheckedIntegerStatus ruda_saturating_integer_shift_right(
    RudaIntegerValue value, RudaIntegerValue count, RudaIntegerValue *result
);

#endif
