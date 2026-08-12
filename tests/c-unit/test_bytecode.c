#include "test_support.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/bytecode/bytecode.h"

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
        RUDA_BYTECODE_TYPE_BOOL
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
            {RUDA_BYTECODE_OPERAND_NONE, RUDA_BYTECODE_OPERAND_NONE, RUDA_BYTECODE_OPERAND_NONE}, RUDA_OPCODE_FLAG_TERMINATOR}
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
        "bytecode function v1 result=i64 sources=1 parameters=0 registers=15 blocks=3\n"
        "registers r0:i64 r1:i64 r2:i64 r3:i64 r4:i64 r5:i64 r6:i64 r7:i64 r8:i64 r9:i64 r10:bool r11:bool r12:bool r13:bool r14:bool\n"
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
        "  branch_bool r10, b1, b2 @s0:15..16\n"
        "block b1:\n"
        "  return r3 @s0:16..17\n"
        "block b2:\n"
        "  jump b1 @s0:17..18\n";
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
    mutated[9] = 1;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, sizeof(canonical),
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION);
    mutated[9] = 0;
    mutated[10] = RUDA_BYTECODE_TYPE_INVALID;
    RUDA_EXPECT(ruda_bytecode_decode_function(mutated, sizeof(canonical),
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &output) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
    mutated[10] = RUDA_BYTECODE_TYPE_I64;
    mutated[14] = RUDA_BYTECODE_TYPE_VOID;
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
        instruction->operands[0].unsigned_value = 15;
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
    instruction = ruda_test_bytecode_at(&function, 0, 15);
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
}

void ruda_test_bytecode(void) {
    ruda_test_bytecode_metadata();
    ruda_test_bytecode_valid_and_disassembly();
    ruda_test_bytecode_codec();
    ruda_test_bytecode_decode_rejections();
    ruda_test_bytecode_verifier_rejections();
    ruda_test_bytecode_allocation_failures();
}
