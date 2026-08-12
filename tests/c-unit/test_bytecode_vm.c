#include "test_support.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/bytecode/bytecode.h"
#include "ruda/bytecode/vm.h"

static RudaBytecodeInstruction ruda_test_vm_instruction(
    RudaOpcode opcode,
    uint64_t first,
    uint64_t second,
    uint64_t third,
    uint64_t span_start
) {
    RudaBytecodeInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = opcode;
    instruction.operands[0].unsigned_value = first;
    instruction.operands[1].unsigned_value = second;
    instruction.operands[2].unsigned_value = third;
    instruction.span.start = span_start;
    instruction.span.end = span_start + 1;
    return instruction;
}

static bool ruda_test_vm_append(
    RudaBytecodeFunction *function,
    uint32_t block,
    RudaOpcode opcode,
    uint64_t first,
    uint64_t second,
    uint64_t third,
    uint64_t *span_start
) {
    RudaBytecodeInstruction instruction = ruda_test_vm_instruction(
        opcode,
        first,
        second,
        third,
        *span_start
    );
    if (opcode == RUDA_OPCODE_CONST_I64) {
        instruction.operands[1].signed_value = (int64_t)second;
    }
    *span_start += 1;
    return ruda_bytecode_append_instruction(function, block, &instruction);
}

static bool ruda_test_vm_add_registers(
    RudaBytecodeFunction *function,
    const RudaBytecodeType *types,
    size_t count
) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        uint32_t register_id = UINT32_MAX;
        if (!ruda_bytecode_add_register(function, types[index], &register_id)) {
            return false;
        }
    }
    return true;
}

/* Both branch outcomes exercise a distinct terminator path; every opcode runs. */
static bool ruda_test_vm_build_all_opcodes(
    RudaBytecodeFunction *function,
    RudaAllocator allocator
) {
    static const RudaBytecodeType parameters[] = {
        RUDA_BYTECODE_TYPE_BOOL, RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64
    };
    static const RudaBytecodeType types[] = {
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_BOOL, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_BOOL, RUDA_BYTECODE_TYPE_BOOL, RUDA_BYTECODE_TYPE_BOOL,
        RUDA_BYTECODE_TYPE_BOOL,
        RUDA_BYTECODE_TYPE_BOOL, RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_I64
    };
    uint32_t blocks[4];
    uint64_t span_start = 0;
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();
    bool success = false;

    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_I64,
            1, parameters, sizeof(parameters) / sizeof(parameters[0]), options)
        || !ruda_test_vm_add_registers(function, types, sizeof(types) / sizeof(types[0]))
        || !ruda_bytecode_add_block(function, &blocks[0])
        || !ruda_bytecode_add_block(function, &blocks[1])
        || !ruda_bytecode_add_block(function, &blocks[2])
        || !ruda_bytecode_add_block(function, &blocks[3])) {
        goto cleanup;
    }
    if (!ruda_test_vm_append(function, 0, RUDA_OPCODE_CONST_I64, 3, 1, 0,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_CONST_BOOL, 4, 0, 0,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_COPY, 5, 1, 0,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_ADD_CHECKED, 6, 5, 2,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_SUBTRACT_CHECKED, 7, 6, 2,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_MULTIPLY_CHECKED, 8, 7, 3,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_DIVIDE_CHECKED, 9, 8, 3,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_REMAINDER_CHECKED, 10, 9, 3,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_NEGATE_CHECKED, 11, 10, 0,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_EQUAL, 12, 1, 2,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_BOOL_NOT, 13, 12, 0,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_BOOL_EQUAL, 14, 13, 4,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_LESS, 15, 2, 1,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_LESS_EQUAL, 16, 2, 1,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_BITWISE_NOT, 17, 1, 0,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_BITWISE_AND, 18, 1, 2,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_BITWISE_XOR, 19, 1, 2,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_BITWISE_OR, 20, 1, 2,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED,
            21, 3, 3, &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_I64_SHIFT_RIGHT, 22, 1, 3,
            &span_start)
        || !ruda_test_vm_append(function, 0, RUDA_OPCODE_BRANCH_BOOL, 0, 1, 2,
            &span_start)
        || !ruda_test_vm_append(function, 1, RUDA_OPCODE_RETURN, 6, 0, 0,
            &span_start)
        || !ruda_test_vm_append(function, 2, RUDA_OPCODE_JUMP, 3, 0, 0,
            &span_start)
        || !ruda_test_vm_append(function, 3, RUDA_OPCODE_RETURN, 6, 0, 0,
            &span_start)) {
        goto cleanup;
    }
    success = true;

cleanup:
    if (!success) {
        ruda_bytecode_function_dispose(function);
    }
    return success;
}

static bool ruda_test_vm_build_binary(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaOpcode opcode
) {
    static const RudaBytecodeType parameters[] = {
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64
    };
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 10;
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();

    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_I64,
            1, parameters, sizeof(parameters) / sizeof(parameters[0]), options)
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_I64, &result)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(function, block, opcode, result, 0, 1, &span_start)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_RETURN, result, 0, 0,
            &span_start)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_i64_comparison(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaOpcode opcode
) {
    static const RudaBytecodeType parameters[] = {
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64
    };
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 30;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(
            function,
            allocator,
            RUDA_BYTECODE_TYPE_BOOL,
            1,
            parameters,
            2,
            options
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_BOOL,
            &result
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function,
            block,
            opcode,
            result,
            0,
            1,
            &span_start
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RETURN,
            result,
            0,
            0,
            &span_start
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_bool_operation(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaOpcode opcode
) {
    static const RudaBytecodeType parameters[] = {
        RUDA_BYTECODE_TYPE_BOOL, RUDA_BYTECODE_TYPE_BOOL
    };
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 40;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(
            function,
            allocator,
            RUDA_BYTECODE_TYPE_BOOL,
            1,
            parameters,
            2,
            options
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_BOOL,
            &result
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function,
            block,
            opcode,
            result,
            0,
            1,
            &span_start
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RETURN,
            result,
            0,
            0,
            &span_start
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_constants_and_copy(
    RudaBytecodeFunction *function,
    RudaAllocator allocator
) {
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();
    uint32_t value = UINT32_MAX;
    uint32_t copy = UINT32_MAX;
    uint32_t condition = UINT32_MAX;
    uint32_t blocks[2];
    uint64_t span_start = 50;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(
            function,
            allocator,
            RUDA_BYTECODE_TYPE_I64,
            1,
            NULL,
            0,
            options
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_I64,
            &value
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_I64,
            &copy
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_BOOL,
            &condition
        ) || !ruda_bytecode_add_block(function, &blocks[0])
        || !ruda_bytecode_add_block(function, &blocks[1])
        || !ruda_test_vm_append(
            function,
            blocks[0],
            RUDA_OPCODE_CONST_I64,
            value,
            42,
            0,
            &span_start
        ) || !ruda_test_vm_append(
            function,
            blocks[0],
            RUDA_OPCODE_COPY,
            copy,
            value,
            0,
            &span_start
        ) || !ruda_test_vm_append(
            function,
            blocks[0],
            RUDA_OPCODE_CONST_BOOL,
            condition,
            1,
            0,
            &span_start
        ) || !ruda_test_vm_append(
            function,
            blocks[0],
            RUDA_OPCODE_BRANCH_BOOL,
            condition,
            blocks[1],
            blocks[1],
            &span_start
        ) || !ruda_test_vm_append(
            function,
            blocks[1],
            RUDA_OPCODE_RETURN,
            copy,
            0,
            0,
            &span_start
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_bool_constant_copy(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    bool value
) {
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();
    uint32_t constant = UINT32_MAX;
    uint32_t copy = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 60;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(
            function,
            allocator,
            RUDA_BYTECODE_TYPE_BOOL,
            1,
            NULL,
            0,
            options
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_BOOL,
            &constant
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_BOOL,
            &copy
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_CONST_BOOL,
            constant,
            value ? 1 : 0,
            0,
            &span_start
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_COPY,
            copy,
            constant,
            0,
            &span_start
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RETURN,
            copy,
            0,
            0,
            &span_start
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_loop(
    RudaBytecodeFunction *function,
    RudaAllocator allocator
) {
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 0;
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();

    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_VOID,
            1, NULL, 0, options)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_JUMP, block, 0, 0,
            &span_start)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_void(
    RudaBytecodeFunction *function,
    RudaAllocator allocator
) {
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 0;

    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_VOID,
            1, NULL, 0, options)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_RETURN_VOID, 0, 0, 0,
            &span_start)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_negate(
    RudaBytecodeFunction *function,
    RudaAllocator allocator
) {
    static const RudaBytecodeType parameters[] = {RUDA_BYTECODE_TYPE_I64};
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 20;

    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_I64,
            1, parameters, 1, options)
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_I64, &result)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_I64_NEGATE_CHECKED,
            result, 0, 0, &span_start)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_RETURN, result, 0, 0,
            &span_start)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static RudaBytecodeValue ruda_test_vm_bool(bool value) {
    RudaBytecodeValue result;
    result.type = RUDA_BYTECODE_TYPE_BOOL;
    result.as.boolean = value;
    return result;
}

static RudaBytecodeValue ruda_test_vm_i64(int64_t value) {
    RudaBytecodeValue result;
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = value;
    return result;
}

static void ruda_test_vm_all_opcodes_and_limits(void) {
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

static void ruda_test_vm_checked_arithmetic(void) {
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

static void ruda_test_vm_scalar_results(void) {
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

static void ruda_test_vm_bitwise_and_shifts(void) {
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

static void ruda_test_vm_rejection_round_trip_and_allocation(void) {
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

void ruda_test_bytecode_vm(void) {
    ruda_test_vm_all_opcodes_and_limits();
    ruda_test_vm_checked_arithmetic();
    ruda_test_vm_scalar_results();
    ruda_test_vm_bitwise_and_shifts();
    ruda_test_vm_rejection_round_trip_and_allocation();
}
