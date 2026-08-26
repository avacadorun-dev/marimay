#include "test_bytecode_support.h"

void mmay_test_bytecode_metadata(void) {
    static const MarimayTestOpcodeExpectation expected[] = {
        {MMAY_OPCODE_INVALID, 0, "invalid", 0,
            {MMAY_BYTECODE_OPERAND_NONE, MMAY_BYTECODE_OPERAND_NONE, MMAY_BYTECODE_OPERAND_NONE},
            MMAY_OPCODE_FLAG_INVALID},
        {MMAY_OPCODE_CONST_I64, 1, "const_i64", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_I64, MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_CONST_BOOL, 2, "const_bool", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_BOOL, MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_COPY, 3, "copy", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_I64_ADD_CHECKED, 4, "i64_add_checked", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_I64_SUBTRACT_CHECKED, 5, "i64_subtract_checked", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_I64_MULTIPLY_CHECKED, 6, "i64_multiply_checked", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_I64_DIVIDE_CHECKED, 7, "i64_divide_checked", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_I64_REMAINDER_CHECKED, 8, "i64_remainder_checked", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_I64_NEGATE_CHECKED, 9, "i64_negate_checked", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_BOOL_NOT, 10, "bool_not", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_I64_EQUAL, 11, "i64_equal", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_BOOL_EQUAL, 12, "bool_equal", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_I64_LESS, 13, "i64_less", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_I64_LESS_EQUAL, 14, "i64_less_equal", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_JUMP, 15, "jump", 1,
            {MMAY_BYTECODE_OPERAND_BLOCK, MMAY_BYTECODE_OPERAND_NONE, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_TERMINATOR | MMAY_OPCODE_FLAG_BRANCH},
        {MMAY_OPCODE_BRANCH_BOOL, 16, "branch_bool", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_BLOCK, MMAY_BYTECODE_OPERAND_BLOCK}, MMAY_OPCODE_FLAG_TERMINATOR | MMAY_OPCODE_FLAG_BRANCH},
        {MMAY_OPCODE_RETURN, 17, "return", 1,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_NONE, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_TERMINATOR},
        {MMAY_OPCODE_RETURN_VOID, 18, "return_void", 0,
            {MMAY_BYTECODE_OPERAND_NONE, MMAY_BYTECODE_OPERAND_NONE, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_TERMINATOR},
        {MMAY_OPCODE_I64_BITWISE_NOT, 19, "i64_bitwise_not", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_I64_BITWISE_AND, 20, "i64_bitwise_and", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_I64_BITWISE_XOR, 21, "i64_bitwise_xor", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_I64_BITWISE_OR, 22, "i64_bitwise_or", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, 23, "i64_shift_left_checked", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_I64_SHIFT_RIGHT, 24, "i64_shift_right", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_CALL_DIRECT, 25, "call_direct", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_CALL, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_CALL_DIRECT_VOID, 26, "call_direct_void", 1,
            {MMAY_BYTECODE_OPERAND_CALL, MMAY_BYTECODE_OPERAND_NONE, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_CONST_STRING, 27, "const_string", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_STRING, MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_STRING_EQUAL, 28, "string_equal", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_PANIC_STRING, 29, "panic_string", 1,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_NONE, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_TERMINATOR | MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_CALL_DIRECT_NEVER, 30, "call_direct_never", 1,
            {MMAY_BYTECODE_OPERAND_CALL, MMAY_BYTECODE_OPERAND_NONE, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_TERMINATOR | MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_CONST_F64, 31, "const_f64", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_F64_BITS, MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_F64_ADD, 32, "f64_add", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_F64_SUBTRACT, 33, "f64_subtract", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_F64_MULTIPLY, 34, "f64_multiply", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_F64_DIVIDE, 35, "f64_divide", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_F64_NEGATE, 36, "f64_negate", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_F64_EQUAL, 37, "f64_equal", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_F64_LESS, 38, "f64_less", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_F64_LESS_EQUAL, 39, "f64_less_equal", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_ARRAY_NEW, 40, "array_new", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_ARRAY_CONSTRUCTION, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_ARRAY_LENGTH, 41, "array_length", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 42, "array_require_index", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_ARRAY_GET, 43, "array_get", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_ARRAY_SET, 44, "array_set", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_RECORD_NEW, 45, "record_new", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_RECORD_CONSTRUCTION, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_RECORD_GET, 46, "record_get", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_I64}, 0},
        {MMAY_OPCODE_RECORD_SET, 47, "record_set", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_I64}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_RECORD_IDENTITY_SAME, 48, "record_identity_same", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_ENUM_NEW, 49, "enum_new", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_ENUM_CONSTRUCTION, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_ENUM_IS_VARIANT, 50, "enum_is_variant", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_I64}, 0},
        {MMAY_OPCODE_ENUM_REQUIRE_VARIANT, 51, "enum_require_variant", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_I64}, 0},
        {MMAY_OPCODE_ENUM_GET_PAYLOAD, 52, "enum_get_payload", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_I64}, 0},
        {MMAY_OPCODE_INTERFACE_NEW, 53, "interface_new", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_INTERFACE_CONSTRUCTION, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_CALL_INTERFACE, 54, "call_interface", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_CALL, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_ARRAY_PUSH, 55, "array_push", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_CONST_INTEGER, 56, "const_integer", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_U64, MMAY_BYTECODE_OPERAND_NONE}, 0},
#define MMAY_NUMERIC_BINARY(opcode, wire, mnemonic, flags) \
        {opcode, wire, mnemonic, 3, \
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER}, flags}
#define MMAY_NUMERIC_UNARY(opcode, wire, mnemonic, flags) \
        {opcode, wire, mnemonic, 2, \
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_NONE}, flags}
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_CHECKED_ADD, 57,
            "integer_checked_add", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_CHECKED_SUBTRACT, 58,
            "integer_checked_subtract", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_CHECKED_MULTIPLY, 59,
            "integer_checked_multiply", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_CHECKED_DIVIDE, 60,
            "integer_checked_divide", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_CHECKED_REMAINDER, 61,
            "integer_checked_remainder", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_UNARY(MMAY_OPCODE_INTEGER_CHECKED_NEGATE, 62,
            "integer_checked_negate", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_CHECKED_SHIFT_LEFT, 63,
            "integer_checked_shift_left", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_CHECKED_SHIFT_RIGHT, 64,
            "integer_checked_shift_right", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_WRAPPING_ADD, 65,
            "integer_wrapping_add", 0),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_WRAPPING_SUBTRACT, 66,
            "integer_wrapping_subtract", 0),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_WRAPPING_MULTIPLY, 67,
            "integer_wrapping_multiply", 0),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_WRAPPING_DIVIDE, 68,
            "integer_wrapping_divide", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_WRAPPING_REMAINDER, 69,
            "integer_wrapping_remainder", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_UNARY(MMAY_OPCODE_INTEGER_WRAPPING_NEGATE, 70,
            "integer_wrapping_negate", 0),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_WRAPPING_SHIFT_LEFT, 71,
            "integer_wrapping_shift_left", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_WRAPPING_SHIFT_RIGHT, 72,
            "integer_wrapping_shift_right", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_SATURATING_ADD, 73,
            "integer_saturating_add", 0),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_SATURATING_SUBTRACT, 74,
            "integer_saturating_subtract", 0),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_SATURATING_MULTIPLY, 75,
            "integer_saturating_multiply", 0),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_SATURATING_DIVIDE, 76,
            "integer_saturating_divide", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_SATURATING_REMAINDER, 77,
            "integer_saturating_remainder", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_UNARY(MMAY_OPCODE_INTEGER_SATURATING_NEGATE, 78,
            "integer_saturating_negate", 0),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_SATURATING_SHIFT_LEFT, 79,
            "integer_saturating_shift_left", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_BINARY(MMAY_OPCODE_INTEGER_SATURATING_SHIFT_RIGHT, 80,
            "integer_saturating_shift_right", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_UNARY(MMAY_OPCODE_INTEGER_CONVERT_TOTAL, 81,
            "integer_convert_total", 0),
        MMAY_NUMERIC_UNARY(MMAY_OPCODE_INTEGER_CONVERT_OPTION, 82,
            "integer_convert_option", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_UNARY(MMAY_OPCODE_FORMAT_F32, 83,
            "format_f32", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_UNARY(MMAY_OPCODE_FORMAT_F64, 84,
            "format_f64", MMAY_OPCODE_FLAG_MAY_PANIC),
        {MMAY_OPCODE_DEFER_REGISTER, 85, "defer_register", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_CALL,
                MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_DEFER_CALL, 86, "defer_call", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_CALL,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_IO_PRINTLN, 87, "io_println", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_IO_WRITE, 105, "io_write", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_I64}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_IO_FLUSH, 106, "io_flush", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_I64,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_IO_READ_LINE, 107, "io_read_line", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_I64}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_TEST_OUTPUT_BEGIN, 88, "test_output_begin", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_TEST_OUTPUT_END, 89, "test_output_end", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_ARRAY_POP, 90, "array_pop", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_ARRAY_TRUNCATE, 91, "array_truncate", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_ARRAY_CLEAR, 92, "array_clear", 1,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_ARRAY_RESERVE, 93, "array_reserve", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_ARRAY_GENERATION, 94, "array_generation", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_ARRAY_REQUIRE_GENERATION, 95,
            "array_require_generation", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_ARRAY_GET_IN_BOUNDS, 96, "array_get_in_bounds", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_ARRAY_SET_IN_BOUNDS, 97, "array_set_in_bounds", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_I64_ADD_PROVEN, 98, "i64_add_proven", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_I64_SUBTRACT_PROVEN, 99, "i64_subtract_proven", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_I64_MULTIPLY_PROVEN, 100, "i64_multiply_proven", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        MMAY_NUMERIC_UNARY(MMAY_OPCODE_FORMAT_BOOL, 101,
            "format_bool", MMAY_OPCODE_FLAG_MAY_PANIC),
        MMAY_NUMERIC_UNARY(MMAY_OPCODE_FORMAT_INTEGER, 102,
            "format_integer", MMAY_OPCODE_FLAG_MAY_PANIC),
        {MMAY_OPCODE_STRING_CONCAT, 103, "string_concat", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_PARSE_INTEGER, 104, "parse_integer", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_TEXT_BYTE_LENGTH, 108, "text_byte_length", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_TEXT_RUNE_COUNT, 109, "text_rune_count", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_TEXT_GRAPHEME_COUNT, 110, "text_grapheme_count", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_TEXT_IS_RUNE_BOUNDARY, 111, "text_is_rune_boundary", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_TEXT_DROP_PREFIX, 112, "text_drop_prefix", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_TEXT_TAKE_PREFIX, 113, "text_take_prefix", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_TEXT_CONTAINS, 114, "text_contains", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_TEXT_STARTS_WITH, 115, "text_starts_with", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_TEXT_ENDS_WITH, 116, "text_ends_with", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_TEXT_TRIM_ASCII, 117, "text_trim_ascii", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_TEXT_SPLIT_COUNT, 118, "text_split_count", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, 0},
        {MMAY_OPCODE_TEXT_SPLIT, 119, "text_split", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_TEXT_JOIN, 120, "text_join", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_REGISTER}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_TEXT_RUNES, 121, "text_runes", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_TEXT_GRAPHEMES, 122, "text_graphemes", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_TEXT_ENCODE_UTF8, 123, "text_encode_utf8", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_TEXT_INVALID_UTF8_OFFSET, 124,
            "text_invalid_utf8_offset", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, 0},
        {MMAY_OPCODE_TEXT_DECODE_UTF8, 125, "text_decode_utf8", 2,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_NONE}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_IO_READ_BYTES, 126, "io_read_bytes", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_I64}, MMAY_OPCODE_FLAG_MAY_PANIC},
        {MMAY_OPCODE_IO_WRITE_BYTES, 127, "io_write_bytes", 3,
            {MMAY_BYTECODE_OPERAND_REGISTER, MMAY_BYTECODE_OPERAND_REGISTER,
                MMAY_BYTECODE_OPERAND_I64}, MMAY_OPCODE_FLAG_MAY_PANIC}
#undef MMAY_NUMERIC_UNARY
#undef MMAY_NUMERIC_BINARY
    };
    size_t index = 0;

    MMAY_EXPECT(MMAY_OPCODE_INVALID == 0);
    MMAY_EXPECT(MMAY_OPCODE_CONST_I64 == 1);
    MMAY_EXPECT(MMAY_OPCODE_CONST_BOOL == 2);
    MMAY_EXPECT(MMAY_OPCODE_COPY == 3);
    MMAY_EXPECT(MMAY_OPCODE_I64_ADD_CHECKED == 4);
    MMAY_EXPECT(MMAY_OPCODE_I64_SUBTRACT_CHECKED == 5);
    MMAY_EXPECT(MMAY_OPCODE_I64_MULTIPLY_CHECKED == 6);
    MMAY_EXPECT(MMAY_OPCODE_I64_DIVIDE_CHECKED == 7);
    MMAY_EXPECT(MMAY_OPCODE_I64_REMAINDER_CHECKED == 8);
    MMAY_EXPECT(MMAY_OPCODE_I64_NEGATE_CHECKED == 9);
    MMAY_EXPECT(MMAY_OPCODE_BOOL_NOT == 10);
    MMAY_EXPECT(MMAY_OPCODE_I64_EQUAL == 11);
    MMAY_EXPECT(MMAY_OPCODE_BOOL_EQUAL == 12);
    MMAY_EXPECT(MMAY_OPCODE_I64_LESS == 13);
    MMAY_EXPECT(MMAY_OPCODE_I64_LESS_EQUAL == 14);
    MMAY_EXPECT(MMAY_OPCODE_JUMP == 15);
    MMAY_EXPECT(MMAY_OPCODE_BRANCH_BOOL == 16);
    MMAY_EXPECT(MMAY_OPCODE_RETURN == 17);
    MMAY_EXPECT(MMAY_OPCODE_RETURN_VOID == 18);
    MMAY_EXPECT(MMAY_OPCODE_I64_BITWISE_NOT == 19);
    MMAY_EXPECT(MMAY_OPCODE_I64_BITWISE_AND == 20);
    MMAY_EXPECT(MMAY_OPCODE_I64_BITWISE_XOR == 21);
    MMAY_EXPECT(MMAY_OPCODE_I64_BITWISE_OR == 22);
    MMAY_EXPECT(MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED == 23);
    MMAY_EXPECT(MMAY_OPCODE_I64_SHIFT_RIGHT == 24);
    MMAY_EXPECT(MMAY_OPCODE_CALL_DIRECT == 25);
    MMAY_EXPECT(MMAY_OPCODE_CALL_DIRECT_VOID == 26);
    MMAY_EXPECT(MMAY_OPCODE_CONST_STRING == 27);
    MMAY_EXPECT(MMAY_OPCODE_STRING_EQUAL == 28);
    MMAY_EXPECT(MMAY_OPCODE_PANIC_STRING == 29);
    MMAY_EXPECT(MMAY_OPCODE_CALL_DIRECT_NEVER == 30);
    MMAY_EXPECT(MMAY_OPCODE_CONST_F64 == 31);
    MMAY_EXPECT(MMAY_OPCODE_F64_ADD == 32);
    MMAY_EXPECT(MMAY_OPCODE_F64_SUBTRACT == 33);
    MMAY_EXPECT(MMAY_OPCODE_F64_MULTIPLY == 34);
    MMAY_EXPECT(MMAY_OPCODE_F64_DIVIDE == 35);
    MMAY_EXPECT(MMAY_OPCODE_F64_NEGATE == 36);
    MMAY_EXPECT(MMAY_OPCODE_F64_EQUAL == 37);
    MMAY_EXPECT(MMAY_OPCODE_F64_LESS == 38);
    MMAY_EXPECT(MMAY_OPCODE_F64_LESS_EQUAL == 39);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_NEW == 40);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_LENGTH == 41);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_REQUIRE_INDEX == 42);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_GET == 43);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_SET == 44);
    MMAY_EXPECT(MMAY_OPCODE_RECORD_NEW == 45);
    MMAY_EXPECT(MMAY_OPCODE_RECORD_GET == 46);
    MMAY_EXPECT(MMAY_OPCODE_RECORD_SET == 47);
    MMAY_EXPECT(MMAY_OPCODE_RECORD_IDENTITY_SAME == 48);
    MMAY_EXPECT(MMAY_OPCODE_ENUM_NEW == 49);
    MMAY_EXPECT(MMAY_OPCODE_ENUM_IS_VARIANT == 50);
    MMAY_EXPECT(MMAY_OPCODE_ENUM_REQUIRE_VARIANT == 51);
    MMAY_EXPECT(MMAY_OPCODE_ENUM_GET_PAYLOAD == 52);
    MMAY_EXPECT(MMAY_OPCODE_INTERFACE_NEW == 53);
    MMAY_EXPECT(MMAY_OPCODE_CALL_INTERFACE == 54);
    MMAY_EXPECT(MMAY_OPCODE_CONST_INTEGER == 56);
    MMAY_EXPECT(MMAY_OPCODE_INTEGER_CHECKED_ADD == 57);
    MMAY_EXPECT(MMAY_OPCODE_INTEGER_CONVERT_OPTION == 82);
    MMAY_EXPECT(MMAY_OPCODE_FORMAT_F64 == 84);
    MMAY_EXPECT(MMAY_OPCODE_DEFER_REGISTER == 85);
    MMAY_EXPECT(MMAY_OPCODE_DEFER_CALL == 86);
    MMAY_EXPECT(MMAY_OPCODE_IO_PRINTLN == 87);
    MMAY_EXPECT(MMAY_OPCODE_TEST_OUTPUT_BEGIN == 88);
    MMAY_EXPECT(MMAY_OPCODE_TEST_OUTPUT_END == 89);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_POP == 90);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_TRUNCATE == 91);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_CLEAR == 92);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_RESERVE == 93);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_GENERATION == 94);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_REQUIRE_GENERATION == 95);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_GET_IN_BOUNDS == 96);
    MMAY_EXPECT(MMAY_OPCODE_ARRAY_SET_IN_BOUNDS == 97);
    MMAY_EXPECT(MMAY_OPCODE_I64_ADD_PROVEN == 98);
    MMAY_EXPECT(MMAY_OPCODE_I64_SUBTRACT_PROVEN == 99);
    MMAY_EXPECT(MMAY_OPCODE_I64_MULTIPLY_PROVEN == 100);
    MMAY_EXPECT(MMAY_OPCODE_FORMAT_BOOL == 101);
    MMAY_EXPECT(MMAY_OPCODE_FORMAT_INTEGER == 102);
    MMAY_EXPECT(MMAY_OPCODE_STRING_CONCAT == 103);
    MMAY_EXPECT(MMAY_OPCODE_PARSE_INTEGER == 104);
    MMAY_EXPECT(MMAY_OPCODE_IO_WRITE == 105);
    MMAY_EXPECT(MMAY_OPCODE_IO_FLUSH == 106);
    MMAY_EXPECT(MMAY_OPCODE_IO_READ_LINE == 107);
    MMAY_EXPECT(MMAY_OPCODE_TEXT_BYTE_LENGTH == 108);
    MMAY_EXPECT(MMAY_OPCODE_TEXT_DECODE_UTF8 == 125);
    MMAY_EXPECT(MMAY_OPCODE_IO_READ_BYTES == 126);
    MMAY_EXPECT(MMAY_OPCODE_IO_WRITE_BYTES == 127);
    MMAY_EXPECT(MMAY_OPCODE_COUNT == sizeof(expected) / sizeof(expected[0]));
    for (index = 0; index < sizeof(expected) / sizeof(expected[0]); index += 1) {
        const MarimayOpcodeMetadata *metadata = mmay_opcode_metadata(expected[index].opcode);
        const MarimayOpcodeMetadata *from_wire = mmay_opcode_metadata_from_wire(
            expected[index].wire_id
        );
        MMAY_EXPECT(metadata != NULL);
        MMAY_EXPECT(from_wire == metadata);
        if (metadata != NULL) {
            MMAY_EXPECT(metadata->opcode == expected[index].opcode);
            MMAY_EXPECT(metadata->wire_id == expected[index].wire_id);
            MMAY_EXPECT(strcmp(metadata->mnemonic, expected[index].mnemonic) == 0);
            MMAY_EXPECT(metadata->operand_count == expected[index].operand_count);
            MMAY_EXPECT(memcmp(metadata->operands, expected[index].operands,
                sizeof(metadata->operands)) == 0);
            MMAY_EXPECT(metadata->flags == expected[index].flags);
        }
    }
    MMAY_EXPECT(mmay_opcode_metadata((MarimayOpcode)-1) == NULL);
    MMAY_EXPECT(mmay_opcode_metadata((MarimayOpcode)MMAY_OPCODE_COUNT) == NULL);
    MMAY_EXPECT(mmay_opcode_metadata_from_wire(MMAY_OPCODE_COUNT) == NULL);
}

void mmay_test_bytecode_valid_and_disassembly(void) {
    static const unsigned char fuzz_regression[] = {
        0x52, 0x55, 0x44, 0x41, 0x42, 0x43, 0x46, 0x00, 0x01, 0x00,
        0x01, 0x63, 0x12, 0x12, 0x13, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x12, 0x12, 0x12, 0x12, 0x13, 0x12, 0x12, 0x12, 0x12, 0x01,
        0x01, 0x01, 0x01, 0x0b, 0x0b, 0x03, 0x0b, 0x0b, 0x0b, 0x0b,
        0x0b, 0x50, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x38, 0x0b,
        0x0b, 0x0b, 0x0b, 0x0b, 0x18, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
        0x0b, 0x0b, 0x0f, 0x0b, 0x0b, 0x0b, 0x0b, 0xeb, 0x0e, 0x13,
        0x0b, 0x0b, 0x0b, 0x0b, 0x27, 0x15, 0x0b, 0x0b, 0x0b, 0x0b,
        0x0b, 0xeb, 0x0e, 0x13, 0x0b, 0x0b, 0x0b, 0x0b, 0x27, 0x15,
        0x0b, 0x0b, 0x09, 0x0b, 0x0b, 0x0f, 0x0b, 0x0f, 0x0b, 0x0b,
        0x1b, 0x01, 0x2a, 0x10, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0f
    };
    static const char expected_disassembly[] =
        "bytecode function v1 result=i64 sources=1 parameters=0 registers=21 blocks=3\n"
        "registers r0:i64 r1:i64 r2:i64 r3:i64 r4:i64 r5:i64 r6:i64 r7:i64 r8:i64 r9:i64 r10:bool r11:bool r12:bool r13:bool r14:bool r15:i64 r16:i64 r17:i64 r18:i64 r19:i64 r20:i64\n"
        "block b0:\n"
        "  const_i64 r0, 40 @s0:0..1\n"
        "  const_i64 r1, 2 @s0:1..2\n"
        "  const_bool r10, true @s0:2..3\n"
        "  copy r2, r0 @s0:3..4\n"
        "  i64_add_checked r3, r0, r1 @s0:4..5\n"
        "  i64_subtract_checked r4, r3, r1 @s0:5..6\n"
        "  i64_multiply_checked r5, r4, r1 @s0:6..7\n"
        "  i64_divide_checked r6, r5, r1 @s0:7..8\n"
        "  i64_remainder_checked r7, r6, r1 @s0:8..9\n"
        "  i64_negate_checked r8, r7 @s0:9..10\n"
        "  i64_equal r11, r0, r1 @s0:10..11\n"
        "  i64_less r12, r1, r0 @s0:11..12\n"
        "  i64_less_equal r13, r1, r0 @s0:12..13\n"
        "  bool_not r14, r11 @s0:13..14\n"
        "  bool_equal r10, r10, r14 @s0:14..15\n"
        "  i64_bitwise_not r15, r0 @s0:15..16\n"
        "  i64_bitwise_and r16, r0, r1 @s0:16..17\n"
        "  i64_bitwise_xor r17, r0, r1 @s0:17..18\n"
        "  i64_bitwise_or r18, r0, r1 @s0:18..19\n"
        "  i64_shift_left_checked r19, r1, r1 @s0:19..20\n"
        "  i64_shift_right r20, r0, r1 @s0:20..21\n"
        "  branch_bool r10, b1, b2 @s0:21..22\n"
        "block b1:\n"
        "  return r3 @s0:22..23\n"
        "block b2:\n"
        "  jump b1 @s0:23..24\n";
    MarimayBytecodeFunction function;
    MarimayBytecodeFunction decoded_regression;
    MarimayBytecodeFunction void_function;
    MarimayTestBuffer buffer;

    memset(&decoded_regression, 0, sizeof(decoded_regression));
    MMAY_EXPECT(mmay_bytecode_decode_function(
        fuzz_regression,
        sizeof(fuzz_regression),
        mmay_system_allocator(),
        mmay_bytecode_function_default_options(),
        &decoded_regression
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_verify(&decoded_regression)
        == MMAY_BYTECODE_VERIFY_OK);
    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_bytecode_disassemble(
        &decoded_regression,
        mmay_test_buffer_writer(&buffer)
    ) == MMAY_BYTECODE_DISASSEMBLE_OK);
    mmay_bytecode_function_dispose(&decoded_regression);

    MMAY_EXPECT(mmay_test_bytecode_build_full(&function, mmay_system_allocator()));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_bytecode_disassemble(
        &function,
        mmay_test_buffer_writer(&buffer)
    ) == MMAY_BYTECODE_DISASSEMBLE_OK);
    MMAY_EXPECT(strcmp(buffer.bytes, expected_disassembly) == 0);
    mmay_test_buffer_init(&buffer);
    buffer.failure_index = 0;
    MMAY_EXPECT(mmay_bytecode_disassemble(
        &function,
        mmay_test_buffer_writer(&buffer)
    ) == MMAY_BYTECODE_DISASSEMBLE_WRITE_FAILED);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_bytecode_build_void(
        &void_function,
        mmay_system_allocator()
    ));
    MMAY_EXPECT(mmay_bytecode_verify(&void_function) == MMAY_BYTECODE_VERIFY_OK);
    mmay_bytecode_function_dispose(&void_function);
}

void mmay_test_bytecode_codec(void) {
    static const unsigned char expected_minimum[] = {
        'R', 'U', 'D', 'A', 'B', 'C', 'F', 0,
        1, 0, 2, 1, 0, 1, 2, 1, 2,
        1, 0,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01,
        0, 0, 1,
        17, 0, 0, 1, 2
    };
    static const int64_t boundary_values[] = {
        INT64_MIN, INT64_MAX, -1, 0, 1
    };
    MarimayBytecodeFunction minimum;
    MarimayBytecodeFunction function;
    MarimayBytecodeFunction decoded;
    MarimayOwnedBytes minimum_bytes;
    MarimayOwnedBytes encoded;
    MarimayOwnedBytes encoded_again;
    size_t index = 0;

    memset(&minimum_bytes, 0, sizeof(minimum_bytes));
    MMAY_EXPECT(mmay_test_bytecode_build_constant(
        &minimum,
        mmay_system_allocator(),
        INT64_MIN
    ));
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &minimum,
        mmay_system_allocator(),
        &minimum_bytes
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(minimum_bytes.length == sizeof(expected_minimum));
    MMAY_EXPECT(memcmp(minimum_bytes.data, expected_minimum,
        sizeof(expected_minimum)) == 0);
    mmay_owned_bytes_dispose(&minimum_bytes);
    mmay_bytecode_function_dispose(&minimum);

    for (index = 0; index < sizeof(boundary_values) / sizeof(boundary_values[0]);
        index += 1) {
        memset(&minimum_bytes, 0, sizeof(minimum_bytes));
        memset(&decoded, 0, sizeof(decoded));
        MMAY_EXPECT(mmay_test_bytecode_build_constant(
            &minimum,
            mmay_system_allocator(),
            boundary_values[index]
        ));
        MMAY_EXPECT(mmay_bytecode_encode_function(
            &minimum,
            mmay_system_allocator(),
            &minimum_bytes
        ) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_decode_function(
            minimum_bytes.data,
            minimum_bytes.length,
            mmay_system_allocator(),
            mmay_bytecode_function_default_options(),
            &decoded
        ) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_verify(&decoded) == MMAY_BYTECODE_VERIFY_OK);
        mmay_bytecode_function_dispose(&decoded);
        mmay_owned_bytes_dispose(&minimum_bytes);
        mmay_bytecode_function_dispose(&minimum);
    }

    memset(&encoded, 0, sizeof(encoded));
    memset(&encoded_again, 0, sizeof(encoded_again));
    memset(&decoded, 0, sizeof(decoded));
    MMAY_EXPECT(mmay_test_bytecode_build_full(&function, mmay_system_allocator()));
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &function,
        mmay_system_allocator(),
        &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data,
        encoded.length,
        mmay_system_allocator(),
        mmay_bytecode_function_default_options(),
        &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &decoded,
        mmay_system_allocator(),
        &encoded_again
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(encoded.length == encoded_again.length);
    MMAY_EXPECT(memcmp(encoded.data, encoded_again.data, encoded.length) == 0);
    mmay_owned_bytes_dispose(&encoded_again);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&function);
}

static bool mmay_test_bytecode_build_f64(
    MarimayBytecodeFunction *function,
    uint64_t left_bits,
    uint64_t right_bits,
    MarimayOpcode operation,
    MarimayBytecodeType result_type
) {
    uint32_t left = UINT32_MAX;
    uint32_t right = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 80;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(function, mmay_system_allocator(),
            result_type, 1, NULL, 0, mmay_bytecode_function_default_options())
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_F64, &left)
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_F64, &right)
        || !mmay_bytecode_add_register(function, result_type, &result)
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_bytecode_append(function, block, MMAY_OPCODE_CONST_F64,
            left, left_bits, 0, &span)
        || !mmay_test_bytecode_append(function, block, MMAY_OPCODE_CONST_F64,
            right, right_bits, 0, &span)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    if (operation == MMAY_OPCODE_F64_NEGATE) {
        if (!mmay_test_bytecode_append(function, block, operation, result, left,
                0, &span)) {
            mmay_bytecode_function_dispose(function);
            return false;
        }
    } else if (!mmay_test_bytecode_append(function, block, operation, result,
            left, right, &span)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    if (!mmay_test_bytecode_append(function, block, MMAY_OPCODE_RETURN, result,
            0, 0, &span)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

void mmay_test_bytecode_f64_codec_and_verifier(void) {
    MarimayBytecodeFunction function;
    MarimayBytecodeFunction decoded;
    MarimayOwnedBytes encoded;
    MarimayOwnedBytes encoded_again;
    MarimayTestBuffer buffer;
    size_t index = 0;
    static const uint64_t bits[] = {
        UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000),
        UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000),
        UINT64_C(0x7ff8000000000001)
    };

    memset(&encoded, 0, sizeof(encoded));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded_again, 0, sizeof(encoded_again));
    MMAY_EXPECT(mmay_test_bytecode_build_f64(&function,
        UINT64_C(0x8000000000000000), UINT64_C(0x3ff0000000000000),
        MMAY_OPCODE_F64_NEGATE, MMAY_BYTECODE_TYPE_F64));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_bytecode_disassemble(&function,
        mmay_test_buffer_writer(&buffer)) == MMAY_BYTECODE_DISASSEMBLE_OK);
    MMAY_EXPECT(strstr(buffer.bytes, "const_f64 r0, 0x8000000000000000")
        != NULL && strstr(buffer.bytes, "f64_negate r2, r0") != NULL);
    MMAY_EXPECT(mmay_bytecode_encode_function(&function, mmay_system_allocator(),
        &encoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(encoded.length > 9 && encoded.data[9] == 4);
    if (encoded.length > 9) {
        MarimayAllocator allocator = mmay_system_allocator();
        unsigned char *mutated = mmay_allocate(&allocator, encoded.length);
        MMAY_EXPECT(mutated != NULL);
        if (mutated != NULL) {
            memcpy(mutated, encoded.data, encoded.length);
            mutated[9] = 0;
            MMAY_EXPECT(mmay_bytecode_decode_function(
                mutated,
                encoded.length,
                allocator,
                mmay_bytecode_function_default_options(),
                &decoded
            ) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
            MMAY_EXPECT(!decoded.initialized);
            mmay_deallocate(&allocator, mutated, encoded.length);
        }
    }
    for (index = 0; index < encoded.length; index += 1) {
        MarimayBytecodeFunction output;
        memset(&output, 0, sizeof(output));
        MMAY_EXPECT(mmay_bytecode_decode_function(encoded.data, index,
            mmay_system_allocator(), mmay_bytecode_function_default_options(),
            &output) == MMAY_BYTECODE_CODEC_TRUNCATED);
        mmay_bytecode_function_dispose(&output);
    }
    MMAY_EXPECT(mmay_bytecode_decode_function(encoded.data, encoded.length,
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &decoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(&decoded, mmay_system_allocator(),
        &encoded_again) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(encoded.length == encoded_again.length
        && memcmp(encoded.data, encoded_again.data, encoded.length) == 0);
    mmay_owned_bytes_dispose(&encoded_again);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&function);

    for (index = 0; index < sizeof(bits) / sizeof(bits[0]); index += 1) {
        memset(&encoded, 0, sizeof(encoded));
        memset(&decoded, 0, sizeof(decoded));
        MMAY_EXPECT(mmay_test_bytecode_build_f64(&function, bits[index],
            UINT64_C(0x3ff0000000000000), MMAY_OPCODE_F64_ADD,
            MMAY_BYTECODE_TYPE_F64));
        MMAY_EXPECT(mmay_bytecode_encode_function(&function,
            mmay_system_allocator(), &encoded) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_decode_function(encoded.data, encoded.length,
            mmay_system_allocator(), mmay_bytecode_function_default_options(),
            &decoded) == MMAY_BYTECODE_CODEC_OK);
        mmay_bytecode_function_dispose(&decoded);
        mmay_owned_bytes_dispose(&encoded);
        mmay_bytecode_function_dispose(&function);
    }

    MMAY_EXPECT(mmay_test_bytecode_build_f64(&function,
        UINT64_C(0x3ff0000000000000), UINT64_C(0x4000000000000000),
        MMAY_OPCODE_F64_ADD, MMAY_BYTECODE_TYPE_F64));
    {
        MarimayBytecodeInstruction *instruction = mmay_vector_at(
            &((MarimayBytecodeBlock *)mmay_vector_at(&function.blocks, 0))->instructions,
            2
        );
        MMAY_EXPECT(instruction != NULL);
        if (instruction != NULL) {
            instruction->opcode = MMAY_OPCODE_F64_EQUAL;
            MMAY_EXPECT(mmay_bytecode_verify(&function)
                == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
            instruction->opcode = MMAY_OPCODE_F64_ADD;
            instruction->operands[1].unsigned_value = 9;
            MMAY_EXPECT(mmay_bytecode_verify(&function)
                == MMAY_BYTECODE_VERIFY_INVALID_REGISTER);
        }
    }
    mmay_bytecode_function_dispose(&function);
}

void mmay_test_bytecode_decode_rejections(void) {
    static const unsigned char canonical[] = {
        'R', 'U', 'D', 'A', 'B', 'C', 'F', 0,
        1, 0, 2, 1, 0, 1, 2, 1, 2,
        1, 0,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01,
        0, 0, 1,
        17, 0, 0, 1, 2
    };
    unsigned char mutated[sizeof(canonical) + 1];
    MarimayBytecodeFunction output;
    size_t index = 0;

    memset(&output, 0xff, sizeof(output));
    MMAY_EXPECT(mmay_bytecode_decode_function(
        NULL,
        0,
        mmay_system_allocator(),
        mmay_bytecode_function_default_options(),
        &output
    ) == MMAY_BYTECODE_CODEC_INVALID_ARGUMENT);
    MMAY_EXPECT(!output.initialized);
    MMAY_EXPECT(output.blocks.data == NULL);
    MMAY_EXPECT(output.register_types.data == NULL);

    for (index = 0; index < sizeof(canonical); index += 1) {
        memset(&output, 0, sizeof(output));
        MMAY_EXPECT(mmay_bytecode_decode_function(
            canonical,
            index,
            mmay_system_allocator(),
            mmay_bytecode_function_default_options(),
            &output
        ) == MMAY_BYTECODE_CODEC_TRUNCATED);
        MMAY_EXPECT(!output.initialized);
    }

    memcpy(mutated, canonical, sizeof(canonical));
    mutated[0] = 'X';
    MMAY_EXPECT(mmay_bytecode_decode_function(mutated, sizeof(canonical),
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &output) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
    mutated[0] = 'R';
    mutated[8] = 2;
    MMAY_EXPECT(mmay_bytecode_decode_function(mutated, sizeof(canonical),
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &output) == MMAY_BYTECODE_CODEC_UNSUPPORTED_VERSION);
    mutated[8] = 1;
    /* Feature bits 0..4 include nominal metadata; bit five is unknown. */
    mutated[9] = 32;
    MMAY_EXPECT(mmay_bytecode_decode_function(mutated, sizeof(canonical),
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &output) == MMAY_BYTECODE_CODEC_UNSUPPORTED_VERSION);
    mutated[9] = 0;
    mutated[9] = 4;
    MMAY_EXPECT(mmay_bytecode_decode_function(mutated, sizeof(canonical),
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &output) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
    mutated[9] = 0;
    mutated[10] = MMAY_BYTECODE_TYPE_INVALID;
    MMAY_EXPECT(mmay_bytecode_decode_function(mutated, sizeof(canonical),
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &output) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
    mutated[10] = MMAY_BYTECODE_TYPE_I64;
    mutated[14] = MMAY_BYTECODE_TYPE_INVALID;
    MMAY_EXPECT(mmay_bytecode_decode_function(mutated, sizeof(canonical),
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &output) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
    mutated[14] = MMAY_BYTECODE_TYPE_I64;
    mutated[17] = 0;
    MMAY_EXPECT(mmay_bytecode_decode_function(mutated, sizeof(canonical),
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &output) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
    mutated[17] = MMAY_OPCODE_CONST_BOOL;
    MMAY_EXPECT(mmay_bytecode_decode_function(mutated, sizeof(canonical),
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &output) == MMAY_BYTECODE_CODEC_VERIFY_FAILED);
    mutated[17] = (unsigned char)MMAY_OPCODE_COUNT;
    MMAY_EXPECT(mmay_bytecode_decode_function(mutated, sizeof(canonical),
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &output) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);

    memcpy(mutated, canonical, 8);
    mutated[8] = 0x81;
    mutated[9] = 0x00;
    MMAY_EXPECT(mmay_bytecode_decode_function(mutated, 10,
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &output) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
    memcpy(mutated, canonical, 8);
    for (index = 0; index < 9; index += 1) {
        mutated[8 + index] = 0x80;
    }
    mutated[17] = 0x02;
    MMAY_EXPECT(mmay_bytecode_decode_function(mutated, 18,
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &output) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);

    memcpy(mutated, canonical, sizeof(canonical));
    mutated[sizeof(canonical)] = 0;
    MMAY_EXPECT(mmay_bytecode_decode_function(mutated, sizeof(mutated),
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &output) == MMAY_BYTECODE_CODEC_TRAILING_DATA);
    MMAY_EXPECT(!output.initialized);
}

void mmay_test_bytecode_verifier_rejections(void) {
    MarimayBytecodeFunction function;
    MarimayBytecodeInstruction *instruction = NULL;
    static const struct {
        size_t instruction;
        size_t operand;
    } new_opcode_inputs[] = {
        {15, 1},
        {16, 1}, {16, 2},
        {17, 1}, {17, 2},
        {18, 1}, {18, 2},
        {19, 1}, {19, 2},
        {20, 1}, {20, 2}
    };
    static const size_t new_opcode_indices[] = {15, 16, 17, 18, 19, 20};
    size_t input_index = 0;

    MMAY_EXPECT(mmay_test_bytecode_build_full(&function, mmay_system_allocator()));
    instruction = mmay_test_bytecode_at(&function, 1, 0);
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->opcode = MMAY_OPCODE_CONST_BOOL;
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_bytecode_build_full(&function, mmay_system_allocator()));
    instruction = mmay_test_bytecode_at(&function, 2, 0);
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[0].unsigned_value = 3;
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_bytecode_build_full(&function, mmay_system_allocator()));
    instruction = mmay_test_bytecode_at(&function, 0, 0);
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[0].unsigned_value = 21;
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_INVALID_REGISTER);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_bytecode_build_full(&function, mmay_system_allocator()));
    instruction = mmay_test_bytecode_at(&function, 0, 0);
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[0].unsigned_value = 10;
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
    mmay_bytecode_function_dispose(&function);

    for (input_index = 0;
        input_index < sizeof(new_opcode_inputs) / sizeof(new_opcode_inputs[0]);
        input_index += 1) {
        MMAY_EXPECT(mmay_test_bytecode_build_full(
            &function,
            mmay_system_allocator()
        ));
        instruction = mmay_test_bytecode_at(
            &function,
            0,
            new_opcode_inputs[input_index].instruction
        );
        MMAY_EXPECT(instruction != NULL);
        if (instruction != NULL) {
            instruction->operands[
                new_opcode_inputs[input_index].operand
            ].unsigned_value = 20;
        }
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
        mmay_bytecode_function_dispose(&function);
    }

    for (input_index = 0;
        input_index < sizeof(new_opcode_indices) / sizeof(new_opcode_indices[0]);
        input_index += 1) {
        MMAY_EXPECT(mmay_test_bytecode_build_full(
            &function,
            mmay_system_allocator()
        ));
        instruction = mmay_test_bytecode_at(
            &function,
            0,
            new_opcode_indices[input_index]
        );
        MMAY_EXPECT(instruction != NULL);
        if (instruction != NULL) {
            instruction->operands[0].unsigned_value = 14;
        }
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        mmay_bytecode_function_dispose(&function);
    }

    MMAY_EXPECT(mmay_test_bytecode_build_full(&function, mmay_system_allocator()));
    instruction = mmay_test_bytecode_at(&function, 0, 19);
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[2].unsigned_value = 10;
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_bytecode_build_full(&function, mmay_system_allocator()));
    instruction = mmay_test_bytecode_at(&function, 0, 3);
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[1].unsigned_value = 9;
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_bytecode_build_full(&function, mmay_system_allocator()));
    instruction = mmay_test_bytecode_at(&function, 0, 0);
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->span.source = 1;
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_bytecode_build_full(&function, mmay_system_allocator()));
    instruction = mmay_test_bytecode_at(&function, 0, 21);
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[2].unsigned_value = 1;
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_bytecode_build_full(&function, mmay_system_allocator()));
    function.options.max_verify_work = 1;
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_LIMIT_REACHED);
    mmay_bytecode_function_dispose(&function);
}

void mmay_test_bytecode_integer_widths(void) {
    static const MarimayBytecodeType types[] = {
        MMAY_BYTECODE_TYPE_I8, MMAY_BYTECODE_TYPE_I16,
        MMAY_BYTECODE_TYPE_I32, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_U8, MMAY_BYTECODE_TYPE_U16,
        MMAY_BYTECODE_TYPE_U32, MMAY_BYTECODE_TYPE_U64
    };
    static const uint64_t bits[] = {
        UINT64_C(0x80), UINT64_C(0x8000), UINT64_C(0x80000000),
        UINT64_C(0x8000000000000000), UINT64_C(0xff),
        UINT64_C(0xffff), UINT64_C(0xffffffff), UINT64_MAX
    };
    size_t index = 0;
    for (index = 0; index < sizeof(types) / sizeof(types[0]); index += 1) {
        MarimayBytecodeFunction function;
        MarimayBytecodeFunction decoded;
        MarimayBytecodeInstruction instruction;
        MarimayBytecodeInstruction *decoded_instruction = NULL;
        MarimayOwnedBytes encoded;
        MarimayTestBuffer disassembly;
        uint32_t register_id = UINT32_MAX;
        uint32_t block = UINT32_MAX;
        memset(&function, 0, sizeof(function));
        memset(&decoded, 0, sizeof(decoded));
        memset(&encoded, 0, sizeof(encoded));
        MMAY_EXPECT(mmay_bytecode_function_init(
            &function,
            mmay_system_allocator(),
            types[index],
            1,
            NULL,
            0,
            mmay_bytecode_function_default_options()
        ));
        MMAY_EXPECT(mmay_bytecode_add_register(
            &function, types[index], &register_id
        ));
        MMAY_EXPECT(mmay_bytecode_add_block(&function, &block));
        instruction = mmay_test_bytecode_instruction(
            MMAY_OPCODE_CONST_INTEGER, register_id, bits[index], 0, 10
        );
        MMAY_EXPECT(mmay_bytecode_append_instruction(
            &function, block, &instruction
        ));
        instruction = mmay_test_bytecode_instruction(
            MMAY_OPCODE_RETURN, register_id, 0, 0, 11
        );
        MMAY_EXPECT(mmay_bytecode_append_instruction(
            &function, block, &instruction
        ));
        MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
        if (types[index] == MMAY_BYTECODE_TYPE_U64) {
            mmay_test_buffer_init(&disassembly);
            MMAY_EXPECT(mmay_bytecode_disassemble(
                &function,
                mmay_test_buffer_writer(&disassembly)
            ) == MMAY_BYTECODE_DISASSEMBLE_OK);
            MMAY_EXPECT(strstr(
                disassembly.bytes,
                "const_integer r0, 18446744073709551615"
            ) != NULL);
        }
        MMAY_EXPECT(mmay_bytecode_encode_function(
            &function, mmay_system_allocator(), &encoded
        ) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_decode_function(
            encoded.data,
            encoded.length,
            mmay_system_allocator(),
            mmay_bytecode_function_default_options(),
            &decoded
        ) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_verify(&decoded) == MMAY_BYTECODE_VERIFY_OK);
        decoded_instruction = mmay_test_bytecode_at(&decoded, 0, 0);
        MMAY_EXPECT(decoded_instruction != NULL);
        if (decoded_instruction != NULL) {
            MMAY_EXPECT(decoded_instruction->opcode == MMAY_OPCODE_CONST_INTEGER);
            MMAY_EXPECT(decoded_instruction->operands[1].unsigned_value
                == bits[index]);
        }
        if (decoded.register_types.data != NULL) {
            MarimayBytecodeType *register_type = mmay_vector_at(
                &decoded.register_types, 0
            );
            MMAY_EXPECT(register_type != NULL);
            if (register_type != NULL) {
                *register_type = MMAY_BYTECODE_TYPE_BOOL;
                MMAY_EXPECT(mmay_bytecode_verify(&decoded)
                    == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
            }
        }
        mmay_owned_bytes_dispose(&encoded);
        mmay_bytecode_function_dispose(&decoded);
        mmay_bytecode_function_dispose(&function);
    }
}
