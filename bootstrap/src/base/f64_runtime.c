#include "ruda/base/f64.h"

#include <fenv.h>
#include <float.h>
#include <string.h>

#if defined(_MSC_VER)
#pragma fenv_access(on)
#else
#pragma STDC FENV_ACCESS ON
#pragma STDC FP_CONTRACT OFF
#endif

static const uint64_t ruda_f64_canonical_nan =
    UINT64_C(0x7ff8000000000000);
static const uint64_t ruda_f64_sign_mask = UINT64_C(0x8000000000000000);
static const uint64_t ruda_f64_exponent_mask = UINT64_C(0x7ff0000000000000);
static const uint64_t ruda_f64_fraction_mask = UINT64_C(0x000fffffffffffff);

double ruda_f64_to_double(uint64_t bits) {
    double value = 0.0;
    if (sizeof(value) == sizeof(bits)) {
        memcpy(&value, &bits, sizeof(value));
    }
    return value;
}

uint64_t ruda_f64_bits(double value) {
    uint64_t bits = 0;
    if (sizeof(value) == sizeof(bits)) {
        memcpy(&bits, &value, sizeof(bits));
    }
    return bits;
}

bool ruda_f64_binary64_supported(void) {
    fenv_t environment;
    volatile double minimum_normal = 0.0;
    volatile double minimum_subnormal = 0.0;
    volatile double one_half = 0.5;
    volatile double subnormal = 0.0;
    volatile double consumed_subnormal = 0.0;
    bool supported = sizeof(double) == sizeof(uint64_t)
        && FLT_RADIX == 2
        && DBL_MANT_DIG == 53
        && DBL_MAX_EXP == 1024
        && ruda_f64_bits(1.0) == UINT64_C(0x3ff0000000000000)
        && ruda_f64_bits(-0.0) == UINT64_C(0x8000000000000000);
#if defined(__FAST_MATH__) || defined(_M_FP_FAST)
    supported = false;
#endif
    if (!supported || feholdexcept(&environment) != 0) {
        return false;
    }
    supported = fesetround(FE_TONEAREST) == 0
        && fegetround() == FE_TONEAREST;
    if (supported) {
        minimum_normal = ruda_f64_to_double(UINT64_C(0x0010000000000000));
        minimum_subnormal = ruda_f64_to_double(UINT64_C(0x0000000000000001));
        subnormal = minimum_normal * one_half;
        consumed_subnormal = minimum_normal + minimum_subnormal;
        supported = ruda_f64_bits(subnormal)
                == UINT64_C(0x0008000000000000)
            && ruda_f64_bits(consumed_subnormal)
                == UINT64_C(0x0010000000000001);
    }
    if (fesetenv(&environment) != 0) {
        return false;
    }
    return supported;
}

static bool ruda_f64_is_nan(uint64_t bits) {
    return (bits & ruda_f64_exponent_mask) == ruda_f64_exponent_mask
        && (bits & ruda_f64_fraction_mask) != 0;
}

typedef enum RudaF64BinaryOperation {
    RUDA_F64_BINARY_ADD,
    RUDA_F64_BINARY_SUBTRACT,
    RUDA_F64_BINARY_MULTIPLY,
    RUDA_F64_BINARY_DIVIDE
} RudaF64BinaryOperation;

static uint64_t ruda_f64_binary_operation(
    uint64_t left,
    uint64_t right,
    RudaF64BinaryOperation operation
) {
    fenv_t environment;
    volatile double left_value = 0.0;
    volatile double right_value = 0.0;
    volatile double computed = 0.0;
    uint64_t bits = ruda_f64_canonical_nan;
    if (ruda_f64_is_nan(left) || ruda_f64_is_nan(right)
        || !ruda_f64_binary64_supported()) {
        return ruda_f64_canonical_nan;
    }
    if (feholdexcept(&environment) != 0) {
        return ruda_f64_canonical_nan;
    }
    if (fesetround(FE_TONEAREST) != 0) {
        (void)fesetenv(&environment);
        return ruda_f64_canonical_nan;
    }
    left_value = ruda_f64_to_double(left);
    right_value = ruda_f64_to_double(right);
    switch (operation) {
        case RUDA_F64_BINARY_ADD:
            computed = left_value + right_value;
            break;
        case RUDA_F64_BINARY_SUBTRACT:
            computed = left_value - right_value;
            break;
        case RUDA_F64_BINARY_MULTIPLY:
            computed = left_value * right_value;
            break;
        case RUDA_F64_BINARY_DIVIDE:
            computed = left_value / right_value;
            break;
    }
    bits = ruda_f64_bits(computed);
    if (ruda_f64_is_nan(bits)) {
        bits = ruda_f64_canonical_nan;
    }
    if (fesetenv(&environment) != 0) {
        return ruda_f64_canonical_nan;
    }
    return bits;
}

uint64_t ruda_f64_add(uint64_t left, uint64_t right) {
    return ruda_f64_binary_operation(left, right, RUDA_F64_BINARY_ADD);
}

uint64_t ruda_f64_subtract(uint64_t left, uint64_t right) {
    return ruda_f64_binary_operation(left, right, RUDA_F64_BINARY_SUBTRACT);
}

uint64_t ruda_f64_multiply(uint64_t left, uint64_t right) {
    return ruda_f64_binary_operation(left, right, RUDA_F64_BINARY_MULTIPLY);
}

uint64_t ruda_f64_divide(uint64_t left, uint64_t right) {
    return ruda_f64_binary_operation(left, right, RUDA_F64_BINARY_DIVIDE);
}

uint64_t ruda_f64_negate(uint64_t value) {
    return value ^ ruda_f64_sign_mask;
}

bool ruda_f64_equal(uint64_t left, uint64_t right) {
    if (ruda_f64_is_nan(left) || ruda_f64_is_nan(right)) {
        return false;
    }
    if ((left & ~ruda_f64_sign_mask) == 0
        && (right & ~ruda_f64_sign_mask) == 0) {
        return true;
    }
    return left == right;
}

bool ruda_f64_less(uint64_t left, uint64_t right) {
    uint64_t left_order = 0;
    uint64_t right_order = 0;
    if (ruda_f64_is_nan(left) || ruda_f64_is_nan(right)
        || ruda_f64_equal(left, right)) {
        return false;
    }
    left_order = (left & ruda_f64_sign_mask) != 0
        ? ~left : left | ruda_f64_sign_mask;
    right_order = (right & ruda_f64_sign_mask) != 0
        ? ~right : right | ruda_f64_sign_mask;
    return left_order < right_order;
}

bool ruda_f64_less_equal(uint64_t left, uint64_t right) {
    return ruda_f64_equal(left, right) || ruda_f64_less(left, right);
}
