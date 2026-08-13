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

static RudaBytecodeInstruction *ruda_test_vm_at(
    RudaBytecodeFunction *function,
    size_t block,
    size_t instruction
) {
    RudaBytecodeBlock *target = function != NULL
        ? ruda_vector_at(&function->blocks, block)
        : NULL;
    return target != NULL
        ? ruda_vector_at(&target->instructions, instruction)
        : NULL;
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

static bool ruda_test_vm_build_direct_constant(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    int64_t value,
    uint64_t span_start
) {
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    RudaBytecodeInstruction constant;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_I64,
            1, NULL, 0, ruda_bytecode_function_default_options())
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_I64, &result)
        || !ruda_bytecode_add_block(function, &block)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    constant = ruda_test_vm_instruction(RUDA_OPCODE_CONST_I64, result, 0, 0,
        span_start);
    constant.operands[1].signed_value = value;
    span_start += 1;
    if (!ruda_bytecode_append_instruction(function, block, &constant)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_RETURN, result, 0,
            0, &span_start)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_direct_forward(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaSymbolId callee,
    uint64_t span_start
) {
    RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_I64,
            1, NULL, 0, ruda_bytecode_function_default_options())
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_I64, &result)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_bytecode_add_call(function, callee, RUDA_BYTECODE_TYPE_I64,
            NULL, 0, &call)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_CALL_DIRECT, result,
            call, 0, &span_start)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_RETURN, result, 0,
            0, &span_start)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_nested_program(
    RudaBytecodeProgram *program,
    RudaAllocator allocator
) {
    const RudaSymbolId leaf = {{0x10}};
    const RudaSymbolId middle = {{0x20}};
    const RudaSymbolId entry = {{0x30}};
    RudaBytecodeFunction function;
    uint32_t id = UINT32_MAX;
    memset(program, 0, sizeof(*program));
    memset(&function, 0, sizeof(function));
    if (!ruda_bytecode_program_init(program, allocator, 1,
            ruda_bytecode_program_default_options())
        || !ruda_test_vm_build_direct_constant(&function, allocator, 42, 50)
        || !ruda_bytecode_program_take_function(program, leaf, &function, &id)
        || id != 0
        || !ruda_test_vm_build_direct_forward(&function, allocator, leaf, 40)
        || !ruda_bytecode_program_take_function(program, middle, &function, &id)
        || id != 1
        || !ruda_test_vm_build_direct_forward(&function, allocator, middle, 30)
        || !ruda_bytecode_program_take_function(program, entry, &function, &id)
        || id != 2 || !ruda_bytecode_program_set_entry(program, entry)) {
        ruda_bytecode_function_dispose(&function);
        ruda_bytecode_program_dispose(program);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_recursive_program(
    RudaBytecodeProgram *program,
    RudaAllocator allocator
) {
    const RudaSymbolId symbol = {{0x10}};
    static const RudaBytecodeType parameters[] = {RUDA_BYTECODE_TYPE_I64};
    RudaBytecodeFunction function;
    RudaBytecodeCallArgument argument;
    RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
    uint32_t one = UINT32_MAX;
    uint32_t base_case = UINT32_MAX;
    uint32_t decremented = UINT32_MAX;
    uint32_t recursive_result = UINT32_MAX;
    uint32_t blocks[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    uint32_t id = UINT32_MAX;
    uint64_t span = 120;
    memset(program, 0, sizeof(*program));
    memset(&function, 0, sizeof(function));
    if (!ruda_bytecode_function_init(
            &function,
            allocator,
            RUDA_BYTECODE_TYPE_I64,
            1,
            parameters,
            1,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            &function,
            RUDA_BYTECODE_TYPE_I64,
            &one
        ) || !ruda_bytecode_add_register(
            &function,
            RUDA_BYTECODE_TYPE_BOOL,
            &base_case
        ) || !ruda_bytecode_add_register(
            &function,
            RUDA_BYTECODE_TYPE_I64,
            &decremented
        ) || !ruda_bytecode_add_register(
            &function,
            RUDA_BYTECODE_TYPE_I64,
            &recursive_result
        ) || !ruda_bytecode_add_block(&function, &blocks[0])
        || !ruda_bytecode_add_block(&function, &blocks[1])
        || !ruda_bytecode_add_block(&function, &blocks[2])) {
        goto cleanup;
    }
    argument.register_id = decremented;
    argument.type = RUDA_BYTECODE_TYPE_I64;
    if (!ruda_bytecode_add_call(
            &function,
            symbol,
            RUDA_BYTECODE_TYPE_I64,
            &argument,
            1,
            &call
        ) || !ruda_test_vm_append(
            &function,
            blocks[0],
            RUDA_OPCODE_CONST_I64,
            one,
            1,
            0,
            &span
        ) || !ruda_test_vm_append(
            &function,
            blocks[0],
            RUDA_OPCODE_I64_LESS_EQUAL,
            base_case,
            0,
            one,
            &span
        ) || !ruda_test_vm_append(
            &function,
            blocks[0],
            RUDA_OPCODE_BRANCH_BOOL,
            base_case,
            blocks[1],
            blocks[2],
            &span
        ) || !ruda_test_vm_append(
            &function,
            blocks[1],
            RUDA_OPCODE_RETURN,
            one,
            0,
            0,
            &span
        ) || !ruda_test_vm_append(
            &function,
            blocks[2],
            RUDA_OPCODE_I64_SUBTRACT_CHECKED,
            decremented,
            0,
            one,
            &span
        ) || !ruda_test_vm_append(
            &function,
            blocks[2],
            RUDA_OPCODE_CALL_DIRECT,
            recursive_result,
            call,
            0,
            &span
        ) || !ruda_test_vm_append(
            &function,
            blocks[2],
            RUDA_OPCODE_RETURN,
            recursive_result,
            0,
            0,
            &span
        ) || !ruda_bytecode_program_init(
            program,
            allocator,
            1,
            ruda_bytecode_program_default_options()
        ) || !ruda_bytecode_program_take_function(
            program,
            symbol,
            &function,
            &id
        ) || id != 0 || !ruda_bytecode_program_set_entry(program, symbol)) {
        goto cleanup;
    }
    return true;

cleanup:
    ruda_bytecode_function_dispose(&function);
    ruda_bytecode_program_dispose(program);
    return false;
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

static bool ruda_test_vm_build_f64(
    RudaBytecodeFunction *function,
    RudaOpcode opcode,
    uint64_t left_bits,
    uint64_t right_bits,
    RudaBytecodeType result_type
) {
    uint32_t left = UINT32_MAX;
    uint32_t right = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 500;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, ruda_system_allocator(), result_type,
            1, NULL, 0, ruda_bytecode_function_default_options())
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_F64, &left)
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_F64, &right)
        || !ruda_bytecode_add_register(function, result_type, &result)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_CONST_F64, left,
            left_bits, 0, &span)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_CONST_F64, right,
            right_bits, 0, &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    if (opcode == RUDA_OPCODE_F64_NEGATE) {
        if (!ruda_test_vm_append(function, block, opcode, result, left, 0, &span)) {
            ruda_bytecode_function_dispose(function);
            return false;
        }
    } else if (!ruda_test_vm_append(function, block, opcode, result, left, right,
            &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    if (!ruda_test_vm_append(function, block, RUDA_OPCODE_RETURN, result, 0, 0,
            &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_vm_f64(void) {
    typedef struct RudaTestVmF64Case {
        RudaOpcode opcode;
        uint64_t left;
        uint64_t right;
        RudaBytecodeType type;
        uint64_t expected_bits;
        bool expected_boolean;
    } RudaTestVmF64Case;
    static const RudaTestVmF64Case cases[] = {
        {RUDA_OPCODE_F64_ADD, UINT64_C(0x3ff8000000000000),
            UINT64_C(0x4002000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0x400e000000000000), false},
        {RUDA_OPCODE_F64_SUBTRACT, UINT64_C(0x4014000000000000),
            UINT64_C(0x4000000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0x4008000000000000), false},
        {RUDA_OPCODE_F64_MULTIPLY, UINT64_C(0xc000000000000000),
            UINT64_C(0x4008000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0xc018000000000000), false},
        {RUDA_OPCODE_F64_DIVIDE, UINT64_C(0xc018000000000000),
            UINT64_C(0x4008000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0xc000000000000000), false},
        {RUDA_OPCODE_F64_DIVIDE, UINT64_C(0x3ff0000000000000),
            UINT64_C(0x0000000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0x7ff0000000000000), false},
        {RUDA_OPCODE_F64_DIVIDE, UINT64_C(0xbff0000000000000),
            UINT64_C(0x0000000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0xfff0000000000000), false},
        {RUDA_OPCODE_F64_DIVIDE, UINT64_C(0x0000000000000000),
            UINT64_C(0x0000000000000000), RUDA_BYTECODE_TYPE_F64,
            UINT64_C(0x7ff8000000000000), false},
        {RUDA_OPCODE_F64_NEGATE, UINT64_C(0x0000000000000000), 0,
            RUDA_BYTECODE_TYPE_F64, UINT64_C(0x8000000000000000), false},
        {RUDA_OPCODE_F64_NEGATE, UINT64_C(0x7ff8000000000001), 0,
            RUDA_BYTECODE_TYPE_F64, UINT64_C(0xfff8000000000001), false},
        {RUDA_OPCODE_F64_EQUAL, UINT64_C(0x8000000000000000),
            UINT64_C(0x0000000000000000), RUDA_BYTECODE_TYPE_BOOL, 0, true},
        {RUDA_OPCODE_F64_EQUAL, UINT64_C(0x7ff8000000000001),
            UINT64_C(0x7ff8000000000001), RUDA_BYTECODE_TYPE_BOOL, 0, false},
        {RUDA_OPCODE_F64_LESS, UINT64_C(0xbff0000000000000),
            UINT64_C(0x0000000000000000), RUDA_BYTECODE_TYPE_BOOL, 0, true},
        {RUDA_OPCODE_F64_LESS, UINT64_C(0x7ff8000000000001),
            UINT64_C(0x3ff0000000000000), RUDA_BYTECODE_TYPE_BOOL, 0, false},
        {RUDA_OPCODE_F64_LESS_EQUAL, UINT64_C(0x8000000000000000),
            UINT64_C(0x0000000000000000), RUDA_BYTECODE_TYPE_BOOL, 0, true}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaBytecodeFunction function;
        RudaBytecodeValue result;
        memset(&function, 0, sizeof(function));
        result = ruda_test_vm_i64(1234);
        RUDA_EXPECT(ruda_test_vm_build_f64(&function, cases[index].opcode,
            cases[index].left, cases[index].right, cases[index].type));
        RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0,
            ruda_system_allocator(), ruda_bytecode_run_default_options(), &result)
            == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(result.type == cases[index].type);
        if (cases[index].type == RUDA_BYTECODE_TYPE_F64) {
            RUDA_EXPECT(result.as.f64_bits == cases[index].expected_bits);
        } else {
            RUDA_EXPECT(result.as.boolean == cases[index].expected_boolean);
        }
        ruda_bytecode_function_dispose(&function);
    }
}

static bool ruda_test_vm_build_string_literal(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    const unsigned char *bytes,
    size_t length,
    bool panic
) {
    RudaBytecodeStringId string_id = RUDA_BYTECODE_NO_STRING;
    uint32_t value = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 140;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator,
            panic ? RUDA_BYTECODE_TYPE_NEVER : RUDA_BYTECODE_TYPE_STRING,
            1, NULL, 0, ruda_bytecode_function_default_options())
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_STRING,
            &value)
        || !ruda_bytecode_add_string_constant(function, bytes, length, &string_id)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_CONST_STRING,
            value, string_id, 0, &span)
        || !ruda_test_vm_append(function, block,
            panic ? RUDA_OPCODE_PANIC_STRING : RUDA_OPCODE_RETURN,
            value, 0, 0, &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_string_equal(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    const unsigned char *left,
    size_t left_length,
    const unsigned char *right,
    size_t right_length
) {
    RudaBytecodeStringId left_id = RUDA_BYTECODE_NO_STRING;
    RudaBytecodeStringId right_id = RUDA_BYTECODE_NO_STRING;
    uint32_t left_register = UINT32_MAX;
    uint32_t right_register = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 150;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_BOOL,
            1, NULL, 0, ruda_bytecode_function_default_options())
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_STRING,
            &left_register)
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_STRING,
            &right_register)
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_BOOL, &result)
        || !ruda_bytecode_add_string_constant(function, left, left_length, &left_id)
        || !ruda_bytecode_add_string_constant(function, right, right_length, &right_id)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_CONST_STRING,
            left_register, left_id, 0, &span)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_CONST_STRING,
            right_register, right_id, 0, &span)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_STRING_EQUAL,
            result, left_register, right_register, &span)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_RETURN,
            result, 0, 0, &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_string_identity(
    RudaBytecodeFunction *function,
    RudaAllocator allocator
) {
    static const RudaBytecodeType parameters[] = {RUDA_BYTECODE_TYPE_STRING};
    uint32_t block = UINT32_MAX;
    uint64_t span = 170;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator,
            RUDA_BYTECODE_TYPE_STRING, 1, parameters, 1,
            ruda_bytecode_function_default_options())
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(function, block, RUDA_OPCODE_RETURN,
            0, 0, 0, &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_vm_strings_and_panic(void) {
    static const unsigned char message[] = {'p', 0, '!', 0xe2, 0x98, 0x83};
    RudaBytecodeFunction function;
    RudaBytecodeExecution execution;
    RudaBytecodePanic panic;
    RudaBytecodeRunOptions options;
    RudaBytecodeValue result;
    const unsigned char *bytes = NULL;
    size_t length = 0;

    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    RUDA_EXPECT(ruda_bytecode_execution_init(&execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()));
    RUDA_EXPECT(ruda_test_vm_build_string_literal(&function,
        ruda_system_allocator(), message, sizeof(message), false));
    result = ruda_test_vm_i64(1234);
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_INVALID_ARGUMENT);
    options = ruda_bytecode_run_default_options();
    options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_STRING);
    RUDA_EXPECT(ruda_bytecode_execution_string_bytes(&execution, result,
        &bytes, &length));
    RUDA_EXPECT(length == sizeof(message)
        && memcmp(bytes, message, sizeof(message)) == 0);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_vm_build_string_equal(&function,
        ruda_system_allocator(), message, sizeof(message), message,
        sizeof(message)));
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_BOOL && result.as.boolean);
    ruda_bytecode_function_dispose(&function);
    RUDA_EXPECT(ruda_test_vm_build_string_equal(&function,
        ruda_system_allocator(), (const unsigned char *)"a", 1,
        (const unsigned char *)"b", 1));
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_BOOL && !result.as.boolean);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
    RUDA_EXPECT(ruda_test_vm_build_string_literal(&function,
        ruda_system_allocator(), message, sizeof(message), true));
    options.panic = &panic;
    result = ruda_test_vm_i64(1234);
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_PANIC);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
    RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
    RUDA_EXPECT(panic.message.length == sizeof(message)
        && memcmp(panic.message.data, message, sizeof(message)) == 0);
    RUDA_EXPECT(panic.span.source == 0 && panic.span.start == 141
        && panic.span.end == 142 && panic.frames.length == 1);
    ruda_bytecode_function_dispose(&function);
    ruda_bytecode_panic_dispose(&panic);
    ruda_bytecode_execution_dispose(&execution);

    {
        RudaBytecodeExecution foreign_execution;
        RudaFailAllocator state;
        RudaBytecodeValue argument;
        memset(&foreign_execution, 0, sizeof(foreign_execution));
        RUDA_EXPECT(ruda_bytecode_execution_init(&foreign_execution,
            ruda_system_allocator(), ruda_bytecode_execution_default_options()));
        RUDA_EXPECT(ruda_bytecode_execution_init(&execution,
            ruda_system_allocator(), ruda_bytecode_execution_default_options()));
        RUDA_EXPECT(ruda_test_vm_build_string_literal(&function,
            ruda_system_allocator(), message, sizeof(message), false));
        options = ruda_bytecode_run_default_options();
        options.execution = &execution;
        RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0,
            ruda_system_allocator(), options, &argument) == RUDA_BYTECODE_RUN_OK);
        ruda_bytecode_function_dispose(&function);
        RUDA_EXPECT(ruda_test_vm_build_string_identity(&function,
            ruda_system_allocator()));
        options.execution = &foreign_execution;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), 0);
        result = ruda_test_vm_i64(1234);
        RUDA_EXPECT(ruda_bytecode_run(&function, &argument, 1,
            ruda_fail_allocator(&state), options, &result)
            == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        argument.as.reference = RUDA_BYTECODE_INVALID_REFERENCE;
        result = ruda_test_vm_i64(5678);
        RUDA_EXPECT(ruda_bytecode_run(&function, &argument, 1,
            ruda_fail_allocator(&state), options, &result)
            == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 5678);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        ruda_bytecode_function_dispose(&function);
        ruda_bytecode_execution_dispose(&execution);
        ruda_bytecode_execution_dispose(&foreign_execution);
    }

    {
        const RudaSymbolId callee_symbol = {{0x10}};
        const RudaSymbolId entry_symbol = {{0x20}};
        RudaBytecodeProgram program;
        RudaBytecodeFunction callee;
        RudaBytecodeFunction entry;
        RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
        uint32_t block = UINT32_MAX;
        uint32_t id = UINT32_MAX;
        uint64_t span = 160;
        const RudaBytecodePanicFrame *callee_frame = NULL;
        const RudaBytecodePanicFrame *caller_frame = NULL;
        memset(&program, 0, sizeof(program));
        memset(&callee, 0, sizeof(callee));
        memset(&entry, 0, sizeof(entry));
        memset(&execution, 0, sizeof(execution));
        memset(&panic, 0, sizeof(panic));
        RUDA_EXPECT(ruda_bytecode_execution_init(&execution,
            ruda_system_allocator(), ruda_bytecode_execution_default_options()));
        RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
        RUDA_EXPECT(ruda_test_vm_build_string_literal(&callee,
            ruda_system_allocator(), message, sizeof(message), true));
        RUDA_EXPECT(ruda_bytecode_function_init(&entry,
            ruda_system_allocator(), RUDA_BYTECODE_TYPE_NEVER, 1, NULL, 0,
            ruda_bytecode_function_default_options()));
        RUDA_EXPECT(ruda_bytecode_add_block(&entry, &block));
        RUDA_EXPECT(ruda_bytecode_add_call(&entry, callee_symbol,
            RUDA_BYTECODE_TYPE_NEVER, NULL, 0, &call));
        RUDA_EXPECT(ruda_test_vm_append(&entry, block,
            RUDA_OPCODE_CALL_DIRECT_NEVER, call, 0, 0, &span));
        RUDA_EXPECT(ruda_bytecode_program_init(&program, ruda_system_allocator(),
            1, ruda_bytecode_program_default_options()));
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, callee_symbol,
            &callee, &id) && id == 0);
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, entry_symbol,
            &entry, &id) && id == 1);
        RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, entry_symbol));
        options = ruda_bytecode_run_default_options();
        options.execution = &execution;
        options.panic = &panic;
        result = ruda_test_vm_i64(1234);
        RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
            ruda_system_allocator(), options, &result) == RUDA_BYTECODE_RUN_PANIC);
        RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
        RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
        RUDA_EXPECT(panic.message.length == sizeof(message)
            && memcmp(panic.message.data, message, sizeof(message)) == 0);
        RUDA_EXPECT(memcmp(&panic.symbol, &callee_symbol, sizeof(panic.symbol)) == 0
            && panic.span.start == 141 && panic.span.end == 142
            && panic.frames.length == 2);
        callee_frame = ruda_bytecode_panic_frame(&panic, 0);
        caller_frame = ruda_bytecode_panic_frame(&panic, 1);
        RUDA_EXPECT(callee_frame != NULL && caller_frame != NULL);
        if (callee_frame != NULL) {
            RUDA_EXPECT(memcmp(&callee_frame->symbol, &callee_symbol,
                sizeof(callee_symbol)) == 0 && callee_frame->call_span.start == 141
                && callee_frame->call_span.end == 142);
        }
        if (caller_frame != NULL) {
            RUDA_EXPECT(memcmp(&caller_frame->symbol, &entry_symbol,
                sizeof(entry_symbol)) == 0 && caller_frame->call_span.start == 160
                && caller_frame->call_span.end == 161);
        }
        ruda_bytecode_program_dispose(&program);
        ruda_bytecode_panic_dispose(&panic);
        ruda_bytecode_execution_dispose(&execution);
    }

    {
        size_t failure_index = 0;
        bool reached_success = false;
        RUDA_EXPECT(ruda_test_vm_build_string_literal(&function,
            ruda_system_allocator(), message, sizeof(message), false));
        for (failure_index = 0; failure_index < 32 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeExecution failed_execution;
            RudaBytecodeRunStatus status;
            memset(&failed_execution, 0, sizeof(failed_execution));
            ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
            if (!ruda_bytecode_execution_init(&failed_execution,
                    ruda_fail_allocator(&state),
                    ruda_bytecode_execution_default_options())) {
                RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
                continue;
            }
            options = ruda_bytecode_run_default_options();
            options.execution = &failed_execution;
            result = ruda_test_vm_i64(1234);
            status = ruda_bytecode_run(&function, NULL, 0,
                ruda_system_allocator(), options, &result);
            RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED
                || status == RUDA_BYTECODE_RUN_OK);
            if (status == RUDA_BYTECODE_RUN_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64
                    && result.as.i64 == 1234);
            }
            ruda_bytecode_execution_dispose(&failed_execution);
            RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
        ruda_bytecode_function_dispose(&function);
    }
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

static void ruda_test_vm_direct_calls(void) {
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

static bool ruda_test_vm_build_array_function(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    int64_t access_index,
    bool nested
) {
    static const RudaBytecodeType flat_types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64
    };
    static const RudaBytecodeType nested_types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_ARRAY_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_F64
    };
    RudaBytecodeArrayArgument inner_arguments[2];
    RudaBytecodeArrayArgument outer_argument;
    RudaBytecodeArrayConstructionId inner =
        RUDA_BYTECODE_NO_ARRAY_CONSTRUCTION;
    RudaBytecodeArrayConstructionId outer =
        RUDA_BYTECODE_NO_ARRAY_CONSTRUCTION;
    uint32_t block = UINT32_MAX;
    uint64_t span = 700;
    const RudaBytecodeType *types = nested ? nested_types : flat_types;
    size_t type_count = nested
        ? sizeof(nested_types) / sizeof(nested_types[0])
        : sizeof(flat_types) / sizeof(flat_types[0]);
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(
            function, allocator, RUDA_BYTECODE_TYPE_F64, 1, NULL, 0,
            ruda_bytecode_function_default_options()
        ) || !ruda_test_vm_add_registers(function, types, type_count)
        || !ruda_bytecode_add_block(function, &block)) {
        goto failure;
    }
    inner_arguments[0].register_id = 0;
    inner_arguments[0].type = RUDA_BYTECODE_TYPE_F64;
    inner_arguments[1].register_id = nested ? 0 : 1;
    inner_arguments[1].type = RUDA_BYTECODE_TYPE_F64;
    if (!ruda_bytecode_add_array_construction(
            function, RUDA_BYTECODE_TYPE_ARRAY_F64, inner_arguments,
            nested ? 1 : 2, &inner
        )) {
        goto failure;
    }
    if (nested) {
        outer_argument.register_id = 1;
        outer_argument.type = RUDA_BYTECODE_TYPE_ARRAY_F64;
        if (!ruda_bytecode_add_array_construction(
                function, RUDA_BYTECODE_TYPE_ARRAY_ARRAY_F64,
                &outer_argument, 1, &outer
            )) {
            goto failure;
        }
    }
    if (!ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_F64, 0,
            UINT64_C(0x3ff8000000000000), 0, &span
        )) {
        goto failure;
    }
    if (!nested && !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_F64, 1,
            UINT64_C(0x4004000000000000), 0, &span
        )) {
        goto failure;
    }
    if (!ruda_test_vm_append(
            function, block, RUDA_OPCODE_ARRAY_NEW, nested ? 1 : 2,
            inner, 0, &span
        )) {
        goto failure;
    }
    if (nested) {
        if (!ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_NEW, 2, outer, 0, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_CONST_I64, 3, 0, 0, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_GET, 4, 2, 3, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_GET, 5, 4, 3, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_RETURN, 5, 0, 0, &span
            )) {
            goto failure;
        }
    } else {
        if (!ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_LENGTH, 3, 2, 0, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_CONST_I64, 4,
                (uint64_t)access_index, 0, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_REQUIRE_INDEX, 7, 2, 4,
                &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_CONST_F64, 5,
                UINT64_C(0x4022000000000000), 0, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_SET, 7, 4, 5, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_GET, 6, 2, 4, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_RETURN, 6, 0, 0, &span
            )) {
            goto failure;
        }
    }
    return true;

failure:
    ruda_bytecode_function_dispose(function);
    return false;
}

static void ruda_test_vm_managed_arrays(void) {
    static const int64_t invalid_indices[] = {
        -1, 2, INT64_MIN, INT64_MAX
    };
    static const char *expected_messages[] = {
        "array index -1 is outside length 2",
        "array index 2 is outside length 2",
        "array index -9223372036854775808 is outside length 2",
        "array index 9223372036854775807 is outside length 2"
    };
    RudaBytecodeFunction function;
    RudaBytecodeFunction decoded;
    RudaBytecodeExecution execution;
    RudaBytecodePanic panic;
    RudaBytecodeValue result;
    RudaOwnedBytes encoded;
    RudaTestBuffer disassembly;
    RudaBytecodeRunOptions run_options;
    RudaBytecodeExecutionOptions execution_options =
        ruda_bytecode_execution_default_options();
    size_t index = 0;
    memset(&function, 0, sizeof(function));
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&encoded, 0, sizeof(encoded));
    execution_options.managed_heap.collect_before_allocation = true;
    RUDA_EXPECT(ruda_test_vm_build_array_function(
        &function, ruda_system_allocator(), 1, false
    ));
    {
        RudaBytecodeArrayConstruction *construction = ruda_vector_at(
            &function.array_constructions, 0
        );
        RudaBytecodeArrayArgument *argument = ruda_vector_at(
            &function.array_arguments, 0
        );
        RudaBytecodeInstruction *array_new = ruda_test_vm_at(&function, 0, 2);
        RUDA_EXPECT(construction != NULL && argument != NULL
            && array_new != NULL);
        if (construction != NULL) {
            construction->array_type = RUDA_BYTECODE_TYPE_F64;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            construction->array_type = RUDA_BYTECODE_TYPE_ARRAY_F64;
        }
        if (argument != NULL) {
            argument->type = RUDA_BYTECODE_TYPE_BOOL;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
            argument->type = RUDA_BYTECODE_TYPE_F64;
        }
        if (array_new != NULL) {
            array_new->operands[1].unsigned_value = UINT64_MAX;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            array_new->operands[1].unsigned_value = 0;
        }
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    {
        RudaBytecodeInstruction *set = ruda_test_vm_at(&function, 0, 7);
        RudaBytecodeInstruction *rhs = ruda_test_vm_at(&function, 0, 6);
        RudaBytecodeInstruction saved_rhs;
        uint64_t original_array = set != NULL
            ? set->operands[0].unsigned_value
            : UINT64_MAX;
        uint64_t original_index = set != NULL
            ? set->operands[1].unsigned_value
            : UINT64_MAX;
        RUDA_EXPECT(set != NULL && rhs != NULL);
        memset(&saved_rhs, 0, sizeof(saved_rhs));
        if (rhs != NULL) {
            saved_rhs = *rhs;
        }
        if (set != NULL && rhs != NULL) {
            set->operands[0].unsigned_value = 2;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            RUDA_EXPECT(ruda_bytecode_run(
                &function, NULL, 0, ruda_system_allocator(),
                ruda_bytecode_run_default_options(), &result
            ) == RUDA_BYTECODE_RUN_INVALID_BYTECODE);
            set->operands[0].unsigned_value = original_array;
            set->operands[1].unsigned_value = 3;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            set->operands[1].unsigned_value = original_index;
            *rhs = ruda_test_vm_instruction(
                RUDA_OPCODE_COPY, 7, 2, 0, saved_rhs.span.start
            );
            set->operands[2].unsigned_value = 0;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            *rhs = ruda_test_vm_instruction(
                RUDA_OPCODE_CONST_I64, 4, 0, 0, saved_rhs.span.start
            );
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            *rhs = saved_rhs;
            set->operands[2].unsigned_value = 5;
        }
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    ruda_test_buffer_init(&disassembly);
    RUDA_EXPECT(ruda_bytecode_disassemble(
        &function, ruda_test_buffer_writer(&disassembly)
    ) == RUDA_BYTECODE_DISASSEMBLE_OK);
    RUDA_EXPECT(strstr(disassembly.bytes, "array_new r2, a0") != NULL);
    RUDA_EXPECT(strstr(disassembly.bytes, "array_set r7, r4, r5") != NULL);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &function, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    {
        bool reached_success = false;
        size_t failure_index = 0;
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeFunction attempted;
            RudaBytecodeCodecStatus decode_status;
            memset(&attempted, 0, sizeof(attempted));
            ruda_fail_allocator_init(
                &state, ruda_system_allocator(), failure_index
            );
            decode_status = ruda_bytecode_decode_function(
                encoded.data, encoded.length, ruda_fail_allocator(&state),
                ruda_bytecode_function_default_options(), &attempted
            );
            if (decode_status == RUDA_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(decode_status
                    == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            }
            ruda_bytecode_function_dispose(&attempted);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
    }
    RUDA_EXPECT(ruda_bytecode_decode_function(
        encoded.data, encoded.length, ruda_system_allocator(),
        ruda_bytecode_function_default_options(), &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(), execution_options
    ));
    RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &panic;
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_bytecode_run(
        &decoded, NULL, 0, ruda_system_allocator(), run_options, &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x4022000000000000));
    RUDA_EXPECT(execution.managed_heap.roots == NULL);
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&function);

    {
        bool reached_success = false;
        size_t failure_index = 0;
        RUDA_EXPECT(ruda_test_vm_build_array_function(
            &function, ruda_system_allocator(), 1, false
        ));
        for (failure_index = 0; failure_index < 32 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeExecution failed_execution;
            RudaBytecodeRunStatus status;
            memset(&failed_execution, 0, sizeof(failed_execution));
            ruda_fail_allocator_init(
                &state, ruda_system_allocator(), SIZE_MAX
            );
            RUDA_EXPECT(ruda_bytecode_execution_init(
                &failed_execution, ruda_fail_allocator(&state),
                ruda_bytecode_execution_default_options()
            ));
            state.failure_index = state.attempts + failure_index;
            run_options = ruda_bytecode_run_default_options();
            run_options.execution = &failed_execution;
            status = ruda_bytecode_run(
                &function, NULL, 0, ruda_fail_allocator(&state),
                run_options, &result
            );
            if (status == RUDA_BYTECODE_RUN_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED);
            }
            ruda_bytecode_execution_dispose(&failed_execution);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
        ruda_bytecode_function_dispose(&function);
    }
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &panic;

    RUDA_EXPECT(ruda_test_vm_build_array_function(
        &function, ruda_system_allocator(), 0, true
    ));
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_bytecode_run(
        &function, NULL, 0, ruda_system_allocator(), run_options, &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x3ff8000000000000));
    RUDA_EXPECT(execution.managed_heap.roots == NULL);
    ruda_bytecode_function_dispose(&function);

    for (index = 0;
        index < sizeof(invalid_indices) / sizeof(invalid_indices[0]);
        index += 1) {
        RUDA_EXPECT(ruda_test_vm_build_array_function(
            &function, ruda_system_allocator(), invalid_indices[index], false
        ));
        memset(&result, 0, sizeof(result));
        RUDA_EXPECT(ruda_bytecode_run(
            &function, NULL, 0, ruda_system_allocator(), run_options, &result
        ) == RUDA_BYTECODE_RUN_PANIC);
        RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
        RUDA_EXPECT(panic.message.length == strlen(expected_messages[index]));
        RUDA_EXPECT(memcmp(
            panic.message.data, expected_messages[index], panic.message.length
        ) == 0);
        RUDA_EXPECT(panic.span.start == 705 && panic.span.end == 706);
        RUDA_EXPECT(execution.managed_heap.roots == NULL);
        if (index == 0) {
            run_options.panic = NULL;
            RUDA_EXPECT(ruda_bytecode_run(
                &function, NULL, 0, ruda_system_allocator(), run_options,
                &result
            ) == RUDA_BYTECODE_RUN_PANIC);
            RUDA_EXPECT(execution.managed_heap.roots == NULL);
            run_options.panic = &panic;
        }
        ruda_bytecode_function_dispose(&function);
    }
    ruda_bytecode_panic_dispose(&panic);
    ruda_bytecode_execution_dispose(&execution);
}

static void ruda_test_vm_array_feature_call_metadata(void) {
    const RudaSymbolId callee = {{0x10}};
    RudaBytecodeFunction function;
    RudaBytecodeFunction decoded;
    RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
    RudaOwnedBytes encoded;
    uint32_t block = UINT32_MAX;
    uint64_t span = 880;
    const size_t feature_offset = 9;
    memset(&function, 0, sizeof(function));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_bytecode_function_init(
        &function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_VOID,
        1, NULL, 0, ruda_bytecode_function_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_add_call(
        &function, callee, RUDA_BYTECODE_TYPE_ARRAY_F64, NULL, 0, &call
    ));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &block));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, block, RUDA_OPCODE_RETURN_VOID, 0, 0, 0, &span
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &function, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(encoded.length > feature_offset);
    if (encoded.length > feature_offset) {
        /* v1 and the current feature mask are one-byte canonical varints. */
        RUDA_EXPECT(encoded.data[feature_offset] == ((1U << 0) | (1U << 3)));
        RUDA_EXPECT(ruda_bytecode_decode_function(
            encoded.data, encoded.length, ruda_system_allocator(),
            ruda_bytecode_function_default_options(), &decoded
        ) == RUDA_BYTECODE_CODEC_OK);
        ruda_bytecode_function_dispose(&decoded);
        encoded.data[feature_offset] = 1U << 0;
        RUDA_EXPECT(ruda_bytecode_decode_function(
            encoded.data, encoded.length, ruda_system_allocator(),
            ruda_bytecode_function_default_options(), &decoded
        ) != RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(!decoded.initialized);
    }
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&function);
}

static void ruda_test_vm_checked_index_cfg(void) {
    static const RudaBytecodeType parameters[] = {
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_BOOL
    };
    RudaBytecodeFunction function;
    RudaBytecodeInstruction *second_require;
    uint32_t blocks[4] = {
        UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX
    };
    uint32_t checked = UINT32_MAX;
    uint32_t alternate = UINT32_MAX;
    uint64_t span = 1080;
    memset(&function, 0, sizeof(function));
    RUDA_EXPECT(ruda_bytecode_function_init(
        &function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_F64,
        1, parameters, sizeof(parameters) / sizeof(parameters[0]),
        ruda_bytecode_function_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_add_register(
        &function, RUDA_BYTECODE_TYPE_ARRAY_F64, &checked
    ));
    RUDA_EXPECT(ruda_bytecode_add_register(
        &function, RUDA_BYTECODE_TYPE_ARRAY_F64, &alternate
    ));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &blocks[0]));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &blocks[1]));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &blocks[2]));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &blocks[3]));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[0], RUDA_OPCODE_BRANCH_BOOL, 3, blocks[1],
        blocks[2], &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[1], RUDA_OPCODE_ARRAY_REQUIRE_INDEX, checked, 0, 1,
        &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[1], RUDA_OPCODE_JUMP, blocks[3], 0, 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[2], RUDA_OPCODE_ARRAY_REQUIRE_INDEX, checked, 0, 1,
        &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[2], RUDA_OPCODE_JUMP, blocks[3], 0, 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[3], RUDA_OPCODE_ARRAY_SET, checked, 1, 2, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[3], RUDA_OPCODE_RETURN, 2, 0, 0, &span
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    second_require = ruda_test_vm_at(&function, blocks[2], 0);
    RUDA_EXPECT(second_require != NULL);
    if (second_require != NULL) {
        second_require->operands[0].unsigned_value = alternate;
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
        second_require->operands[0].unsigned_value = checked;
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    ruda_bytecode_function_dispose(&function);
}

static bool ruda_test_vm_build_array_rhs_panic_program(
    RudaBytecodeProgram *program,
    int64_t access_index
) {
    const RudaSymbolId rhs_symbol = {{0x10}};
    const RudaSymbolId entry_symbol = {{0x20}};
    static const unsigned char rhs_message[] = "rhs evaluated";
    static const RudaBytecodeType entry_types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_ARRAY_F64
    };
    RudaBytecodeFunction rhs;
    RudaBytecodeFunction entry;
    RudaBytecodeArrayArgument argument;
    RudaBytecodeArrayConstructionId construction =
        RUDA_BYTECODE_NO_ARRAY_CONSTRUCTION;
    RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
    RudaBytecodeInstruction *require_index;
    uint32_t block = UINT32_MAX;
    uint32_t function_id = UINT32_MAX;
    uint64_t span = 1100;
    memset(program, 0, sizeof(*program));
    memset(&rhs, 0, sizeof(rhs));
    memset(&entry, 0, sizeof(entry));
    if (!ruda_bytecode_program_init(
            program, ruda_system_allocator(), 1,
            ruda_bytecode_program_default_options()
        ) || !ruda_test_vm_build_string_literal(
            &rhs, ruda_system_allocator(), rhs_message,
            sizeof(rhs_message) - 1, true
        ) || !ruda_bytecode_program_take_function(
            program, rhs_symbol, &rhs, &function_id
        ) || !ruda_bytecode_function_init(
            &entry, ruda_system_allocator(), RUDA_BYTECODE_TYPE_NEVER,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_test_vm_add_registers(
            &entry, entry_types, sizeof(entry_types) / sizeof(entry_types[0])
        ) || !ruda_bytecode_add_block(&entry, &block)) {
        goto failure;
    }
    argument.register_id = 0;
    argument.type = RUDA_BYTECODE_TYPE_F64;
    if (!ruda_bytecode_add_array_construction(
            &entry, RUDA_BYTECODE_TYPE_ARRAY_F64, &argument, 1,
            &construction
        ) || !ruda_bytecode_add_call(
            &entry, rhs_symbol, RUDA_BYTECODE_TYPE_NEVER, NULL, 0, &call
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_CONST_F64, 0,
            UINT64_C(0x3ff0000000000000), 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_CONST_I64, 2,
            (uint64_t)access_index, 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_ARRAY_REQUIRE_INDEX, 3, 1, 2, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_CALL_DIRECT_NEVER, call, 0, 0, &span
        )) {
        goto failure;
    }
    require_index = ruda_test_vm_at(&entry, 0, 3);
    if (require_index == NULL) {
        goto failure;
    }
    require_index->span.start = 1120;
    require_index->span.end = 1134;
    if (!ruda_bytecode_program_take_function(
            program, entry_symbol, &entry, &function_id
        ) || !ruda_bytecode_program_set_entry(program, entry_symbol)) {
        goto failure;
    }
    return true;

failure:
    ruda_bytecode_function_dispose(&entry);
    ruda_bytecode_function_dispose(&rhs);
    ruda_bytecode_program_dispose(program);
    return false;
}

static void ruda_test_vm_array_rhs_ordering(void) {
    static const struct {
        int64_t index;
        const char *code;
    } cases[] = {
        {1, "G4002"},
        {0, "G4000"}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaBytecodeProgram program;
        RudaBytecodeProgram decoded;
        RudaBytecodeExecution execution;
        RudaBytecodePanic panic;
        RudaBytecodeRunOptions options;
        RudaBytecodeValue result;
        RudaOwnedBytes encoded;
        memset(&program, 0, sizeof(program));
        memset(&decoded, 0, sizeof(decoded));
        memset(&execution, 0, sizeof(execution));
        memset(&panic, 0, sizeof(panic));
        memset(&encoded, 0, sizeof(encoded));
        RUDA_EXPECT(ruda_test_vm_build_array_rhs_panic_program(
            &program, cases[index].index
        ));
        RUDA_EXPECT(ruda_bytecode_program_verify(&program)
            == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
        RUDA_EXPECT(ruda_bytecode_encode_program(
            &program, ruda_system_allocator(), &encoded
        ) == RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(ruda_bytecode_decode_program(
            encoded.data, encoded.length, ruda_system_allocator(),
            ruda_bytecode_program_default_options(), &decoded
        ) == RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(ruda_bytecode_execution_init(
            &execution, ruda_system_allocator(),
            ruda_bytecode_execution_default_options()
        ));
        RUDA_EXPECT(ruda_bytecode_panic_init(
            &panic, ruda_system_allocator()
        ));
        options = ruda_bytecode_run_default_options();
        options.execution = &execution;
        options.panic = &panic;
        RUDA_EXPECT(ruda_bytecode_run_program(
            &decoded, NULL, 0, ruda_system_allocator(), options, &result
        ) == RUDA_BYTECODE_RUN_PANIC);
        RUDA_EXPECT(panic.code != NULL
            && strcmp(panic.code, cases[index].code) == 0);
        if (cases[index].index == 1) {
            RUDA_EXPECT(panic.span.start == 1120 && panic.span.end == 1134);
            RUDA_EXPECT(panic.message.length
                == strlen("array index 1 is outside length 1"));
            RUDA_EXPECT(memcmp(
                panic.message.data, "array index 1 is outside length 1",
                panic.message.length
            ) == 0);
        } else {
            RUDA_EXPECT(panic.message.length == strlen("rhs evaluated"));
            RUDA_EXPECT(memcmp(
                panic.message.data, "rhs evaluated", panic.message.length
            ) == 0);
        }
        RUDA_EXPECT(execution.managed_heap.roots == NULL);
        ruda_bytecode_panic_dispose(&panic);
        ruda_bytecode_execution_dispose(&execution);
        ruda_bytecode_program_dispose(&decoded);
        ruda_owned_bytes_dispose(&encoded);
        ruda_bytecode_program_dispose(&program);
    }
}

static bool ruda_test_vm_build_array_call_program(
    RudaBytecodeProgram *program
) {
    const RudaSymbolId leaf_symbol = {{0x10}};
    const RudaSymbolId entry_symbol = {{0x20}};
    static const RudaBytecodeType leaf_types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64
    };
    static const RudaBytecodeType entry_types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64
    };
    RudaBytecodeFunction leaf;
    RudaBytecodeFunction entry;
    RudaBytecodeArrayArgument array_argument;
    RudaBytecodeArrayConstructionId construction;
    RudaBytecodeCallId call;
    uint32_t block;
    uint32_t function_id;
    uint64_t span = 900;
    memset(program, 0, sizeof(*program));
    memset(&leaf, 0, sizeof(leaf));
    memset(&entry, 0, sizeof(entry));
    if (!ruda_bytecode_program_init(
            program, ruda_system_allocator(), 1,
            ruda_bytecode_program_default_options()
        ) || !ruda_bytecode_function_init(
            &leaf, ruda_system_allocator(), RUDA_BYTECODE_TYPE_ARRAY_F64,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_test_vm_add_registers(
            &leaf, leaf_types, sizeof(leaf_types) / sizeof(leaf_types[0])
        ) || !ruda_bytecode_add_block(&leaf, &block)) {
        goto failure;
    }
    array_argument.register_id = 0;
    array_argument.type = RUDA_BYTECODE_TYPE_F64;
    if (!ruda_bytecode_add_array_construction(
            &leaf, RUDA_BYTECODE_TYPE_ARRAY_F64, &array_argument, 1,
            &construction
        ) || !ruda_test_vm_append(
            &leaf, block, RUDA_OPCODE_CONST_F64, 0,
            UINT64_C(0x4008000000000000), 0, &span
        ) || !ruda_test_vm_append(
            &leaf, block, RUDA_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !ruda_test_vm_append(
            &leaf, block, RUDA_OPCODE_RETURN, 1, 0, 0, &span
        ) || !ruda_bytecode_program_take_function(
            program, leaf_symbol, &leaf, &function_id
        )) {
        goto failure;
    }
    span = 920;
    if (!ruda_bytecode_function_init(
            &entry, ruda_system_allocator(), RUDA_BYTECODE_TYPE_ARRAY_F64,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_test_vm_add_registers(
            &entry, entry_types, sizeof(entry_types) / sizeof(entry_types[0])
        ) || !ruda_bytecode_add_block(&entry, &block)) {
        goto failure;
    }
    array_argument.register_id = 0;
    if (!ruda_bytecode_add_array_construction(
            &entry, RUDA_BYTECODE_TYPE_ARRAY_F64, &array_argument, 1,
            &construction
        ) || !ruda_bytecode_add_call(
            &entry, leaf_symbol, RUDA_BYTECODE_TYPE_ARRAY_F64, NULL, 0, &call
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_CONST_F64, 0,
            UINT64_C(0x3ff8000000000000), 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_CALL_DIRECT, 2, call, 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_ARRAY_NEW, 5, construction, 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_CONST_I64, 3, 0, 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_ARRAY_GET, 4, 2, 3, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_RETURN, 2, 0, 0, &span
        ) || !ruda_bytecode_program_take_function(
            program, entry_symbol, &entry, &function_id
        ) || !ruda_bytecode_program_set_entry(program, entry_symbol)) {
        goto failure;
    }
    return true;

failure:
    ruda_bytecode_function_dispose(&entry);
    ruda_bytecode_function_dispose(&leaf);
    ruda_bytecode_program_dispose(program);
    return false;
}

static void ruda_test_vm_managed_array_calls(void) {
    RudaBytecodeProgram program;
    RudaBytecodeProgram decoded;
    RudaBytecodeExecution execution;
    RudaBytecodeValue result;
    RudaBytecodeRunOptions run_options;
    RudaManagedRootFrame host_root;
    RudaManagedReference *host_locations[1];
    RudaManagedCell cell;
    size_t length = 0;
    RudaBytecodeExecutionOptions execution_options =
        ruda_bytecode_execution_default_options();
    RudaOwnedBytes encoded;
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&encoded, 0, sizeof(encoded));
    memset(&host_root, 0, sizeof(host_root));
    memset(&cell, 0, sizeof(cell));
    execution_options.managed_heap.collect_before_allocation = true;
    RUDA_EXPECT(ruda_test_vm_build_array_call_program(&program));
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    program.options.max_total_array_constructions = 1;
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
    program.options.max_total_array_constructions = 1000000;
    program.options.max_total_array_arguments = 1;
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
    program.options.max_total_array_arguments = 10000000;
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_encode_program(
        &program, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    {
        RudaBytecodeProgramOptions limited =
            ruda_bytecode_program_default_options();
        RudaFailAllocator tracking;
        limited.max_total_array_constructions = 1;
        ruda_fail_allocator_init(
            &tracking, ruda_system_allocator(), SIZE_MAX
        );
        RUDA_EXPECT(ruda_bytecode_decode_program(
            encoded.data, encoded.length, ruda_fail_allocator(&tracking),
            limited, &decoded
        ) == RUDA_BYTECODE_CODEC_LIMIT_REACHED);
        RUDA_EXPECT(!decoded.initialized);
        ruda_bytecode_program_dispose(&decoded);
        RUDA_EXPECT(tracking.live_allocations == 0);
        RUDA_EXPECT(tracking.live_bytes == 0);
    }
    {
        RudaBytecodeProgramOptions limited =
            ruda_bytecode_program_default_options();
        RudaFailAllocator tracking;
        limited.max_total_array_arguments = 1;
        ruda_fail_allocator_init(
            &tracking, ruda_system_allocator(), SIZE_MAX
        );
        RUDA_EXPECT(ruda_bytecode_decode_program(
            encoded.data, encoded.length, ruda_fail_allocator(&tracking),
            limited, &decoded
        ) == RUDA_BYTECODE_CODEC_LIMIT_REACHED);
        RUDA_EXPECT(!decoded.initialized);
        ruda_bytecode_program_dispose(&decoded);
        RUDA_EXPECT(tracking.live_allocations == 0);
        RUDA_EXPECT(tracking.live_bytes == 0);
    }
    RUDA_EXPECT(ruda_bytecode_decode_program(
        encoded.data, encoded.length, ruda_system_allocator(),
        ruda_bytecode_program_default_options(), &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(), execution_options
    ));
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_bytecode_run_program(
        &decoded, NULL, 0, ruda_system_allocator(), run_options, &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_ARRAY_F64);
    RUDA_EXPECT(ruda_managed_array_length(
        &execution.managed_heap, result.as.managed_reference, &length
    ) == RUDA_MANAGED_OK && length == 1);
    host_locations[0] = &result.as.managed_reference;
    RUDA_EXPECT(ruda_managed_root_push(
        &execution.managed_heap, &host_root, host_locations, 1
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&execution.managed_heap)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_get(
        &execution.managed_heap, result.as.managed_reference, 0, &cell, &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(cell.f64_bits == UINT64_C(0x4008000000000000));
    RUDA_EXPECT(ruda_managed_root_pop(
        &execution.managed_heap, &host_root
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&execution.managed_heap)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_length(
        &execution.managed_heap, result.as.managed_reference, &length
    ) == RUDA_MANAGED_INVALID_REFERENCE);
    RUDA_EXPECT(execution.managed_heap.roots == NULL);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_program_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_program_dispose(&program);
}

static void ruda_test_vm_managed_array_provenance(void) {
    const RudaBytecodeType parameter = RUDA_BYTECODE_TYPE_ARRAY_F64;
    RudaBytecodeFunction function;
    RudaBytecodeExecution first;
    RudaBytecodeExecution second;
    RudaBytecodeValue argument;
    RudaBytecodeValue result;
    RudaBytecodeRunOptions options;
    RudaManagedCell cell;
    RudaManagedReference first_reference;
    RudaManagedReference second_reference;
    RudaManagedReference wrong_descriptor_reference;
    RudaManagedReference stale_reference;
    RudaManagedCell nested_cell;
    uint32_t length_register = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 950;
    memset(&function, 0, sizeof(function));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&cell, 0, sizeof(cell));
    memset(&nested_cell, 0, sizeof(nested_cell));
    cell.f64_bits = UINT64_C(0x3ff0000000000000);
    RUDA_EXPECT(ruda_bytecode_function_init(
        &function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_I64,
        1, &parameter, 1, ruda_bytecode_function_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_add_register(
        &function, RUDA_BYTECODE_TYPE_I64, &length_register
    ));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &block));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, block, RUDA_OPCODE_ARRAY_LENGTH, length_register, 0, 0,
        &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, block, RUDA_OPCODE_RETURN, length_register, 0, 0, &span
    ));
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &first, ruda_system_allocator(), ruda_bytecode_execution_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &second, ruda_system_allocator(), ruda_bytecode_execution_default_options()
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &first.managed_heap, &ruda_managed_array_f64_descriptor,
        &cell, 1, &first_reference
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &second.managed_heap, &ruda_managed_array_f64_descriptor,
        &cell, 1, &second_reference
    ) == RUDA_MANAGED_OK);
    nested_cell.reference = second_reference;
    RUDA_EXPECT(ruda_managed_array_new(
        &second.managed_heap, &ruda_managed_array_array_f64_descriptor,
        &nested_cell, 1, &wrong_descriptor_reference
    ) == RUDA_MANAGED_OK);
    argument.type = RUDA_BYTECODE_TYPE_ARRAY_F64;
    argument.as.managed_reference = first_reference;
    options = ruda_bytecode_run_default_options();
    options.execution = &second;
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_bytecode_run(
        &function, &argument, 1, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
    argument.as.managed_reference = wrong_descriptor_reference;
    RUDA_EXPECT(ruda_bytecode_run(
        &function, &argument, 1, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
    stale_reference = second_reference;
    ruda_bytecode_execution_dispose(&second);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &second, ruda_system_allocator(), ruda_bytecode_execution_default_options()
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &second.managed_heap, &ruda_managed_array_f64_descriptor,
        &cell, 1, &second_reference
    ) == RUDA_MANAGED_OK);
    argument.as.managed_reference = stale_reference;
    options.execution = &second;
    RUDA_EXPECT(ruda_bytecode_run(
        &function, &argument, 1, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
    ruda_bytecode_execution_dispose(&second);
    ruda_bytecode_execution_dispose(&first);
    ruda_bytecode_function_dispose(&function);
}

static void ruda_test_vm_managed_array_panic_frames(void) {
    const RudaSymbolId leaf_symbol = {{0x30}};
    const RudaSymbolId entry_symbol = {{0x40}};
    static const RudaBytecodeType leaf_types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64
    };
    static const RudaBytecodeType entry_types[] = {
        RUDA_BYTECODE_TYPE_I64
    };
    RudaBytecodeFunction leaf;
    RudaBytecodeFunction entry;
    RudaBytecodeProgram program;
    RudaBytecodeExecution execution;
    RudaBytecodePanic panic;
    RudaBytecodeValue result;
    RudaBytecodeRunOptions options;
    RudaBytecodeArrayArgument array_argument;
    RudaBytecodeArrayConstructionId construction;
    RudaBytecodeCallId call;
    const RudaBytecodePanicFrame *frame;
    uint32_t block;
    uint32_t function_id;
    uint64_t span = 980;
    memset(&leaf, 0, sizeof(leaf));
    memset(&entry, 0, sizeof(entry));
    memset(&program, 0, sizeof(program));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    RUDA_EXPECT(ruda_bytecode_program_init(
        &program, ruda_system_allocator(), 1,
        ruda_bytecode_program_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_function_init(
        &leaf, ruda_system_allocator(), RUDA_BYTECODE_TYPE_I64,
        1, NULL, 0, ruda_bytecode_function_default_options()
    ));
    RUDA_EXPECT(ruda_test_vm_add_registers(
        &leaf, leaf_types, sizeof(leaf_types) / sizeof(leaf_types[0])
    ));
    RUDA_EXPECT(ruda_bytecode_add_block(&leaf, &block));
    array_argument.register_id = 0;
    array_argument.type = RUDA_BYTECODE_TYPE_F64;
    RUDA_EXPECT(ruda_bytecode_add_array_construction(
        &leaf, RUDA_BYTECODE_TYPE_ARRAY_F64, &array_argument, 1,
        &construction
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &leaf, block, RUDA_OPCODE_CONST_F64, 0,
        UINT64_C(0x3ff0000000000000), 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &leaf, block, RUDA_OPCODE_ARRAY_NEW, 1, construction, 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &leaf, block, RUDA_OPCODE_CONST_I64, 2, (uint64_t)-1, 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &leaf, block, RUDA_OPCODE_ARRAY_REQUIRE_INDEX, 3, 1, 2, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &leaf, block, RUDA_OPCODE_ARRAY_LENGTH, 4, 3, 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &leaf, block, RUDA_OPCODE_RETURN, 4, 0, 0, &span
    ));
    RUDA_EXPECT(ruda_bytecode_program_take_function(
        &program, leaf_symbol, &leaf, &function_id
    ));
    span = 1000;
    RUDA_EXPECT(ruda_bytecode_function_init(
        &entry, ruda_system_allocator(), RUDA_BYTECODE_TYPE_I64,
        1, NULL, 0, ruda_bytecode_function_default_options()
    ));
    RUDA_EXPECT(ruda_test_vm_add_registers(
        &entry, entry_types, sizeof(entry_types) / sizeof(entry_types[0])
    ));
    RUDA_EXPECT(ruda_bytecode_add_call(
        &entry, leaf_symbol, RUDA_BYTECODE_TYPE_I64, NULL, 0, &call
    ));
    RUDA_EXPECT(ruda_bytecode_add_block(&entry, &block));
    RUDA_EXPECT(ruda_test_vm_append(
        &entry, block, RUDA_OPCODE_CALL_DIRECT, 0, call, 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &entry, block, RUDA_OPCODE_RETURN, 0, 0, 0, &span
    ));
    RUDA_EXPECT(ruda_bytecode_program_take_function(
        &program, entry_symbol, &entry, &function_id
    ));
    RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, entry_symbol));
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(), ruda_bytecode_execution_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
    options = ruda_bytecode_run_default_options();
    options.execution = &execution;
    options.panic = &panic;
    RUDA_EXPECT(ruda_bytecode_run_program(
        &program, NULL, 0, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_PANIC);
    RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
    RUDA_EXPECT(panic.message.length
        == strlen("array index -1 is outside length 1"));
    RUDA_EXPECT(memcmp(
        panic.message.data, "array index -1 is outside length 1",
        panic.message.length
    ) == 0);
    RUDA_EXPECT(memcmp(&panic.symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
        && panic.span.start == 983 && panic.span.end == 984
        && panic.frames.length == 2);
    frame = ruda_bytecode_panic_frame(&panic, 1);
    RUDA_EXPECT(frame != NULL
        && memcmp(&frame->symbol, &entry_symbol, sizeof(entry_symbol)) == 0
        && frame->call_span.start == 1000 && frame->call_span.end == 1001);
    RUDA_EXPECT(execution.managed_heap.roots == NULL);
    ruda_bytecode_panic_dispose(&panic);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_program_dispose(&program);
}

void ruda_test_bytecode_vm(void) {
    ruda_test_vm_all_opcodes_and_limits();
    ruda_test_vm_checked_arithmetic();
    ruda_test_vm_scalar_results();
    ruda_test_vm_bitwise_and_shifts();
    ruda_test_vm_f64();
    ruda_test_vm_strings_and_panic();
    ruda_test_vm_direct_calls();
    ruda_test_vm_managed_arrays();
    ruda_test_vm_array_feature_call_metadata();
    ruda_test_vm_checked_index_cfg();
    ruda_test_vm_array_rhs_ordering();
    ruda_test_vm_managed_array_calls();
    ruda_test_vm_managed_array_provenance();
    ruda_test_vm_managed_array_panic_frames();
    ruda_test_vm_rejection_round_trip_and_allocation();
}
