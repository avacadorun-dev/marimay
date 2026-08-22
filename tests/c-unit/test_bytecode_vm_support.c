#include "test_bytecode_vm_support.h"


#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/bytecode/bytecode.h"
#include "mmay/bytecode/vm.h"

MarimayBytecodeInstruction mmay_test_vm_instruction(
    MarimayOpcode opcode,
    uint64_t first,
    uint64_t second,
    uint64_t third,
    uint64_t span_start
) {
    MarimayBytecodeInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = opcode;
    instruction.operands[0].unsigned_value = first;
    instruction.operands[1].unsigned_value = second;
    instruction.operands[2].unsigned_value = third;
    instruction.span.start = span_start;
    instruction.span.end = span_start + 1;
    return instruction;
}

bool mmay_test_vm_append(
    MarimayBytecodeFunction *function,
    uint32_t block,
    MarimayOpcode opcode,
    uint64_t first,
    uint64_t second,
    uint64_t third,
    uint64_t *span_start
) {
    MarimayBytecodeInstruction instruction = mmay_test_vm_instruction(
        opcode,
        first,
        second,
        third,
        *span_start
    );
    if (opcode == MMAY_OPCODE_CONST_I64) {
        instruction.operands[1].signed_value = (int64_t)second;
    }
    *span_start += 1;
    return mmay_bytecode_append_instruction(function, block, &instruction);
}

MarimayBytecodeInstruction *mmay_test_vm_at(
    MarimayBytecodeFunction *function,
    size_t block,
    size_t instruction
) {
    MarimayBytecodeBlock *target = function != NULL
        ? mmay_vector_at(&function->blocks, block)
        : NULL;
    return target != NULL
        ? mmay_vector_at(&target->instructions, instruction)
        : NULL;
}

bool mmay_test_vm_add_registers(
    MarimayBytecodeFunction *function,
    const MarimayBytecodeType *types,
    size_t count
) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        uint32_t register_id = UINT32_MAX;
        if (!mmay_bytecode_add_register(function, types[index], &register_id)) {
            return false;
        }
    }
    return true;
}

/* Both branch outcomes exercise a distinct terminator path; every opcode runs. */
bool mmay_test_vm_build_all_opcodes(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator
) {
    static const MarimayBytecodeType parameters[] = {
        MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64
    };
    static const MarimayBytecodeType types[] = {
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_BOOL,
        MMAY_BYTECODE_TYPE_BOOL,
        MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_I64
    };
    uint32_t blocks[4];
    uint64_t span_start = 0;
    MarimayBytecodeFunctionOptions options = mmay_bytecode_function_default_options();
    bool success = false;

    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(function, allocator, MMAY_BYTECODE_TYPE_I64,
            1, parameters, sizeof(parameters) / sizeof(parameters[0]), options)
        || !mmay_test_vm_add_registers(function, types, sizeof(types) / sizeof(types[0]))
        || !mmay_bytecode_add_block(function, &blocks[0])
        || !mmay_bytecode_add_block(function, &blocks[1])
        || !mmay_bytecode_add_block(function, &blocks[2])
        || !mmay_bytecode_add_block(function, &blocks[3])) {
        goto cleanup;
    }
    if (!mmay_test_vm_append(function, 0, MMAY_OPCODE_CONST_I64, 3, 1, 0,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_CONST_BOOL, 4, 0, 0,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_COPY, 5, 1, 0,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_ADD_CHECKED, 6, 5, 2,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_SUBTRACT_CHECKED, 7, 6, 2,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_MULTIPLY_CHECKED, 8, 7, 3,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_DIVIDE_CHECKED, 9, 8, 3,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_REMAINDER_CHECKED, 10, 9, 3,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_NEGATE_CHECKED, 11, 10, 0,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_EQUAL, 12, 1, 2,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_BOOL_NOT, 13, 12, 0,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_BOOL_EQUAL, 14, 13, 4,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_LESS, 15, 2, 1,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_LESS_EQUAL, 16, 2, 1,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_BITWISE_NOT, 17, 1, 0,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_BITWISE_AND, 18, 1, 2,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_BITWISE_XOR, 19, 1, 2,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_BITWISE_OR, 20, 1, 2,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED,
            21, 3, 3, &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_I64_SHIFT_RIGHT, 22, 1, 3,
            &span_start)
        || !mmay_test_vm_append(function, 0, MMAY_OPCODE_BRANCH_BOOL, 0, 1, 2,
            &span_start)
        || !mmay_test_vm_append(function, 1, MMAY_OPCODE_RETURN, 6, 0, 0,
            &span_start)
        || !mmay_test_vm_append(function, 2, MMAY_OPCODE_JUMP, 3, 0, 0,
            &span_start)
        || !mmay_test_vm_append(function, 3, MMAY_OPCODE_RETURN, 6, 0, 0,
            &span_start)) {
        goto cleanup;
    }
    success = true;

cleanup:
    if (!success) {
        mmay_bytecode_function_dispose(function);
    }
    return success;
}

bool mmay_test_vm_build_binary(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayOpcode opcode
) {
    static const MarimayBytecodeType parameters[] = {
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64
    };
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 10;
    MarimayBytecodeFunctionOptions options = mmay_bytecode_function_default_options();

    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(function, allocator, MMAY_BYTECODE_TYPE_I64,
            1, parameters, sizeof(parameters) / sizeof(parameters[0]), options)
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_I64, &result)
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(function, block, opcode, result, 0, 1, &span_start)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_RETURN, result, 0, 0,
            &span_start)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_i64_comparison(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayOpcode opcode
) {
    static const MarimayBytecodeType parameters[] = {
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_I64
    };
    MarimayBytecodeFunctionOptions options = mmay_bytecode_function_default_options();
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 30;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(
            function,
            allocator,
            MMAY_BYTECODE_TYPE_BOOL,
            1,
            parameters,
            2,
            options
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_BOOL,
            &result
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function,
            block,
            opcode,
            result,
            0,
            1,
            &span_start
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RETURN,
            result,
            0,
            0,
            &span_start
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_bool_operation(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayOpcode opcode
) {
    static const MarimayBytecodeType parameters[] = {
        MMAY_BYTECODE_TYPE_BOOL, MMAY_BYTECODE_TYPE_BOOL
    };
    MarimayBytecodeFunctionOptions options = mmay_bytecode_function_default_options();
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 40;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(
            function,
            allocator,
            MMAY_BYTECODE_TYPE_BOOL,
            1,
            parameters,
            2,
            options
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_BOOL,
            &result
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function,
            block,
            opcode,
            result,
            0,
            1,
            &span_start
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RETURN,
            result,
            0,
            0,
            &span_start
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_constants_and_copy(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator
) {
    MarimayBytecodeFunctionOptions options = mmay_bytecode_function_default_options();
    uint32_t value = UINT32_MAX;
    uint32_t copy = UINT32_MAX;
    uint32_t condition = UINT32_MAX;
    uint32_t blocks[2];
    uint64_t span_start = 50;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(
            function,
            allocator,
            MMAY_BYTECODE_TYPE_I64,
            1,
            NULL,
            0,
            options
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_I64,
            &value
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_I64,
            &copy
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_BOOL,
            &condition
        ) || !mmay_bytecode_add_block(function, &blocks[0])
        || !mmay_bytecode_add_block(function, &blocks[1])
        || !mmay_test_vm_append(
            function,
            blocks[0],
            MMAY_OPCODE_CONST_I64,
            value,
            42,
            0,
            &span_start
        ) || !mmay_test_vm_append(
            function,
            blocks[0],
            MMAY_OPCODE_COPY,
            copy,
            value,
            0,
            &span_start
        ) || !mmay_test_vm_append(
            function,
            blocks[0],
            MMAY_OPCODE_CONST_BOOL,
            condition,
            1,
            0,
            &span_start
        ) || !mmay_test_vm_append(
            function,
            blocks[0],
            MMAY_OPCODE_BRANCH_BOOL,
            condition,
            blocks[1],
            blocks[1],
            &span_start
        ) || !mmay_test_vm_append(
            function,
            blocks[1],
            MMAY_OPCODE_RETURN,
            copy,
            0,
            0,
            &span_start
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_bool_constant_copy(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    bool value
) {
    MarimayBytecodeFunctionOptions options = mmay_bytecode_function_default_options();
    uint32_t constant = UINT32_MAX;
    uint32_t copy = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 60;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(
            function,
            allocator,
            MMAY_BYTECODE_TYPE_BOOL,
            1,
            NULL,
            0,
            options
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_BOOL,
            &constant
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_BOOL,
            &copy
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_CONST_BOOL,
            constant,
            value ? 1 : 0,
            0,
            &span_start
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_COPY,
            copy,
            constant,
            0,
            &span_start
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RETURN,
            copy,
            0,
            0,
            &span_start
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_loop(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator
) {
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 0;
    MarimayBytecodeFunctionOptions options = mmay_bytecode_function_default_options();

    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(function, allocator, MMAY_BYTECODE_TYPE_VOID,
            1, NULL, 0, options)
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_JUMP, block, 0, 0,
            &span_start)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_void(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator
) {
    MarimayBytecodeFunctionOptions options = mmay_bytecode_function_default_options();
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 0;

    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(function, allocator, MMAY_BYTECODE_TYPE_VOID,
            1, NULL, 0, options)
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_RETURN_VOID, 0, 0, 0,
            &span_start)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_direct_constant(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    int64_t value,
    uint64_t span_start
) {
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    MarimayBytecodeInstruction constant;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(function, allocator, MMAY_BYTECODE_TYPE_I64,
            1, NULL, 0, mmay_bytecode_function_default_options())
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_I64, &result)
        || !mmay_bytecode_add_block(function, &block)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    constant = mmay_test_vm_instruction(MMAY_OPCODE_CONST_I64, result, 0, 0,
        span_start);
    constant.operands[1].signed_value = value;
    span_start += 1;
    if (!mmay_bytecode_append_instruction(function, block, &constant)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_RETURN, result, 0,
            0, &span_start)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_direct_forward(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimaySymbolId callee,
    uint64_t span_start
) {
    MarimayBytecodeCallId call = MMAY_BYTECODE_NO_CALL;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(function, allocator, MMAY_BYTECODE_TYPE_I64,
            1, NULL, 0, mmay_bytecode_function_default_options())
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_I64, &result)
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_bytecode_add_call(function, callee, MMAY_BYTECODE_TYPE_I64,
            NULL, 0, &call)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_CALL_DIRECT, result,
            call, 0, &span_start)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_RETURN, result, 0,
            0, &span_start)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_nested_program(
    MarimayBytecodeProgram *program,
    MarimayAllocator allocator
) {
    const MarimaySymbolId leaf = {{0x10}};
    const MarimaySymbolId middle = {{0x20}};
    const MarimaySymbolId entry = {{0x30}};
    MarimayBytecodeFunction function;
    uint32_t id = UINT32_MAX;
    memset(program, 0, sizeof(*program));
    memset(&function, 0, sizeof(function));
    if (!mmay_bytecode_program_init(program, allocator, 1,
            mmay_bytecode_program_default_options())
        || !mmay_test_vm_build_direct_constant(&function, allocator, 42, 50)
        || !mmay_bytecode_program_take_function(program, leaf, &function, &id)
        || id != 0
        || !mmay_test_vm_build_direct_forward(&function, allocator, leaf, 40)
        || !mmay_bytecode_program_take_function(program, middle, &function, &id)
        || id != 1
        || !mmay_test_vm_build_direct_forward(&function, allocator, middle, 30)
        || !mmay_bytecode_program_take_function(program, entry, &function, &id)
        || id != 2 || !mmay_bytecode_program_set_entry(program, entry)) {
        mmay_bytecode_function_dispose(&function);
        mmay_bytecode_program_dispose(program);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_recursive_program(
    MarimayBytecodeProgram *program,
    MarimayAllocator allocator
) {
    const MarimaySymbolId symbol = {{0x10}};
    static const MarimayBytecodeType parameters[] = {MMAY_BYTECODE_TYPE_I64};
    MarimayBytecodeFunction function;
    MarimayBytecodeCallArgument argument;
    MarimayBytecodeCallId call = MMAY_BYTECODE_NO_CALL;
    uint32_t one = UINT32_MAX;
    uint32_t base_case = UINT32_MAX;
    uint32_t decremented = UINT32_MAX;
    uint32_t recursive_result = UINT32_MAX;
    uint32_t blocks[3] = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    uint32_t id = UINT32_MAX;
    uint64_t span = 120;
    memset(program, 0, sizeof(*program));
    memset(&function, 0, sizeof(function));
    if (!mmay_bytecode_function_init(
            &function,
            allocator,
            MMAY_BYTECODE_TYPE_I64,
            1,
            parameters,
            1,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            &function,
            MMAY_BYTECODE_TYPE_I64,
            &one
        ) || !mmay_bytecode_add_register(
            &function,
            MMAY_BYTECODE_TYPE_BOOL,
            &base_case
        ) || !mmay_bytecode_add_register(
            &function,
            MMAY_BYTECODE_TYPE_I64,
            &decremented
        ) || !mmay_bytecode_add_register(
            &function,
            MMAY_BYTECODE_TYPE_I64,
            &recursive_result
        ) || !mmay_bytecode_add_block(&function, &blocks[0])
        || !mmay_bytecode_add_block(&function, &blocks[1])
        || !mmay_bytecode_add_block(&function, &blocks[2])) {
        goto cleanup;
    }
    argument.register_id = decremented;
    argument.type = MMAY_BYTECODE_TYPE_I64;
    if (!mmay_bytecode_add_call(
            &function,
            symbol,
            MMAY_BYTECODE_TYPE_I64,
            &argument,
            1,
            &call
        ) || !mmay_test_vm_append(
            &function,
            blocks[0],
            MMAY_OPCODE_CONST_I64,
            one,
            1,
            0,
            &span
        ) || !mmay_test_vm_append(
            &function,
            blocks[0],
            MMAY_OPCODE_I64_LESS_EQUAL,
            base_case,
            0,
            one,
            &span
        ) || !mmay_test_vm_append(
            &function,
            blocks[0],
            MMAY_OPCODE_BRANCH_BOOL,
            base_case,
            blocks[1],
            blocks[2],
            &span
        ) || !mmay_test_vm_append(
            &function,
            blocks[1],
            MMAY_OPCODE_RETURN,
            one,
            0,
            0,
            &span
        ) || !mmay_test_vm_append(
            &function,
            blocks[2],
            MMAY_OPCODE_I64_SUBTRACT_CHECKED,
            decremented,
            0,
            one,
            &span
        ) || !mmay_test_vm_append(
            &function,
            blocks[2],
            MMAY_OPCODE_CALL_DIRECT,
            recursive_result,
            call,
            0,
            &span
        ) || !mmay_test_vm_append(
            &function,
            blocks[2],
            MMAY_OPCODE_RETURN,
            recursive_result,
            0,
            0,
            &span
        ) || !mmay_bytecode_program_init(
            program,
            allocator,
            1,
            mmay_bytecode_program_default_options()
        ) || !mmay_bytecode_program_take_function(
            program,
            symbol,
            &function,
            &id
        ) || id != 0 || !mmay_bytecode_program_set_entry(program, symbol)) {
        goto cleanup;
    }
    return true;

cleanup:
    mmay_bytecode_function_dispose(&function);
    mmay_bytecode_program_dispose(program);
    return false;
}

bool mmay_test_vm_build_negate(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator
) {
    static const MarimayBytecodeType parameters[] = {MMAY_BYTECODE_TYPE_I64};
    MarimayBytecodeFunctionOptions options = mmay_bytecode_function_default_options();
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span_start = 20;

    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(function, allocator, MMAY_BYTECODE_TYPE_I64,
            1, parameters, 1, options)
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_I64, &result)
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_I64_NEGATE_CHECKED,
            result, 0, 0, &span_start)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_RETURN, result, 0, 0,
            &span_start)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

MarimayBytecodeValue mmay_test_vm_bool(bool value) {
    MarimayBytecodeValue result;
    result.type = MMAY_BYTECODE_TYPE_BOOL;
    result.as.boolean = value;
    return result;
}

MarimayBytecodeValue mmay_test_vm_i64(int64_t value) {
    MarimayBytecodeValue result;
    result.type = MMAY_BYTECODE_TYPE_I64;
    result.as.i64 = value;
    return result;
}

bool mmay_test_vm_build_f64(
    MarimayBytecodeFunction *function,
    MarimayOpcode opcode,
    uint64_t left_bits,
    uint64_t right_bits,
    MarimayBytecodeType result_type
) {
    uint32_t left = UINT32_MAX;
    uint32_t right = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 500;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(function, mmay_system_allocator(), result_type,
            1, NULL, 0, mmay_bytecode_function_default_options())
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_F64, &left)
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_F64, &right)
        || !mmay_bytecode_add_register(function, result_type, &result)
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_CONST_F64, left,
            left_bits, 0, &span)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_CONST_F64, right,
            right_bits, 0, &span)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    if (opcode == MMAY_OPCODE_F64_NEGATE) {
        if (!mmay_test_vm_append(function, block, opcode, result, left, 0, &span)) {
            mmay_bytecode_function_dispose(function);
            return false;
        }
    } else if (!mmay_test_vm_append(function, block, opcode, result, left, right,
            &span)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    if (!mmay_test_vm_append(function, block, MMAY_OPCODE_RETURN, result, 0, 0,
            &span)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_string_literal(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    const unsigned char *bytes,
    size_t length,
    bool panic
) {
    MarimayBytecodeStringId string_id = MMAY_BYTECODE_NO_STRING;
    uint32_t value = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 140;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(function, allocator,
            panic ? MMAY_BYTECODE_TYPE_NEVER : MMAY_BYTECODE_TYPE_STRING,
            1, NULL, 0, mmay_bytecode_function_default_options())
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_STRING,
            &value)
        || !mmay_bytecode_add_string_constant(function, bytes, length, &string_id)
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_CONST_STRING,
            value, string_id, 0, &span)
        || !mmay_test_vm_append(function, block,
            panic ? MMAY_OPCODE_PANIC_STRING : MMAY_OPCODE_RETURN,
            value, 0, 0, &span)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_string_equal(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    const unsigned char *left,
    size_t left_length,
    const unsigned char *right,
    size_t right_length
) {
    MarimayBytecodeStringId left_id = MMAY_BYTECODE_NO_STRING;
    MarimayBytecodeStringId right_id = MMAY_BYTECODE_NO_STRING;
    uint32_t left_register = UINT32_MAX;
    uint32_t right_register = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 150;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(function, allocator, MMAY_BYTECODE_TYPE_BOOL,
            1, NULL, 0, mmay_bytecode_function_default_options())
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_STRING,
            &left_register)
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_STRING,
            &right_register)
        || !mmay_bytecode_add_register(function, MMAY_BYTECODE_TYPE_BOOL, &result)
        || !mmay_bytecode_add_string_constant(function, left, left_length, &left_id)
        || !mmay_bytecode_add_string_constant(function, right, right_length, &right_id)
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_CONST_STRING,
            left_register, left_id, 0, &span)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_CONST_STRING,
            right_register, right_id, 0, &span)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_STRING_EQUAL,
            result, left_register, right_register, &span)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_RETURN,
            result, 0, 0, &span)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool mmay_test_vm_build_string_identity(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator
) {
    static const MarimayBytecodeType parameters[] = {MMAY_BYTECODE_TYPE_STRING};
    uint32_t block = UINT32_MAX;
    uint64_t span = 170;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(function, allocator,
            MMAY_BYTECODE_TYPE_STRING, 1, parameters, 1,
            mmay_bytecode_function_default_options())
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(function, block, MMAY_OPCODE_RETURN,
            0, 0, 0, &span)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}
