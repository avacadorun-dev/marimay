#include "test_bytecode_vm_support.h"


#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/bytecode/bytecode.h"
#include "ruda/bytecode/vm.h"

RudaBytecodeInstruction ruda_test_vm_instruction(
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

bool ruda_test_vm_append(
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

RudaBytecodeInstruction *ruda_test_vm_at(
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

bool ruda_test_vm_add_registers(
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
bool ruda_test_vm_build_all_opcodes(
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

bool ruda_test_vm_build_binary(
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

bool ruda_test_vm_build_i64_comparison(
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

bool ruda_test_vm_build_bool_operation(
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

bool ruda_test_vm_build_constants_and_copy(
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

bool ruda_test_vm_build_bool_constant_copy(
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

bool ruda_test_vm_build_loop(
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

bool ruda_test_vm_build_void(
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

bool ruda_test_vm_build_direct_constant(
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

bool ruda_test_vm_build_direct_forward(
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

bool ruda_test_vm_build_nested_program(
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

bool ruda_test_vm_build_recursive_program(
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

bool ruda_test_vm_build_negate(
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

RudaBytecodeValue ruda_test_vm_bool(bool value) {
    RudaBytecodeValue result;
    result.type = RUDA_BYTECODE_TYPE_BOOL;
    result.as.boolean = value;
    return result;
}

RudaBytecodeValue ruda_test_vm_i64(int64_t value) {
    RudaBytecodeValue result;
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = value;
    return result;
}

bool ruda_test_vm_build_f64(
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

bool ruda_test_vm_build_string_literal(
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

bool ruda_test_vm_build_string_equal(
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

bool ruda_test_vm_build_string_identity(
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
