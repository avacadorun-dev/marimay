#include "test_support.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/bytecode/bytecode.h"
#include "ruda/bytecode/program.h"

typedef struct RudaTestOpcodeExpectation {
    RudaOpcode opcode;
    uint8_t wire_id;
    const char *mnemonic;
    uint8_t operand_count;
    RudaBytecodeOperandKind operands[3];
    uint32_t flags;
} RudaTestOpcodeExpectation;

static RudaBytecodeInstruction ruda_test_bytecode_instruction(
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

static bool ruda_test_bytecode_append(
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

static bool ruda_test_bytecode_read_u64(
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
static bool ruda_test_bytecode_build_full(
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

static bool ruda_test_bytecode_build_void(
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

static bool ruda_test_bytecode_build_constant(
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

static bool ruda_test_bytecode_build_direct_callee(
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

static bool ruda_test_bytecode_build_direct_caller(
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

static bool ruda_test_bytecode_build_string_constant(
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

static bool ruda_test_bytecode_build_string_equality(
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

static bool ruda_test_bytecode_build_panic_string(
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

static bool ruda_test_bytecode_build_direct_program(
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

static RudaBytecodeInstruction *ruda_test_bytecode_at(
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

static void ruda_test_bytecode_metadata(void) {
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
            {RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER, RUDA_BYTECODE_OPERAND_REGISTER}, RUDA_OPCODE_FLAG_MAY_PANIC}
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

static void ruda_test_bytecode_valid_and_disassembly(void) {
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

static void ruda_test_bytecode_codec(void) {
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

static void ruda_test_bytecode_f64_codec_and_verifier(void) {
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

static void ruda_test_bytecode_decode_rejections(void) {
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
    /* Feature bits 0..3 are calls/strings/f64/arrays; bit four is unknown. */
    mutated[9] = 16;
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

static void ruda_test_bytecode_verifier_rejections(void) {
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

static void ruda_test_bytecode_calls_and_program(void) {
    const RudaSymbolId callee_symbol = {{0x10}};
    const RudaSymbolId caller_symbol = {{0x20}};
    RudaBytecodeFunction caller;
    RudaBytecodeFunction callee;
    RudaBytecodeFunction decoded_function;
    RudaBytecodeProgram program;
    RudaBytecodeProgram decoded_program;
    RudaOwnedBytes function_bytes;
    RudaOwnedBytes program_bytes;
    RudaOwnedBytes program_again;
    RudaTestBuffer buffer;
    RudaAllocator allocator = ruda_system_allocator();
    size_t index = 0;

    memset(&caller, 0, sizeof(caller));
    memset(&callee, 0, sizeof(callee));
    memset(&decoded_function, 0, sizeof(decoded_function));
    memset(&program, 0, sizeof(program));
    memset(&decoded_program, 0, sizeof(decoded_program));
    memset(&function_bytes, 0, sizeof(function_bytes));
    memset(&program_bytes, 0, sizeof(program_bytes));
    memset(&program_again, 0, sizeof(program_again));
    RUDA_EXPECT(ruda_test_bytecode_build_direct_caller(
        &caller, ruda_system_allocator(), callee_symbol));
    RUDA_EXPECT(ruda_bytecode_verify(&caller) == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(caller.calls.length == 1 && caller.call_arguments.length == 2);
    RUDA_EXPECT(ruda_bytecode_encode_function(&caller, ruda_system_allocator(),
        &function_bytes) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(function_bytes.length > 10 && function_bytes.data[9] == 1);
    for (index = 0; index < function_bytes.length; index += 1) {
        RUDA_EXPECT(ruda_bytecode_decode_function(function_bytes.data, index,
            ruda_system_allocator(), ruda_bytecode_function_default_options(),
            &decoded_function) == RUDA_BYTECODE_CODEC_TRUNCATED);
        RUDA_EXPECT(!decoded_function.initialized);
    }
    RUDA_EXPECT(ruda_bytecode_decode_function(function_bytes.data,
        function_bytes.length, ruda_system_allocator(),
        ruda_bytecode_function_default_options(), &decoded_function)
        == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(decoded_function.calls.length == 1
        && decoded_function.call_arguments.length == 2);
    ruda_bytecode_function_dispose(&decoded_function);
    ruda_test_buffer_init(&buffer);
    RUDA_EXPECT(ruda_bytecode_disassemble(&caller, ruda_test_buffer_writer(&buffer))
        == RUDA_BYTECODE_DISASSEMBLE_OK);
    RUDA_EXPECT(strstr(buffer.bytes, "call_direct r2, c0 @s0:30..31\n") != NULL);

    {
        RudaBytecodeInstruction *call_instruction =
            ruda_test_bytecode_at(&caller, 0, 0);
        RudaBytecodeCall *call = ruda_vector_at(&caller.calls, 0);
        RudaBytecodeCallArgument *argument = ruda_vector_at(
            &caller.call_arguments, 0);
        RUDA_EXPECT(call_instruction != NULL && call != NULL && argument != NULL);
        if (call_instruction != NULL) {
            call_instruction->operands[1].unsigned_value = 1;
            RUDA_EXPECT(ruda_bytecode_verify(&caller)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            call_instruction->operands[1].unsigned_value = 0;
        }
        if (argument != NULL) {
            argument->register_id = 9;
            RUDA_EXPECT(ruda_bytecode_verify(&caller)
                == RUDA_BYTECODE_VERIFY_INVALID_REGISTER);
            argument->register_id = 1;
            argument->type = RUDA_BYTECODE_TYPE_BOOL;
            RUDA_EXPECT(ruda_bytecode_verify(&caller)
                == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
            argument->type = RUDA_BYTECODE_TYPE_I64;
        }
        if (call != NULL) {
            call->argument_start = 1;
            RUDA_EXPECT(ruda_bytecode_verify(&caller)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            call->argument_start = 0;
        }
    }

    RUDA_EXPECT(ruda_test_bytecode_build_direct_callee(
        &callee, ruda_system_allocator()));
    RUDA_EXPECT(ruda_bytecode_program_init(&program, ruda_system_allocator(), 1,
        ruda_bytecode_program_default_options()));
    {
        uint32_t function_id = UINT32_MAX;
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, callee_symbol,
            &callee, &function_id));
        RUDA_EXPECT(function_id == 0);
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, caller_symbol,
            &caller, &function_id));
        RUDA_EXPECT(function_id == 1);
    }
    RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, caller_symbol));
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    program.options.max_verify_work = 29;
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
    program.options.max_verify_work = 30;
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    program.options.max_verify_work =
        ruda_bytecode_program_default_options().max_verify_work;
    RUDA_EXPECT(ruda_bytecode_program_function_by_symbol(&program, callee_symbol)
        == ruda_bytecode_program_function(&program, 0));
    RUDA_EXPECT(ruda_bytecode_encode_program(&program, ruda_system_allocator(),
        &program_bytes) == RUDA_BYTECODE_CODEC_OK);
    for (index = 0; index < program_bytes.length; index += 1) {
        RUDA_EXPECT(ruda_bytecode_decode_program(program_bytes.data, index,
            ruda_system_allocator(), ruda_bytecode_program_default_options(),
            &decoded_program) == RUDA_BYTECODE_CODEC_TRUNCATED);
        RUDA_EXPECT(!decoded_program.initialized);
    }
    RUDA_EXPECT(ruda_bytecode_decode_program(program_bytes.data, program_bytes.length,
        ruda_system_allocator(), ruda_bytecode_program_default_options(),
        &decoded_program) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_encode_program(&decoded_program,
        ruda_system_allocator(), &program_again) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(program_bytes.length == program_again.length
        && memcmp(program_bytes.data, program_again.data, program_bytes.length) == 0);
    {
        RudaBytecodeProgramOptions limited =
            ruda_bytecode_program_default_options();
        limited.max_total_registers = 5;
        ruda_bytecode_program_dispose(&decoded_program);
        RUDA_EXPECT(ruda_bytecode_decode_program(
            program_bytes.data,
            program_bytes.length,
            ruda_system_allocator(),
            limited,
            &decoded_program
        ) == RUDA_BYTECODE_CODEC_LIMIT_REACHED);
        RUDA_EXPECT(!decoded_program.initialized);
        RUDA_EXPECT(ruda_bytecode_decode_program(
            program_bytes.data,
            program_bytes.length,
            ruda_system_allocator(),
            ruda_bytecode_program_default_options(),
            &decoded_program
        ) == RUDA_BYTECODE_CODEC_OK);
    }

    {
        RudaBytecodeProgramFunction *callee_function = ruda_vector_at(&program.functions, 0);
        RudaBytecodeProgramFunction *caller_function = ruda_vector_at(&program.functions, 1);
        RudaBytecodeCall *call = caller_function != NULL
            ? ruda_vector_at(&caller_function->body.calls, 0) : NULL;
        RudaBytecodeInstruction *instruction = caller_function != NULL
            ? ruda_test_bytecode_at(&caller_function->body, 0, 0) : NULL;
        uint32_t entry = program.entry_function;
        RUDA_EXPECT(callee_function != NULL && caller_function != NULL
            && call != NULL && instruction != NULL);
        program.entry_function = 2;
        RUDA_EXPECT(ruda_bytecode_program_verify(&program)
            == RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_ARGUMENT);
        program.entry_function = entry;
        if (call != NULL) {
            call->callee.bytes[0] = 0x30;
            RUDA_EXPECT(ruda_bytecode_program_verify(&program)
                == RUDA_BYTECODE_PROGRAM_VERIFY_UNKNOWN_CALLEE);
            call->callee = callee_symbol;
            RudaBytecodeType *parameter = ruda_vector_at(
                &callee_function->body.register_types, 0);
            RUDA_EXPECT(parameter != NULL);
            if (parameter != NULL) {
                *parameter = RUDA_BYTECODE_TYPE_BOOL;
            }
            RUDA_EXPECT(ruda_bytecode_program_verify(&program)
                == RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH);
            if (parameter != NULL) {
                *parameter = RUDA_BYTECODE_TYPE_I64;
            }
        }
        if (instruction != NULL) {
            instruction->operands[0].unsigned_value = 99;
            RUDA_EXPECT(ruda_bytecode_program_verify(&program)
                == RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION);
            instruction->operands[0].unsigned_value = 2;
        }
        if (callee_function != NULL && caller_function != NULL) {
            RudaSymbolId saved = callee_function->symbol;
            callee_function->symbol = caller_function->symbol;
            RUDA_EXPECT(ruda_bytecode_program_verify(&program)
                == RUDA_BYTECODE_PROGRAM_VERIFY_DUPLICATE_SYMBOL);
            callee_function->symbol = saved;
            callee_function->symbol.bytes[0] = 0x30;
            RUDA_EXPECT(ruda_bytecode_program_verify(&program)
                == RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_ARGUMENT);
            callee_function->symbol = saved;
        }
    }
    {
        unsigned char *mutated = ruda_allocate_array(&allocator,
            program_bytes.length + 1, sizeof(unsigned char));
        RUDA_EXPECT(mutated != NULL);
        if (mutated != NULL) {
            size_t payload_offset = 8;
            uint64_t ignored = 0;
            uint64_t payload_length = 0;
            RUDA_EXPECT(ruda_test_bytecode_read_u64(
                program_bytes.data, program_bytes.length, &payload_offset, &ignored
            ));
            RUDA_EXPECT(ruda_test_bytecode_read_u64(
                program_bytes.data, program_bytes.length, &payload_offset, &ignored
            ));
            RUDA_EXPECT(ruda_test_bytecode_read_u64(
                program_bytes.data, program_bytes.length, &payload_offset,
                &payload_length
            ));
            RUDA_EXPECT(payload_length != 0
                && payload_length <= program_bytes.length - payload_offset - 32);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[0] = 'X';
            RUDA_EXPECT(ruda_bytecode_decode_program(mutated, program_bytes.length,
                ruda_system_allocator(), ruda_bytecode_program_default_options(),
                &decoded_program) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            if (payload_length != 0
                && payload_length <= program_bytes.length - payload_offset - 32) {
                /* Keep the envelope digest intact: only payload integrity changes. */
                mutated[payload_offset] ^= 1;
                RUDA_EXPECT(ruda_bytecode_decode_program(mutated,
                    program_bytes.length, ruda_system_allocator(),
                    ruda_bytecode_program_default_options(), &decoded_program)
                    == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
            }
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[8] = 2;
            RUDA_EXPECT(ruda_bytecode_decode_program(mutated, program_bytes.length,
                ruda_system_allocator(), ruda_bytecode_program_default_options(),
                &decoded_program) == RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[9] = 1;
            RUDA_EXPECT(ruda_bytecode_decode_program(mutated, program_bytes.length,
                ruda_system_allocator(), ruda_bytecode_program_default_options(),
                &decoded_program) == RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[program_bytes.length - 1] ^= 1;
            RUDA_EXPECT(ruda_bytecode_decode_program(mutated, program_bytes.length,
                ruda_system_allocator(), ruda_bytecode_program_default_options(),
                &decoded_program) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[program_bytes.length] = 0;
            RUDA_EXPECT(ruda_bytecode_decode_program(mutated, program_bytes.length + 1,
                ruda_system_allocator(), ruda_bytecode_program_default_options(),
                &decoded_program) == RUDA_BYTECODE_CODEC_TRAILING_DATA);
            ruda_deallocate(&allocator, mutated,
                program_bytes.length + 1);
        }
    }
    ruda_owned_bytes_dispose(&program_again);
    ruda_bytecode_program_dispose(&decoded_program);
    ruda_owned_bytes_dispose(&program_bytes);
    ruda_owned_bytes_dispose(&function_bytes);
    ruda_bytecode_program_dispose(&program);
}

static void ruda_test_bytecode_string_pool(void) {
    static const unsigned char expected_feature_strings[] = {
        'R', 'U', 'D', 'A', 'B', 'C', 'F', 0,
        1, 2, 4, 1, 0, 1, 4, 1, 2,
        27, 0, 0, 0, 70, 71,
        17, 0, 0, 71, 72,
        3, 3, 'a', 0, 'b', 0, 3, 0xe2, 0x98, 0x83
    };
    static const unsigned char empty[] = "";
    static const unsigned char embedded[] = {'a', 0, 'b'};
    static const unsigned char unicode[] = {0xe2, 0x98, 0x83};
    static const unsigned char invalid_utf8[] = {0xc0, 0x80};
    RudaBytecodeFunction function;
    RudaBytecodeFunction decoded;
    RudaOwnedBytes encoded;
    RudaOwnedBytes encoded_again;
    RudaTestBuffer buffer;
    size_t index = 0;

    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&encoded_again, 0, sizeof(encoded_again));
    RUDA_EXPECT(ruda_test_bytecode_build_string_constant(&function,
        ruda_system_allocator(), embedded, sizeof(embedded)));
    RUDA_EXPECT(ruda_bytecode_add_string_constant(&function, empty, 0, &(RudaBytecodeStringId){0}));
    RUDA_EXPECT(ruda_bytecode_add_string_constant(&function, unicode, sizeof(unicode),
        &(RudaBytecodeStringId){0}));
    RUDA_EXPECT(!ruda_bytecode_add_string_constant(&function, invalid_utf8,
        sizeof(invalid_utf8), &(RudaBytecodeStringId){0}));
    RUDA_EXPECT(!ruda_bytecode_add_string_constant(&function, NULL, 1,
        &(RudaBytecodeStringId){0}));
    RUDA_EXPECT(function.string_constants.length == 3
        && function.string_bytes == sizeof(embedded) + sizeof(unicode));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    ruda_test_buffer_init(&buffer);
    RUDA_EXPECT(ruda_bytecode_disassemble(&function,
        ruda_test_buffer_writer(&buffer)) == RUDA_BYTECODE_DISASSEMBLE_OK);
    RUDA_EXPECT(strcmp(buffer.bytes,
        "bytecode function v1 result=string sources=1 parameters=0 registers=1 blocks=1\n"
        "registers r0:string\n"
        "block b0:\n"
        "  const_string r0, s0 @s0:70..71\n"
        "  return r0 @s0:71..72\n") == 0);
    RUDA_EXPECT(ruda_bytecode_encode_function(&function, ruda_system_allocator(),
        &encoded) == RUDA_BYTECODE_CODEC_OK);
    /* Feature bit 1 and the raw UTF-8/NUL pool bytes are canonical wire data. */
    RUDA_EXPECT(encoded.length == sizeof(expected_feature_strings));
    RUDA_EXPECT(memcmp(encoded.data, expected_feature_strings,
        sizeof(expected_feature_strings)) == 0);
    for (index = 0; index < encoded.length; index += 1) {
        RUDA_EXPECT(ruda_bytecode_decode_function(encoded.data, index,
            ruda_system_allocator(), ruda_bytecode_function_default_options(),
            &decoded) == RUDA_BYTECODE_CODEC_TRUNCATED);
        RUDA_EXPECT(!decoded.initialized);
    }
    RUDA_EXPECT(ruda_bytecode_decode_function(encoded.data, encoded.length,
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &decoded) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(decoded.string_constants.length == 3
        && decoded.string_bytes == sizeof(embedded) + sizeof(unicode));
    RUDA_EXPECT(ruda_bytecode_encode_function(&decoded, ruda_system_allocator(),
        &encoded_again) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(encoded_again.length == sizeof(expected_feature_strings)
        && memcmp(encoded_again.data, expected_feature_strings,
            sizeof(expected_feature_strings)) == 0);
    {
        const RudaBytecodeStringConstant *first = ruda_bytecode_string_constant(
            &decoded, 0);
        const RudaBytecodeStringConstant *second = ruda_bytecode_string_constant(
            &decoded, 1);
        const RudaBytecodeStringConstant *third = ruda_bytecode_string_constant(
            &decoded, 2);
        RUDA_EXPECT(first != NULL && second != NULL && third != NULL);
        if (first != NULL) {
            RUDA_EXPECT(first->value.length == sizeof(embedded)
                && memcmp(first->value.data, embedded, sizeof(embedded)) == 0);
        }
        if (second != NULL) {
            RUDA_EXPECT(second->value.length == 0);
        }
        if (third != NULL) {
            RUDA_EXPECT(third->value.length == sizeof(unicode)
                && memcmp(third->value.data, unicode, sizeof(unicode)) == 0);
        }
    }
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded_again);

    {
        unsigned char *mutated = ruda_allocate_array(
            &function.allocator, encoded.length + 1, sizeof(unsigned char)
        );
        RUDA_EXPECT(mutated != NULL);
        if (mutated != NULL) {
            /* The format version is the first one-byte varint after the magic. */
            memcpy(mutated, encoded.data, 8);
            mutated[8] = 0x81;
            mutated[9] = 0;
            memcpy(mutated + 10, encoded.data + 9, encoded.length - 9);
            RUDA_EXPECT(ruda_bytecode_decode_function(mutated,
                encoded.length + 1, ruda_system_allocator(),
                ruda_bytecode_function_default_options(), &decoded)
                == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
            memcpy(mutated, encoded.data, encoded.length);
            RUDA_EXPECT(encoded.length >= sizeof(unicode));
            if (encoded.length >= sizeof(unicode)) {
                /* Canonical strings are raw UTF-8 at the end of this fixture. */
                mutated[encoded.length - sizeof(unicode)] = 0xff;
                RUDA_EXPECT(ruda_bytecode_decode_function(mutated, encoded.length,
                    ruda_system_allocator(), ruda_bytecode_function_default_options(),
                    &decoded) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
            }
            ruda_deallocate(&function.allocator, mutated, encoded.length + 1);
        }
    }
    ruda_owned_bytes_dispose(&encoded);

    {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeFunction allocated;
            ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
            if (ruda_test_bytecode_build_string_constant(&allocated,
                    ruda_fail_allocator(&state), embedded, sizeof(embedded))) {
                RudaBytecodeStringId ignored = RUDA_BYTECODE_NO_STRING;
                if (ruda_bytecode_add_string_constant(&allocated, empty,
                        0, &ignored)) {
                    reached_success = true;
                }
                ruda_bytecode_function_dispose(&allocated);
            }
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
    }
    {
        size_t failure_index = 0;
        bool reached_success = false;
        RUDA_EXPECT(ruda_bytecode_encode_function(&function,
            ruda_system_allocator(), &encoded) == RUDA_BYTECODE_CODEC_OK);
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaOwnedBytes failed_encoded;
            RudaBytecodeCodecStatus status;
            memset(&failed_encoded, 0, sizeof(failed_encoded));
            ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
            status = ruda_bytecode_encode_function(&function,
                ruda_fail_allocator(&state), &failed_encoded);
            RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED
                || status == RUDA_BYTECODE_CODEC_OK);
            if (status == RUDA_BYTECODE_CODEC_OK) {
                reached_success = true;
            }
            ruda_owned_bytes_dispose(&failed_encoded);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
        ruda_owned_bytes_dispose(&encoded);
    }

    {
        RudaBytecodeInstruction *instruction = ruda_test_bytecode_at(
            &function, 0, 0);
        RUDA_EXPECT(instruction != NULL);
        if (instruction != NULL) {
            instruction->operands[1].unsigned_value = 3;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            instruction->operands[1].unsigned_value = 0;
        }
    }
    {
        RudaBytecodeFunctionOptions limited =
            ruda_bytecode_function_default_options();
        RudaBytecodeStringId id = RUDA_BYTECODE_NO_STRING;
        limited.max_string_constants = 1;
        RUDA_EXPECT(ruda_bytecode_function_init(&decoded, ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_VOID, 1, NULL, 0, limited));
        RUDA_EXPECT(ruda_bytecode_add_string_constant(&decoded, empty, 0, &id));
        RUDA_EXPECT(!ruda_bytecode_add_string_constant(&decoded, unicode,
            sizeof(unicode), &id));
        ruda_bytecode_function_dispose(&decoded);
        limited = ruda_bytecode_function_default_options();
        limited.max_string_bytes = 2;
        RUDA_EXPECT(ruda_bytecode_function_init(&decoded, ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_VOID, 1, NULL, 0, limited));
        RUDA_EXPECT(!ruda_bytecode_add_string_constant(&decoded, unicode,
            sizeof(unicode), &id));
        ruda_bytecode_function_dispose(&decoded);
    }
    {
        const RudaSymbolId symbol = {{0x10}};
        RudaBytecodeProgram program;
        RudaBytecodeProgramOptions options = ruda_bytecode_program_default_options();
        uint32_t id = UINT32_MAX;
        memset(&program, 0, sizeof(program));
        options.max_total_string_bytes = 2;
        RUDA_EXPECT(ruda_bytecode_program_init(&program, ruda_system_allocator(),
            1, options));
        RUDA_EXPECT(ruda_test_bytecode_build_string_constant(&decoded,
            ruda_system_allocator(), embedded, sizeof(embedded)));
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, symbol,
            &decoded, &id) && id == 0);
        RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, symbol));
        RUDA_EXPECT(ruda_bytecode_program_verify(&program)
            == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
        ruda_bytecode_program_dispose(&program);
    }
    ruda_bytecode_function_dispose(&function);
}

static void ruda_test_bytecode_string_verifier_rejections(void) {
    const RudaSymbolId callee = {{0x10}};
    RudaBytecodeFunction function;
    RudaBytecodeInstruction *instruction;
    RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
    uint32_t block = UINT32_MAX;
    size_t span = 100;

    RUDA_EXPECT(ruda_test_bytecode_build_string_equality(&function,
        ruda_system_allocator(), true));
    instruction = ruda_test_bytecode_at(&function, 0,
        ((RudaBytecodeBlock *)ruda_vector_at(&function.blocks, 0))
            ->instructions.length - 2);
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[1].unsigned_value = 3;
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_string_equality(&function,
        ruda_system_allocator(), false));
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_panic_string(&function,
        ruda_system_allocator(), RUDA_BYTECODE_TYPE_BOOL, true, false));
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
    ruda_bytecode_function_dispose(&function);
    RUDA_EXPECT(ruda_test_bytecode_build_panic_string(&function,
        ruda_system_allocator(), RUDA_BYTECODE_TYPE_STRING, false, false));
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
    ruda_bytecode_function_dispose(&function);
    RUDA_EXPECT(ruda_test_bytecode_build_panic_string(&function,
        ruda_system_allocator(), RUDA_BYTECODE_TYPE_STRING, true, true));
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
    ruda_bytecode_function_dispose(&function);

    memset(&function, 0, sizeof(function));
    RUDA_EXPECT(ruda_bytecode_function_init(&function, ruda_system_allocator(),
        RUDA_BYTECODE_TYPE_NEVER, 1, NULL, 0,
        ruda_bytecode_function_default_options()));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &block));
    RUDA_EXPECT(ruda_bytecode_add_call(&function, callee, RUDA_BYTECODE_TYPE_I64,
        NULL, 0, &call));
    RUDA_EXPECT(ruda_test_bytecode_append(&function, block,
        RUDA_OPCODE_CALL_DIRECT_NEVER, call, 0, 0, &span));
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
    ruda_bytecode_function_dispose(&function);
}

static void ruda_test_bytecode_allocation_failures(void) {
    RudaBytecodeFunction function;
    RudaOwnedBytes bytes;
    size_t failure_index = 0;
    bool reached_success = false;

    for (failure_index = 0; failure_index < 256 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        RUDA_EXPECT(ruda_test_bytecode_build_full(
            &function,
            ruda_fail_allocator(&state)
        ) == (failure_index >= state.attempts));
        if (function.initialized) {
            reached_success = true;
            ruda_bytecode_function_dispose(&function);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    memset(&bytes, 0, sizeof(bytes));
    RUDA_EXPECT(ruda_bytecode_encode_function(&function, ruda_system_allocator(),
        &bytes) == RUDA_BYTECODE_CODEC_OK);
    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaOwnedBytes output;
        RudaBytecodeCodecStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = ruda_bytecode_encode_function(
            &function,
            ruda_fail_allocator(&state),
            &output
        );
        if (status == RUDA_BYTECODE_CODEC_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            RUDA_EXPECT(output.data == NULL);
            RUDA_EXPECT(output.length == 0);
        }
        ruda_owned_bytes_dispose(&output);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeFunction output;
        RudaBytecodeCodecStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = ruda_bytecode_decode_function(
            bytes.data,
            bytes.length,
            ruda_fail_allocator(&state),
            ruda_bytecode_function_default_options(),
            &output
        );
        if (status == RUDA_BYTECODE_CODEC_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            RUDA_EXPECT(!output.initialized);
        }
        ruda_bytecode_function_dispose(&output);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_owned_bytes_dispose(&bytes);
    ruda_bytecode_function_dispose(&function);

    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeVerifyStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_bytecode_build_full(
            &function,
            ruda_fail_allocator(&state)
        ));
        state.failure_index = state.attempts + failure_index;
        status = ruda_bytecode_verify(&function);
        if (status == RUDA_BYTECODE_VERIFY_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED);
        }
        ruda_bytecode_function_dispose(&function);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeProgram program;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        if (ruda_test_bytecode_build_direct_program(
                &program,
                ruda_fail_allocator(&state)
            )) {
            reached_success = true;
        }
        ruda_bytecode_program_dispose(&program);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

    {
        RudaBytecodeProgram program;
        RudaOwnedBytes program_bytes;
        memset(&program_bytes, 0, sizeof(program_bytes));
        RUDA_EXPECT(ruda_test_bytecode_build_direct_program(
            &program,
            ruda_system_allocator()
        ));
        RUDA_EXPECT(ruda_bytecode_encode_program(
            &program,
            ruda_system_allocator(),
            &program_bytes
        ) == RUDA_BYTECODE_CODEC_OK);
        for (failure_index = 0, reached_success = false;
            failure_index < 256 && !reached_success; failure_index += 1) {
            RudaFailAllocator state;
            RudaOwnedBytes output;
            RudaBytecodeCodecStatus status;
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            memset(&output, 0, sizeof(output));
            status = ruda_bytecode_encode_program(
                &program,
                ruda_fail_allocator(&state),
                &output
            );
            if (status == RUDA_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
                RUDA_EXPECT(output.data == NULL && output.length == 0);
            }
            ruda_owned_bytes_dispose(&output);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
        for (failure_index = 0, reached_success = false;
            failure_index < 256 && !reached_success; failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeProgram output;
            RudaBytecodeCodecStatus status;
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            memset(&output, 0, sizeof(output));
            status = ruda_bytecode_decode_program(
                program_bytes.data,
                program_bytes.length,
                ruda_fail_allocator(&state),
                ruda_bytecode_program_default_options(),
                &output
            );
            if (status == RUDA_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
                RUDA_EXPECT(!output.initialized);
            }
            ruda_bytecode_program_dispose(&output);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
        ruda_owned_bytes_dispose(&program_bytes);
        ruda_bytecode_program_dispose(&program);
    }

    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeProgram program;
        RudaBytecodeProgramVerifyStatus status;
        size_t live_allocations = 0;
        size_t live_bytes = 0;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_bytecode_build_direct_program(
            &program,
            ruda_fail_allocator(&state)
        ));
        live_allocations = state.live_allocations;
        live_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_index;
        status = ruda_bytecode_program_verify(&program);
        if (status == RUDA_BYTECODE_PROGRAM_VERIFY_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(
                status == RUDA_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED
            );
        }
        RUDA_EXPECT(state.live_allocations == live_allocations);
        RUDA_EXPECT(state.live_bytes == live_bytes);
        ruda_bytecode_program_dispose(&program);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_bytecode(void) {
    ruda_test_bytecode_metadata();
    ruda_test_bytecode_valid_and_disassembly();
    ruda_test_bytecode_codec();
    ruda_test_bytecode_f64_codec_and_verifier();
    ruda_test_bytecode_decode_rejections();
    ruda_test_bytecode_verifier_rejections();
    ruda_test_bytecode_calls_and_program();
    ruda_test_bytecode_string_pool();
    ruda_test_bytecode_string_verifier_rejections();
    ruda_test_bytecode_allocation_failures();
}
