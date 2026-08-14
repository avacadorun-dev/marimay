#include "test_bytecode_vm_support.h"

void ruda_test_vm_all_opcodes_and_limits(void) {
    RudaBytecodeFunction function;
    RudaBytecodeValue arguments[3];
    RudaBytecodeValue result;
    RudaBytecodeTrap trap;
    RudaBytecodeRunOptions options = ruda_bytecode_run_default_options();
    size_t shared_steps = 0;

    RUDA_EXPECT(ruda_test_vm_build_all_opcodes(&function, ruda_system_allocator()));
    arguments[0] = ruda_test_vm_bool(true);
    arguments[1] = ruda_test_vm_i64(40);
    arguments[2] = ruda_test_vm_i64(2);
    memset(&trap, 0, sizeof(trap));
    options.trap = &trap;
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 42);
    RUDA_EXPECT(!trap.has_span);
    arguments[0] = ruda_test_vm_bool(false);
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 42);

    options = ruda_bytecode_run_default_options();
    options.trap = &trap;
    options.max_steps = 22;
    arguments[0] = ruda_test_vm_bool(true);
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OK);
    options.max_steps = 21;
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_STEP_LIMIT);
    RUDA_EXPECT(trap.has_span);
    RUDA_EXPECT(trap.span.start == 21 && trap.span.end == 22);
    shared_steps = 22;
    options = ruda_bytecode_run_default_options();
    options.trap = &trap;
    options.remaining_shared_steps = &shared_steps;
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(shared_steps == 0);
    shared_steps = 21;
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_STEP_LIMIT);
    RUDA_EXPECT(shared_steps == 0);
    RUDA_EXPECT(trap.has_span);
    RUDA_EXPECT(trap.span.start == 21 && trap.span.end == 22);

    arguments[0] = ruda_test_vm_bool(false);
    options = ruda_bytecode_run_default_options();
    options.trap = &trap;
    options.max_steps = 23;
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OK);
    options.max_steps = 22;
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_STEP_LIMIT);
    RUDA_EXPECT(trap.has_span);
    RUDA_EXPECT(trap.span.start == 23 && trap.span.end == 24);
    shared_steps = 23;
    options = ruda_bytecode_run_default_options();
    options.trap = &trap;
    options.remaining_shared_steps = &shared_steps;
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(shared_steps == 0);
    shared_steps = 22;
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_STEP_LIMIT);
    RUDA_EXPECT(shared_steps == 0);
    RUDA_EXPECT(trap.has_span);
    RUDA_EXPECT(trap.span.start == 23 && trap.span.end == 24);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_vm_build_loop(&function, ruda_system_allocator()));
    options = ruda_bytecode_run_default_options();
    options.trap = &trap;
    options.max_steps = 3;
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_STEP_LIMIT);
    RUDA_EXPECT(trap.has_span);
    RUDA_EXPECT(trap.span.start == 0 && trap.span.end == 1);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_vm_build_void(&function, ruda_system_allocator()));
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        ruda_bytecode_run_default_options(), &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_VOID);
    ruda_bytecode_function_dispose(&function);
}

void ruda_test_vm_checked_arithmetic(void) {
    static const struct {
        RudaOpcode opcode;
        int64_t left;
        int64_t right;
        RudaBytecodeRunStatus status;
        int64_t value;
    } cases[] = {
        {RUDA_OPCODE_I64_ADD_CHECKED, 2, 3, RUDA_BYTECODE_RUN_OK, 5},
        {RUDA_OPCODE_I64_ADD_CHECKED, INT64_MAX, 1, RUDA_BYTECODE_RUN_OVERFLOW, 0},
        {RUDA_OPCODE_I64_ADD_CHECKED, INT64_MIN, -1, RUDA_BYTECODE_RUN_OVERFLOW, 0},
        {RUDA_OPCODE_I64_SUBTRACT_CHECKED, 7, 3, RUDA_BYTECODE_RUN_OK, 4},
        {RUDA_OPCODE_I64_SUBTRACT_CHECKED, INT64_MIN, 1, RUDA_BYTECODE_RUN_OVERFLOW, 0},
        {RUDA_OPCODE_I64_SUBTRACT_CHECKED, INT64_MAX, -1, RUDA_BYTECODE_RUN_OVERFLOW, 0},
        {RUDA_OPCODE_I64_MULTIPLY_CHECKED, -7, 3, RUDA_BYTECODE_RUN_OK, -21},
        {RUDA_OPCODE_I64_MULTIPLY_CHECKED, INT64_MAX, 2, RUDA_BYTECODE_RUN_OVERFLOW, 0},
        {RUDA_OPCODE_I64_MULTIPLY_CHECKED, INT64_MIN, -1, RUDA_BYTECODE_RUN_OVERFLOW, 0},
        {RUDA_OPCODE_I64_DIVIDE_CHECKED, 7, 3, RUDA_BYTECODE_RUN_OK, 2},
        {RUDA_OPCODE_I64_DIVIDE_CHECKED, -7, 3, RUDA_BYTECODE_RUN_OK, -2},
        {RUDA_OPCODE_I64_DIVIDE_CHECKED, 7, -3, RUDA_BYTECODE_RUN_OK, -2},
        {RUDA_OPCODE_I64_DIVIDE_CHECKED, -7, -3, RUDA_BYTECODE_RUN_OK, 2},
        {RUDA_OPCODE_I64_DIVIDE_CHECKED, 1, 0, RUDA_BYTECODE_RUN_DIVISION_BY_ZERO, 0},
        {RUDA_OPCODE_I64_DIVIDE_CHECKED, INT64_MIN, -1, RUDA_BYTECODE_RUN_OVERFLOW, 0},
        {RUDA_OPCODE_I64_REMAINDER_CHECKED, 7, 3, RUDA_BYTECODE_RUN_OK, 1},
        {RUDA_OPCODE_I64_REMAINDER_CHECKED, -7, 3, RUDA_BYTECODE_RUN_OK, -1},
        {RUDA_OPCODE_I64_REMAINDER_CHECKED, 7, -3, RUDA_BYTECODE_RUN_OK, 1},
        {RUDA_OPCODE_I64_REMAINDER_CHECKED, -7, -3, RUDA_BYTECODE_RUN_OK, -1},
        {RUDA_OPCODE_I64_REMAINDER_CHECKED, 1, 0, RUDA_BYTECODE_RUN_DIVISION_BY_ZERO, 0},
        {RUDA_OPCODE_I64_REMAINDER_CHECKED, INT64_MIN, -1, RUDA_BYTECODE_RUN_OVERFLOW, 0}
    };
    RudaBytecodeFunction function;
    RudaBytecodeValue arguments[2];
    RudaBytecodeValue result;
    RudaBytecodeTrap trap;
    size_t index = 0;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaBytecodeRunOptions options = ruda_bytecode_run_default_options();
        memset(&trap, 0, sizeof(trap));
        options.trap = &trap;
        RUDA_EXPECT(ruda_test_vm_build_binary(
            &function,
            ruda_system_allocator(),
            cases[index].opcode
        ));
        arguments[0] = ruda_test_vm_i64(cases[index].left);
        arguments[1] = ruda_test_vm_i64(cases[index].right);
        result = ruda_test_vm_i64(123456789);
        RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 2,
            ruda_system_allocator(), options, &result) == cases[index].status);
        if (cases[index].status == RUDA_BYTECODE_RUN_OK) {
            RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == cases[index].value);
        } else {
            RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 123456789);
            RUDA_EXPECT(trap.has_span);
            RUDA_EXPECT(trap.span.start == 10 && trap.span.end == 11);
        }
        ruda_bytecode_function_dispose(&function);
    }
    RUDA_EXPECT(ruda_test_vm_build_negate(&function, ruda_system_allocator()));
    arguments[0] = ruda_test_vm_i64(INT64_MIN);
    result = ruda_test_vm_i64(123456789);
    memset(&trap, 0, sizeof(trap));
    {
        RudaBytecodeRunOptions options = ruda_bytecode_run_default_options();
        options.trap = &trap;
        RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 1,
            ruda_system_allocator(), options, &result)
            == RUDA_BYTECODE_RUN_OVERFLOW);
    }
    RUDA_EXPECT(trap.has_span);
    RUDA_EXPECT(trap.span.start == 20 && trap.span.end == 21);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64);
    RUDA_EXPECT(result.as.i64 == 123456789);
    arguments[0] = ruda_test_vm_i64(7);
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 1,
        ruda_system_allocator(), ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == -7);
    ruda_bytecode_function_dispose(&function);
}

void ruda_test_vm_scalar_results(void) {
    static const struct {
        RudaOpcode opcode;
        int64_t left;
        int64_t right;
        bool expected;
    } comparisons[] = {
        {RUDA_OPCODE_I64_EQUAL, 5, 5, true},
        {RUDA_OPCODE_I64_EQUAL, 5, 4, false},
        {RUDA_OPCODE_I64_LESS, INT64_MIN, INT64_MAX, true},
        {RUDA_OPCODE_I64_LESS, 5, 5, false},
        {RUDA_OPCODE_I64_LESS_EQUAL, 5, 5, true},
        {RUDA_OPCODE_I64_LESS_EQUAL, INT64_MAX, INT64_MIN, false}
    };
    static const struct {
        RudaOpcode opcode;
        bool left;
        bool right;
        bool expected;
    } booleans[] = {
        {RUDA_OPCODE_BOOL_NOT, false, false, true},
        {RUDA_OPCODE_BOOL_NOT, true, false, false},
        {RUDA_OPCODE_BOOL_EQUAL, false, false, true},
        {RUDA_OPCODE_BOOL_EQUAL, false, true, false},
        {RUDA_OPCODE_BOOL_EQUAL, true, false, false},
        {RUDA_OPCODE_BOOL_EQUAL, true, true, true}
    };
    RudaBytecodeFunction function;
    RudaBytecodeValue arguments[2];
    RudaBytecodeValue result;
    size_t index = 0;
    for (index = 0;
        index < sizeof(comparisons) / sizeof(comparisons[0]);
        index += 1) {
        RUDA_EXPECT(ruda_test_vm_build_i64_comparison(
            &function,
            ruda_system_allocator(),
            comparisons[index].opcode
        ));
        arguments[0] = ruda_test_vm_i64(comparisons[index].left);
        arguments[1] = ruda_test_vm_i64(comparisons[index].right);
        RUDA_EXPECT(ruda_bytecode_run(
            &function,
            arguments,
            2,
            ruda_system_allocator(),
            ruda_bytecode_run_default_options(),
            &result
        ) == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_BOOL);
        RUDA_EXPECT(result.as.boolean == comparisons[index].expected);
        ruda_bytecode_function_dispose(&function);
    }
    for (index = 0; index < sizeof(booleans) / sizeof(booleans[0]);
        index += 1) {
        RUDA_EXPECT(ruda_test_vm_build_bool_operation(
            &function,
            ruda_system_allocator(),
            booleans[index].opcode
        ));
        arguments[0] = ruda_test_vm_bool(booleans[index].left);
        arguments[1] = ruda_test_vm_bool(booleans[index].right);
        RUDA_EXPECT(ruda_bytecode_run(
            &function,
            arguments,
            2,
            ruda_system_allocator(),
            ruda_bytecode_run_default_options(),
            &result
        ) == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_BOOL);
        RUDA_EXPECT(result.as.boolean == booleans[index].expected);
        ruda_bytecode_function_dispose(&function);
    }
    RUDA_EXPECT(ruda_test_vm_build_constants_and_copy(
        &function,
        ruda_system_allocator()
    ));
    RUDA_EXPECT(ruda_bytecode_run(
        &function,
        NULL,
        0,
        ruda_system_allocator(),
        ruda_bytecode_run_default_options(),
        &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64);
    RUDA_EXPECT(result.as.i64 == 42);
    ruda_bytecode_function_dispose(&function);

    for (index = 0; index < 2; index += 1) {
        bool value = index != 0;
        RUDA_EXPECT(ruda_test_vm_build_bool_constant_copy(
            &function,
            ruda_system_allocator(),
            value
        ));
        RUDA_EXPECT(ruda_bytecode_run(
            &function,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_bytecode_run_default_options(),
            &result
        ) == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_BOOL);
        RUDA_EXPECT(result.as.boolean == value);
        ruda_bytecode_function_dispose(&function);
    }
}

static void ruda_test_vm_rejects_before_frame_allocation(
    size_t instruction_index,
    bool replace_opcode,
    RudaOpcode opcode,
    size_t operand_index,
    uint64_t operand
) {
    RudaBytecodeFunction function;
    RudaBytecodeValue arguments[3];
    RudaBytecodeValue result = ruda_test_vm_i64(987654321);
    RudaBytecodeTrap trap;
    RudaBytecodeRunOptions options = ruda_bytecode_run_default_options();
    RudaFailAllocator run_state;
    RudaBytecodeBlock *entry = NULL;
    RudaBytecodeInstruction *instruction = NULL;

    RUDA_EXPECT(ruda_test_vm_build_all_opcodes(
        &function,
        ruda_system_allocator()
    ));
    entry = ruda_vector_at(&function.blocks, 0);
    RUDA_EXPECT(entry != NULL);
    if (entry != NULL) {
        instruction = ruda_vector_at(&entry->instructions, instruction_index);
    }
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        if (replace_opcode) {
            instruction->opcode = opcode;
        }
        if (operand_index < 3) {
            instruction->operands[operand_index].unsigned_value = operand;
        }
    }
    arguments[0] = ruda_test_vm_bool(true);
    arguments[1] = ruda_test_vm_i64(40);
    arguments[2] = ruda_test_vm_i64(2);
    memset(&trap, 0xff, sizeof(trap));
    options.trap = &trap;
    ruda_fail_allocator_init(&run_state, ruda_system_allocator(), 0);
    RUDA_EXPECT(ruda_bytecode_run(
        &function,
        arguments,
        3,
        ruda_fail_allocator(&run_state),
        options,
        &result
    ) == RUDA_BYTECODE_RUN_INVALID_BYTECODE);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64);
    RUDA_EXPECT(result.as.i64 == 987654321);
    RUDA_EXPECT(!trap.has_span);
    RUDA_EXPECT(trap.span.source == 0);
    RUDA_EXPECT(trap.span.start == 0);
    RUDA_EXPECT(trap.span.end == 0);
    RUDA_EXPECT(run_state.attempts == 0);
    RUDA_EXPECT(run_state.live_allocations == 0);
    RUDA_EXPECT(run_state.live_bytes == 0);
    ruda_bytecode_function_dispose(&function);
}

void ruda_test_vm_bitwise_and_shifts(void) {
    static const struct {
        RudaOpcode opcode;
        int64_t left;
        int64_t right;
        RudaBytecodeRunStatus status;
        int64_t value;
    } cases[] = {
        {RUDA_OPCODE_I64_BITWISE_NOT, 0, 0, RUDA_BYTECODE_RUN_OK, -1},
        {RUDA_OPCODE_I64_BITWISE_NOT, -1, 0, RUDA_BYTECODE_RUN_OK, 0},
        {RUDA_OPCODE_I64_BITWISE_AND, -1, INT64_C(0x55), RUDA_BYTECODE_RUN_OK, INT64_C(0x55)},
        {RUDA_OPCODE_I64_BITWISE_XOR, -1, INT64_C(0x55), RUDA_BYTECODE_RUN_OK, -86},
        {RUDA_OPCODE_I64_BITWISE_OR, INT64_MIN, 1, RUDA_BYTECODE_RUN_OK, INT64_MIN + 1},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, 0, RUDA_BYTECODE_RUN_OK, 1},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, 1, RUDA_BYTECODE_RUN_OK, 2},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, 62, RUDA_BYTECODE_RUN_OK,
            INT64_C(4611686018427387904)},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, -2, 1, RUDA_BYTECODE_RUN_OK, -4},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, -1, 63, RUDA_BYTECODE_RUN_OK, INT64_MIN},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, -2, 62, RUDA_BYTECODE_RUN_OK, INT64_MIN},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, -2, 63, RUDA_BYTECODE_RUN_OVERFLOW, 0},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, 0, 63, RUDA_BYTECODE_RUN_OK, 0},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, INT64_MIN, 0, RUDA_BYTECODE_RUN_OK, INT64_MIN},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, INT64_MAX, 0, RUDA_BYTECODE_RUN_OK, INT64_MAX},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, INT64_MAX, 1, RUDA_BYTECODE_RUN_OVERFLOW, 0},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, INT64_MIN, 1, RUDA_BYTECODE_RUN_OVERFLOW, 0},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, -1, RUDA_BYTECODE_RUN_INVALID_SHIFT, 0},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, 64, RUDA_BYTECODE_RUN_INVALID_SHIFT, 0},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, INT64_MIN, RUDA_BYTECODE_RUN_INVALID_SHIFT, 0},
        {RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, INT64_MAX, RUDA_BYTECODE_RUN_INVALID_SHIFT, 0},
        {RUDA_OPCODE_I64_SHIFT_RIGHT, -7, 0, RUDA_BYTECODE_RUN_OK, -7},
        {RUDA_OPCODE_I64_SHIFT_RIGHT, -1, 1, RUDA_BYTECODE_RUN_OK, -1},
        {RUDA_OPCODE_I64_SHIFT_RIGHT, INT64_MIN, 63, RUDA_BYTECODE_RUN_OK, -1},
        {RUDA_OPCODE_I64_SHIFT_RIGHT, INT64_MAX, 62, RUDA_BYTECODE_RUN_OK, 1},
        {RUDA_OPCODE_I64_SHIFT_RIGHT, 1, 63, RUDA_BYTECODE_RUN_OK, 0},
        {RUDA_OPCODE_I64_SHIFT_RIGHT, 1, -1, RUDA_BYTECODE_RUN_INVALID_SHIFT, 0},
        {RUDA_OPCODE_I64_SHIFT_RIGHT, 1, 64, RUDA_BYTECODE_RUN_INVALID_SHIFT, 0},
        {RUDA_OPCODE_I64_SHIFT_RIGHT, 1, INT64_MIN, RUDA_BYTECODE_RUN_INVALID_SHIFT, 0},
        {RUDA_OPCODE_I64_SHIFT_RIGHT, 1, INT64_MAX, RUDA_BYTECODE_RUN_INVALID_SHIFT, 0}
    };
    RudaBytecodeFunction function;
    RudaBytecodeValue arguments[2];
    RudaBytecodeValue result;
    RudaBytecodeTrap trap;
    size_t index = 0;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaBytecodeRunOptions options = ruda_bytecode_run_default_options();
        memset(&trap, 0, sizeof(trap));
        result = ruda_test_vm_i64(9876);
        options.trap = &trap;
        RUDA_EXPECT(ruda_test_vm_build_binary(
            &function,
            ruda_system_allocator(),
            cases[index].opcode
        ));
        arguments[0] = ruda_test_vm_i64(cases[index].left);
        arguments[1] = ruda_test_vm_i64(cases[index].right);
        RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 2,
            ruda_system_allocator(), options, &result) == cases[index].status);
        if (cases[index].status == RUDA_BYTECODE_RUN_OK) {
            RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == cases[index].value);
            RUDA_EXPECT(!trap.has_span);
        } else {
            RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 9876);
            RUDA_EXPECT(trap.has_span);
            RUDA_EXPECT(trap.span.source == 0);
            RUDA_EXPECT(trap.span.start == 10 && trap.span.end == 11);
        }
        ruda_bytecode_function_dispose(&function);
    }
}

void ruda_test_vm_direct_calls(void) {
    RudaBytecodeProgram program;
    RudaBytecodeProgram decoded;
    RudaOwnedBytes encoded;
    RudaBytecodeValue result;
    RudaBytecodeTrap trap;
    RudaBytecodeRunOptions options;
    size_t shared_steps = 0;
    size_t failure_index = 0;
    bool reached_success = false;

    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_test_vm_build_nested_program(&program,
        ruda_system_allocator()));
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    memset(&trap, 0, sizeof(trap));
    options = ruda_bytecode_run_default_options();
    options.trap = &trap;
    options.max_steps = 2;
    options.max_call_depth = 3;
    result = ruda_test_vm_i64(1234);
    RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
        ruda_system_allocator(), options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 42);
    RUDA_EXPECT(!trap.has_span);
    options.max_steps = 1;
    result = ruda_test_vm_i64(1234);
    RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
        ruda_system_allocator(), options, &result) == RUDA_BYTECODE_RUN_STEP_LIMIT);
    RUDA_EXPECT(result.as.i64 == 1234 && trap.has_span);
    RUDA_EXPECT(trap.span.start == 51 && trap.span.end == 52);
    options.max_steps = 2;
    options.max_call_depth = 2;
    result = ruda_test_vm_i64(1234);
    RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
        ruda_system_allocator(), options, &result) == RUDA_BYTECODE_RUN_STEP_LIMIT);
    RUDA_EXPECT(result.as.i64 == 1234 && trap.has_span);
    RUDA_EXPECT(trap.span.start == 40 && trap.span.end == 41);
    options.max_call_depth = 3;
    shared_steps = 6;
    options.remaining_shared_steps = &shared_steps;
    RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
        ruda_system_allocator(), options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(shared_steps == 0);
    shared_steps = 5;
    result = ruda_test_vm_i64(1234);
    RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
        ruda_system_allocator(), options, &result) == RUDA_BYTECODE_RUN_STEP_LIMIT);
    RUDA_EXPECT(shared_steps == 0 && result.as.i64 == 1234 && trap.has_span);
    RUDA_EXPECT(trap.span.start == 31 && trap.span.end == 32);
    options.remaining_shared_steps = NULL;

    /* Per-frame limit is local: the leaf's second instruction is the boundary. */
    options.max_steps = 1;
    options.max_call_depth = 3;
    result = ruda_test_vm_i64(1234);
    memset(&trap, 0, sizeof(trap));
    RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
        ruda_system_allocator(), options, &result) == RUDA_BYTECODE_RUN_STEP_LIMIT);
    RUDA_EXPECT(result.as.i64 == 1234 && trap.has_span);
    RUDA_EXPECT(trap.span.start == 51 && trap.span.end == 52);
    options.max_steps = 2;
    RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
        ruda_system_allocator(), options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 42);

    RUDA_EXPECT(ruda_bytecode_encode_program(&program, ruda_system_allocator(),
        &encoded) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_program(encoded.data, encoded.length,
        ruda_system_allocator(), ruda_bytecode_program_default_options(),
        &decoded) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_run_program(&decoded, NULL, 0,
        ruda_system_allocator(), ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 42);
    ruda_bytecode_program_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);

    for (failure_index = 0; failure_index < 64 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeRunStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        result = ruda_test_vm_i64(1234);
        status = ruda_bytecode_run_program(&program, NULL, 0,
            ruda_fail_allocator(&state), ruda_bytecode_run_default_options(),
            &result);
        if (status == RUDA_BYTECODE_RUN_OK) {
            reached_success = true;
            RUDA_EXPECT(result.as.i64 == 42);
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED);
            RUDA_EXPECT(result.as.i64 == 1234);
        }
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_bytecode_program_dispose(&program);

    {
        RudaBytecodeValue argument = ruda_test_vm_i64(3);
        RUDA_EXPECT(ruda_test_vm_build_recursive_program(
            &program,
            ruda_system_allocator()
        ));
        options = ruda_bytecode_run_default_options();
        options.max_steps = 6;
        options.max_call_depth = 3;
        options.trap = &trap;
        result = ruda_test_vm_i64(1234);
        RUDA_EXPECT(ruda_bytecode_run_program(
            &program,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1);
        options.max_call_depth = 2;
        result = ruda_test_vm_i64(1234);
        RUDA_EXPECT(ruda_bytecode_run_program(
            &program,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_BYTECODE_RUN_STEP_LIMIT);
        RUDA_EXPECT(result.as.i64 == 1234 && trap.has_span);
        RUDA_EXPECT(trap.span.start == 125 && trap.span.end == 126);
        options.max_call_depth = 3;
        shared_steps = 16;
        options.remaining_shared_steps = &shared_steps;
        RUDA_EXPECT(ruda_bytecode_run_program(
            &program,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(shared_steps == 0 && result.as.i64 == 1);
        shared_steps = 15;
        result = ruda_test_vm_i64(1234);
        RUDA_EXPECT(ruda_bytecode_run_program(
            &program,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_BYTECODE_RUN_STEP_LIMIT);
        RUDA_EXPECT(shared_steps == 0 && result.as.i64 == 1234 && trap.has_span);
        RUDA_EXPECT(trap.span.start == 126 && trap.span.end == 127);
        ruda_bytecode_program_dispose(&program);
    }

    {
        const RudaSymbolId callee_symbol = {{0x10}};
        const RudaSymbolId entry_symbol = {{0x20}};
        static const RudaBytecodeType parameters[] = {
            RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64
        };
        RudaBytecodeFunction callee;
        RudaBytecodeFunction entry;
        RudaBytecodeCallArgument call_arguments[2];
        RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
        RudaBytecodeInstruction *operation = NULL;
        uint32_t value = UINT32_MAX;
        uint32_t block = UINT32_MAX;
        uint32_t function_id = UINT32_MAX;
        uint64_t span = 70;
        RudaBytecodeValue arguments[2];
        memset(&callee, 0, sizeof(callee));
        memset(&entry, 0, sizeof(entry));
        RUDA_EXPECT(ruda_bytecode_function_init(&callee, ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_I64, 1, parameters, 2,
            ruda_bytecode_function_default_options()));
        RUDA_EXPECT(ruda_bytecode_add_register(&callee, RUDA_BYTECODE_TYPE_I64,
            &value));
        RUDA_EXPECT(ruda_bytecode_add_block(&callee, &block));
        RUDA_EXPECT(ruda_test_vm_append(&callee, block,
            RUDA_OPCODE_I64_SUBTRACT_CHECKED, value, 0, 1, &span));
        RUDA_EXPECT(ruda_test_vm_append(&callee, block, RUDA_OPCODE_RETURN,
            value, 0, 0, &span));
        call_arguments[0].register_id = 1;
        call_arguments[0].type = RUDA_BYTECODE_TYPE_I64;
        call_arguments[1].register_id = 0;
        call_arguments[1].type = RUDA_BYTECODE_TYPE_I64;
        span = 60;
        RUDA_EXPECT(ruda_bytecode_function_init(&entry, ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_I64, 1, parameters, 2,
            ruda_bytecode_function_default_options()));
        RUDA_EXPECT(ruda_bytecode_add_register(&entry, RUDA_BYTECODE_TYPE_I64,
            &value));
        RUDA_EXPECT(ruda_bytecode_add_block(&entry, &block));
        RUDA_EXPECT(ruda_bytecode_add_call(&entry, callee_symbol,
            RUDA_BYTECODE_TYPE_I64, call_arguments, 2, &call));
        RUDA_EXPECT(ruda_test_vm_append(&entry, block, RUDA_OPCODE_CALL_DIRECT,
            value, call, 0, &span));
        RUDA_EXPECT(ruda_test_vm_append(&entry, block, RUDA_OPCODE_RETURN,
            value, 0, 0, &span));
        RUDA_EXPECT(ruda_bytecode_program_init(&program, ruda_system_allocator(),
            1, ruda_bytecode_program_default_options()));
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, callee_symbol,
            &callee, &function_id) && function_id == 0);
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, entry_symbol,
            &entry, &function_id) && function_id == 1);
        RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, entry_symbol));
        arguments[0] = ruda_test_vm_i64(2);
        arguments[1] = ruda_test_vm_i64(40);
        result = ruda_test_vm_i64(1234);
        RUDA_EXPECT(ruda_bytecode_run_program(&program, arguments, 2,
            ruda_system_allocator(), ruda_bytecode_run_default_options(), &result)
            == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 38);
        operation = ruda_test_vm_at(&((RudaBytecodeProgramFunction *)
            ruda_bytecode_program_function(&program, 0))->body, 0, 0);
        RUDA_EXPECT(operation != NULL);
        if (operation != NULL) {
            operation->opcode = RUDA_OPCODE_I64_DIVIDE_CHECKED;
            arguments[0] = ruda_test_vm_i64(0);
            arguments[1] = ruda_test_vm_i64(1);
            result = ruda_test_vm_i64(1234);
            memset(&trap, 0, sizeof(trap));
            options = ruda_bytecode_run_default_options();
            options.trap = &trap;
            RUDA_EXPECT(ruda_bytecode_run_program(&program, arguments, 2,
                ruda_system_allocator(), options, &result)
                == RUDA_BYTECODE_RUN_DIVISION_BY_ZERO);
            RUDA_EXPECT(result.as.i64 == 1234 && trap.has_span
                && trap.span.start == 70 && trap.span.end == 71);
            operation->opcode = RUDA_OPCODE_I64_ADD_CHECKED;
            arguments[0] = ruda_test_vm_i64(1);
            arguments[1] = ruda_test_vm_i64(INT64_MAX);
            result = ruda_test_vm_i64(1234);
            RUDA_EXPECT(ruda_bytecode_run_program(&program, arguments, 2,
                ruda_system_allocator(), options, &result)
                == RUDA_BYTECODE_RUN_OVERFLOW);
            RUDA_EXPECT(result.as.i64 == 1234 && trap.has_span
                && trap.span.start == 70 && trap.span.end == 71);
            operation->opcode = RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED;
            arguments[0] = ruda_test_vm_i64(-1);
            arguments[1] = ruda_test_vm_i64(1);
            result = ruda_test_vm_i64(1234);
            RUDA_EXPECT(ruda_bytecode_run_program(&program, arguments, 2,
                ruda_system_allocator(), options, &result)
                == RUDA_BYTECODE_RUN_INVALID_SHIFT);
            RUDA_EXPECT(result.as.i64 == 1234 && trap.has_span
                && trap.span.start == 70 && trap.span.end == 71);
        }
        ruda_bytecode_program_dispose(&program);
    }

    {
        const RudaSymbolId bool_callee = {{0x10}};
        const RudaSymbolId bool_entry = {{0x20}};
        static const RudaBytecodeType bool_parameter[] = {RUDA_BYTECODE_TYPE_BOOL};
        RudaBytecodeFunction callee;
        RudaBytecodeFunction entry;
        RudaBytecodeCallArgument argument;
        RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
        RudaBytecodeValue argument_value;
        uint32_t value = UINT32_MAX;
        uint32_t block = UINT32_MAX;
        uint32_t id = UINT32_MAX;
        uint64_t span = 80;
        memset(&callee, 0, sizeof(callee));
        memset(&entry, 0, sizeof(entry));
        RUDA_EXPECT(ruda_bytecode_function_init(&callee, ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_BOOL, 1, bool_parameter, 1,
            ruda_bytecode_function_default_options()));
        RUDA_EXPECT(ruda_bytecode_add_register(&callee, RUDA_BYTECODE_TYPE_BOOL,
            &value) && ruda_bytecode_add_block(&callee, &block));
        RUDA_EXPECT(ruda_test_vm_append(&callee, block, RUDA_OPCODE_BOOL_NOT,
            value, 0, 0, &span));
        RUDA_EXPECT(ruda_test_vm_append(&callee, block, RUDA_OPCODE_RETURN,
            value, 0, 0, &span));
        argument.register_id = 0;
        argument.type = RUDA_BYTECODE_TYPE_BOOL;
        span = 90;
        RUDA_EXPECT(ruda_bytecode_function_init(&entry, ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_BOOL, 1, bool_parameter, 1,
            ruda_bytecode_function_default_options()));
        RUDA_EXPECT(ruda_bytecode_add_register(&entry, RUDA_BYTECODE_TYPE_BOOL,
            &value) && ruda_bytecode_add_block(&entry, &block));
        RUDA_EXPECT(ruda_bytecode_add_call(&entry, bool_callee,
            RUDA_BYTECODE_TYPE_BOOL, &argument, 1, &call));
        RUDA_EXPECT(ruda_test_vm_append(&entry, block, RUDA_OPCODE_CALL_DIRECT,
            value, call, 0, &span));
        RUDA_EXPECT(ruda_test_vm_append(&entry, block, RUDA_OPCODE_RETURN,
            value, 0, 0, &span));
        RUDA_EXPECT(ruda_bytecode_program_init(&program, ruda_system_allocator(),
            1, ruda_bytecode_program_default_options()));
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, bool_callee,
            &callee, &id) && ruda_bytecode_program_take_function(&program,
            bool_entry, &entry, &id));
        RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, bool_entry));
        argument_value = ruda_test_vm_bool(true);
        RUDA_EXPECT(ruda_bytecode_run_program(&program, &argument_value, 1,
            ruda_system_allocator(), ruda_bytecode_run_default_options(), &result)
            == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_BOOL && !result.as.boolean);
        ruda_bytecode_program_dispose(&program);
    }

    {
        const RudaSymbolId void_callee = {{0x10}};
        const RudaSymbolId void_entry = {{0x20}};
        RudaBytecodeFunction callee;
        RudaBytecodeFunction entry;
        RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
        uint32_t block = UINT32_MAX;
        uint32_t id = UINT32_MAX;
        uint64_t span = 100;
        memset(&callee, 0, sizeof(callee));
        memset(&entry, 0, sizeof(entry));
        RUDA_EXPECT(ruda_bytecode_function_init(&callee, ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_VOID, 1, NULL, 0,
            ruda_bytecode_function_default_options())
            && ruda_bytecode_add_block(&callee, &block));
        RUDA_EXPECT(ruda_test_vm_append(&callee, block, RUDA_OPCODE_RETURN_VOID,
            0, 0, 0, &span));
        span = 110;
        RUDA_EXPECT(ruda_bytecode_function_init(&entry, ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_VOID, 1, NULL, 0,
            ruda_bytecode_function_default_options())
            && ruda_bytecode_add_block(&entry, &block));
        RUDA_EXPECT(ruda_bytecode_add_call(&entry, void_callee,
            RUDA_BYTECODE_TYPE_VOID, NULL, 0, &call));
        RUDA_EXPECT(ruda_test_vm_append(&entry, block, RUDA_OPCODE_CALL_DIRECT_VOID,
            call, 0, 0, &span));
        RUDA_EXPECT(ruda_test_vm_append(&entry, block, RUDA_OPCODE_RETURN_VOID,
            0, 0, 0, &span));
        RUDA_EXPECT(ruda_bytecode_program_init(&program, ruda_system_allocator(),
            1, ruda_bytecode_program_default_options()));
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, void_callee,
            &callee, &id) && ruda_bytecode_program_take_function(&program,
            void_entry, &entry, &id));
        RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, void_entry));
        RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
            ruda_system_allocator(), ruda_bytecode_run_default_options(), &result)
            == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_VOID);
        ruda_bytecode_program_dispose(&program);
    }
}

void ruda_test_vm_rejection_round_trip_and_allocation(void) {
    RudaBytecodeFunction function;
    RudaBytecodeFunction decoded;
    RudaBytecodeValue arguments[3];
    RudaBytecodeValue result;
    RudaOwnedBytes encoded;
    RudaBytecodeInstruction *instruction = NULL;
    size_t failure_index = 0;
    bool reached_success = false;

    ruda_test_vm_rejects_before_frame_allocation(
        0,
        true,
        RUDA_OPCODE_RETURN,
        SIZE_MAX,
        0
    );
    ruda_test_vm_rejects_before_frame_allocation(
        20,
        false,
        RUDA_OPCODE_BRANCH_BOOL,
        1,
        99
    );
    ruda_test_vm_rejects_before_frame_allocation(
        2,
        false,
        RUDA_OPCODE_COPY,
        1,
        11
    );
    ruda_test_vm_rejects_before_frame_allocation(
        14,
        false,
        RUDA_OPCODE_I64_BITWISE_NOT,
        1,
        22
    );

    RUDA_EXPECT(ruda_test_vm_build_all_opcodes(&function, ruda_system_allocator()));
    arguments[0] = ruda_test_vm_bool(true);
    arguments[1] = ruda_test_vm_i64(40);
    arguments[2] = ruda_test_vm_i64(2);
    result = ruda_test_vm_i64(1234);
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 2, ruda_system_allocator(),
        ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_INVALID_ARGUMENT);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
    arguments[1].type = RUDA_BYTECODE_TYPE_BOOL;
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_INVALID_ARGUMENT);
    arguments[1] = ruda_test_vm_i64(40);

    instruction = ruda_vector_at(
        &((RudaBytecodeBlock *)function.blocks.data)[0].instructions,
        0
    );
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[0].unsigned_value = 99;
    }
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_INVALID_BYTECODE);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_vm_build_all_opcodes(&function, ruda_system_allocator()));
    function.options.max_verify_work = 1;
    RUDA_EXPECT(ruda_bytecode_run(&function, arguments, 3, ruda_system_allocator(),
        ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_LIMIT_REACHED);
    ruda_bytecode_function_dispose(&function);

    memset(&encoded, 0, sizeof(encoded));
    memset(&decoded, 0, sizeof(decoded));
    RUDA_EXPECT(ruda_test_vm_build_all_opcodes(&function, ruda_system_allocator()));
    RUDA_EXPECT(ruda_bytecode_encode_function(&function, ruda_system_allocator(),
        &encoded) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_function(encoded.data, encoded.length,
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &decoded) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_run(&decoded, arguments, 3, ruda_system_allocator(),
        ruda_bytecode_run_default_options(), &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 42);
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);

    for (failure_index = 0; failure_index < 128 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeRunStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        status = ruda_bytecode_run(&function, arguments, 3,
            ruda_fail_allocator(&state), ruda_bytecode_run_default_options(),
            &result);
        if (status == RUDA_BYTECODE_RUN_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_bytecode_function_dispose(&function);

    {
        RudaFailAllocator state;
        size_t verifier_failure_index = 0;
        size_t function_live_allocations = 0;
        size_t function_live_bytes = 0;
        bool verifier_reached_success = false;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_vm_build_all_opcodes(
            &function,
            ruda_fail_allocator(&state)
        ));
        function_live_allocations = state.live_allocations;
        function_live_bytes = state.live_bytes;
        for (verifier_failure_index = 0;
            verifier_failure_index < 32 && !verifier_reached_success;
            verifier_failure_index += 1) {
            RudaBytecodeRunStatus status;
            state.failure_index = state.attempts + verifier_failure_index;
            result = ruda_test_vm_i64(1234);
            status = ruda_bytecode_run(
                &function,
                arguments,
                3,
                ruda_system_allocator(),
                ruda_bytecode_run_default_options(),
                &result
            );
            if (status == RUDA_BYTECODE_RUN_OK) {
                verifier_reached_success = true;
            } else {
                RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED);
                RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64);
                RUDA_EXPECT(result.as.i64 == 1234);
            }
            RUDA_EXPECT(state.live_allocations == function_live_allocations);
            RUDA_EXPECT(state.live_bytes == function_live_bytes);
        }
        RUDA_EXPECT(verifier_reached_success);
        ruda_bytecode_function_dispose(&function);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
}
