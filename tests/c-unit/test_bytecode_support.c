#include "test_bytecode_support.h"

RudaBytecodeInstruction ruda_test_bytecode_instruction(
    RudaOpcode opcode,
    uint64_t first,
    uint64_t second,
    uint64_t third,
    size_t span_index
) {
    RudaBytecodeInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = opcode;
    instruction.operands[0].unsigned_value = first;
    instruction.operands[1].unsigned_value = second;
    instruction.operands[2].unsigned_value = third;
    instruction.span.source = 0;
    instruction.span.start = span_index;
    instruction.span.end = span_index + 1;
    return instruction;
}

bool ruda_test_bytecode_append(
    RudaBytecodeFunction *function,
    uint32_t block,
    RudaOpcode opcode,
    uint64_t first,
    uint64_t second,
    uint64_t third,
    size_t *span_index
) {
    RudaBytecodeInstruction instruction = ruda_test_bytecode_instruction(
        opcode,
        first,
        second,
        third,
        *span_index
    );
    if (opcode == RUDA_OPCODE_CONST_I64) {
        instruction.operands[1].signed_value = (int64_t)second;
    }
    *span_index += 1;
    return ruda_bytecode_append_instruction(function, block, &instruction);
}

bool ruda_test_bytecode_read_u64(
    const unsigned char *bytes,
    size_t length,
    size_t *offset,
    uint64_t *result
) {
    uint64_t value = 0;
    size_t index = 0;
    if (bytes == NULL || offset == NULL || result == NULL) {
        return false;
    }
    for (index = 0; index < 10 && *offset < length; index += 1) {
        unsigned char byte = bytes[*offset];
        *offset += 1;
        value |= (uint64_t)(byte & 0x7fU) << (index * 7U);
        if ((byte & 0x80U) == 0) {
            *result = value;
            return true;
        }
    }
    return false;
}

/* Builds a valid CFG containing every non-void executable opcode exactly once. */
bool ruda_test_bytecode_build_full(
    RudaBytecodeFunction *function,
    RudaAllocator allocator
) {
    static const RudaBytecodeType types[] = {
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_BOOL, RUDA_BYTECODE_TYPE_BOOL,
        RUDA_BYTECODE_TYPE_BOOL, RUDA_BYTECODE_TYPE_BOOL,
        RUDA_BYTECODE_TYPE_BOOL,
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64
    };
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();
    uint32_t blocks[3];
    size_t type_index = 0;
    size_t span_index = 0;
    bool success = false;

    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(
            function,
            allocator,
            RUDA_BYTECODE_TYPE_I64,
            1,
            NULL,
            0,
            options
        )) {
        return false;
    }
    for (type_index = 0; type_index < sizeof(types) / sizeof(types[0]);
        type_index += 1) {
        uint32_t register_id = UINT32_MAX;
        if (!ruda_bytecode_add_register(function, types[type_index], &register_id)
            || register_id != type_index) {
            goto cleanup;
        }
    }
    if (!ruda_bytecode_add_block(function, &blocks[0])
        || !ruda_bytecode_add_block(function, &blocks[1])
        || !ruda_bytecode_add_block(function, &blocks[2])
        || blocks[0] != 0 || blocks[1] != 1 || blocks[2] != 2) {
        goto cleanup;
    }
    if (!ruda_test_bytecode_append(function, 0, RUDA_OPCODE_CONST_I64, 0, 40, 0,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_CONST_I64, 1, 2, 0,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_CONST_BOOL, 10, 1, 0,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_COPY, 2, 0, 0,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_ADD_CHECKED, 3, 0, 1,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_SUBTRACT_CHECKED, 4, 3, 1,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_MULTIPLY_CHECKED, 5, 4, 1,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_DIVIDE_CHECKED, 6, 5, 1,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_REMAINDER_CHECKED, 7, 6, 1,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_NEGATE_CHECKED, 8, 7, 0,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_EQUAL, 11, 0, 1,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_LESS, 12, 1, 0,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_LESS_EQUAL, 13, 1, 0,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_BOOL_NOT, 14, 11, 0,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_BOOL_EQUAL, 10, 10, 14,
            &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_BITWISE_NOT,
            15, 0, 0, &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_BITWISE_AND,
            16, 0, 1, &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_BITWISE_XOR,
            17, 0, 1, &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_BITWISE_OR,
            18, 0, 1, &span_index)
        || !ruda_test_bytecode_append(function, 0,
            RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED, 19, 1, 1, &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_I64_SHIFT_RIGHT,
            20, 0, 1, &span_index)
        || !ruda_test_bytecode_append(function, 0, RUDA_OPCODE_BRANCH_BOOL, 10, 1, 2,
            &span_index)
        || !ruda_test_bytecode_append(function, 1, RUDA_OPCODE_RETURN, 3, 0, 0,
            &span_index)
        || !ruda_test_bytecode_append(function, 2, RUDA_OPCODE_JUMP, 1, 0, 0,
            &span_index)) {
        goto cleanup;
    }
    success = true;

cleanup:
    if (!success) {
        ruda_bytecode_function_dispose(function);
    }
    return success;
}

bool ruda_test_bytecode_build_void(
    RudaBytecodeFunction *function,
    RudaAllocator allocator
) {
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();
    uint32_t block = UINT32_MAX;
    size_t span_index = 0;

    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(
            function,
            allocator,
            RUDA_BYTECODE_TYPE_VOID,
            1,
            NULL,
            0,
            options
        ) || !ruda_bytecode_add_block(function, &block)
        || block != 0
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_RETURN_VOID,
            0, 0, 0, &span_index)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool ruda_test_bytecode_build_constant(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    int64_t value
) {
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();
    uint32_t register_id = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    RudaBytecodeInstruction constant;
    size_t span_index = 0;

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
            &register_id
        ) || !ruda_bytecode_add_block(function, &block)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    constant = ruda_test_bytecode_instruction(
        RUDA_OPCODE_CONST_I64,
        register_id,
        0,
        0,
        span_index
    );
    constant.operands[1].signed_value = value;
    span_index += 1;
    if (!ruda_bytecode_append_instruction(function, block, &constant)
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_RETURN,
            register_id, 0, 0, &span_index)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool ruda_test_bytecode_build_direct_callee(
    RudaBytecodeFunction *function,
    RudaAllocator allocator
) {
    static const RudaBytecodeType parameters[] = {
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64
    };
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 40;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_I64,
            1, parameters, 2, ruda_bytecode_function_default_options())
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_I64, &result)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_CONST_I64,
            result, 42, 0, &span)
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_RETURN,
            result, 0, 0, &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool ruda_test_bytecode_build_direct_caller(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaSymbolId callee
) {
    static const RudaBytecodeType parameters[] = {
        RUDA_BYTECODE_TYPE_I64, RUDA_BYTECODE_TYPE_I64
    };
    RudaBytecodeCallArgument arguments[2];
    RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 30;
    memset(function, 0, sizeof(*function));
    arguments[0].register_id = 1;
    arguments[0].type = RUDA_BYTECODE_TYPE_I64;
    arguments[1].register_id = 0;
    arguments[1].type = RUDA_BYTECODE_TYPE_I64;
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_I64,
            1, parameters, 2, ruda_bytecode_function_default_options())
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_I64, &result)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_bytecode_add_call(function, callee, RUDA_BYTECODE_TYPE_I64,
            arguments, 2, &call)
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_CALL_DIRECT,
            result, call, 0, &span)
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_RETURN,
            result, 0, 0, &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool ruda_test_bytecode_build_string_constant(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    const unsigned char *bytes,
    size_t length
) {
    RudaBytecodeStringId string_id = RUDA_BYTECODE_NO_STRING;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 70;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_STRING,
            1, NULL, 0, ruda_bytecode_function_default_options())
        || !ruda_bytecode_add_register(function, RUDA_BYTECODE_TYPE_STRING, &result)
        || !ruda_bytecode_add_string_constant(function, bytes, length, &string_id)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_CONST_STRING,
            result, string_id, 0, &span)
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_RETURN,
            result, 0, 0, &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool ruda_test_bytecode_build_string_equality(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    bool assign_second
) {
    static const unsigned char value[] = "x";
    RudaBytecodeStringId string_id = RUDA_BYTECODE_NO_STRING;
    uint32_t registers[4];
    uint32_t block = UINT32_MAX;
    size_t span = 80;
    size_t index = 0;
    static const RudaBytecodeType types[] = {
        RUDA_BYTECODE_TYPE_STRING, RUDA_BYTECODE_TYPE_STRING,
        RUDA_BYTECODE_TYPE_BOOL, RUDA_BYTECODE_TYPE_BOOL
    };
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_BOOL,
            1, NULL, 0, ruda_bytecode_function_default_options())
        || !ruda_bytecode_add_string_constant(function, value, 1, &string_id)
        || !ruda_bytecode_add_block(function, &block)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    for (index = 0; index < sizeof(types) / sizeof(types[0]); index += 1) {
        if (!ruda_bytecode_add_register(function, types[index], &registers[index])) {
            ruda_bytecode_function_dispose(function);
            return false;
        }
    }
    if (!ruda_test_bytecode_append(function, block, RUDA_OPCODE_CONST_STRING,
            registers[0], string_id, 0, &span)
        || (assign_second && !ruda_test_bytecode_append(function, block,
            RUDA_OPCODE_CONST_STRING, registers[1], string_id, 0, &span))
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_CONST_BOOL,
            registers[3], 1, 0, &span)
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_STRING_EQUAL,
            registers[2], registers[0], registers[1], &span)
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_RETURN,
            registers[2], 0, 0, &span)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool ruda_test_bytecode_build_panic_string(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaBytecodeType type,
    bool assign,
    bool nonterminal
) {
    static const unsigned char value[] = "x";
    RudaBytecodeStringId string_id = RUDA_BYTECODE_NO_STRING;
    uint32_t register_id = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 90;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(function, allocator, RUDA_BYTECODE_TYPE_NEVER,
            1, NULL, 0, ruda_bytecode_function_default_options())
        || !ruda_bytecode_add_register(function, type, &register_id)
        || !ruda_bytecode_add_block(function, &block)
        || (type == RUDA_BYTECODE_TYPE_STRING && !ruda_bytecode_add_string_constant(
            function, value, 1, &string_id))
        || (assign && !ruda_test_bytecode_append(function, block,
            type == RUDA_BYTECODE_TYPE_STRING ? RUDA_OPCODE_CONST_STRING
                : RUDA_OPCODE_CONST_BOOL,
            register_id, type == RUDA_BYTECODE_TYPE_STRING ? string_id : 1,
            0, &span))
        || !ruda_test_bytecode_append(function, block, RUDA_OPCODE_PANIC_STRING,
            register_id, 0, 0, &span)
        || (nonterminal && !ruda_test_bytecode_append(function, block,
            RUDA_OPCODE_RETURN_VOID, 0, 0, 0, &span))) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

bool ruda_test_bytecode_build_direct_program(
    RudaBytecodeProgram *program,
    RudaAllocator allocator
) {
    const RudaSymbolId callee_symbol = {{0x10}};
    const RudaSymbolId caller_symbol = {{0x20}};
    RudaBytecodeFunction callee;
    RudaBytecodeFunction caller;
    uint32_t id = UINT32_MAX;
    memset(program, 0, sizeof(*program));
    memset(&callee, 0, sizeof(callee));
    memset(&caller, 0, sizeof(caller));
    if (!ruda_bytecode_program_init(
            program,
            allocator,
            1,
            ruda_bytecode_program_default_options()
        ) || !ruda_test_bytecode_build_direct_callee(&callee, allocator)
        || !ruda_bytecode_program_take_function(
            program,
            callee_symbol,
            &callee,
            &id
        ) || id != 0
        || !ruda_test_bytecode_build_direct_caller(
            &caller,
            allocator,
            callee_symbol
        ) || !ruda_bytecode_program_take_function(
            program,
            caller_symbol,
            &caller,
            &id
        ) || id != 1
        || !ruda_bytecode_program_set_entry(program, caller_symbol)) {
        ruda_bytecode_function_dispose(&caller);
        ruda_bytecode_function_dispose(&callee);
        ruda_bytecode_program_dispose(program);
        return false;
    }
    return true;
}

RudaBytecodeInstruction *ruda_test_bytecode_at(
    RudaBytecodeFunction *function,
    size_t block,
    size_t instruction
) {
    RudaBytecodeBlock *target_block = ruda_vector_at(&function->blocks, block);
    if (target_block == NULL) {
        return NULL;
    }
    return ruda_vector_at(&target_block->instructions, instruction);
}
