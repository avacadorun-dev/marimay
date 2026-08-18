#include "test_bytecode_support.h"

void ruda_test_bytecode_metadata(void) {
    static const RudaTestOpcodeExpectation expected[] = {
        {RUDA_OPCODE_INVALID, 0, "invalid", 0,
            {RUDA_BYTECODE_OPERAND_NONE, RUDA_BYTECODE_OPERAND_NONE, RUDA_BYTECODE_OPERAND_NONE},
            RUDA_OPCODE_FLAG_INVALID},
        {RUDA_OPCODE_CONST_I64, 1, "const_i64", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_I64, RUDA_BYTECODE_OPERAND_NONE}, 0},
        {RUDA_OPCODE_CONST_BOOL, 2, "const_bool", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_BOOL, RUDA_BYTECODE_OPERAND_NONE}, 0},
        {RUDA_OPCODE_COPY, 3, "copy", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_NONE}, 0},
        {RUDA_OPCODE_I64_ADD_CHECKED, 4, "i64_add_checked", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_I64_SUBTRACT_CHECKED, 5, "i64_subtract_checked", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_I64_MULTIPLY_CHECKED, 6, "i64_multiply_checked", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_I64_DIVIDE_CHECKED, 7, "i64_divide_checked", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_I64_REMAINDER_CHECKED, 8, "i64_remainder_checked", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_I64_NEGATE_CHECKED, 9, "i64_negate_checked", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_BOOL_NOT, 10, "bool_not", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_NONE}, 0},
        {RUDA_OPCODE_I64_EQUAL, 11, "i64_equal", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_BOOL_EQUAL, 12, "bool_equal", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_I64_LESS, 13, "i64_less", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_I64_LESS_EQUAL, 14, "i64_less_equal", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_JUMP, 15, "jump", 1,
            {RUDA_BYTECODE_OPERAND_BLOCK, RUDA_BYTECODE_OPERAND_NONE, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_TERMINATOR | RUDA_OPCODE_FLAG_BRANCH},
        {RUDA_OPCODE_BRANCH_BOOL, 16, "branch_bool", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_BLOCK, RUDA_BYTECODE_OPERAND_BLOCK}, RUDA_OPCODE_FLAG_TERMINATOR | RUDA_OPCODE_FLAG_BRANCH},
        {RUDA_OPCODE_RETURN, 17, "return", 1,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_NONE, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_TERMINATOR},
        {RUDA_OPCODE_RETURN_VOID, 18, "return_void", 0,
            {RUDA_BYTECODE_OPERAND_NONE, RUDA_BYTECODE_OPERAND_NONE, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_TERMINATOR},
        {RUDA_OPCODE_I64_BITWISE_NOT, 19, "i64_bitwise_not", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_NONE}, 0},
        {RUDA_OPCODE_I64_BITWISE_AND, 20, "i64_bitwise_and", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_I64_BITWISE_XOR, 21, "i64_bitwise_xor", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_I64_BITWISE_OR, 22, "i64_bitwise_or", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, 23, "i64_shift_left_checked", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_I64_SHIFT_RIGHT, 24, "i64_shift_right", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_CALL_DIRECT, 25, "call_direct", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_CALL, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_CALL_DIRECT_VOID, 26, "call_direct_void", 1,
            {RUDA_BYTECODE_OPERAND_CALL, RUDA_BYTECODE_OPERAND_NONE, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_CONST_STRING, 27, "const_string", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_STRING, RUDA_BYTECODE_OPERAND_NONE}, 0},
        {RUDA_OPCODE_STRING_EQUAL, 28, "string_equal", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_PANIC_STRING, 29, "panic_string", 1,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_NONE, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_TERMINATOR | RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_CALL_DIRECT_NEVER, 30, "call_direct_never", 1,
            {RUDA_BYTECODE_OPERAND_CALL, RUDA_BYTECODE_OPERAND_NONE, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_TERMINATOR | RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_CONST_F64, 31, "const_f64", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_F64_BITS, RUDA_BYTECODE_OPERAND_NONE}, 0},
        {RUDA_OPCODE_F64_ADD, 32, "f64_add", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_F64_SUBTRACT, 33, "f64_subtract", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_F64_MULTIPLY, 34, "f64_multiply", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_F64_DIVIDE, 35, "f64_divide", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_F64_NEGATE, 36, "f64_negate", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_NONE}, 0},
        {RUDA_OPCODE_F64_EQUAL, 37, "f64_equal", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_F64_LESS, 38, "f64_less", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_F64_LESS_EQUAL, 39, "f64_less_equal", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_ARRAY_NEW, 40, "array_new", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_ARRAY_CONSTRUCTION, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_ARRAY_LENGTH, 41, "array_length", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_NONE}, 0},
        {RUDA_OPCODE_ARRAY_REQUIRE_INDEX, 42, "array_require_index", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_ARRAY_GET, 43, "array_get", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_ARRAY_SET, 44, "array_set", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_RECORD_NEW, 45, "record_new", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_RECORD_CONSTRUCTION, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_RECORD_GET, 46, "record_get", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_I64}, 0},
        {RUDA_OPCODE_RECORD_SET, 47, "record_set", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_I64}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_RECORD_IDENTITY_SAME, 48, "record_identity_same", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, 0},
        {RUDA_OPCODE_ENUM_NEW, 49, "enum_new", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_ENUM_CONSTRUCTION, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_ENUM_IS_VARIANT, 50, "enum_is_variant", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_I64}, 0},
        {RUDA_OPCODE_ENUM_REQUIRE_VARIANT, 51, "enum_require_variant", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_I64}, 0},
        {RUDA_OPCODE_ENUM_GET_PAYLOAD, 52, "enum_get_payload", 3,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_I64}, 0},
        {RUDA_OPCODE_INTERFACE_NEW, 53, "interface_new", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_INTERFACE_CONSTRUCTION, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_CALL_INTERFACE, 54, "call_interface", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_CALL, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_ARRAY_PUSH, 55, "array_push", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_CONST_INTEGER, 56, "const_integer", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_U64, RUDA_BYTECODE_OPERAND_NONE}, 0},
#define RUDA_NUMERIC_BINARY(opcode, wire, mnemonic, flags) \
        {opcode, wire, mnemonic, 3, \
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, flags}
#define RUDA_NUMERIC_UNARY(opcode, wire, mnemonic, flags) \
        {opcode, wire, mnemonic, 2, \
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_NONE}, flags}
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_CHECKED_ADD, 57,
            "integer_checked_add", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_CHECKED_SUBTRACT, 58,
            "integer_checked_subtract", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_CHECKED_MULTIPLY, 59,
            "integer_checked_multiply", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_CHECKED_DIVIDE, 60,
            "integer_checked_divide", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_CHECKED_REMAINDER, 61,
            "integer_checked_remainder", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_UNARY(RUDA_OPCODE_INTEGER_CHECKED_NEGATE, 62,
            "integer_checked_negate", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_CHECKED_SHIFT_LEFT, 63,
            "integer_checked_shift_left", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_CHECKED_SHIFT_RIGHT, 64,
            "integer_checked_shift_right", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_WRAPPING_ADD, 65,
            "integer_wrapping_add", 0),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_WRAPPING_SUBTRACT, 66,
            "integer_wrapping_subtract", 0),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_WRAPPING_MULTIPLY, 67,
            "integer_wrapping_multiply", 0),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_WRAPPING_DIVIDE, 68,
            "integer_wrapping_divide", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_WRAPPING_REMAINDER, 69,
            "integer_wrapping_remainder", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_UNARY(RUDA_OPCODE_INTEGER_WRAPPING_NEGATE, 70,
            "integer_wrapping_negate", 0),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_WRAPPING_SHIFT_LEFT, 71,
            "integer_wrapping_shift_left", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_WRAPPING_SHIFT_RIGHT, 72,
            "integer_wrapping_shift_right", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_SATURATING_ADD, 73,
            "integer_saturating_add", 0),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_SATURATING_SUBTRACT, 74,
            "integer_saturating_subtract", 0),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_SATURATING_MULTIPLY, 75,
            "integer_saturating_multiply", 0),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_SATURATING_DIVIDE, 76,
            "integer_saturating_divide", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_SATURATING_REMAINDER, 77,
            "integer_saturating_remainder", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_UNARY(RUDA_OPCODE_INTEGER_SATURATING_NEGATE, 78,
            "integer_saturating_negate", 0),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_SATURATING_SHIFT_LEFT, 79,
            "integer_saturating_shift_left", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_BINARY(RUDA_OPCODE_INTEGER_SATURATING_SHIFT_RIGHT, 80,
            "integer_saturating_shift_right", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_UNARY(RUDA_OPCODE_INTEGER_CONVERT_TOTAL, 81,
            "integer_convert_total", 0),
        RUDA_NUMERIC_UNARY(RUDA_OPCODE_INTEGER_CONVERT_OPTION, 82,
            "integer_convert_option", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_UNARY(RUDA_OPCODE_FORMAT_F32, 83,
            "format_f32", RUDA_OPCODE_FLAG_MAY_PANIC),
        RUDA_NUMERIC_UNARY(RUDA_OPCODE_FORMAT_F64, 84,
            "format_f64", RUDA_OPCODE_FLAG_MAY_PANIC),
        {RUDA_OPCODE_DEFER_REGISTER, 85, "defer_register", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_CALL,
                RUDA_BYTECODE_OPERAND_NONE}, 0},
        {RUDA_OPCODE_DEFER_CALL, 86, "defer_call", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_CALL,
                RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_MAY_PANIC},
        {RUDA_OPCODE_IO_PRINTLN, 87, "io_println", 2,
            {RUDA_BYTECODE_OPERAND_REGISTER,
                RUDA_BYTECODE_OPERAND_REGISTER,
                RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_MAY_PANIC}
#undef RUDA_NUMERIC_UNARY
#undef RUDA_NUMERIC_BINARY
    };
    size_t index = 0;

    RUDA_EXPECT(RUDA_OPCODE_INVALID == 0);
    RUDA_EXPECT(RUDA_OPCODE_CONST_I64 == 1);
    RUDA_EXPECT(RUDA_OPCODE_CONST_BOOL == 2);
    RUDA_EXPECT(RUDA_OPCODE_COPY == 3);
    RUDA_EXPECT(RUDA_OPCODE_I64_ADD_CHECKED == 4);
    RUDA_EXPECT(RUDA_OPCODE_I64_SUBTRACT_CHECKED == 5);
    RUDA_EXPECT(RUDA_OPCODE_I64_MULTIPLY_CHECKED == 6);
    RUDA_EXPECT(RUDA_OPCODE_I64_DIVIDE_CHECKED == 7);
    RUDA_EXPECT(RUDA_OPCODE_I64_REMAINDER_CHECKED == 8);
    RUDA_EXPECT(RUDA_OPCODE_I64_NEGATE_CHECKED == 9);
    RUDA_EXPECT(RUDA_OPCODE_BOOL_NOT == 10);
    RUDA_EXPECT(RUDA_OPCODE_I64_EQUAL == 11);
    RUDA_EXPECT(RUDA_OPCODE_BOOL_EQUAL == 12);
    RUDA_EXPECT(RUDA_OPCODE_I64_LESS == 13);
    RUDA_EXPECT(RUDA_OPCODE_I64_LESS_EQUAL == 14);
    RUDA_EXPECT(RUDA_OPCODE_JUMP == 15);
    RUDA_EXPECT(RUDA_OPCODE_BRANCH_BOOL == 16);
    RUDA_EXPECT(RUDA_OPCODE_RETURN == 17);
    RUDA_EXPECT(RUDA_OPCODE_RETURN_VOID == 18);
    RUDA_EXPECT(RUDA_OPCODE_I64_BITWISE_NOT == 19);
    RUDA_EXPECT(RUDA_OPCODE_I64_BITWISE_AND == 20);
    RUDA_EXPECT(RUDA_OPCODE_I64_BITWISE_XOR == 21);
    RUDA_EXPECT(RUDA_OPCODE_I64_BITWISE_OR == 22);
    RUDA_EXPECT(RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED == 23);
    RUDA_EXPECT(RUDA_OPCODE_I64_SHIFT_RIGHT == 24);
    RUDA_EXPECT(RUDA_OPCODE_CALL_DIRECT == 25);
    RUDA_EXPECT(RUDA_OPCODE_CALL_DIRECT_VOID == 26);
    RUDA_EXPECT(RUDA_OPCODE_CONST_STRING == 27);
    RUDA_EXPECT(RUDA_OPCODE_STRING_EQUAL == 28);
    RUDA_EXPECT(RUDA_OPCODE_PANIC_STRING == 29);
    RUDA_EXPECT(RUDA_OPCODE_CALL_DIRECT_NEVER == 30);
    RUDA_EXPECT(RUDA_OPCODE_CONST_F64 == 31);
    RUDA_EXPECT(RUDA_OPCODE_F64_ADD == 32);
    RUDA_EXPECT(RUDA_OPCODE_F64_SUBTRACT == 33);
    RUDA_EXPECT(RUDA_OPCODE_F64_MULTIPLY == 34);
    RUDA_EXPECT(RUDA_OPCODE_F64_DIVIDE == 35);
    RUDA_EXPECT(RUDA_OPCODE_F64_NEGATE == 36);
    RUDA_EXPECT(RUDA_OPCODE_F64_EQUAL == 37);
    RUDA_EXPECT(RUDA_OPCODE_F64_LESS == 38);
    RUDA_EXPECT(RUDA_OPCODE_F64_LESS_EQUAL == 39);
    RUDA_EXPECT(RUDA_OPCODE_ARRAY_NEW == 40);
    RUDA_EXPECT(RUDA_OPCODE_ARRAY_LENGTH == 41);
    RUDA_EXPECT(RUDA_OPCODE_ARRAY_REQUIRE_INDEX == 42);
    RUDA_EXPECT(RUDA_OPCODE_ARRAY_GET == 43);
    RUDA_EXPECT(RUDA_OPCODE_ARRAY_SET == 44);
    RUDA_EXPECT(RUDA_OPCODE_RECORD_NEW == 45);
    RUDA_EXPECT(RUDA_OPCODE_RECORD_GET == 46);
    RUDA_EXPECT(RUDA_OPCODE_RECORD_SET == 47);
    RUDA_EXPECT(RUDA_OPCODE_RECORD_IDENTITY_SAME == 48);
    RUDA_EXPECT(RUDA_OPCODE_ENUM_NEW == 49);
    RUDA_EXPECT(RUDA_OPCODE_ENUM_IS_VARIANT == 50);
    RUDA_EXPECT(RUDA_OPCODE_ENUM_REQUIRE_VARIANT == 51);
    RUDA_EXPECT(RUDA_OPCODE_ENUM_GET_PAYLOAD == 52);
    RUDA_EXPECT(RUDA_OPCODE_INTERFACE_NEW == 53);
    RUDA_EXPECT(RUDA_OPCODE_CALL_INTERFACE == 54);
    RUDA_EXPECT(RUDA_OPCODE_CONST_INTEGER == 56);
    RUDA_EXPECT(RUDA_OPCODE_INTEGER_CHECKED_ADD == 57);
    RUDA_EXPECT(RUDA_OPCODE_INTEGER_CONVERT_OPTION == 82);
    RUDA_EXPECT(RUDA_OPCODE_FORMAT_F64 == 84);
    RUDA_EXPECT(RUDA_OPCODE_DEFER_REGISTER == 85);
    RUDA_EXPECT(RUDA_OPCODE_DEFER_CALL == 86);
    RUDA_EXPECT(RUDA_OPCODE_IO_PRINTLN == 87);
    RUDA_EXPECT(RUDA_OPCODE_COUNT == sizeof(expected) / sizeof(expected[0]));
    for (index = 0; index < sizeof(expected) / sizeof(expected[0]); index += 1) {
        const RudaOpcodeMetadata *metadata = ruda_opcode_metadata(expected[index].opcode);
        const RudaOpcodeMetadata *from_wire = ruda_opcode_metadata_from_wire(
            expected[index].wire_id
        );
        RUDA_EXPECT(metadata != NULL);
        RUDA_EXPECT(from_wire == metadata);
        if (metadata != NULL) {
            RUDA_EXPECT(metadata->opcode == expected[index].opcode);
            RUDA_EXPECT(metadata->wire_id == expected[index].wire_id);
            RUDA_EXPECT(strcmp(metadata->mnemonic, expected[index].mnemonic) == 0);
            RUDA_EXPECT(metadata->operand_count == expected[index].operand_count);
            RUDA_EXPECT(memcmp(metadata->operands, expected[index].operands,
                sizeof(metadata->operands)) == 0);
            RUDA_EXPECT(metadata->flags == expected[index].flags);
        }
    }
    RUDA_EXPECT(ruda_opcode_metadata((RudaOpcode)-1) == NULL);
    RUDA_EXPECT(ruda_opcode_metadata((RudaOpcode)RUDA_OPCODE_COUNT) == NULL);
    RUDA_EXPECT(ruda_opcode_metadata_from_wire(RUDA_OPCODE_COUNT) == NULL);
}

void ruda_test_bytecode_valid_and_disassembly(void) {
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
    RudaBytecodeFunction function;
    RudaBytecodeFunction void_function;
    RudaTestBuffer buffer;

    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    ruda_test_buffer_init(&buffer);
    RUDA_EXPECT(ruda_bytecode_disassemble(
        &function,
        ruda_test_buffer_writer(&buffer)
    ) == RUDA_BYTECODE_DISASSEMBLE_OK);
    RUDA_EXPECT(strcmp(buffer.bytes, expected_disassembly) == 0);
    ruda_test_buffer_init(&buffer);
    buffer.failure_index = 0;
    RUDA_EXPECT(ruda_bytecode_disassemble(
        &function,
        ruda_test_buffer_writer(&buffer)
    ) == RUDA_BYTECODE_DISASSEMBLE_WRITE_FAILED);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_void(
        &void_function,
        ruda_system_allocator()
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&void_function) == RUDA_BYTECODE_VERIFY_OK);
    ruda_bytecode_function_dispose(&void_function);
}

void ruda_test_bytecode_codec(void) {
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
    RudaBytecodeFunction minimum;
    RudaBytecodeFunction function;
    RudaBytecodeFunction decoded;
    RudaOwnedBytes minimum_bytes;
    RudaOwnedBytes encoded;
    RudaOwnedBytes encoded_again;
    size_t index = 0;

    memset(&minimum_bytes, 0, sizeof(minimum_bytes));
    RUDA_EXPECT(ruda_test_bytecode_build_constant(
        &minimum,
        ruda_system_allocator(),
        INT64_MIN
    ));
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &minimum,
        ruda_system_allocator(),
        &minimum_bytes
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(minimum_bytes.length == sizeof(expected_minimum));
    RUDA_EXPECT(memcmp(minimum_bytes.data, expected_minimum,
        sizeof(expected_minimum)) == 0);
    ruda_owned_bytes_dispose(&minimum_bytes);
    ruda_bytecode_function_dispose(&minimum);

    for (index = 0; index < sizeof(boundary_values) / sizeof(boundary_values[0]);
        index += 1) {
        memset(&minimum_bytes, 0, sizeof(minimum_bytes));
        memset(&decoded, 0, sizeof(decoded));
        RUDA_EXPECT(ruda_test_bytecode_build_constant(
            &minimum,
            ruda_system_allocator(),
            boundary_values[index]
        ));
        RUDA_EXPECT(ruda_bytecode_encode_function(
            &minimum,
            ruda_system_allocator(),
            &minimum_bytes
        ) == RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(ruda_bytecode_decode_function(
            minimum_bytes.data,
            minimum_bytes.length,
            ruda_system_allocator(),
            ruda_bytecode_function_default_options(),
            &decoded
        ) == RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(ruda_bytecode_verify(&decoded) == RUDA_BYTECODE_VERIFY_OK);
        ruda_bytecode_function_dispose(&decoded);
        ruda_owned_bytes_dispose(&minimum_bytes);
        ruda_bytecode_function_dispose(&minimum);
    }

    memset(&encoded, 0, sizeof(encoded));
    memset(&encoded_again, 0, sizeof(encoded_again));
    memset(&decoded, 0, sizeof(decoded));
    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &function,
        ruda_system_allocator(),
        &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_function(
        encoded.data,
        encoded.length,
        ruda_system_allocator(),
        ruda_bytecode_function_default_options(),
        &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &decoded,
        ruda_system_allocator(),
        &encoded_again
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(encoded.length == encoded_again.length);
    RUDA_EXPECT(memcmp(encoded.data, encoded_again.data, encoded.length) == 0);
    ruda_owned_bytes_dispose(&encoded_again);
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&function);
}

static bool ruda_test_bytecode_build_f64(
    RudaBytecodeFunction *function,
    uint64_t left_bits,
    uint64_t right_bits,
    RudaOpcode operation,
    RudaBytecodeType result_type
) {
    uint32_t left = UINT32_MAX;
    uint32_t right = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 80;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, ruda_system_allocator(),
            result_type, 1, NULL, 0, ruda_bytecode_function_default_options())
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_F64, &left)
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_F64, &right)
        || !ruda_bytecode_add_register(function, result_type, &result)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_CONST_F64,
            left, left_bits, 0, &span)
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_CONST_F64,
            right, right_bits, 0, &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    if (operation == RUDA_OPCODE_F64_NEGATE) {
        if (!ruda_test_bytecode_append(function, block, operation, result, left,
                0, &span)) {
            ruda_bytecode_function_dispose(function);
            return false;
        }
    } else if (!ruda_test_bytecode_append(function, block, operation, result,
            left, right, &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    if (!ruda_test_bytecode_append(function, block, RUDA_OPCODE_RETURN, result,
            0, 0, &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

void ruda_test_bytecode_f64_codec_and_verifier(void) {
    RudaBytecodeFunction function;
    RudaBytecodeFunction decoded;
    RudaOwnedBytes encoded;
    RudaOwnedBytes encoded_again;
    RudaTestBuffer buffer;
    size_t index = 0;
    static const uint64_t bits[] = {
        UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000),
        UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000),
        UINT64_C(0x7ff8000000000001)
    };

    memset(&encoded, 0, sizeof(encoded));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded_again, 0, sizeof(encoded_again));
    RUDA_EXPECT(ruda_test_bytecode_build_f64(&function,
        UINT64_C(0x8000000000000000), UINT64_C(0x3ff0000000000000),
        RUDA_OPCODE_F64_NEGATE, RUDA_BYTECODE_TYPE_F64));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    ruda_test_buffer_init(&buffer);
    RUDA_EXPECT(ruda_bytecode_disassemble(&function,
        ruda_test_buffer_writer(&buffer)) == RUDA_BYTECODE_DISASSEMBLE_OK);
    RUDA_EXPECT(strstr(buffer.bytes, "const_f64 r0, 0x8000000000000000")
        != NULL && strstr(buffer.bytes, "f64_negate r2, r0") != NULL);
    RUDA_EXPECT(ruda_bytecode_encode_function(&function, ruda_system_allocator(),
        &encoded) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(encoded.length > 9 && encoded.data[9] == 4);
    if (encoded.length > 9) {
        RudaAllocator allocator = ruda_system_allocator();
        unsigned char *mutated = ruda_allocate(&allocator, encoded.length);
        RUDA_EXPECT(mutated != NULL);
        if (mutated != NULL) {
            memcpy(mutated, encoded.data, encoded.length);
            mutated[9] = 0;
            RUDA_EXPECT(ruda_bytecode_decode_function(
                mutated,
                encoded.length,
                allocator,
                ruda_bytecode_function_default_options(),
                &decoded
            ) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
            RUDA_EXPECT(!decoded.initialized);
            ruda_deallocate(&allocator, mutated, encoded.length);
        }
    }
    for (index = 0; index < encoded.length; index += 1) {
        RudaBytecodeFunction output;
        memset(&output, 0, sizeof(output));
        RUDA_EXPECT(ruda_bytecode_decode_function(encoded.data, index,
            ruda_system_allocator(), ruda_bytecode_function_default_options(),
            &output) == RUDA_BYTECODE_CODEC_TRUNCATED);
        ruda_bytecode_function_dispose(&output);
    }
    RUDA_EXPECT(ruda_bytecode_decode_function(encoded.data, encoded.length,
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &decoded) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_encode_function(&decoded, ruda_system_allocator(),
        &encoded_again) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(encoded.length == encoded_again.length
        && memcmp(encoded.data, encoded_again.data, encoded.length) == 0);
    ruda_owned_bytes_dispose(&encoded_again);
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&function);

    for (index = 0; index < sizeof(bits) / sizeof(bits[0]); index += 1) {
        memset(&encoded, 0, sizeof(encoded));
        memset(&decoded, 0, sizeof(decoded));
        RUDA_EXPECT(ruda_test_bytecode_build_f64(&function, bits[index],
            UINT64_C(0x3ff0000000000000), RUDA_OPCODE_F64_ADD,
            RUDA_BYTECODE_TYPE_F64));
        RUDA_EXPECT(ruda_bytecode_encode_function(&function,
            ruda_system_allocator(), &encoded) == RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(ruda_bytecode_decode_function(encoded.data, encoded.length,
            ruda_system_allocator(), ruda_bytecode_function_default_options(),
            &decoded) == RUDA_BYTECODE_CODEC_OK);
        ruda_bytecode_function_dispose(&decoded);
        ruda_owned_bytes_dispose(&encoded);
        ruda_bytecode_function_dispose(&function);
    }

    RUDA_EXPECT(ruda_test_bytecode_build_f64(&function,
        UINT64_C(0x3ff0000000000000), UINT64_C(0x4000000000000000),
        RUDA_OPCODE_F64_ADD, RUDA_BYTECODE_TYPE_F64));
    {
        RudaBytecodeInstruction *instruction = ruda_vector_at(
            &((RudaBytecodeBlock *)ruda_vector_at(&function.blocks, 0))->instructions,
            2
        );
        RUDA_EXPECT(instruction != NULL);
        if (instruction != NULL) {
            instruction->opcode = RUDA_OPCODE_F64_EQUAL;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
            instruction->opcode = RUDA_OPCODE_F64_ADD;
            instruction->operands[1].unsigned_value = 9;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_REGISTER);
        }
    }
    ruda_bytecode_function_dispose(&function);
}

void ruda_test_bytecode_decode_rejections(void) {
    static const unsigned char canonical[] = {
        'R', 'U', 'D', 'A', 'B', 'C', 'F', 0,
        1, 0, 2, 1, 0, 1, 2, 1, 2,
        1, 0,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01,
        0, 0, 1,
        17, 0, 0, 1, 2
    };
    unsigned char mutated[sizeof(canonical) + 1];
    RudaBytecodeFunction output;
    size_t index = 0;

    memset(&output, 0xff, sizeof(output));
    RUDA_EXPECT(ruda_bytecode_decode_function(
        NULL,
        0,
        ruda_system_allocator(),
        ruda_bytecode_function_default_options(),
        &output
    ) == RUDA_BYTECODE_CODEC_INVALID_ARGUMENT);
    RUDA_EXPECT(!output.initialized);
    RUDA_EXPECT(output.blocks.data == NULL);
    RUDA_EXPECT(output.register_types.data == NULL);

    for (index = 0; index < sizeof(canonical); index += 1) {
        memset(&output, 0, sizeof(output));
        RUDA_EXPECT(ruda_bytecode_decode_function(
            canonical,
            index,
            ruda_system_allocator(),
            ruda_bytecode_function_default_options(),
            &output
        ) == RUDA_BYTECODE_CODEC_TRUNCATED);
        RUDA_EXPECT(!output.initialized);
    }

    memcpy(mutated, canonical, sizeof(canonical));
    mutated[0] = 'X';
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, sizeof(canonical),
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
    mutated[0] = 'R';
    mutated[8] = 2;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, sizeof(canonical),
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION);
    mutated[8] = 1;
    /* Feature bits 0..4 include nominal metadata; bit five is unknown. */
    mutated[9] = 32;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, sizeof(canonical),
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION);
    mutated[9] = 0;
    mutated[9] = 4;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, sizeof(canonical),
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
    mutated[9] = 0;
    mutated[10] = RUDA_BYTECODE_TYPE_INVALID;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, sizeof(canonical),
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
    mutated[10] = RUDA_BYTECODE_TYPE_I64;
    mutated[14] = RUDA_BYTECODE_TYPE_INVALID;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, sizeof(canonical),
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
    mutated[14] = RUDA_BYTECODE_TYPE_I64;
    mutated[17] = 0;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, sizeof(canonical),
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
    mutated[17] = RUDA_OPCODE_CONST_BOOL;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, sizeof(canonical),
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_VERIFY_FAILED);
    mutated[17] = (unsigned char)RUDA_OPCODE_COUNT;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, sizeof(canonical),
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);

    memcpy(mutated, canonical, 8);
    mutated[8] = 0x81;
    mutated[9] = 0x00;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, 10,
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
    memcpy(mutated, canonical, 8);
    for (index = 0; index < 9; index += 1) {
        mutated[8 + index] = 0x80;
    }
    mutated[17] = 0x02;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, 18,
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);

    memcpy(mutated, canonical, sizeof(canonical));
    mutated[sizeof(canonical)] = 0;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, sizeof(mutated),
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_TRAILING_DATA);
    RUDA_EXPECT(!output.initialized);
}

void ruda_test_bytecode_verifier_rejections(void) {
    RudaBytecodeFunction function;
    RudaBytecodeInstruction *instruction = NULL;
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

    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    instruction = ruda_test_bytecode_at(&function, 1, 0);
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->opcode = RUDA_OPCODE_CONST_BOOL;
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    instruction = ruda_test_bytecode_at(&function, 2, 0);
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[0].unsigned_value = 3;
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    instruction = ruda_test_bytecode_at(&function, 0, 0);
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[0].unsigned_value = 21;
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_INVALID_REGISTER);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    instruction = ruda_test_bytecode_at(&function, 0, 0);
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[0].unsigned_value = 10;
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
    ruda_bytecode_function_dispose(&function);

    for (input_index = 0;
        input_index < sizeof(new_opcode_inputs) / sizeof(new_opcode_inputs[0]);
        input_index += 1) {
        RUDA_EXPECT(ruda_test_bytecode_build_full(
            &function,
            ruda_system_allocator()
        ));
        instruction = ruda_test_bytecode_at(
            &function,
            0,
            new_opcode_inputs[input_index].instruction
        );
        RUDA_EXPECT(instruction != NULL);
        if (instruction != NULL) {
            instruction->operands[
                new_opcode_inputs[input_index].operand
            ].unsigned_value = 20;
        }
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
        ruda_bytecode_function_dispose(&function);
    }

    for (input_index = 0;
        input_index < sizeof(new_opcode_indices) / sizeof(new_opcode_indices[0]);
        input_index += 1) {
        RUDA_EXPECT(ruda_test_bytecode_build_full(
            &function,
            ruda_system_allocator()
        ));
        instruction = ruda_test_bytecode_at(
            &function,
            0,
            new_opcode_indices[input_index]
        );
        RUDA_EXPECT(instruction != NULL);
        if (instruction != NULL) {
            instruction->operands[0].unsigned_value = 14;
        }
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
        ruda_bytecode_function_dispose(&function);
    }

    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    instruction = ruda_test_bytecode_at(&function, 0, 19);
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[2].unsigned_value = 10;
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    instruction = ruda_test_bytecode_at(&function, 0, 3);
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[1].unsigned_value = 9;
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    instruction = ruda_test_bytecode_at(&function, 0, 0);
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->span.source = 1;
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    instruction = ruda_test_bytecode_at(&function, 0, 21);
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[2].unsigned_value = 1;
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    function.options.max_verify_work = 1;
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_LIMIT_REACHED);
    ruda_bytecode_function_dispose(&function);
}

void ruda_test_bytecode_integer_widths(void) {
    static const RudaBytecodeType types[] = {
        RUDA_BYTECODE_TYPE_I8, RUDA_BYTECODE_TYPE_I16,
        RUDA_BYTECODE_TYPE_I32, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_U8, RUDA_BYTECODE_TYPE_U16,
        RUDA_BYTECODE_TYPE_U32, RUDA_BYTECODE_TYPE_U64
    };
    static const uint64_t bits[] = {
        UINT64_C(0x80), UINT64_C(0x8000), UINT64_C(0x80000000),
        UINT64_C(0x8000000000000000), UINT64_C(0xff),
        UINT64_C(0xffff), UINT64_C(0xffffffff), UINT64_MAX
    };
    size_t index = 0;
    for (index = 0; index < sizeof(types) / sizeof(types[0]); index += 1) {
        RudaBytecodeFunction function;
        RudaBytecodeFunction decoded;
        RudaBytecodeInstruction instruction;
        RudaBytecodeInstruction *decoded_instruction = NULL;
        RudaOwnedBytes encoded;
        uint32_t register_id = UINT32_MAX;
        uint32_t block = UINT32_MAX;
        memset(&function, 0, sizeof(function));
        memset(&decoded, 0, sizeof(decoded));
        memset(&encoded, 0, sizeof(encoded));
        RUDA_EXPECT(ruda_bytecode_function_init(
            &function,
            ruda_system_allocator(),
            types[index],
            1,
            NULL,
            0,
            ruda_bytecode_function_default_options()
        ));
        RUDA_EXPECT(ruda_bytecode_add_register(
            &function, types[index], &register_id
        ));
        RUDA_EXPECT(ruda_bytecode_add_block(&function, &block));
        instruction = ruda_test_bytecode_instruction(
            RUDA_OPCODE_CONST_INTEGER, register_id, bits[index], 0, 10
        );
        RUDA_EXPECT(ruda_bytecode_append_instruction(
            &function, block, &instruction
        ));
        instruction = ruda_test_bytecode_instruction(
            RUDA_OPCODE_RETURN, register_id, 0, 0, 11
        );
        RUDA_EXPECT(ruda_bytecode_append_instruction(
            &function, block, &instruction
        ));
        RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
        RUDA_EXPECT(ruda_bytecode_encode_function(
            &function, ruda_system_allocator(), &encoded
        ) == RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(ruda_bytecode_decode_function(
            encoded.data,
            encoded.length,
            ruda_system_allocator(),
            ruda_bytecode_function_default_options(),
            &decoded
        ) == RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(ruda_bytecode_verify(&decoded) == RUDA_BYTECODE_VERIFY_OK);
        decoded_instruction = ruda_test_bytecode_at(&decoded, 0, 0);
        RUDA_EXPECT(decoded_instruction != NULL);
        if (decoded_instruction != NULL) {
            RUDA_EXPECT(decoded_instruction->opcode == RUDA_OPCODE_CONST_INTEGER);
            RUDA_EXPECT(decoded_instruction->operands[1].unsigned_value
                == bits[index]);
        }
        if (decoded.register_types.data != NULL) {
            RudaBytecodeType *register_type = ruda_vector_at(
                &decoded.register_types, 0
            );
            RUDA_EXPECT(register_type != NULL);
            if (register_type != NULL) {
                *register_type = RUDA_BYTECODE_TYPE_BOOL;
                RUDA_EXPECT(ruda_bytecode_verify(&decoded)
                    == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
            }
        }
        ruda_owned_bytes_dispose(&encoded);
        ruda_bytecode_function_dispose(&decoded);
        ruda_bytecode_function_dispose(&function);
    }
}
