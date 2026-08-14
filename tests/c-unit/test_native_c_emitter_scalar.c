#include "test_native_c_emitter_support.h"

#include <inttypes.h>
#include <string.h>

typedef enum RudaTestNativeScalarOutcome {
    RUDA_TEST_NATIVE_SCALAR_I64,
    RUDA_TEST_NATIVE_SCALAR_BOOL,
    RUDA_TEST_NATIVE_SCALAR_OVERFLOW,
    RUDA_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO,
    RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT
} RudaTestNativeScalarOutcome;

typedef struct RudaTestNativeScalarCase {
    RudaMirInstructionKind kind;
    int64_t left;
    int64_t right;
    bool left_boolean;
    bool right_boolean;
    RudaTestNativeScalarOutcome outcome;
    int64_t expected_i64;
    bool expected_boolean;
    bool native;
} RudaTestNativeScalarCase;

static bool ruda_test_native_scalar_is_boolean(RudaMirInstructionKind kind) {
    return kind == RUDA_MIR_NOT_BOOL || kind == RUDA_MIR_EQUAL_BOOL
        || kind == RUDA_MIR_EQUAL_I64 || kind == RUDA_MIR_LESS_I64
        || kind == RUDA_MIR_LESS_EQUAL_I64;
}

static bool ruda_test_native_scalar_is_boolean_input(
    RudaMirInstructionKind kind
) {
    return kind == RUDA_MIR_NOT_BOOL || kind == RUDA_MIR_EQUAL_BOOL;
}

static bool ruda_test_native_scalar_is_unary(RudaMirInstructionKind kind) {
    return kind == RUDA_MIR_NOT_BOOL || kind == RUDA_MIR_NEGATE_CHECKED_I64
        || kind == RUDA_MIR_BITWISE_NOT_I64;
}

static bool ruda_test_native_build_scalar(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    const RudaTestNativeScalarCase *test_case
) {
    RudaTypeId input_type = ruda_test_native_scalar_is_boolean_input(
        test_case->kind
    ) ? RUDA_TYPE_BOOL : RUDA_TYPE_I64;
    RudaTypeId result_type = ruda_test_native_scalar_is_boolean(
        test_case->kind
    ) ? RUDA_TYPE_BOOL : RUDA_TYPE_I64;
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, result_type, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, input_type, &left)
        || (!ruda_test_native_scalar_is_unary(test_case->kind)
            && !ruda_mir_add_register(function, input_type, &right))
        || !ruda_mir_add_register(function, result_type, &result)
        || !ruda_mir_add_block(function, &block)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    if (ruda_test_native_scalar_is_unary(test_case->kind)) {
        right = left;
    }
    instruction = ruda_test_native_instruction(
        input_type == RUDA_TYPE_BOOL ? RUDA_MIR_CONSTANT_BOOL
            : RUDA_MIR_CONSTANT_I64,
        left, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, test_case->left,
        test_case->left_boolean, 300
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)) {
        goto failure;
    }
    if (!ruda_test_native_scalar_is_unary(test_case->kind)) {
        instruction = ruda_test_native_instruction(
            input_type == RUDA_TYPE_BOOL ? RUDA_MIR_CONSTANT_BOOL
                : RUDA_MIR_CONSTANT_I64,
            right, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, test_case->right,
            test_case->right_boolean, 301
        );
        if (!ruda_mir_append_instruction(function, block, &instruction)) {
            goto failure;
        }
    }
    instruction = ruda_test_native_instruction(test_case->kind, result, left,
        right, 0, false, 302);
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 303))) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static void ruda_test_native_scalar_differential(void) {
    static const RudaTestNativeScalarCase cases[] = {
        {RUDA_MIR_NOT_BOOL, 0, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_BOOL, 0, true, true},
        {RUDA_MIR_EQUAL_BOOL, 0, 0, true, true,
            RUDA_TEST_NATIVE_SCALAR_BOOL, 0, true, false},
        {RUDA_MIR_EQUAL_I64, -1, -1, false, false,
            RUDA_TEST_NATIVE_SCALAR_BOOL, 0, true, false},
        {RUDA_MIR_LESS_I64, INT64_MIN, INT64_MAX, false, false,
            RUDA_TEST_NATIVE_SCALAR_BOOL, 0, true, false},
        {RUDA_MIR_LESS_EQUAL_I64, 7, 7, false, false,
            RUDA_TEST_NATIVE_SCALAR_BOOL, 0, true, false},
        {RUDA_MIR_BITWISE_NOT_I64, -1, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, 0, false, true},
        {RUDA_MIR_BITWISE_AND_I64, -1, 0x55, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, 0x55, false, false},
        {RUDA_MIR_BITWISE_XOR_I64, -1, 0x55, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, ~INT64_C(0x55), false, false},
        {RUDA_MIR_BITWISE_OR_I64, INT64_MIN, 7, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, INT64_MIN | 7, false, false},
        {RUDA_MIR_ADD_CHECKED_I64, 2, 3, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, 5, false, false},
        {RUDA_MIR_ADD_CHECKED_I64, INT64_MAX, 1, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, true},
        {RUDA_MIR_SUBTRACT_CHECKED_I64, INT64_MIN, 1, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {RUDA_MIR_MULTIPLY_CHECKED_I64, -7, 3, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -21, false, false},
        {RUDA_MIR_MULTIPLY_CHECKED_I64, INT64_MIN, -1, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, true},
        {RUDA_MIR_NEGATE_CHECKED_I64, INT64_MIN, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {RUDA_MIR_DIVIDE_CHECKED_I64, -7, 3, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -2, false, false},
        {RUDA_MIR_DIVIDE_CHECKED_I64, 7, -3, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -2, false, false},
        {RUDA_MIR_DIVIDE_CHECKED_I64, 1, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO, 0, false, true},
        {RUDA_MIR_DIVIDE_CHECKED_I64, INT64_MIN, -1, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {RUDA_MIR_REMAINDER_CHECKED_I64, -7, 3, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -1, false, false},
        {RUDA_MIR_REMAINDER_CHECKED_I64, 7, -3, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, 1, false, false},
        {RUDA_MIR_REMAINDER_CHECKED_I64, 1, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO, 0, false, false},
        {RUDA_MIR_REMAINDER_CHECKED_I64, INT64_MIN, -1, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, -1, false, false,
            RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT, 0, false, false},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, 1, false, false},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, 1, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, 2, false, false},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, 62, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, INT64_C(1) << 62, false, false},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, 63, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, 64, false, false,
            RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT, 0, false, true},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, INT64_MAX, 1, false, false,
            RUDA_TEST_NATIVE_SCALAR_OVERFLOW, 0, false, false},
        {RUDA_MIR_SHIFT_RIGHT_I64, -8, -1, false, false,
            RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT, 0, false, false},
        {RUDA_MIR_SHIFT_RIGHT_I64, -8, 0, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -8, false, false},
        {RUDA_MIR_SHIFT_RIGHT_I64, -8, 1, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -4, false, false},
        {RUDA_MIR_SHIFT_RIGHT_I64, -8, 62, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -1, false, false},
        {RUDA_MIR_SHIFT_RIGHT_I64, -8, 63, false, false,
            RUDA_TEST_NATIVE_SCALAR_I64, -1, false, false},
        {RUDA_MIR_SHIFT_RIGHT_I64, -8, 64, false, false,
            RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT, 0, false, true}
    };
    const RudaSymbolId symbol = {{0x30}};
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaMirFunction input;
        RudaMirValue mir_result;
        RudaMirInterpretStatus mir_status;
        RudaBytecodeFunction bytecode;
        RudaBytecodeValue bytecode_result;
        RudaBytecodeTrap trap;
        RudaBytecodeRunStatus bytecode_status;
        RudaOwnedBytes source;
        const RudaMirFunction *functions[1];
        RudaNativeCProgram program;
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
        RUDA_EXPECT(ruda_test_native_build_scalar(&input, symbol, &cases[index]));
        mir_status = ruda_mir_interpret(&input, NULL, 0,
            ruda_system_allocator(), ruda_mir_interpret_default_options(),
            &mir_result);
        RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input,
            ruda_system_allocator(), ruda_mir_bytecode_lower_default_options(),
            &bytecode) == RUDA_MIR_BYTECODE_LOWER_OK);
        {
            RudaBytecodeRunOptions options = ruda_bytecode_run_default_options();
            options.trap = &trap;
            bytecode_result.type = RUDA_BYTECODE_TYPE_I64;
            bytecode_result.as.i64 = 1234;
            bytecode_status = ruda_bytecode_run(&bytecode, NULL, 0,
                ruda_system_allocator(), options, &bytecode_result);
        }
        if (cases[index].outcome == RUDA_TEST_NATIVE_SCALAR_I64) {
            RUDA_EXPECT(mir_status == RUDA_MIR_INTERPRET_OK
                && bytecode_status == RUDA_BYTECODE_RUN_OK);
            RUDA_EXPECT(mir_result.type == RUDA_TYPE_I64
                && mir_result.as.i64 == cases[index].expected_i64);
            RUDA_EXPECT(bytecode_result.type == RUDA_BYTECODE_TYPE_I64
                && bytecode_result.as.i64 == cases[index].expected_i64);
            (void)snprintf(expected_output, sizeof(expected_output), "ok:i64:%lld\n",
                (long long)cases[index].expected_i64);
        } else if (cases[index].outcome == RUDA_TEST_NATIVE_SCALAR_BOOL) {
            RUDA_EXPECT(mir_status == RUDA_MIR_INTERPRET_OK
                && bytecode_status == RUDA_BYTECODE_RUN_OK);
            RUDA_EXPECT(mir_result.type == RUDA_TYPE_BOOL
                && mir_result.as.boolean == cases[index].expected_boolean);
            RUDA_EXPECT(bytecode_result.type == RUDA_BYTECODE_TYPE_BOOL
                && bytecode_result.as.boolean == cases[index].expected_boolean);
            (void)snprintf(expected_output, sizeof(expected_output), "ok:bool:%s\n",
                cases[index].expected_boolean ? "true" : "false");
        } else {
            RudaMirInterpretStatus expected_mir = RUDA_MIR_INTERPRET_OVERFLOW;
            RudaBytecodeRunStatus expected_bytecode = RUDA_BYTECODE_RUN_OVERFLOW;
            if (cases[index].outcome
                == RUDA_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO) {
                expected_mir = RUDA_MIR_INTERPRET_DIVISION_BY_ZERO;
                expected_bytecode = RUDA_BYTECODE_RUN_DIVISION_BY_ZERO;
                native_status = 2;
            } else if (cases[index].outcome
                == RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT) {
                expected_mir = RUDA_MIR_INTERPRET_INVALID_SHIFT;
                expected_bytecode = RUDA_BYTECODE_RUN_INVALID_SHIFT;
                native_status = 3;
            } else {
                native_status = 1;
            }
            RUDA_EXPECT(mir_status == expected_mir
                && bytecode_status == expected_bytecode);
            RUDA_EXPECT(bytecode_result.type == RUDA_BYTECODE_TYPE_I64
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
            RUDA_EXPECT(ruda_native_c_emit_program(&program,
                ruda_system_allocator(), ruda_native_c_emit_default_options(),
                &source) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
            ruda_test_native_compile_and_run(&source, expected_output, expected_exit);
#endif
            ruda_owned_bytes_dispose(&source);
        }
        ruda_bytecode_function_dispose(&bytecode);
        ruda_mir_function_dispose(&input);
    }
}

static bool ruda_test_native_f64_is_boolean(RudaMirInstructionKind kind) {
    return kind == RUDA_MIR_EQUAL_F64 || kind == RUDA_MIR_LESS_F64
        || kind == RUDA_MIR_LESS_EQUAL_F64;
}

static bool ruda_test_native_f64_is_unary(RudaMirInstructionKind kind) {
    return kind == RUDA_MIR_NEGATE_F64;
}

static bool ruda_test_native_build_f64_operation(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaMirInstructionKind kind,
    uint64_t left_bits,
    uint64_t right_bits
) {
    RudaMirRegister left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister right = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaTypeId result_type = ruda_test_native_f64_is_boolean(kind)
        ? RUDA_TYPE_BOOL : RUDA_TYPE_F64;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, result_type, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &left)
        || (!ruda_test_native_f64_is_unary(kind)
            && !ruda_mir_add_register(function, RUDA_TYPE_F64, &right))
        || !ruda_mir_add_register(function, result_type, &result)
        || !ruda_mir_add_block(function, &block)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    if (ruda_test_native_f64_is_unary(kind)) {
        right = left;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_F64, left,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 500);
    instruction.f64_bits = left_bits;
    if (!ruda_mir_append_instruction(function, block, &instruction)) goto failure;
    if (!ruda_test_native_f64_is_unary(kind)) {
        instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_F64, right,
            RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 501);
        instruction.f64_bits = right_bits;
        if (!ruda_mir_append_instruction(function, block, &instruction)) goto failure;
    }
    instruction = ruda_test_native_instruction(kind, result, left, right,
        0, false, 502);
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 503))) goto failure;
    return true;
failure:
    ruda_mir_function_dispose(function);
    return false;
}

static bool ruda_test_native_build_f64_identity(
    RudaMirFunction *function, RudaSymbolId symbol
) {
    const RudaTypeId parameter = RUDA_TYPE_F64;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol,
            &parameter, 1, RUDA_TYPE_F64, ruda_mir_function_default_options())
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, 0,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 510))) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_native_build_f64_direct_entry(
    RudaMirFunction *function, RudaSymbolId symbol, RudaSymbolId callee
) {
    const RudaMirCallArgument argument = {0, RUDA_TYPE_F64};
    RudaMirRegister value = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(function, ruda_system_allocator(), symbol, NULL,
            0, RUDA_TYPE_F64, ruda_mir_function_default_options())
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &value)
        || !ruda_mir_add_register(function, RUDA_TYPE_F64, &result)
        || !ruda_mir_add_block(function, &block)
        || !ruda_mir_add_call(function, callee, RUDA_TYPE_F64, &argument, 1, &call)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_native_instruction(RUDA_MIR_CONSTANT_F64, value,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 520);
    instruction.f64_bits = UINT64_C(0x8000000000000000);
    if (!ruda_mir_append_instruction(function, block, &instruction)) goto failure;
    instruction = ruda_test_native_instruction(RUDA_MIR_CALL, result,
        RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER, 0, false, 521);
    instruction.call = call;
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(function, block,
            ruda_test_native_terminator(RUDA_MIR_RETURN, result,
                RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 522))) goto failure;
    return true;
failure:
    ruda_mir_function_dispose(function);
    return false;
}

static void ruda_test_native_f64_differential(void) {
    typedef struct RudaTestNativeF64Case {
        RudaMirInstructionKind kind;
        uint64_t left;
        uint64_t right;
        bool boolean;
        uint64_t expected_bits;
    } RudaTestNativeF64Case;
    static const RudaTestNativeF64Case cases[] = {
        {RUDA_MIR_ADD_F64, UINT64_C(0x3ff8000000000000), UINT64_C(0x4002000000000000), false, UINT64_C(0x400e000000000000)},
        {RUDA_MIR_DIVIDE_F64, UINT64_C(0x3ff0000000000000), UINT64_C(0x8000000000000000), false, UINT64_C(0xfff0000000000000)},
        {RUDA_MIR_MULTIPLY_F64, UINT64_C(0x7ff0000000000000), UINT64_C(0x0000000000000000), false, UINT64_C(0x7ff8000000000000)},
        {RUDA_MIR_NEGATE_F64, UINT64_C(0x7ff0000000000001), 0, false, UINT64_C(0xfff0000000000001)},
        {RUDA_MIR_EQUAL_F64, UINT64_C(0x0000000000000000), UINT64_C(0x8000000000000000), true, 1},
        {RUDA_MIR_EQUAL_F64, UINT64_C(0x7ff8000000000000), UINT64_C(0x7ff8000000000000), true, 0},
        {RUDA_MIR_LESS_F64, UINT64_C(0x7ff8000000000000), UINT64_C(0x3ff0000000000000), true, 0},
        {RUDA_MIR_LESS_EQUAL_F64, UINT64_C(0xbff0000000000000), UINT64_C(0xbff0000000000000), true, 1}
    };
    const RudaSymbolId symbol = {{0x50}};
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaMirFunction function;
        RudaMirValue mir_result;
        RudaBytecodeFunction bytecode;
        RudaBytecodeValue vm_result;
        RudaOwnedBytes source;
        const RudaMirFunction *functions[1];
        RudaNativeCProgram program;
        char expected[64];
        memset(&function, 0, sizeof(function)); memset(&mir_result, 0, sizeof(mir_result));
        memset(&bytecode, 0, sizeof(bytecode)); memset(&vm_result, 0, sizeof(vm_result));
        memset(&source, 0, sizeof(source));
        RUDA_EXPECT(ruda_test_native_build_f64_operation(&function, symbol,
            cases[index].kind, cases[index].left, cases[index].right));
        RUDA_EXPECT(ruda_mir_interpret(&function, NULL, 0, ruda_system_allocator(),
            ruda_mir_interpret_default_options(), &mir_result) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_lower_bytecode_function(&function, ruda_system_allocator(),
            ruda_mir_bytecode_lower_default_options(), &bytecode) == RUDA_MIR_BYTECODE_LOWER_OK);
        RUDA_EXPECT(ruda_bytecode_run(&bytecode, NULL, 0, ruda_system_allocator(),
            ruda_bytecode_run_default_options(), &vm_result) == RUDA_BYTECODE_RUN_OK);
        if (cases[index].boolean) {
            RUDA_EXPECT(mir_result.type == RUDA_TYPE_BOOL && mir_result.as.boolean == (cases[index].expected_bits != 0));
            RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_BOOL && vm_result.as.boolean == (cases[index].expected_bits != 0));
            (void)snprintf(expected, sizeof(expected), "ok:bool:%s\n", cases[index].expected_bits != 0 ? "true" : "false");
        } else {
            RUDA_EXPECT(mir_result.type == RUDA_TYPE_F64 && mir_result.as.f64_bits == cases[index].expected_bits);
            RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_F64 && vm_result.as.f64_bits == cases[index].expected_bits);
            (void)snprintf(expected, sizeof(expected), "ok:f64:%016llx\n", (unsigned long long)cases[index].expected_bits);
        }
        functions[0] = &function;
        program.functions = functions; program.function_count = 1; program.entry_symbol = symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &source) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(&source, expected, 0);
#endif
        ruda_owned_bytes_dispose(&source); ruda_bytecode_function_dispose(&bytecode);
        ruda_mir_function_dispose(&function);
    }
}

static void ruda_test_native_f64_direct_call(void) {
    const RudaSymbolId callee_symbol = {{0x51}};
    const RudaSymbolId entry_symbol = {{0x52}};
    RudaMirFunction callee;
    RudaMirFunction entry;
    const RudaMirFunction *functions[2];
    RudaBytecodeProgram bytecode;
    RudaBytecodeValue result;
    RudaOwnedBytes first;
    RudaOwnedBytes second;
    RudaNativeCProgram program;
    memset(&callee, 0, sizeof(callee)); memset(&entry, 0, sizeof(entry));
    memset(&bytecode, 0, sizeof(bytecode)); memset(&result, 0, sizeof(result));
    memset(&first, 0, sizeof(first)); memset(&second, 0, sizeof(second));
    RUDA_EXPECT(ruda_test_native_build_f64_identity(&callee, callee_symbol));
    RUDA_EXPECT(ruda_test_native_build_f64_direct_entry(&entry, entry_symbol,
        callee_symbol));
    functions[0] = &callee; functions[1] = &entry;
    RUDA_EXPECT(ruda_mir_lower_bytecode_program(functions, 2, entry_symbol,
        ruda_system_allocator(), ruda_mir_bytecode_program_lower_default_options(),
        &bytecode) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_run_program(&bytecode, NULL, 0,
        ruda_system_allocator(), ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x8000000000000000));
    ruda_bytecode_program_dispose(&bytecode);
    program.functions = functions; program.function_count = 2;
    program.entry_symbol = entry_symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
        ruda_native_c_emit_default_options(), &first) == RUDA_NATIVE_C_EMIT_OK);
    {
        const RudaMirFunction *permuted[2] = {&entry, &callee};
        program.functions = permuted;
        RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &second) == RUDA_NATIVE_C_EMIT_OK);
    }
    RUDA_EXPECT(first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
#if !defined(_WIN32)
    ruda_test_native_compile_and_run(&first, "ok:f64:8000000000000000\n", 0);
#endif
    ruda_owned_bytes_dispose(&second); ruda_owned_bytes_dispose(&first);
    ruda_mir_function_dispose(&entry); ruda_mir_function_dispose(&callee);
}


void ruda_test_native_scalar_f64(void) {
    ruda_test_native_scalar_differential();
    ruda_test_native_f64_differential();
    ruda_test_native_f64_direct_call();
}
