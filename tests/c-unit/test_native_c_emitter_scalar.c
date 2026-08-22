#include "test_native_c_emitter_support.h"

#include <inttypes.h>
#include <string.h>

#include "mmay/base/checked_integer.h"

typedef enum MarimayTestNativeScalarOutcome {
    MMAY_TEST_NATIVE_SCALAR_I64,
    MMAY_TEST_NATIVE_SCALAR_BOOL,
    MMAY_TEST_NATIVE_SCALAR_OVERFLOW,
    MMAY_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO,
    MMAY_TEST_NATIVE_SCALAR_INVALID_SHIFT
} MarimayTestNativeScalarOutcome;

typedef struct MarimayTestNativeScalarCase {
    MarimayMirInstructionKind kind;
    int64_t left;
    int64_t right;
    bool left_boolean;
    bool right_boolean;
    MarimayTestNativeScalarOutcome outcome;
    int64_t expected_i64;
    bool expected_boolean;
    bool native;
} MarimayTestNativeScalarCase;

static bool mmay_test_native_scalar_is_boolean(MarimayMirInstructionKind kind) {
    return kind == MMAY_MIR_NOT_BOOL || kind == MMAY_MIR_EQUAL_BOOL
        || kind == MMAY_MIR_EQUAL_I64 || kind == MMAY_MIR_LESS_I64
        || kind == MMAY_MIR_LESS_EQUAL_I64;
}

static bool mmay_test_native_scalar_is_boolean_input(
    MarimayMirInstructionKind kind
) {
    return kind == MMAY_MIR_NOT_BOOL || kind == MMAY_MIR_EQUAL_BOOL;
}

static bool mmay_test_native_scalar_is_unary(MarimayMirInstructionKind kind) {
    return kind == MMAY_MIR_NOT_BOOL || kind == MMAY_MIR_NEGATE_CHECKED_I64
        || kind == MMAY_MIR_BITWISE_NOT_I64;
}

static bool mmay_test_native_build_scalar(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    const MarimayTestNativeScalarCase *test_case
) {
    MarimayTypeId input_type = mmay_test_native_scalar_is_boolean_input(
        test_case->kind
    ) ? MMAY_TYPE_BOOL : MMAY_TYPE_I64;
    MarimayTypeId result_type = mmay_test_native_scalar_is_boolean(
        test_case->kind
    ) ? MMAY_TYPE_BOOL : MMAY_TYPE_I64;
    MarimayMirRegister left = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister right = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, result_type, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, input_type, &left)
        || (!mmay_test_native_scalar_is_unary(test_case->kind)
            && !mmay_mir_add_register(function, input_type, &right))
        || !mmay_mir_add_register(function, result_type, &result)
        || !mmay_mir_add_block(function, &block)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    if (mmay_test_native_scalar_is_unary(test_case->kind)) {
        right = left;
    }
    instruction = mmay_test_native_instruction(
        input_type == MMAY_TYPE_BOOL ? MMAY_MIR_CONSTANT_BOOL
            : MMAY_MIR_CONSTANT_I64,
        left, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, test_case->left,
        test_case->left_boolean, 300
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    if (!mmay_test_native_scalar_is_unary(test_case->kind)) {
        instruction = mmay_test_native_instruction(
            input_type == MMAY_TYPE_BOOL ? MMAY_MIR_CONSTANT_BOOL
                : MMAY_MIR_CONSTANT_I64,
            right, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, test_case->right,
            test_case->right_boolean, 301
        );
        if (!mmay_mir_append_instruction(function, block, &instruction)) {
            goto failure;
        }
    }
    instruction = mmay_test_native_instruction(test_case->kind, result, left,
        right, 0, false, 302);
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_native_terminator(MMAY_MIR_RETURN, result,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 303))) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

static void mmay_test_native_scalar_differential(void) {
    static const MarimayTestNativeScalarCase cases[] = {
        {MMAY_MIR_NOT_BOOL, 0, 0, false, false,
            MMAY_TEST_NATIVE_SCALAR_BOOL, 0, true, true},
        {MMAY_MIR_EQUAL_BOOL, 0, 0, true, true,
            MMAY_TEST_NATIVE_SCALAR_BOOL, 0, true, false},
        {MMAY_MIR_EQUAL_I64, -1, -1, false, false,
            MMAY_TEST_NATIVE_SCALAR_BOOL, 0, true, false},
        {MMAY_MIR_LESS_I64, INT64_MIN, INT64_MAX, false, false,
            MMAY_TEST_NATIVE_SCALAR_BOOL, 0, true, false},
        {MMAY_MIR_LESS_EQUAL_I64, 7, 7, false, false,
            MMAY_TEST_NATIVE_SCALAR_BOOL, 0, true, false},
        {MMAY_MIR_BITWISE_NOT_I64, -1, 0, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, 0, false, true},
        {MMAY_MIR_BITWISE_AND_I64, -1, 0x55, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, 0x55, false, false},
        {MMAY_MIR_BITWISE_XOR_I64, -1, 0x55, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, ~INT64_C(0x55), false, false},
        {MMAY_MIR_BITWISE_OR_I64, INT64_MIN, 7, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, INT64_MIN | 7, false, false},
        {MMAY_MIR_ADD_CHECKED_I64, 2, 3, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, 5, false, false},
        {MMAY_MIR_ADD_CHECKED_I64, INT64_MAX, 1, false, false,
            MMAY_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, true},
        {MMAY_MIR_SUBTRACT_CHECKED_I64, INT64_MIN, 1, false, false,
            MMAY_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {MMAY_MIR_MULTIPLY_CHECKED_I64, -7, 3, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, -21, false, false},
        {MMAY_MIR_MULTIPLY_CHECKED_I64, INT64_MIN, -1, false, false,
            MMAY_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, true},
        {MMAY_MIR_NEGATE_CHECKED_I64, INT64_MIN, 0, false, false,
            MMAY_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {MMAY_MIR_DIVIDE_CHECKED_I64, -7, 3, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, -2, false, false},
        {MMAY_MIR_DIVIDE_CHECKED_I64, 7, -3, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, -2, false, false},
        {MMAY_MIR_DIVIDE_CHECKED_I64, 1, 0, false, false,
            MMAY_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO, 0, false, true},
        {MMAY_MIR_DIVIDE_CHECKED_I64, INT64_MIN, -1, false, false,
            MMAY_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {MMAY_MIR_REMAINDER_CHECKED_I64, -7, 3, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, -1, false, false},
        {MMAY_MIR_REMAINDER_CHECKED_I64, 7, -3, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, 1, false, false},
        {MMAY_MIR_REMAINDER_CHECKED_I64, 1, 0, false, false,
            MMAY_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO, 0, false, false},
        {MMAY_MIR_REMAINDER_CHECKED_I64, INT64_MIN, -1, false, false,
            MMAY_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, 1, -1, false, false,
            MMAY_TEST_NATIVE_SCALAR_INVALID_SHIFT, 0, false, false},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, 1, 0, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, 1, false, false},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, 1, 1, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, 2, false, false},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, 1, 62, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, INT64_C(1) << 62, false, false},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, 1, 63, false, false,
            MMAY_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, 1, 64, false, false,
            MMAY_TEST_NATIVE_SCALAR_INVALID_SHIFT, 0, false, true},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, INT64_MAX, 1, false, false,
            MMAY_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {MMAY_MIR_SHIFT_RIGHT_I64, -8, -1, false, false,
            MMAY_TEST_NATIVE_SCALAR_INVALID_SHIFT, 0, false, false},
        {MMAY_MIR_SHIFT_RIGHT_I64, -8, 0, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, -8, false, false},
        {MMAY_MIR_SHIFT_RIGHT_I64, -8, 1, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, -4, false, false},
        {MMAY_MIR_SHIFT_RIGHT_I64, -8, 62, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, -1, false, false},
        {MMAY_MIR_SHIFT_RIGHT_I64, -8, 63, false, false,
            MMAY_TEST_NATIVE_SCALAR_I64, -1, false, false},
        {MMAY_MIR_SHIFT_RIGHT_I64, -8, 64, false, false,
            MMAY_TEST_NATIVE_SCALAR_INVALID_SHIFT, 0, false, true}
    };
    const MarimaySymbolId symbol = {{0x30}};
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayMirFunction input;
        MarimayMirValue mir_result;
        MarimayMirInterpretStatus mir_status;
        MarimayBytecodeFunction bytecode;
        MarimayBytecodeValue bytecode_result;
        MarimayBytecodeTrap trap;
        MarimayBytecodeRunStatus bytecode_status;
        MarimayOwnedBytes source;
        const MarimayMirFunction *functions[1];
        MarimayNativeCProgram program;
        char expected_output[64];
#if !defined(_WIN32)
        uint32_t expected_exit = 0;
#endif
        int native_status = 0;
        memset(&input, 0, sizeof(input));
        memset(&mir_result, 0, sizeof(mir_result));
        memset(&bytecode, 0, sizeof(bytecode));
        memset(&bytecode_result, 0, sizeof(bytecode_result));
        memset(&trap, 0, sizeof(trap));
        memset(&source, 0, sizeof(source));
        MMAY_EXPECT(mmay_test_native_build_scalar(&input, symbol, &cases[index]));
        mir_status = mmay_mir_interpret(&input, NULL, 0,
            mmay_system_allocator(), mmay_mir_interpret_default_options(),
            &mir_result);
        MMAY_EXPECT(mmay_mir_lower_bytecode_function(&input,
            mmay_system_allocator(), mmay_mir_bytecode_lower_default_options(),
            &bytecode) == MMAY_MIR_BYTECODE_LOWER_OK);
        {
            MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
            options.trap = &trap;
            bytecode_result.type = MMAY_BYTECODE_TYPE_I64;
            bytecode_result.as.i64 = 1234;
            bytecode_status = mmay_bytecode_run(&bytecode, NULL, 0,
                mmay_system_allocator(), options, &bytecode_result);
        }
        if (cases[index].outcome == MMAY_TEST_NATIVE_SCALAR_I64) {
            MMAY_EXPECT(mir_status == MMAY_MIR_INTERPRET_OK
                && bytecode_status == MMAY_BYTECODE_RUN_OK);
            MMAY_EXPECT(mir_result.type == MMAY_TYPE_I64
                && mir_result.as.i64 == cases[index].expected_i64);
            MMAY_EXPECT(bytecode_result.type == MMAY_BYTECODE_TYPE_I64
                && bytecode_result.as.i64 == cases[index].expected_i64);
            (void)snprintf(expected_output, sizeof(expected_output), "ok:i64:%lld\n",
                (long long)cases[index].expected_i64);
        } else if (cases[index].outcome == MMAY_TEST_NATIVE_SCALAR_BOOL) {
            MMAY_EXPECT(mir_status == MMAY_MIR_INTERPRET_OK
                && bytecode_status == MMAY_BYTECODE_RUN_OK);
            MMAY_EXPECT(mir_result.type == MMAY_TYPE_BOOL
                && mir_result.as.boolean == cases[index].expected_boolean);
            MMAY_EXPECT(bytecode_result.type == MMAY_BYTECODE_TYPE_BOOL
                && bytecode_result.as.boolean == cases[index].expected_boolean);
            (void)snprintf(expected_output, sizeof(expected_output), "ok:bool:%s\n",
                cases[index].expected_boolean ? "true" : "false");
        } else {
            MarimayMirInterpretStatus expected_mir = MMAY_MIR_INTERPRET_OVERFLOW;
            MarimayBytecodeRunStatus expected_bytecode = MMAY_BYTECODE_RUN_OVERFLOW;
            if (cases[index].outcome
                == MMAY_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO) {
                expected_mir = MMAY_MIR_INTERPRET_DIVISION_BY_ZERO;
                expected_bytecode = MMAY_BYTECODE_RUN_DIVISION_BY_ZERO;
                native_status = 2;
            } else if (cases[index].outcome
                == MMAY_TEST_NATIVE_SCALAR_INVALID_SHIFT) {
                expected_mir = MMAY_MIR_INTERPRET_INVALID_SHIFT;
                expected_bytecode = MMAY_BYTECODE_RUN_INVALID_SHIFT;
                native_status = 3;
            } else {
                native_status = 1;
            }
            MMAY_EXPECT(mir_status == expected_mir
                && bytecode_status == expected_bytecode);
            MMAY_EXPECT(bytecode_result.type == MMAY_BYTECODE_TYPE_I64
                && bytecode_result.as.i64 == 1234 && trap.has_span
                && trap.span.start == 302 && trap.span.end == 303);
            (void)snprintf(expected_output, sizeof(expected_output),
                "trap:%d:302:303\n", native_status);
#if !defined(_WIN32)
            expected_exit = 1;
#endif
        }
        if (cases[index].native) {
            functions[0] = &input;
            program.functions = functions;
            program.function_count = 1;
            program.entry_symbol = symbol;
            MMAY_EXPECT(mmay_native_c_emit_program(&program,
                mmay_system_allocator(), mmay_native_c_emit_default_options(),
                &source) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
            mmay_test_native_compile_and_run(&source, expected_output, expected_exit);
#endif
            mmay_owned_bytes_dispose(&source);
        }
        mmay_bytecode_function_dispose(&bytecode);
        mmay_mir_function_dispose(&input);
    }
}

static bool mmay_test_native_f64_is_boolean(MarimayMirInstructionKind kind) {
    return kind == MMAY_MIR_EQUAL_F64 || kind == MMAY_MIR_LESS_F64
        || kind == MMAY_MIR_LESS_EQUAL_F64;
}

static bool mmay_test_native_f64_is_unary(MarimayMirInstructionKind kind) {
    return kind == MMAY_MIR_NEGATE_F64;
}

static bool mmay_test_native_build_f64_operation(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimayMirInstructionKind kind,
    uint64_t left_bits,
    uint64_t right_bits
) {
    MarimayMirRegister left = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister right = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayTypeId result_type = mmay_test_native_f64_is_boolean(kind)
        ? MMAY_TYPE_BOOL : MMAY_TYPE_F64;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, result_type, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &left)
        || (!mmay_test_native_f64_is_unary(kind)
            && !mmay_mir_add_register(function, MMAY_TYPE_F64, &right))
        || !mmay_mir_add_register(function, result_type, &result)
        || !mmay_mir_add_block(function, &block)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    if (mmay_test_native_f64_is_unary(kind)) {
        right = left;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_CONSTANT_F64, left,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, 500);
    instruction.f64_bits = left_bits;
    if (!mmay_mir_append_instruction(function, block, &instruction)) goto failure;
    if (!mmay_test_native_f64_is_unary(kind)) {
        instruction = mmay_test_native_instruction(MMAY_MIR_CONSTANT_F64, right,
            MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, 501);
        instruction.f64_bits = right_bits;
        if (!mmay_mir_append_instruction(function, block, &instruction)) goto failure;
    }
    instruction = mmay_test_native_instruction(kind, result, left, right,
        0, false, 502);
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_native_terminator(MMAY_MIR_RETURN, result,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 503))) goto failure;
    return true;
failure:
    mmay_mir_function_dispose(function);
    return false;
}

static bool mmay_test_native_build_f64_identity(
    MarimayMirFunction *function, MarimaySymbolId symbol
) {
    const MarimayTypeId parameter = MMAY_TYPE_F64;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol,
            &parameter, 1, MMAY_TYPE_F64, mmay_mir_function_default_options())
        || !mmay_mir_add_block(function, &block)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_native_terminator(MMAY_MIR_RETURN, 0,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 510))) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_native_build_f64_direct_entry(
    MarimayMirFunction *function, MarimaySymbolId symbol, MarimaySymbolId callee
) {
    const MarimayMirCallArgument argument = {0, MMAY_TYPE_F64};
    MarimayMirRegister value = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL,
            0, MMAY_TYPE_F64, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &value)
        || !mmay_mir_add_register(function, MMAY_TYPE_F64, &result)
        || !mmay_mir_add_block(function, &block)
        || !mmay_mir_add_call(function, callee, MMAY_TYPE_F64, &argument, 1, &call)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_instruction(MMAY_MIR_CONSTANT_F64, value,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, 520);
    instruction.f64_bits = UINT64_C(0x8000000000000000);
    if (!mmay_mir_append_instruction(function, block, &instruction)) goto failure;
    instruction = mmay_test_native_instruction(MMAY_MIR_CALL, result,
        MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0, false, 521);
    instruction.call = call;
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_native_terminator(MMAY_MIR_RETURN, result,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 522))) goto failure;
    return true;
failure:
    mmay_mir_function_dispose(function);
    return false;
}

static void mmay_test_native_f64_differential(void) {
    typedef struct MarimayTestNativeF64Case {
        MarimayMirInstructionKind kind;
        uint64_t left;
        uint64_t right;
        bool boolean;
        uint64_t expected_bits;
    } MarimayTestNativeF64Case;
    static const MarimayTestNativeF64Case cases[] = {
        {MMAY_MIR_ADD_F64, UINT64_C(0x3ff8000000000000), UINT64_C(0x4002000000000000), false, UINT64_C(0x400e000000000000)},
        {MMAY_MIR_DIVIDE_F64, UINT64_C(0x3ff0000000000000), UINT64_C(0x8000000000000000), false, UINT64_C(0xfff0000000000000)},
        {MMAY_MIR_MULTIPLY_F64, UINT64_C(0x7ff0000000000000), UINT64_C(0x0000000000000000), false, UINT64_C(0x7ff8000000000000)},
        {MMAY_MIR_NEGATE_F64, UINT64_C(0x7ff0000000000001), 0, false, UINT64_C(0xfff0000000000001)},
        {MMAY_MIR_EQUAL_F64, UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000), true, 1},
        {MMAY_MIR_EQUAL_F64, UINT64_C(0x7ff8000000000000), UINT64_C(0x7ff8000000000000), true, 0},
        {MMAY_MIR_LESS_F64, UINT64_C(0x7ff8000000000000), UINT64_C(0x3ff0000000000000), true, 0},
        {MMAY_MIR_LESS_EQUAL_F64, UINT64_C(0xbff0000000000000), UINT64_C(0xbff0000000000000), true, 1}
    };
    const MarimaySymbolId symbol = {{0x50}};
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayMirFunction function;
        MarimayMirValue mir_result;
        MarimayBytecodeFunction bytecode;
        MarimayBytecodeValue vm_result;
        MarimayOwnedBytes source;
        const MarimayMirFunction *functions[1];
        MarimayNativeCProgram program;
        char expected[64];
        memset(&function, 0, sizeof(function)); memset(&mir_result, 0, sizeof(mir_result));
        memset(&bytecode, 0, sizeof(bytecode)); memset(&vm_result, 0, sizeof(vm_result));
        memset(&source, 0, sizeof(source));
        MMAY_EXPECT(mmay_test_native_build_f64_operation(&function, symbol,
            cases[index].kind, cases[index].left, cases[index].right));
        MMAY_EXPECT(mmay_mir_interpret(&function, NULL, 0, mmay_system_allocator(),
            mmay_mir_interpret_default_options(), &mir_result) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_mir_lower_bytecode_function(&function, mmay_system_allocator(),
            mmay_mir_bytecode_lower_default_options(), &bytecode) == MMAY_MIR_BYTECODE_LOWER_OK);
        MMAY_EXPECT(mmay_bytecode_run(&bytecode, NULL, 0, mmay_system_allocator(),
            mmay_bytecode_run_default_options(), &vm_result) == MMAY_BYTECODE_RUN_OK);
        if (cases[index].boolean) {
            MMAY_EXPECT(mir_result.type == MMAY_TYPE_BOOL && mir_result.as.boolean == (cases[index].expected_bits != 0));
            MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_BOOL && vm_result.as.boolean == (cases[index].expected_bits != 0));
            (void)snprintf(expected, sizeof(expected), "ok:bool:%s\n", cases[index].expected_bits != 0 ? "true" : "false");
        } else {
            MMAY_EXPECT(mir_result.type == MMAY_TYPE_F64 && mir_result.as.f64_bits == cases[index].expected_bits);
            MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_F64 && vm_result.as.f64_bits == cases[index].expected_bits);
            (void)snprintf(expected, sizeof(expected), "ok:f64:%016llx\n", (unsigned long long)cases[index].expected_bits);
        }
        functions[0] = &function;
        program.functions = functions; program.function_count = 1; program.entry_symbol = symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(&program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &source) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(&source, expected, 0);
#endif
        mmay_owned_bytes_dispose(&source); mmay_bytecode_function_dispose(&bytecode);
        mmay_mir_function_dispose(&function);
    }
}

static void mmay_test_native_f64_direct_call(void) {
    const MarimaySymbolId callee_symbol = {{0x51}};
    const MarimaySymbolId entry_symbol = {{0x52}};
    MarimayMirFunction callee;
    MarimayMirFunction entry;
    const MarimayMirFunction *functions[2];
    MarimayBytecodeProgram bytecode;
    MarimayBytecodeValue result;
    MarimayOwnedBytes first;
    MarimayOwnedBytes second;
    MarimayNativeCProgram program;
    memset(&callee, 0, sizeof(callee)); memset(&entry, 0, sizeof(entry));
    memset(&bytecode, 0, sizeof(bytecode)); memset(&result, 0, sizeof(result));
    memset(&first, 0, sizeof(first)); memset(&second, 0, sizeof(second));
    MMAY_EXPECT(mmay_test_native_build_f64_identity(&callee, callee_symbol));
    MMAY_EXPECT(mmay_test_native_build_f64_direct_entry(&entry, entry_symbol,
        callee_symbol));
    functions[0] = &callee; functions[1] = &entry;
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(functions, 2, entry_symbol,
        mmay_system_allocator(), mmay_mir_bytecode_program_lower_default_options(),
        &bytecode) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_run_program(&bytecode, NULL, 0,
        mmay_system_allocator(), mmay_bytecode_run_default_options(), &result)
        == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x8000000000000000));
    mmay_bytecode_program_dispose(&bytecode);
    program.functions = functions; program.function_count = 2;
    program.entry_symbol = entry_symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(&program, mmay_system_allocator(),
        mmay_native_c_emit_default_options(), &first) == MMAY_NATIVE_C_EMIT_OK);
    {
        const MarimayMirFunction *permuted[2] = {&entry, &callee};
        program.functions = permuted;
        MMAY_EXPECT(mmay_native_c_emit_program(&program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &second) == MMAY_NATIVE_C_EMIT_OK);
    }
    MMAY_EXPECT(first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(&first, "ok:f64:8000000000000000\n", 0);
#endif
    mmay_owned_bytes_dispose(&second); mmay_owned_bytes_dispose(&first);
    mmay_mir_function_dispose(&entry); mmay_mir_function_dispose(&callee);
}

static MarimayTypeId mmay_test_native_integer_type(MarimayIntegerKind kind) {
    static const MarimayTypeId types[] = {
        MMAY_TYPE_I8, MMAY_TYPE_I16, MMAY_TYPE_I32, MMAY_TYPE_I64,
        MMAY_TYPE_U8, MMAY_TYPE_U16, MMAY_TYPE_U32, MMAY_TYPE_U64
    };
    return kind <= MMAY_INTEGER_U64 ? types[kind] : MMAY_TYPE_INVALID;
}

static MarimayBytecodeType mmay_test_native_bytecode_integer_type(
    MarimayIntegerKind kind
) {
    static const MarimayBytecodeType types[] = {
        MMAY_BYTECODE_TYPE_I8, MMAY_BYTECODE_TYPE_I16,
        MMAY_BYTECODE_TYPE_I32, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_U8, MMAY_BYTECODE_TYPE_U16,
        MMAY_BYTECODE_TYPE_U32, MMAY_BYTECODE_TYPE_U64
    };
    return kind <= MMAY_INTEGER_U64 ? types[kind] : MMAY_BYTECODE_TYPE_INVALID;
}

static MarimayMirInstruction mmay_test_native_integer_constant(
    MarimayMirRegister target,
    uint64_t bits,
    size_t span
) {
    MarimayMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_INTEGER;
    instruction.target = target;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.integer_bits = bits;
    instruction.span.start = span;
    instruction.span.end = span + 1;
    return instruction;
}

static bool mmay_test_native_build_integer_widths(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimayIntegerKind kind
) {
    MarimayTypeId type = mmay_test_native_integer_type(kind);
    MarimayMirRegister constants[4] = {MMAY_MIR_NO_REGISTER};
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    static const uint64_t constant_bits[] = {6, 1, 2, 3};
    static const MarimayMirInstructionKind operations[] = {
        MMAY_MIR_ADD_CHECKED_I64,
        MMAY_MIR_SUBTRACT_CHECKED_I64,
        MMAY_MIR_MULTIPLY_CHECKED_I64,
        MMAY_MIR_DIVIDE_CHECKED_I64,
        MMAY_MIR_REMAINDER_CHECKED_I64,
        MMAY_MIR_BITWISE_AND_I64,
        MMAY_MIR_BITWISE_XOR_I64,
        MMAY_MIR_BITWISE_OR_I64,
        MMAY_MIR_SHIFT_LEFT_CHECKED_I64,
        MMAY_MIR_SHIFT_RIGHT_I64
    };
    static const MarimayMirRegister operation_right[] = {
        1, 1, 2, 1, 3, 3, 3, 1, 1, 1
    };
    size_t index = 0;
    if (type == MMAY_TYPE_INVALID) {
        return false;
    }
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol,
            NULL, 0, type, mmay_mir_function_default_options())
        || !mmay_mir_add_block(function, &block)
        || !mmay_mir_add_register(function, type, &constants[0])
        || !mmay_mir_add_register(function, type, &constants[1])
        || !mmay_mir_add_register(function, type, &constants[2])
        || !mmay_mir_add_register(function, type, &constants[3])
        || !mmay_mir_add_register(function, type, &result)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    for (index = 0; index < 4; index += 1) {
        instruction = mmay_test_native_integer_constant(
            constants[index], constant_bits[index], index + 10
        );
        if (!mmay_mir_append_instruction(function, block, &instruction)) {
            mmay_mir_function_dispose(function);
            return false;
        }
    }
    for (index = 0; index < sizeof(operations) / sizeof(operations[0]);
        index += 1) {
        instruction = mmay_test_native_instruction(
            operations[index], result, result == MMAY_MIR_NO_REGISTER
                ? constants[0] : result,
            operation_right[index], 0, false, 20 + index
        );
        if (index == 0) {
            instruction.first = constants[0];
        }
        if (!mmay_mir_append_instruction(function, block, &instruction)) {
            mmay_mir_function_dispose(function);
            return false;
        }
    }
    if (!mmay_mir_set_terminator(function, block,
            mmay_test_native_terminator(MMAY_MIR_RETURN, result,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 40))) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_native_build_integer_comparison(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimayIntegerKind kind,
    MarimayMirInstructionKind operation,
    uint64_t left_bits,
    uint64_t right_bits
) {
    MarimayTypeId type = mmay_test_native_integer_type(kind);
    MarimayMirRegister left = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister right = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol,
            NULL, 0, MMAY_TYPE_BOOL, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, type, &left)
        || !mmay_mir_add_register(function, type, &right)
        || !mmay_mir_add_register(function, MMAY_TYPE_BOOL, &result)
        || !mmay_mir_add_block(function, &block)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_integer_constant(left, left_bits, 50);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_integer_constant(right, right_bits, 51);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_instruction(
        operation, result, left, right, 0, false, 52
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_native_terminator(MMAY_MIR_RETURN, result,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 53))) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static void mmay_test_native_integer_comparisons(void) {
    static const MarimayIntegerKind kinds[] = {
        MMAY_INTEGER_I8, MMAY_INTEGER_I16, MMAY_INTEGER_I32, MMAY_INTEGER_I64,
        MMAY_INTEGER_U8, MMAY_INTEGER_U16, MMAY_INTEGER_U32, MMAY_INTEGER_U64
    };
    static const MarimayMirInstructionKind operations[] = {
        MMAY_MIR_EQUAL_I64, MMAY_MIR_LESS_I64
    };
    const MarimaySymbolId symbol = {{0x63}};
    size_t kind_index = 0;
    size_t operation_index = 0;
    for (kind_index = 0; kind_index < sizeof(kinds) / sizeof(kinds[0]);
        kind_index += 1) {
        MarimayIntegerKind kind = kinds[kind_index];
        unsigned int width = mmay_integer_bit_width(kind);
        uint64_t sign_bit = UINT64_C(1) << (width - 1U);
        const uint64_t left_bits[] = {sign_bit, sign_bit};
        const uint64_t right_bits[] = {sign_bit, 1};
        for (operation_index = 0; operation_index < 2; operation_index += 1) {
            MarimayMirFunction function;
            MarimayMirValue mir_result;
            MarimayBytecodeFunction bytecode;
            MarimayBytecodeValue bytecode_result;
            MarimayOwnedBytes source;
            MarimayNativeCProgram program;
            const MarimayMirFunction *functions[1];
            bool expected = operation_index == 0
                ? true : mmay_integer_kind_is_signed(kind);
            char expected_output[64];
            memset(&function, 0, sizeof(function));
            memset(&mir_result, 0, sizeof(mir_result));
            memset(&bytecode, 0, sizeof(bytecode));
            memset(&bytecode_result, 0, sizeof(bytecode_result));
            memset(&source, 0, sizeof(source));
            MMAY_EXPECT(mmay_test_native_build_integer_comparison(
                &function, symbol, kind, operations[operation_index],
                left_bits[operation_index], right_bits[operation_index]
            ));
            MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
            MMAY_EXPECT(mmay_mir_interpret(
                &function, NULL, 0, mmay_system_allocator(),
                mmay_mir_interpret_default_options(), &mir_result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(mir_result.type == MMAY_TYPE_BOOL
                && mir_result.as.boolean == expected);
            MMAY_EXPECT(mmay_mir_lower_bytecode_function(
                &function, mmay_system_allocator(),
                mmay_mir_bytecode_lower_default_options(), &bytecode
            ) == MMAY_MIR_BYTECODE_LOWER_OK);
            bytecode_result.type = MMAY_BYTECODE_TYPE_BOOL;
            MMAY_EXPECT(mmay_bytecode_run(
                &bytecode, NULL, 0, mmay_system_allocator(),
                mmay_bytecode_run_default_options(), &bytecode_result
            ) == MMAY_BYTECODE_RUN_OK);
            MMAY_EXPECT(bytecode_result.as.boolean == expected);
            functions[0] = &function;
            program.functions = functions;
            program.function_count = 1;
            program.entry_symbol = symbol;
            MMAY_EXPECT(mmay_native_c_emit_program(
                &program, mmay_system_allocator(),
                mmay_native_c_emit_default_options(), &source
            ) == MMAY_NATIVE_C_EMIT_OK);
            (void)snprintf(expected_output, sizeof(expected_output),
                "ok:bool:%s\n", expected ? "true" : "false");
#if !defined(_WIN32)
            mmay_test_native_compile_and_run(&source, expected_output, 0);
#endif
            mmay_owned_bytes_dispose(&source);
            mmay_bytecode_function_dispose(&bytecode);
            mmay_mir_function_dispose(&function);
        }
    }
}

static bool mmay_test_native_build_integer_fault(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimayIntegerKind kind,
    MarimayMirInstructionKind operation,
    uint64_t left_bits,
    uint64_t right_bits,
    size_t span
) {
    MarimayTypeId type = mmay_test_native_integer_type(kind);
    MarimayMirRegister left = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister right = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol,
            NULL, 0, type, mmay_mir_function_default_options())
        || !mmay_mir_add_register(function, type, &left)
        || !mmay_mir_add_register(function, type, &right)
        || !mmay_mir_add_register(function, type, &result)
        || !mmay_mir_add_block(function, &block)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_integer_constant(left, left_bits, span);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_integer_constant(right, right_bits, span + 1);
    if (!mmay_mir_append_instruction(function, block, &instruction)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_native_instruction(
        operation, result, left, right, 0, false, span + 2
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(function, block,
            mmay_test_native_terminator(MMAY_MIR_RETURN, result,
                MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, span + 3))) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static uint64_t mmay_test_native_integer_mask(MarimayIntegerKind kind) {
    unsigned int width = mmay_integer_bit_width(kind);
    return width == 64U ? UINT64_MAX
        : (UINT64_C(1) << width) - UINT64_C(1);
}

static void mmay_test_native_integer_faults(void) {
    static const MarimayIntegerKind kinds[] = {
        MMAY_INTEGER_I8, MMAY_INTEGER_I16, MMAY_INTEGER_I32, MMAY_INTEGER_I64,
        MMAY_INTEGER_U8, MMAY_INTEGER_U16, MMAY_INTEGER_U32, MMAY_INTEGER_U64
    };
    static const MarimayMirInstructionKind overflow_operations[] = {
        MMAY_MIR_ADD_CHECKED_I64,
        MMAY_MIR_DIVIDE_CHECKED_I64,
        MMAY_MIR_SHIFT_LEFT_CHECKED_I64
    };
    static const MarimayMirInterpretStatus overflow_statuses[] = {
        MMAY_MIR_INTERPRET_OVERFLOW,
        MMAY_MIR_INTERPRET_DIVISION_BY_ZERO,
        MMAY_MIR_INTERPRET_INVALID_SHIFT
    };
    const MarimaySymbolId symbol = {{0x62}};
    size_t kind_index = 0;
    size_t case_index = 0;
    for (kind_index = 0; kind_index < sizeof(kinds) / sizeof(kinds[0]);
        kind_index += 1) {
        MarimayIntegerKind kind = kinds[kind_index];
        uint64_t mask = mmay_test_native_integer_mask(kind);
        uint64_t width = mmay_integer_bit_width(kind);
        const uint64_t left_bits[] = {
            mmay_integer_kind_is_signed(kind)
                ? (UINT64_C(1) << (width - 1U)) - UINT64_C(1)
                : mask,
            1,
            1
        };
        const uint64_t right_bits[] = {1, 0, width};
        for (case_index = 0;
            case_index < sizeof(overflow_operations)
                / sizeof(overflow_operations[0]);
            case_index += 1) {
            MarimayMirFunction function;
            MarimayMirValue mir_result;
            MarimayBytecodeFunction bytecode;
            MarimayBytecodeValue bytecode_result;
            MarimayBytecodeTrap trap;
            MarimayBytecodeRunOptions options;
            MarimayOwnedBytes source;
            MarimayNativeCProgram program;
            const MarimayMirFunction *functions[1];
            char expected[64];
            MarimayMirInterpretStatus mir_status;
            MarimayBytecodeRunStatus bytecode_status;
            MarimayBytecodeType bytecode_type =
                mmay_test_native_bytecode_integer_type(kind);
            memset(&function, 0, sizeof(function));
            memset(&mir_result, 0, sizeof(mir_result));
            memset(&bytecode, 0, sizeof(bytecode));
            memset(&bytecode_result, 0, sizeof(bytecode_result));
            memset(&trap, 0, sizeof(trap));
            memset(&source, 0, sizeof(source));
            MMAY_EXPECT(mmay_test_native_build_integer_fault(
                &function, symbol, kind, overflow_operations[case_index],
                left_bits[case_index], right_bits[case_index], 70
            ));
            mir_status = mmay_mir_interpret(
                &function, NULL, 0, mmay_system_allocator(),
                mmay_mir_interpret_default_options(), &mir_result
            );
            MMAY_EXPECT(mir_status == overflow_statuses[case_index]);
            MMAY_EXPECT(mmay_mir_lower_bytecode_function(
                &function, mmay_system_allocator(),
                mmay_mir_bytecode_lower_default_options(), &bytecode
            ) == MMAY_MIR_BYTECODE_LOWER_OK);
            bytecode_result.type = bytecode_type;
            bytecode_result.as.integer_bits = UINT64_C(0xfeed);
            options = mmay_bytecode_run_default_options();
            options.trap = &trap;
            bytecode_status = mmay_bytecode_run(
                &bytecode, NULL, 0, mmay_system_allocator(), options,
                &bytecode_result
            );
            MMAY_EXPECT((case_index == 0
                ? bytecode_status == MMAY_BYTECODE_RUN_OVERFLOW
                : case_index == 1
                    ? bytecode_status == MMAY_BYTECODE_RUN_DIVISION_BY_ZERO
                    : bytecode_status == MMAY_BYTECODE_RUN_INVALID_SHIFT));
            functions[0] = &function;
            program.functions = functions;
            program.function_count = 1;
            program.entry_symbol = symbol;
            MMAY_EXPECT(mmay_native_c_emit_program(
                &program, mmay_system_allocator(),
                mmay_native_c_emit_default_options(), &source
            ) == MMAY_NATIVE_C_EMIT_OK);
            (void)snprintf(expected, sizeof(expected), "trap:%d:72:73\n",
                case_index == 0 ? 1 : case_index == 1 ? 2 : 3);
#if !defined(_WIN32)
            mmay_test_native_compile_and_run(&source, expected, 1);
#endif
            mmay_owned_bytes_dispose(&source);
            mmay_bytecode_function_dispose(&bytecode);
            mmay_mir_function_dispose(&function);
        }
        if (mmay_integer_kind_is_signed(kind)) {
            const MarimayMirInstructionKind signed_operations[] = {
                MMAY_MIR_DIVIDE_CHECKED_I64,
                MMAY_MIR_REMAINDER_CHECKED_I64
            };
            for (case_index = 0; case_index < 2; case_index += 1) {
                MarimayMirFunction function;
                MarimayMirValue mir_result;
                MarimayBytecodeFunction bytecode;
                MarimayBytecodeValue bytecode_result;
                MarimayBytecodeTrap trap;
                MarimayBytecodeRunOptions options;
                MarimayOwnedBytes source;
                MarimayNativeCProgram program;
                const MarimayMirFunction *functions[1];
                char expected[64];
                MarimayBytecodeRunStatus bytecode_status;
                MarimayBytecodeType bytecode_type =
                    mmay_test_native_bytecode_integer_type(kind);
                memset(&function, 0, sizeof(function));
                memset(&mir_result, 0, sizeof(mir_result));
                memset(&bytecode, 0, sizeof(bytecode));
                memset(&bytecode_result, 0, sizeof(bytecode_result));
                memset(&trap, 0, sizeof(trap));
                memset(&source, 0, sizeof(source));
                MMAY_EXPECT(mmay_test_native_build_integer_fault(
                    &function, symbol, kind, signed_operations[case_index],
                    UINT64_C(1) << (width - 1U), mask, 80
                ));
                MMAY_EXPECT(mmay_mir_interpret(
                    &function, NULL, 0, mmay_system_allocator(),
                    mmay_mir_interpret_default_options(), &mir_result
                ) == MMAY_MIR_INTERPRET_OVERFLOW);
                MMAY_EXPECT(mmay_mir_lower_bytecode_function(
                    &function, mmay_system_allocator(),
                    mmay_mir_bytecode_lower_default_options(), &bytecode
                ) == MMAY_MIR_BYTECODE_LOWER_OK);
                bytecode_result.type = bytecode_type;
                options = mmay_bytecode_run_default_options();
                options.trap = &trap;
                bytecode_status = mmay_bytecode_run(
                    &bytecode, NULL, 0, mmay_system_allocator(), options,
                    &bytecode_result
                );
                MMAY_EXPECT(bytecode_status == MMAY_BYTECODE_RUN_OVERFLOW);
                functions[0] = &function;
                program.functions = functions;
                program.function_count = 1;
                program.entry_symbol = symbol;
                MMAY_EXPECT(mmay_native_c_emit_program(
                    &program, mmay_system_allocator(),
                    mmay_native_c_emit_default_options(), &source
                ) == MMAY_NATIVE_C_EMIT_OK);
                (void)snprintf(expected, sizeof(expected), "trap:1:82:83\n");
#if !defined(_WIN32)
                mmay_test_native_compile_and_run(&source, expected, 1);
#endif
                mmay_owned_bytes_dispose(&source);
                mmay_bytecode_function_dispose(&bytecode);
                mmay_mir_function_dispose(&function);
            }
        }
    }
}

static void mmay_test_native_integer_widths(void) {
    static const MarimayIntegerKind kinds[] = {
        MMAY_INTEGER_I8, MMAY_INTEGER_I16, MMAY_INTEGER_I32, MMAY_INTEGER_I64,
        MMAY_INTEGER_U8, MMAY_INTEGER_U16, MMAY_INTEGER_U32, MMAY_INTEGER_U64
    };
    const MarimaySymbolId symbol = {{0x61}};
    size_t index = 0;
    for (index = 0; index < sizeof(kinds) / sizeof(kinds[0]); index += 1) {
        MarimayMirFunction function;
        MarimayMirValue mir_result;
        MarimayBytecodeFunction bytecode;
        MarimayBytecodeValue bytecode_result;
        MarimayOwnedBytes source;
        MarimayNativeCProgram program;
        const MarimayMirFunction *functions[1];
        char expected[64];
        MarimayTypeId type = mmay_test_native_integer_type(kinds[index]);
        MarimayBytecodeType bytecode_type =
            mmay_test_native_bytecode_integer_type(kinds[index]);
        int64_t signed_result = 0;
        uint64_t unsigned_result = 0;
        memset(&function, 0, sizeof(function));
        memset(&mir_result, 0, sizeof(mir_result));
        memset(&bytecode, 0, sizeof(bytecode));
        memset(&bytecode_result, 0, sizeof(bytecode_result));
        memset(&source, 0, sizeof(source));
        MMAY_EXPECT(mmay_test_native_build_integer_widths(
            &function, symbol, kinds[index]
        ));
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(mmay_mir_interpret(
            &function, NULL, 0, mmay_system_allocator(),
            mmay_mir_interpret_default_options(), &mir_result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mir_result.type == type);
        MMAY_EXPECT(mir_result.as.integer_bits == UINT64_C(3));
        MMAY_EXPECT(mmay_mir_lower_bytecode_function(
            &function, mmay_system_allocator(),
            mmay_mir_bytecode_lower_default_options(), &bytecode
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        bytecode_result.type = bytecode_type;
        bytecode_result.as.integer_bits = UINT64_MAX;
        MMAY_EXPECT(mmay_bytecode_run(
            &bytecode, NULL, 0, mmay_system_allocator(),
            mmay_bytecode_run_default_options(), &bytecode_result
        ) == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(bytecode_result.type == bytecode_type);
        MMAY_EXPECT(bytecode_result.as.integer_bits == UINT64_C(3));
        functions[0] = &function;
        program.functions = functions;
        program.function_count = 1;
        program.entry_symbol = symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &source
        ) == MMAY_NATIVE_C_EMIT_OK);
        if (mmay_integer_kind_is_signed(kinds[index])) {
            MMAY_EXPECT(mmay_integer_to_i64(
                (MarimayIntegerValue){kinds[index], 3}, &signed_result
            ));
            (void)snprintf(expected, sizeof(expected), "ok:%s:%lld\n",
                index == 0 ? "i8" : index == 1 ? "i16"
                    : index == 2 ? "i32" : "i64",
                (long long)signed_result);
        } else {
            MMAY_EXPECT(mmay_integer_to_u64(
                (MarimayIntegerValue){kinds[index], 3}, &unsigned_result
            ));
            (void)snprintf(expected, sizeof(expected), "ok:%s:%llu\n",
                index == 4 ? "u8" : index == 5 ? "u16"
                    : index == 6 ? "u32" : "u64",
                (unsigned long long)unsigned_result);
        }
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(&source, expected, 0);
#endif
        mmay_owned_bytes_dispose(&source);
        mmay_bytecode_function_dispose(&bytecode);
        mmay_mir_function_dispose(&function);
    }
}


void mmay_test_native_scalar_f64(void) {
    mmay_test_native_integer_widths();
    mmay_test_native_integer_comparisons();
    mmay_test_native_integer_faults();
    mmay_test_native_scalar_differential();
    mmay_test_native_f64_differential();
    mmay_test_native_f64_direct_call();
}
