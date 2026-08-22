#include "test_bytecode_vm_support.h"

void mmay_test_vm_all_opcodes_and_limits(void) {
    MarimayBytecodeFunction function;
    MarimayBytecodeValue arguments[3];
    MarimayBytecodeValue result;
    MarimayBytecodeTrap trap;
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    size_t shared_steps = 0;

    MMAY_EXPECT(mmay_test_vm_build_all_opcodes(&function, mmay_system_allocator()));
    arguments[0] = mmay_test_vm_bool(true);
    arguments[1] = mmay_test_vm_i64(40);
    arguments[2] = mmay_test_vm_i64(2);
    memset(&trap, 0, sizeof(trap));
    options.trap = &trap;
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 42);
    MMAY_EXPECT(!trap.has_span);
    arguments[0] = mmay_test_vm_bool(false);
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 42);

    options = mmay_bytecode_run_default_options();
    options.trap = &trap;
    options.max_steps = 22;
    arguments[0] = mmay_test_vm_bool(true);
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_OK);
    options.max_steps = 21;
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_STEP_LIMIT);
    MMAY_EXPECT(trap.has_span);
    MMAY_EXPECT(trap.span.start == 21 && trap.span.end == 22);
    shared_steps = 22;
    options = mmay_bytecode_run_default_options();
    options.trap = &trap;
    options.remaining_shared_steps = &shared_steps;
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(shared_steps == 0);
    shared_steps = 21;
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_STEP_LIMIT);
    MMAY_EXPECT(shared_steps == 0);
    MMAY_EXPECT(trap.has_span);
    MMAY_EXPECT(trap.span.start == 21 && trap.span.end == 22);

    arguments[0] = mmay_test_vm_bool(false);
    options = mmay_bytecode_run_default_options();
    options.trap = &trap;
    options.max_steps = 23;
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_OK);
    options.max_steps = 22;
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_STEP_LIMIT);
    MMAY_EXPECT(trap.has_span);
    MMAY_EXPECT(trap.span.start == 23 && trap.span.end == 24);
    shared_steps = 23;
    options = mmay_bytecode_run_default_options();
    options.trap = &trap;
    options.remaining_shared_steps = &shared_steps;
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(shared_steps == 0);
    shared_steps = 22;
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_STEP_LIMIT);
    MMAY_EXPECT(shared_steps == 0);
    MMAY_EXPECT(trap.has_span);
    MMAY_EXPECT(trap.span.start == 23 && trap.span.end == 24);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_vm_build_loop(&function, mmay_system_allocator()));
    options = mmay_bytecode_run_default_options();
    options.trap = &trap;
    options.max_steps = 3;
    MMAY_EXPECT(mmay_bytecode_run(&function, NULL, 0, mmay_system_allocator(),
        options, &result) == MMAY_BYTECODE_RUN_STEP_LIMIT);
    MMAY_EXPECT(trap.has_span);
    MMAY_EXPECT(trap.span.start == 0 && trap.span.end == 1);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_vm_build_void(&function, mmay_system_allocator()));
    MMAY_EXPECT(mmay_bytecode_run(&function, NULL, 0, mmay_system_allocator(),
        mmay_bytecode_run_default_options(), &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_VOID);
    mmay_bytecode_function_dispose(&function);
}

void mmay_test_vm_checked_arithmetic(void) {
    static const struct {
        MarimayOpcode opcode;
        int64_t left;
        int64_t right;
        MarimayBytecodeRunStatus status;
        int64_t value;
    } cases[] = {
        {MMAY_OPCODE_I64_ADD_CHECKED, 2, 3, MMAY_BYTECODE_RUN_OK, 5},
        {MMAY_OPCODE_I64_ADD_CHECKED, INT64_MAX, 1, MMAY_BYTECODE_RUN_OVERFLOW, 0},
        {MMAY_OPCODE_I64_ADD_CHECKED, INT64_MIN, -1, MMAY_BYTECODE_RUN_OVERFLOW, 0},
        {MMAY_OPCODE_I64_SUBTRACT_CHECKED, 7, 3, MMAY_BYTECODE_RUN_OK, 4},
        {MMAY_OPCODE_I64_SUBTRACT_CHECKED, INT64_MIN, 1, MMAY_BYTECODE_RUN_OVERFLOW, 0},
        {MMAY_OPCODE_I64_SUBTRACT_CHECKED, INT64_MAX, -1, MMAY_BYTECODE_RUN_OVERFLOW, 0},
        {MMAY_OPCODE_I64_MULTIPLY_CHECKED, -7, 3, MMAY_BYTECODE_RUN_OK, -21},
        {MMAY_OPCODE_I64_MULTIPLY_CHECKED, INT64_MAX, 2, MMAY_BYTECODE_RUN_OVERFLOW, 0},
        {MMAY_OPCODE_I64_MULTIPLY_CHECKED, INT64_MIN, -1, MMAY_BYTECODE_RUN_OVERFLOW, 0},
        {MMAY_OPCODE_I64_DIVIDE_CHECKED, 7, 3, MMAY_BYTECODE_RUN_OK, 2},
        {MMAY_OPCODE_I64_DIVIDE_CHECKED, -7, 3, MMAY_BYTECODE_RUN_OK, -2},
        {MMAY_OPCODE_I64_DIVIDE_CHECKED, 7, -3, MMAY_BYTECODE_RUN_OK, -2},
        {MMAY_OPCODE_I64_DIVIDE_CHECKED, -7, -3, MMAY_BYTECODE_RUN_OK, 2},
        {MMAY_OPCODE_I64_DIVIDE_CHECKED, 1, 0, MMAY_BYTECODE_RUN_DIVISION_BY_ZERO, 0},
        {MMAY_OPCODE_I64_DIVIDE_CHECKED, INT64_MIN, -1, MMAY_BYTECODE_RUN_OVERFLOW, 0},
        {MMAY_OPCODE_I64_REMAINDER_CHECKED, 7, 3, MMAY_BYTECODE_RUN_OK, 1},
        {MMAY_OPCODE_I64_REMAINDER_CHECKED, -7, 3, MMAY_BYTECODE_RUN_OK, -1},
        {MMAY_OPCODE_I64_REMAINDER_CHECKED, 7, -3, MMAY_BYTECODE_RUN_OK, 1},
        {MMAY_OPCODE_I64_REMAINDER_CHECKED, -7, -3, MMAY_BYTECODE_RUN_OK, -1},
        {MMAY_OPCODE_I64_REMAINDER_CHECKED, 1, 0, MMAY_BYTECODE_RUN_DIVISION_BY_ZERO, 0},
        {MMAY_OPCODE_I64_REMAINDER_CHECKED, INT64_MIN, -1, MMAY_BYTECODE_RUN_OVERFLOW, 0}
    };
    MarimayBytecodeFunction function;
    MarimayBytecodeValue arguments[2];
    MarimayBytecodeValue result;
    MarimayBytecodeTrap trap;
    size_t index = 0;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        memset(&trap, 0, sizeof(trap));
        options.trap = &trap;
        MMAY_EXPECT(mmay_test_vm_build_binary(
            &function,
            mmay_system_allocator(),
            cases[index].opcode
        ));
        arguments[0] = mmay_test_vm_i64(cases[index].left);
        arguments[1] = mmay_test_vm_i64(cases[index].right);
        result = mmay_test_vm_i64(123456789);
        MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 2,
            mmay_system_allocator(), options, &result) == cases[index].status);
        if (cases[index].status == MMAY_BYTECODE_RUN_OK) {
            MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == cases[index].value);
        } else {
            MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 123456789);
            MMAY_EXPECT(trap.has_span);
            MMAY_EXPECT(trap.span.start == 10 && trap.span.end == 11);
        }
        mmay_bytecode_function_dispose(&function);
    }
    MMAY_EXPECT(mmay_test_vm_build_negate(&function, mmay_system_allocator()));
    arguments[0] = mmay_test_vm_i64(INT64_MIN);
    result = mmay_test_vm_i64(123456789);
    memset(&trap, 0, sizeof(trap));
    {
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        options.trap = &trap;
        MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 1,
            mmay_system_allocator(), options, &result)
            == MMAY_BYTECODE_RUN_OVERFLOW);
    }
    MMAY_EXPECT(trap.has_span);
    MMAY_EXPECT(trap.span.start == 20 && trap.span.end == 21);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64);
    MMAY_EXPECT(result.as.i64 == 123456789);
    arguments[0] = mmay_test_vm_i64(7);
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 1,
        mmay_system_allocator(), mmay_bytecode_run_default_options(), &result)
        == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == -7);
    mmay_bytecode_function_dispose(&function);
}

void mmay_test_vm_scalar_results(void) {
    static const struct {
        MarimayOpcode opcode;
        int64_t left;
        int64_t right;
        bool expected;
    } comparisons[] = {
        {MMAY_OPCODE_I64_EQUAL, 5, 5, true},
        {MMAY_OPCODE_I64_EQUAL, 5, 4, false},
        {MMAY_OPCODE_I64_LESS, INT64_MIN, INT64_MAX, true},
        {MMAY_OPCODE_I64_LESS, 5, 5, false},
        {MMAY_OPCODE_I64_LESS_EQUAL, 5, 5, true},
        {MMAY_OPCODE_I64_LESS_EQUAL, INT64_MAX, INT64_MIN, false}
    };
    static const struct {
        MarimayOpcode opcode;
        bool left;
        bool right;
        bool expected;
    } booleans[] = {
        {MMAY_OPCODE_BOOL_NOT, false, false, true},
        {MMAY_OPCODE_BOOL_NOT, true, false, false},
        {MMAY_OPCODE_BOOL_EQUAL, false, false, true},
        {MMAY_OPCODE_BOOL_EQUAL, false, true, false},
        {MMAY_OPCODE_BOOL_EQUAL, true, false, false},
        {MMAY_OPCODE_BOOL_EQUAL, true, true, true}
    };
    MarimayBytecodeFunction function;
    MarimayBytecodeValue arguments[2];
    MarimayBytecodeValue result;
    size_t index = 0;
    for (index = 0;
        index < sizeof(comparisons) / sizeof(comparisons[0]);
        index += 1) {
        MMAY_EXPECT(mmay_test_vm_build_i64_comparison(
            &function,
            mmay_system_allocator(),
            comparisons[index].opcode
        ));
        arguments[0] = mmay_test_vm_i64(comparisons[index].left);
        arguments[1] = mmay_test_vm_i64(comparisons[index].right);
        MMAY_EXPECT(mmay_bytecode_run(
            &function,
            arguments,
            2,
            mmay_system_allocator(),
            mmay_bytecode_run_default_options(),
            &result
        ) == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_BOOL);
        MMAY_EXPECT(result.as.boolean == comparisons[index].expected);
        mmay_bytecode_function_dispose(&function);
    }
    for (index = 0; index < sizeof(booleans) / sizeof(booleans[0]);
        index += 1) {
        MMAY_EXPECT(mmay_test_vm_build_bool_operation(
            &function,
            mmay_system_allocator(),
            booleans[index].opcode
        ));
        arguments[0] = mmay_test_vm_bool(booleans[index].left);
        arguments[1] = mmay_test_vm_bool(booleans[index].right);
        MMAY_EXPECT(mmay_bytecode_run(
            &function,
            arguments,
            2,
            mmay_system_allocator(),
            mmay_bytecode_run_default_options(),
            &result
        ) == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_BOOL);
        MMAY_EXPECT(result.as.boolean == booleans[index].expected);
        mmay_bytecode_function_dispose(&function);
    }
    MMAY_EXPECT(mmay_test_vm_build_constants_and_copy(
        &function,
        mmay_system_allocator()
    ));
    MMAY_EXPECT(mmay_bytecode_run(
        &function,
        NULL,
        0,
        mmay_system_allocator(),
        mmay_bytecode_run_default_options(),
        &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64);
    MMAY_EXPECT(result.as.i64 == 42);
    mmay_bytecode_function_dispose(&function);

    for (index = 0; index < 2; index += 1) {
        bool value = index != 0;
        MMAY_EXPECT(mmay_test_vm_build_bool_constant_copy(
            &function,
            mmay_system_allocator(),
            value
        ));
        MMAY_EXPECT(mmay_bytecode_run(
            &function,
            NULL,
            0,
            mmay_system_allocator(),
            mmay_bytecode_run_default_options(),
            &result
        ) == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_BOOL);
        MMAY_EXPECT(result.as.boolean == value);
        mmay_bytecode_function_dispose(&function);
    }
}

static void mmay_test_vm_rejects_before_frame_allocation(
    size_t instruction_index,
    bool replace_opcode,
    MarimayOpcode opcode,
    size_t operand_index,
    uint64_t operand
) {
    MarimayBytecodeFunction function;
    MarimayBytecodeValue arguments[3];
    MarimayBytecodeValue result = mmay_test_vm_i64(987654321);
    MarimayBytecodeTrap trap;
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    MarimayFailAllocator run_state;
    MarimayBytecodeBlock *entry = NULL;
    MarimayBytecodeInstruction *instruction = NULL;

    MMAY_EXPECT(mmay_test_vm_build_all_opcodes(
        &function,
        mmay_system_allocator()
    ));
    entry = mmay_vector_at(&function.blocks, 0);
    MMAY_EXPECT(entry != NULL);
    if (entry != NULL) {
        instruction = mmay_vector_at(&entry->instructions, instruction_index);
    }
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        if (replace_opcode) {
            instruction->opcode = opcode;
        }
        if (operand_index < 3) {
            instruction->operands[operand_index].unsigned_value = operand;
        }
    }
    arguments[0] = mmay_test_vm_bool(true);
    arguments[1] = mmay_test_vm_i64(40);
    arguments[2] = mmay_test_vm_i64(2);
    memset(&trap, 0xff, sizeof(trap));
    options.trap = &trap;
    mmay_fail_allocator_init(&run_state, mmay_system_allocator(), 0);
    MMAY_EXPECT(mmay_bytecode_run(
        &function,
        arguments,
        3,
        mmay_fail_allocator(&run_state),
        options,
        &result
    ) == MMAY_BYTECODE_RUN_INVALID_BYTECODE);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64);
    MMAY_EXPECT(result.as.i64 == 987654321);
    MMAY_EXPECT(!trap.has_span);
    MMAY_EXPECT(trap.span.source == 0);
    MMAY_EXPECT(trap.span.start == 0);
    MMAY_EXPECT(trap.span.end == 0);
    MMAY_EXPECT(run_state.attempts == 0);
    MMAY_EXPECT(run_state.live_allocations == 0);
    MMAY_EXPECT(run_state.live_bytes == 0);
    mmay_bytecode_function_dispose(&function);
}

void mmay_test_vm_bitwise_and_shifts(void) {
    static const struct {
        MarimayOpcode opcode;
        int64_t left;
        int64_t right;
        MarimayBytecodeRunStatus status;
        int64_t value;
    } cases[] = {
        {MMAY_OPCODE_I64_BITWISE_NOT, 0, 0, MMAY_BYTECODE_RUN_OK, -1},
        {MMAY_OPCODE_I64_BITWISE_NOT, -1, 0, MMAY_BYTECODE_RUN_OK, 0},
        {MMAY_OPCODE_I64_BITWISE_AND, -1, INT64_C(0x55), MMAY_BYTECODE_RUN_OK, INT64_C(0x55)},
        {MMAY_OPCODE_I64_BITWISE_XOR, -1, INT64_C(0x55), MMAY_BYTECODE_RUN_OK, -86},
        {MMAY_OPCODE_I64_BITWISE_OR, INT64_MIN, 1, MMAY_BYTECODE_RUN_OK, INT64_MIN + 1},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, 0, MMAY_BYTECODE_RUN_OK, 1},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, 1, MMAY_BYTECODE_RUN_OK, 2},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, 62, MMAY_BYTECODE_RUN_OK,
            INT64_C(4611686018427387904)},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, -2, 1, MMAY_BYTECODE_RUN_OK, -4},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, -1, 63, MMAY_BYTECODE_RUN_OK, INT64_MIN},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, -2, 62, MMAY_BYTECODE_RUN_OK, INT64_MIN},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, -2, 63, MMAY_BYTECODE_RUN_OVERFLOW, 0},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, 0, 63, MMAY_BYTECODE_RUN_OK, 0},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, INT64_MIN, 0, MMAY_BYTECODE_RUN_OK, INT64_MIN},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, INT64_MAX, 0, MMAY_BYTECODE_RUN_OK, INT64_MAX},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, INT64_MAX, 1, MMAY_BYTECODE_RUN_OVERFLOW, 0},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, INT64_MIN, 1, MMAY_BYTECODE_RUN_OVERFLOW, 0},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, -1, MMAY_BYTECODE_RUN_INVALID_SHIFT, 0},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, 64, MMAY_BYTECODE_RUN_INVALID_SHIFT, 0},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, INT64_MIN, MMAY_BYTECODE_RUN_INVALID_SHIFT, 0},
        {MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED, 1, INT64_MAX, MMAY_BYTECODE_RUN_INVALID_SHIFT, 0},
        {MMAY_OPCODE_I64_SHIFT_RIGHT, -7, 0, MMAY_BYTECODE_RUN_OK, -7},
        {MMAY_OPCODE_I64_SHIFT_RIGHT, -1, 1, MMAY_BYTECODE_RUN_OK, -1},
        {MMAY_OPCODE_I64_SHIFT_RIGHT, INT64_MIN, 63, MMAY_BYTECODE_RUN_OK, -1},
        {MMAY_OPCODE_I64_SHIFT_RIGHT, INT64_MAX, 62, MMAY_BYTECODE_RUN_OK, 1},
        {MMAY_OPCODE_I64_SHIFT_RIGHT, 1, 63, MMAY_BYTECODE_RUN_OK, 0},
        {MMAY_OPCODE_I64_SHIFT_RIGHT, 1, -1, MMAY_BYTECODE_RUN_INVALID_SHIFT, 0},
        {MMAY_OPCODE_I64_SHIFT_RIGHT, 1, 64, MMAY_BYTECODE_RUN_INVALID_SHIFT, 0},
        {MMAY_OPCODE_I64_SHIFT_RIGHT, 1, INT64_MIN, MMAY_BYTECODE_RUN_INVALID_SHIFT, 0},
        {MMAY_OPCODE_I64_SHIFT_RIGHT, 1, INT64_MAX, MMAY_BYTECODE_RUN_INVALID_SHIFT, 0}
    };
    MarimayBytecodeFunction function;
    MarimayBytecodeValue arguments[2];
    MarimayBytecodeValue result;
    MarimayBytecodeTrap trap;
    size_t index = 0;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        memset(&trap, 0, sizeof(trap));
        result = mmay_test_vm_i64(9876);
        options.trap = &trap;
        MMAY_EXPECT(mmay_test_vm_build_binary(
            &function,
            mmay_system_allocator(),
            cases[index].opcode
        ));
        arguments[0] = mmay_test_vm_i64(cases[index].left);
        arguments[1] = mmay_test_vm_i64(cases[index].right);
        MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 2,
            mmay_system_allocator(), options, &result) == cases[index].status);
        if (cases[index].status == MMAY_BYTECODE_RUN_OK) {
            MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == cases[index].value);
            MMAY_EXPECT(!trap.has_span);
        } else {
            MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 9876);
            MMAY_EXPECT(trap.has_span);
            MMAY_EXPECT(trap.span.source == 0);
            MMAY_EXPECT(trap.span.start == 10 && trap.span.end == 11);
        }
        mmay_bytecode_function_dispose(&function);
    }
}

void mmay_test_vm_direct_calls(void) {
    MarimayBytecodeProgram program;
    MarimayBytecodeProgram decoded;
    MarimayOwnedBytes encoded;
    MarimayBytecodeValue result;
    MarimayBytecodeTrap trap;
    MarimayBytecodeRunOptions options;
    size_t shared_steps = 0;
    size_t failure_index = 0;
    bool reached_success = false;

    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_vm_build_nested_program(&program,
        mmay_system_allocator()));
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    memset(&trap, 0, sizeof(trap));
    options = mmay_bytecode_run_default_options();
    options.trap = &trap;
    options.max_steps = 2;
    options.max_call_depth = 3;
    result = mmay_test_vm_i64(1234);
    MMAY_EXPECT(mmay_bytecode_run_program(&program, NULL, 0,
        mmay_system_allocator(), options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 42);
    MMAY_EXPECT(!trap.has_span);
    options.max_steps = 1;
    result = mmay_test_vm_i64(1234);
    MMAY_EXPECT(mmay_bytecode_run_program(&program, NULL, 0,
        mmay_system_allocator(), options, &result) == MMAY_BYTECODE_RUN_STEP_LIMIT);
    MMAY_EXPECT(result.as.i64 == 1234 && trap.has_span);
    MMAY_EXPECT(trap.span.start == 51 && trap.span.end == 52);
    options.max_steps = 2;
    options.max_call_depth = 2;
    result = mmay_test_vm_i64(1234);
    MMAY_EXPECT(mmay_bytecode_run_program(&program, NULL, 0,
        mmay_system_allocator(), options, &result) == MMAY_BYTECODE_RUN_STEP_LIMIT);
    MMAY_EXPECT(result.as.i64 == 1234 && trap.has_span);
    MMAY_EXPECT(trap.span.start == 40 && trap.span.end == 41);
    options.max_call_depth = 3;
    shared_steps = 6;
    options.remaining_shared_steps = &shared_steps;
    MMAY_EXPECT(mmay_bytecode_run_program(&program, NULL, 0,
        mmay_system_allocator(), options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(shared_steps == 0);
    shared_steps = 5;
    result = mmay_test_vm_i64(1234);
    MMAY_EXPECT(mmay_bytecode_run_program(&program, NULL, 0,
        mmay_system_allocator(), options, &result) == MMAY_BYTECODE_RUN_STEP_LIMIT);
    MMAY_EXPECT(shared_steps == 0 && result.as.i64 == 1234 && trap.has_span);
    MMAY_EXPECT(trap.span.start == 31 && trap.span.end == 32);
    options.remaining_shared_steps = NULL;

    /* Per-frame limit is local: the leaf's second instruction is the boundary. */
    options.max_steps = 1;
    options.max_call_depth = 3;
    result = mmay_test_vm_i64(1234);
    memset(&trap, 0, sizeof(trap));
    MMAY_EXPECT(mmay_bytecode_run_program(&program, NULL, 0,
        mmay_system_allocator(), options, &result) == MMAY_BYTECODE_RUN_STEP_LIMIT);
    MMAY_EXPECT(result.as.i64 == 1234 && trap.has_span);
    MMAY_EXPECT(trap.span.start == 51 && trap.span.end == 52);
    options.max_steps = 2;
    MMAY_EXPECT(mmay_bytecode_run_program(&program, NULL, 0,
        mmay_system_allocator(), options, &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 42);

    MMAY_EXPECT(mmay_bytecode_encode_program(&program, mmay_system_allocator(),
        &encoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_program(encoded.data, encoded.length,
        mmay_system_allocator(), mmay_bytecode_program_default_options(),
        &decoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_run_program(&decoded, NULL, 0,
        mmay_system_allocator(), mmay_bytecode_run_default_options(), &result)
        == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 42);
    mmay_bytecode_program_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);

    for (failure_index = 0; failure_index < 64 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeRunStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        result = mmay_test_vm_i64(1234);
        status = mmay_bytecode_run_program(&program, NULL, 0,
            mmay_fail_allocator(&state), mmay_bytecode_run_default_options(),
            &result);
        if (status == MMAY_BYTECODE_RUN_OK) {
            reached_success = true;
            MMAY_EXPECT(result.as.i64 == 42);
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
            MMAY_EXPECT(result.as.i64 == 1234);
        }
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
    mmay_bytecode_program_dispose(&program);

    {
        MarimayBytecodeValue argument = mmay_test_vm_i64(3);
        MMAY_EXPECT(mmay_test_vm_build_recursive_program(
            &program,
            mmay_system_allocator()
        ));
        options = mmay_bytecode_run_default_options();
        options.max_steps = 6;
        options.max_call_depth = 3;
        options.trap = &trap;
        result = mmay_test_vm_i64(1234);
        MMAY_EXPECT(mmay_bytecode_run_program(
            &program,
            &argument,
            1,
            mmay_system_allocator(),
            options,
            &result
        ) == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 1);
        options.max_call_depth = 2;
        result = mmay_test_vm_i64(1234);
        MMAY_EXPECT(mmay_bytecode_run_program(
            &program,
            &argument,
            1,
            mmay_system_allocator(),
            options,
            &result
        ) == MMAY_BYTECODE_RUN_STEP_LIMIT);
        MMAY_EXPECT(result.as.i64 == 1234 && trap.has_span);
        MMAY_EXPECT(trap.span.start == 125 && trap.span.end == 126);
        options.max_call_depth = 3;
        shared_steps = 16;
        options.remaining_shared_steps = &shared_steps;
        MMAY_EXPECT(mmay_bytecode_run_program(
            &program,
            &argument,
            1,
            mmay_system_allocator(),
            options,
            &result
        ) == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(shared_steps == 0 && result.as.i64 == 1);
        shared_steps = 15;
        result = mmay_test_vm_i64(1234);
        MMAY_EXPECT(mmay_bytecode_run_program(
            &program,
            &argument,
            1,
            mmay_system_allocator(),
            options,
            &result
        ) == MMAY_BYTECODE_RUN_STEP_LIMIT);
        MMAY_EXPECT(shared_steps == 0 && result.as.i64 == 1234 && trap.has_span);
        MMAY_EXPECT(trap.span.start == 126 && trap.span.end == 127);
        mmay_bytecode_program_dispose(&program);
    }

    {
        const MarimaySymbolId callee_symbol = {{0x10}};
        const MarimaySymbolId entry_symbol = {{0x20}};
        static const MarimayBytecodeType parameters[] = {
            MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64
        };
        MarimayBytecodeFunction callee;
        MarimayBytecodeFunction entry;
        MarimayBytecodeCallArgument call_arguments[2];
        MarimayBytecodeCallId call = MMAY_BYTECODE_NO_CALL;
        MarimayBytecodeInstruction *operation = NULL;
        uint32_t value = UINT32_MAX;
        uint32_t block = UINT32_MAX;
        uint32_t function_id = UINT32_MAX;
        uint64_t span = 70;
        MarimayBytecodeValue arguments[2];
        memset(&callee, 0, sizeof(callee));
        memset(&entry, 0, sizeof(entry));
        MMAY_EXPECT(mmay_bytecode_function_init(&callee, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_I64, 1, parameters, 2,
            mmay_bytecode_function_default_options()));
        MMAY_EXPECT(mmay_bytecode_add_register(&callee, MMAY_BYTECODE_TYPE_I64,
            &value));
        MMAY_EXPECT(mmay_bytecode_add_block(&callee, &block));
        MMAY_EXPECT(mmay_test_vm_append(&callee, block,
            MMAY_OPCODE_I64_SUBTRACT_CHECKED, value, 0, 1, &span));
        MMAY_EXPECT(mmay_test_vm_append(&callee, block, MMAY_OPCODE_RETURN,
            value, 0, 0, &span));
        call_arguments[0].register_id = 1;
        call_arguments[0].type = MMAY_BYTECODE_TYPE_I64;
        call_arguments[1].register_id = 0;
        call_arguments[1].type = MMAY_BYTECODE_TYPE_I64;
        span = 60;
        MMAY_EXPECT(mmay_bytecode_function_init(&entry, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_I64, 1, parameters, 2,
            mmay_bytecode_function_default_options()));
        MMAY_EXPECT(mmay_bytecode_add_register(&entry, MMAY_BYTECODE_TYPE_I64,
            &value));
        MMAY_EXPECT(mmay_bytecode_add_block(&entry, &block));
        MMAY_EXPECT(mmay_bytecode_add_call(&entry, callee_symbol,
            MMAY_BYTECODE_TYPE_I64, call_arguments, 2, &call));
        MMAY_EXPECT(mmay_test_vm_append(&entry, block, MMAY_OPCODE_CALL_DIRECT,
            value, call, 0, &span));
        MMAY_EXPECT(mmay_test_vm_append(&entry, block, MMAY_OPCODE_RETURN,
            value, 0, 0, &span));
        MMAY_EXPECT(mmay_bytecode_program_init(&program, mmay_system_allocator(),
            1, mmay_bytecode_program_default_options()));
        MMAY_EXPECT(mmay_bytecode_program_take_function(&program, callee_symbol,
            &callee, &function_id) && function_id == 0);
        MMAY_EXPECT(mmay_bytecode_program_take_function(&program, entry_symbol,
            &entry, &function_id) && function_id == 1);
        MMAY_EXPECT(mmay_bytecode_program_set_entry(&program, entry_symbol));
        arguments[0] = mmay_test_vm_i64(2);
        arguments[1] = mmay_test_vm_i64(40);
        result = mmay_test_vm_i64(1234);
        MMAY_EXPECT(mmay_bytecode_run_program(&program, arguments, 2,
            mmay_system_allocator(), mmay_bytecode_run_default_options(), &result)
            == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 38);
        operation = mmay_test_vm_at(&((MarimayBytecodeProgramFunction *)
            mmay_bytecode_program_function(&program, 0))->body, 0, 0);
        MMAY_EXPECT(operation != NULL);
        if (operation != NULL) {
            operation->opcode = MMAY_OPCODE_I64_DIVIDE_CHECKED;
            arguments[0] = mmay_test_vm_i64(0);
            arguments[1] = mmay_test_vm_i64(1);
            result = mmay_test_vm_i64(1234);
            memset(&trap, 0, sizeof(trap));
            options = mmay_bytecode_run_default_options();
            options.trap = &trap;
            MMAY_EXPECT(mmay_bytecode_run_program(&program, arguments, 2,
                mmay_system_allocator(), options, &result)
                == MMAY_BYTECODE_RUN_DIVISION_BY_ZERO);
            MMAY_EXPECT(result.as.i64 == 1234 && trap.has_span
                && trap.span.start == 70 && trap.span.end == 71);
            operation->opcode = MMAY_OPCODE_I64_ADD_CHECKED;
            arguments[0] = mmay_test_vm_i64(1);
            arguments[1] = mmay_test_vm_i64(INT64_MAX);
            result = mmay_test_vm_i64(1234);
            MMAY_EXPECT(mmay_bytecode_run_program(&program, arguments, 2,
                mmay_system_allocator(), options, &result)
                == MMAY_BYTECODE_RUN_OVERFLOW);
            MMAY_EXPECT(result.as.i64 == 1234 && trap.has_span
                && trap.span.start == 70 && trap.span.end == 71);
            operation->opcode = MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED;
            arguments[0] = mmay_test_vm_i64(-1);
            arguments[1] = mmay_test_vm_i64(1);
            result = mmay_test_vm_i64(1234);
            MMAY_EXPECT(mmay_bytecode_run_program(&program, arguments, 2,
                mmay_system_allocator(), options, &result)
                == MMAY_BYTECODE_RUN_INVALID_SHIFT);
            MMAY_EXPECT(result.as.i64 == 1234 && trap.has_span
                && trap.span.start == 70 && trap.span.end == 71);
        }
        mmay_bytecode_program_dispose(&program);
    }

    {
        const MarimaySymbolId bool_callee = {{0x10}};
        const MarimaySymbolId bool_entry = {{0x20}};
        static const MarimayBytecodeType bool_parameter[] = {MMAY_BYTECODE_TYPE_BOOL};
        MarimayBytecodeFunction callee;
        MarimayBytecodeFunction entry;
        MarimayBytecodeCallArgument argument;
        MarimayBytecodeCallId call = MMAY_BYTECODE_NO_CALL;
        MarimayBytecodeValue argument_value;
        uint32_t value = UINT32_MAX;
        uint32_t block = UINT32_MAX;
        uint32_t id = UINT32_MAX;
        uint64_t span = 80;
        memset(&callee, 0, sizeof(callee));
        memset(&entry, 0, sizeof(entry));
        MMAY_EXPECT(mmay_bytecode_function_init(&callee, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_BOOL, 1, bool_parameter, 1,
            mmay_bytecode_function_default_options()));
        MMAY_EXPECT(mmay_bytecode_add_register(&callee, MMAY_BYTECODE_TYPE_BOOL,
            &value) && mmay_bytecode_add_block(&callee, &block));
        MMAY_EXPECT(mmay_test_vm_append(&callee, block, MMAY_OPCODE_BOOL_NOT,
            value, 0, 0, &span));
        MMAY_EXPECT(mmay_test_vm_append(&callee, block, MMAY_OPCODE_RETURN,
            value, 0, 0, &span));
        argument.register_id = 0;
        argument.type = MMAY_BYTECODE_TYPE_BOOL;
        span = 90;
        MMAY_EXPECT(mmay_bytecode_function_init(&entry, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_BOOL, 1, bool_parameter, 1,
            mmay_bytecode_function_default_options()));
        MMAY_EXPECT(mmay_bytecode_add_register(&entry, MMAY_BYTECODE_TYPE_BOOL,
            &value) && mmay_bytecode_add_block(&entry, &block));
        MMAY_EXPECT(mmay_bytecode_add_call(&entry, bool_callee,
            MMAY_BYTECODE_TYPE_BOOL, &argument, 1, &call));
        MMAY_EXPECT(mmay_test_vm_append(&entry, block, MMAY_OPCODE_CALL_DIRECT,
            value, call, 0, &span));
        MMAY_EXPECT(mmay_test_vm_append(&entry, block, MMAY_OPCODE_RETURN,
            value, 0, 0, &span));
        MMAY_EXPECT(mmay_bytecode_program_init(&program, mmay_system_allocator(),
            1, mmay_bytecode_program_default_options()));
        MMAY_EXPECT(mmay_bytecode_program_take_function(&program, bool_callee,
            &callee, &id) && mmay_bytecode_program_take_function(&program,
            bool_entry, &entry, &id));
        MMAY_EXPECT(mmay_bytecode_program_set_entry(&program, bool_entry));
        argument_value = mmay_test_vm_bool(true);
        MMAY_EXPECT(mmay_bytecode_run_program(&program, &argument_value, 1,
            mmay_system_allocator(), mmay_bytecode_run_default_options(), &result)
            == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_BOOL && !result.as.boolean);
        mmay_bytecode_program_dispose(&program);
    }

    {
        const MarimaySymbolId void_callee = {{0x10}};
        const MarimaySymbolId void_entry = {{0x20}};
        MarimayBytecodeFunction callee;
        MarimayBytecodeFunction entry;
        MarimayBytecodeCallId call = MMAY_BYTECODE_NO_CALL;
        uint32_t block = UINT32_MAX;
        uint32_t id = UINT32_MAX;
        uint64_t span = 100;
        memset(&callee, 0, sizeof(callee));
        memset(&entry, 0, sizeof(entry));
        MMAY_EXPECT(mmay_bytecode_function_init(&callee, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_VOID, 1, NULL, 0,
            mmay_bytecode_function_default_options())
            && mmay_bytecode_add_block(&callee, &block));
        MMAY_EXPECT(mmay_test_vm_append(&callee, block, MMAY_OPCODE_RETURN_VOID,
            0, 0, 0, &span));
        span = 110;
        MMAY_EXPECT(mmay_bytecode_function_init(&entry, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_VOID, 1, NULL, 0,
            mmay_bytecode_function_default_options())
            && mmay_bytecode_add_block(&entry, &block));
        MMAY_EXPECT(mmay_bytecode_add_call(&entry, void_callee,
            MMAY_BYTECODE_TYPE_VOID, NULL, 0, &call));
        MMAY_EXPECT(mmay_test_vm_append(&entry, block, MMAY_OPCODE_CALL_DIRECT_VOID,
            call, 0, 0, &span));
        MMAY_EXPECT(mmay_test_vm_append(&entry, block, MMAY_OPCODE_RETURN_VOID,
            0, 0, 0, &span));
        MMAY_EXPECT(mmay_bytecode_program_init(&program, mmay_system_allocator(),
            1, mmay_bytecode_program_default_options()));
        MMAY_EXPECT(mmay_bytecode_program_take_function(&program, void_callee,
            &callee, &id) && mmay_bytecode_program_take_function(&program,
            void_entry, &entry, &id));
        MMAY_EXPECT(mmay_bytecode_program_set_entry(&program, void_entry));
        MMAY_EXPECT(mmay_bytecode_run_program(&program, NULL, 0,
            mmay_system_allocator(), mmay_bytecode_run_default_options(), &result)
            == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_VOID);
        mmay_bytecode_program_dispose(&program);
    }
}

void mmay_test_vm_rejection_round_trip_and_allocation(void) {
    MarimayBytecodeFunction function;
    MarimayBytecodeFunction decoded;
    MarimayBytecodeValue arguments[3];
    MarimayBytecodeValue result;
    MarimayOwnedBytes encoded;
    MarimayBytecodeInstruction *instruction = NULL;
    size_t failure_index = 0;
    bool reached_success = false;

    mmay_test_vm_rejects_before_frame_allocation(
        0,
        true,
        MMAY_OPCODE_RETURN,
        SIZE_MAX,
        0
    );
    mmay_test_vm_rejects_before_frame_allocation(
        20,
        false,
        MMAY_OPCODE_BRANCH_BOOL,
        1,
        99
    );
    mmay_test_vm_rejects_before_frame_allocation(
        2,
        false,
        MMAY_OPCODE_COPY,
        1,
        11
    );
    mmay_test_vm_rejects_before_frame_allocation(
        14,
        false,
        MMAY_OPCODE_I64_BITWISE_NOT,
        1,
        22
    );

    MMAY_EXPECT(mmay_test_vm_build_all_opcodes(&function, mmay_system_allocator()));
    arguments[0] = mmay_test_vm_bool(true);
    arguments[1] = mmay_test_vm_i64(40);
    arguments[2] = mmay_test_vm_i64(2);
    result = mmay_test_vm_i64(1234);
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 2, mmay_system_allocator(),
        mmay_bytecode_run_default_options(), &result)
        == MMAY_BYTECODE_RUN_INVALID_ARGUMENT);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
    arguments[1].type = MMAY_BYTECODE_TYPE_BOOL;
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        mmay_bytecode_run_default_options(), &result)
        == MMAY_BYTECODE_RUN_INVALID_ARGUMENT);
    arguments[1] = mmay_test_vm_i64(40);

    instruction = mmay_vector_at(
        &((MarimayBytecodeBlock *)function.blocks.data)[0].instructions,
        0
    );
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[0].unsigned_value = 99;
    }
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        mmay_bytecode_run_default_options(), &result)
        == MMAY_BYTECODE_RUN_INVALID_BYTECODE);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_vm_build_all_opcodes(&function, mmay_system_allocator()));
    function.options.max_verify_work = 1;
    MMAY_EXPECT(mmay_bytecode_run(&function, arguments, 3, mmay_system_allocator(),
        mmay_bytecode_run_default_options(), &result)
        == MMAY_BYTECODE_RUN_LIMIT_REACHED);
    mmay_bytecode_function_dispose(&function);

    memset(&encoded, 0, sizeof(encoded));
    memset(&decoded, 0, sizeof(decoded));
    MMAY_EXPECT(mmay_test_vm_build_all_opcodes(&function, mmay_system_allocator()));
    MMAY_EXPECT(mmay_bytecode_encode_function(&function, mmay_system_allocator(),
        &encoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(encoded.data, encoded.length,
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &decoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_run(&decoded, arguments, 3, mmay_system_allocator(),
        mmay_bytecode_run_default_options(), &result) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 42);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);

    for (failure_index = 0; failure_index < 128 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeRunStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        status = mmay_bytecode_run(&function, arguments, 3,
            mmay_fail_allocator(&state), mmay_bytecode_run_default_options(),
            &result);
        if (status == MMAY_BYTECODE_RUN_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
    mmay_bytecode_function_dispose(&function);

    {
        MarimayFailAllocator state;
        size_t verifier_failure_index = 0;
        size_t function_live_allocations = 0;
        size_t function_live_bytes = 0;
        bool verifier_reached_success = false;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_vm_build_all_opcodes(
            &function,
            mmay_fail_allocator(&state)
        ));
        function_live_allocations = state.live_allocations;
        function_live_bytes = state.live_bytes;
        for (verifier_failure_index = 0;
            verifier_failure_index < 32 && !verifier_reached_success;
            verifier_failure_index += 1) {
            MarimayBytecodeRunStatus status;
            state.failure_index = state.attempts + verifier_failure_index;
            result = mmay_test_vm_i64(1234);
            status = mmay_bytecode_run(
                &function,
                arguments,
                3,
                mmay_system_allocator(),
                mmay_bytecode_run_default_options(),
                &result
            );
            if (status == MMAY_BYTECODE_RUN_OK) {
                verifier_reached_success = true;
            } else {
                MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
                MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64);
                MMAY_EXPECT(result.as.i64 == 1234);
            }
            MMAY_EXPECT(state.live_allocations == function_live_allocations);
            MMAY_EXPECT(state.live_bytes == function_live_bytes);
        }
        MMAY_EXPECT(verifier_reached_success);
        mmay_bytecode_function_dispose(&function);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
}
