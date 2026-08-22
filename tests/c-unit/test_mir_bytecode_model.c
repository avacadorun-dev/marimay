#include "test_mir_bytecode_support.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"

static bool mmay_test_mir_bytecode_build_scalar(
    MarimayMirFunction *function,
    MarimayAllocator allocator
) {
    static const MarimayTypeId parameters[] = {MMAY_TYPE_I64};
    MarimayMirFunctionOptions options = mmay_mir_function_default_options();
    MarimayMirBlockId entry = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId when_true = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId when_false = MMAY_MIR_NO_BLOCK;
    MarimayMirRegister constant = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister copied = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister arithmetic = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister condition = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister negated = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister equality = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister bool_equality = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister boolean_constant = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister boolean_not = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister subtraction = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister multiplication = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister division = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister remainder = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister less_equal = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister bitwise_not = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister bitwise_and = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister bitwise_xor = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister bitwise_or = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister shift_left = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister shift_right = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;

    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, allocator, (MarimaySymbolId){{0}}, parameters,
            1, MMAY_TYPE_I64, options
        ) || !mmay_mir_add_register(function, MMAY_TYPE_I64, &constant)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &copied)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &arithmetic)
        || !mmay_mir_add_register(function, MMAY_TYPE_BOOL, &condition)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &negated)
        || !mmay_mir_add_register(function, MMAY_TYPE_BOOL, &equality)
        || !mmay_mir_add_register(function, MMAY_TYPE_BOOL, &bool_equality)
        || !mmay_mir_add_register(function, MMAY_TYPE_BOOL, &boolean_constant)
        || !mmay_mir_add_register(function, MMAY_TYPE_BOOL, &boolean_not)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &subtraction)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &multiplication)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &division)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &remainder)
        || !mmay_mir_add_register(function, MMAY_TYPE_BOOL, &less_equal)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &bitwise_not)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &bitwise_and)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &bitwise_xor)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &bitwise_or)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &shift_left)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &shift_right)
        || !mmay_mir_add_block(function, &entry)
        || !mmay_mir_add_block(function, &when_true)
        || !mmay_mir_add_block(function, &when_false)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_CONSTANT_I64, constant, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 7, false, 10
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_COPY, copied, constant, MMAY_MIR_NO_REGISTER, 0, false, 11
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_ADD_CHECKED_I64, arithmetic, copied, 0, 0, false, 12
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_LESS_I64, condition, 0, arithmetic, 0, false, 13
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_NEGATE_CHECKED_I64, negated, arithmetic,
        MMAY_MIR_NO_REGISTER, 0, false, 14
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_EQUAL_I64, equality, negated, arithmetic, 0, false, 15
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_EQUAL_BOOL, bool_equality, condition, equality, 0, false, 16
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_CONSTANT_BOOL, boolean_constant, MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER, 0, true, 17
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_NOT_BOOL, boolean_not, boolean_constant,
        MMAY_MIR_NO_REGISTER, 0, false, 18
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_SUBTRACT_CHECKED_I64, subtraction, arithmetic, copied,
        0, false, 19
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_MULTIPLY_CHECKED_I64, multiplication, copied, copied,
        0, false, 20
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_DIVIDE_CHECKED_I64, division, multiplication, copied,
        0, false, 21
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_REMAINDER_CHECKED_I64, remainder, multiplication, copied,
        0, false, 22
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_LESS_EQUAL_I64, less_equal, copied, arithmetic,
        0, false, 23
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_BITWISE_NOT_I64, bitwise_not, copied,
        MMAY_MIR_NO_REGISTER, 0, false, 24
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_BITWISE_AND_I64, bitwise_and, copied, constant,
        0, false, 25
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_BITWISE_XOR_I64, bitwise_xor, copied, constant,
        0, false, 26
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_BITWISE_OR_I64, bitwise_or, copied, constant,
        0, false, 27
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_SHIFT_LEFT_CHECKED_I64, shift_left, copied, constant,
        0, false, 28
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        MMAY_MIR_SHIFT_RIGHT_I64, shift_right, copied, constant,
        0, false, 29
    );
    if (!mmay_mir_append_instruction(function, entry, &instruction)
        || !mmay_mir_set_terminator(function, entry,
            mmay_test_mir_bytecode_terminator(
                MMAY_MIR_BRANCH, bool_equality, when_true, when_false, 30
            ))
        || !mmay_mir_set_terminator(function, when_true,
            mmay_test_mir_bytecode_terminator(
                MMAY_MIR_RETURN, arithmetic, MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK, 31
            ))
        || !mmay_mir_set_terminator(function, when_false,
            mmay_test_mir_bytecode_terminator(
                MMAY_MIR_GOTO, MMAY_MIR_NO_REGISTER, when_true,
                MMAY_MIR_NO_BLOCK, 32
            ))) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

static void mmay_test_mir_bytecode_scalar_mapping(void) {
    MarimayMirFunction input;
    MarimayBytecodeFunction output;
    MarimayBytecodeFunction decoded;
    MarimayOwnedBytes encoded;
    const MarimayBytecodeInstruction *instruction;
    MarimayMirBytecodeLowerOptions options = mmay_test_mir_bytecode_options();
    size_t index = 0;

    memset(&output, 0, sizeof(output));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_mir_bytecode_build_scalar(
        &input,
        mmay_system_allocator()
    ));
    MMAY_EXPECT(mmay_mir_verify(&input) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        &input, mmay_system_allocator(), options, &output
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(output.result_type == MMAY_BYTECODE_TYPE_I64);
    MMAY_EXPECT(output.source_count == 3);
    MMAY_EXPECT(output.parameter_count == 1);
    MMAY_EXPECT(output.register_types.length == input.register_types.length);
    MMAY_EXPECT(output.blocks.length == input.blocks.length);
    for (index = 0; index < output.register_types.length; index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &output.register_types, index
        );
        MMAY_EXPECT(type != NULL);
        if (type != NULL) {
            if (index == 4 || index == 6 || index == 7 || index == 8
                || index == 9 || index == 14) {
                MMAY_EXPECT(*type == MMAY_BYTECODE_TYPE_BOOL);
            } else {
                MMAY_EXPECT(*type == MMAY_BYTECODE_TYPE_I64);
            }
        }
    }
    instruction = mmay_test_mir_bytecode_at(&output, 0, 0);
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_CONST_I64);
    MMAY_EXPECT(instruction != NULL && instruction->operands[0].unsigned_value == 1);
    MMAY_EXPECT(instruction != NULL && instruction->operands[1].signed_value == 7);
    MMAY_EXPECT(instruction != NULL && instruction->span.source == 2
        && instruction->span.start == 10 && instruction->span.end == 11);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 1);
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_COPY);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 2);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_ADD_CHECKED);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 3);
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_I64_LESS);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 4);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_NEGATE_CHECKED);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 5);
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_I64_EQUAL);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 6);
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_BOOL_EQUAL);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 7);
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_CONST_BOOL);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 8);
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_BOOL_NOT);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 9);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_SUBTRACT_CHECKED);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 10);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_MULTIPLY_CHECKED);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 11);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_DIVIDE_CHECKED);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 12);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_REMAINDER_CHECKED);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 13);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_LESS_EQUAL);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 14);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_BITWISE_NOT);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 15);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_BITWISE_AND);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 16);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_BITWISE_XOR);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 17);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_BITWISE_OR);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 18);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 19);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_I64_SHIFT_RIGHT);
    instruction = mmay_test_mir_bytecode_at(&output, 0, 20);
    MMAY_EXPECT(instruction != NULL
        && instruction->opcode == MMAY_OPCODE_BRANCH_BOOL
        && instruction->operands[0].unsigned_value == 7
        && instruction->operands[1].unsigned_value == 1
        && instruction->operands[2].unsigned_value == 2);
    instruction = mmay_test_mir_bytecode_at(&output, 1, 0);
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_RETURN);
    instruction = mmay_test_mir_bytecode_at(&output, 2, 0);
    MMAY_EXPECT(instruction != NULL && instruction->opcode == MMAY_OPCODE_JUMP);
    MMAY_EXPECT(mmay_bytecode_verify(&output) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &output,
        mmay_system_allocator(),
        &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data,
        encoded.length,
        mmay_system_allocator(),
        mmay_bytecode_function_default_options(),
        &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    {
        static const int64_t values[] = {-7, 5};
        size_t value_index = 0;
        for (value_index = 0;
            value_index < sizeof(values) / sizeof(values[0]);
            value_index += 1) {
            MarimayMirValue mir_argument;
            MarimayMirValue mir_result;
            MarimayBytecodeValue bytecode_argument;
            MarimayBytecodeValue bytecode_result;
            memset(&mir_argument, 0, sizeof(mir_argument));
            memset(&mir_result, 0, sizeof(mir_result));
            memset(&bytecode_argument, 0, sizeof(bytecode_argument));
            memset(&bytecode_result, 0, sizeof(bytecode_result));
            mir_argument.type = MMAY_TYPE_I64;
            mir_argument.as.i64 = values[value_index];
            bytecode_argument.type = MMAY_BYTECODE_TYPE_I64;
            bytecode_argument.as.i64 = values[value_index];
            MMAY_EXPECT(mmay_mir_interpret(
                &input,
                &mir_argument,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &mir_result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(mmay_bytecode_run(
                &decoded,
                &bytecode_argument,
                1,
                mmay_system_allocator(),
                mmay_bytecode_run_default_options(),
                &bytecode_result
            ) == MMAY_BYTECODE_RUN_OK);
            MMAY_EXPECT(mir_result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(bytecode_result.type == MMAY_BYTECODE_TYPE_I64);
            MMAY_EXPECT(mir_result.as.i64 == bytecode_result.as.i64);
            {
                size_t exact_steps = values[value_index] == -7 ? 22 : 23;
                MarimayMirInterpretOptions mir_options =
                    mmay_mir_interpret_default_options();
                MarimayBytecodeRunOptions bytecode_options =
                    mmay_bytecode_run_default_options();
                mir_options.max_steps = exact_steps;
                bytecode_options.max_steps = exact_steps;
                MMAY_EXPECT(mmay_mir_interpret(
                    &input,
                    &mir_argument,
                    1,
                    mmay_system_allocator(),
                    mir_options,
                    &mir_result
                ) == MMAY_MIR_INTERPRET_OK);
                MMAY_EXPECT(mmay_bytecode_run(
                    &decoded,
                    &bytecode_argument,
                    1,
                    mmay_system_allocator(),
                    bytecode_options,
                    &bytecode_result
                ) == MMAY_BYTECODE_RUN_OK);
                mir_options.max_steps -= 1;
                bytecode_options.max_steps -= 1;
                MMAY_EXPECT(mmay_mir_interpret(
                    &input,
                    &mir_argument,
                    1,
                    mmay_system_allocator(),
                    mir_options,
                    &mir_result
                ) == MMAY_MIR_INTERPRET_STEP_LIMIT);
                MMAY_EXPECT(mmay_bytecode_run(
                    &decoded,
                    &bytecode_argument,
                    1,
                    mmay_system_allocator(),
                    bytecode_options,
                    &bytecode_result
                ) == MMAY_BYTECODE_RUN_STEP_LIMIT);
                {
                    size_t mir_shared_steps = exact_steps;
                    size_t bytecode_shared_steps = exact_steps;
                    mir_options = mmay_mir_interpret_default_options();
                    bytecode_options = mmay_bytecode_run_default_options();
                    mir_options.remaining_shared_steps = &mir_shared_steps;
                    bytecode_options.remaining_shared_steps =
                        &bytecode_shared_steps;
                    MMAY_EXPECT(mmay_mir_interpret(
                        &input,
                        &mir_argument,
                        1,
                        mmay_system_allocator(),
                        mir_options,
                        &mir_result
                    ) == MMAY_MIR_INTERPRET_OK);
                    MMAY_EXPECT(mmay_bytecode_run(
                        &decoded,
                        &bytecode_argument,
                        1,
                        mmay_system_allocator(),
                        bytecode_options,
                        &bytecode_result
                    ) == MMAY_BYTECODE_RUN_OK);
                    MMAY_EXPECT(mir_shared_steps == 0);
                    MMAY_EXPECT(bytecode_shared_steps == 0);

                    mir_shared_steps = exact_steps - 1;
                    bytecode_shared_steps = exact_steps - 1;
                    MMAY_EXPECT(mmay_mir_interpret(
                        &input,
                        &mir_argument,
                        1,
                        mmay_system_allocator(),
                        mir_options,
                        &mir_result
                    ) == MMAY_MIR_INTERPRET_STEP_LIMIT);
                    MMAY_EXPECT(mmay_bytecode_run(
                        &decoded,
                        &bytecode_argument,
                        1,
                        mmay_system_allocator(),
                        bytecode_options,
                        &bytecode_result
                    ) == MMAY_BYTECODE_RUN_STEP_LIMIT);
                    MMAY_EXPECT(mir_shared_steps == 0);
                    MMAY_EXPECT(bytecode_shared_steps == 0);
                }
            }
        }
    }
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&output);
    mmay_mir_function_dispose(&input);
}

static void mmay_test_mir_bytecode_void_and_failures(void) {
    MarimayMirFunction input;
    MarimayBytecodeFunction output;
    MarimayMirFunctionOptions mir_options = mmay_mir_function_default_options();
    MarimayMirBytecodeLowerOptions options = mmay_test_mir_bytecode_options();
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirRegister string_register = MMAY_MIR_NO_REGISTER;
    MarimayMirStringId string_id = MMAY_MIR_NO_STRING;
    MarimayMirCallId call_id = MMAY_MIR_NO_CALL;
    MarimayMirInstruction instruction;

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    MMAY_EXPECT(mmay_mir_function_init(&input, mmay_system_allocator(),
        (MarimaySymbolId){{0}}, NULL, 0, MMAY_TYPE_VOID, mir_options));
    MMAY_EXPECT(mmay_mir_add_block(&input, &block));
    MMAY_EXPECT(mmay_mir_set_terminator(&input, block,
        mmay_test_mir_bytecode_terminator(MMAY_MIR_RETURN, MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 20)));
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&input, mmay_system_allocator(),
        options, &output) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_test_mir_bytecode_at(&output, 0, 0)->opcode
        == MMAY_OPCODE_RETURN_VOID);
    mmay_bytecode_function_dispose(&output);
    mmay_mir_function_dispose(&input);

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    MMAY_EXPECT(mmay_mir_function_init(&input, mmay_system_allocator(),
        (MarimaySymbolId){{0}}, NULL, 0, MMAY_TYPE_VOID, mir_options));
    MMAY_EXPECT(mmay_mir_add_register(&input, MMAY_TYPE_STRING,
        &string_register));
    MMAY_EXPECT(mmay_mir_add_block(&input, &block));
    MMAY_EXPECT(mmay_mir_add_string_constant(&input,
        (const unsigned char *)"x", 1, &string_id));
    instruction = mmay_test_mir_bytecode_instruction(MMAY_MIR_CONSTANT_STRING,
        string_register, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER,
        0, false, 21);
    instruction.string_constant = string_id;
    MMAY_EXPECT(mmay_mir_append_instruction(&input, block, &instruction));
    MMAY_EXPECT(mmay_mir_set_terminator(&input, block,
        mmay_test_mir_bytecode_terminator(MMAY_MIR_RETURN, MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 22)));
    MMAY_EXPECT(mmay_mir_verify(&input) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&input, mmay_system_allocator(),
        options, &output) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(output.string_constants.length == 1
        && output.string_bytes == 1);
    {
        const MarimayBytecodeInstruction *bytecode_instruction =
            mmay_test_mir_bytecode_at(&output, 0, 0);
        const MarimayBytecodeStringConstant *constant =
            mmay_bytecode_string_constant(&output, 0);
        MMAY_EXPECT(bytecode_instruction != NULL && constant != NULL);
        if (bytecode_instruction != NULL) {
            MMAY_EXPECT(bytecode_instruction->opcode == MMAY_OPCODE_CONST_STRING);
            MMAY_EXPECT(bytecode_instruction->operands[0].unsigned_value == 0);
            MMAY_EXPECT(bytecode_instruction->operands[1].unsigned_value == 0);
            MMAY_EXPECT(bytecode_instruction->span.source == 2);
            MMAY_EXPECT(bytecode_instruction->span.start == 21);
            MMAY_EXPECT(bytecode_instruction->span.end == 22);
        }
        if (constant != NULL) {
            MMAY_EXPECT(constant->value.length == 1
                && constant->value.data[0] == 'x');
        }
    }
    mmay_bytecode_function_dispose(&output);
    mmay_mir_function_dispose(&input);

    {
        static const MarimayTypeId parameters[] = {MMAY_TYPE_I64};
        MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
        memset(&input, 0, sizeof(input));
        MMAY_EXPECT(mmay_mir_function_init(
            &input,
            mmay_system_allocator(),
            (MarimaySymbolId){{0}},
            parameters,
            1,
            MMAY_TYPE_I64,
            mir_options
        ));
        MMAY_EXPECT(mmay_mir_add_register(&input, MMAY_TYPE_I64, &result));
        MMAY_EXPECT(mmay_mir_add_block(&input, &block));
        instruction = mmay_test_mir_bytecode_instruction(
            MMAY_MIR_BITWISE_NOT_I64,
            result,
            0,
            MMAY_MIR_NO_REGISTER,
            0,
            false,
            21
        );
        MMAY_EXPECT(mmay_mir_append_instruction(&input, block, &instruction));
        MMAY_EXPECT(mmay_mir_set_terminator(
            &input,
            block,
            mmay_test_mir_bytecode_terminator(
                MMAY_MIR_RETURN,
                result,
                MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK,
                22
            )
        ));
    }
    MMAY_EXPECT(mmay_mir_verify(&input) == MMAY_MIR_VERIFY_OK);
    memset(&output, 0, sizeof(output));
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&input, mmay_system_allocator(),
        options, &output) == MMAY_MIR_BYTECODE_LOWER_OK);
    {
        const MarimayBytecodeInstruction *bytecode_instruction =
            mmay_test_mir_bytecode_at(&output, 0, 0);
        MMAY_EXPECT(bytecode_instruction != NULL);
        if (bytecode_instruction != NULL) {
            MMAY_EXPECT(bytecode_instruction->opcode == MMAY_OPCODE_I64_BITWISE_NOT);
            MMAY_EXPECT(bytecode_instruction->operands[0].unsigned_value == 1);
            MMAY_EXPECT(bytecode_instruction->operands[1].unsigned_value == 0);
            MMAY_EXPECT(bytecode_instruction->span.source == 2);
            MMAY_EXPECT(bytecode_instruction->span.start == 21);
            MMAY_EXPECT(bytecode_instruction->span.end == 22);
        }
    }
    mmay_bytecode_function_dispose(&output);
    mmay_mir_function_dispose(&input);

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    MMAY_EXPECT(mmay_mir_function_init(&input, mmay_system_allocator(),
        (MarimaySymbolId){{0}}, NULL, 0, MMAY_TYPE_I64, mir_options));
    MMAY_EXPECT(mmay_mir_add_block(&input, &block));
    MMAY_EXPECT(mmay_mir_add_call(&input, (MarimaySymbolId){{1}}, MMAY_TYPE_NEVER,
        NULL, 0, &call_id));
    {
        MarimayMirTerminator terminator = mmay_test_mir_bytecode_terminator(
            MMAY_MIR_CALL_NEVER, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_BLOCK,
            MMAY_MIR_NO_BLOCK, 23
        );
        terminator.call = call_id;
        MMAY_EXPECT(mmay_mir_set_terminator(&input, block, terminator));
    }
    MMAY_EXPECT(mmay_mir_verify(&input) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&input, mmay_system_allocator(),
        options, &output) == MMAY_MIR_BYTECODE_LOWER_OK);
    {
        const MarimayBytecodeInstruction *bytecode_instruction =
            mmay_test_mir_bytecode_at(&output, 0, 0);
        MMAY_EXPECT(bytecode_instruction != NULL);
        if (bytecode_instruction != NULL) {
            MMAY_EXPECT(bytecode_instruction->opcode
                == MMAY_OPCODE_CALL_DIRECT_NEVER);
            MMAY_EXPECT(bytecode_instruction->operands[0].unsigned_value == 0);
            MMAY_EXPECT(bytecode_instruction->span.source == 2);
            MMAY_EXPECT(bytecode_instruction->span.start == 23);
            MMAY_EXPECT(bytecode_instruction->span.end == 24);
        }
    }
    mmay_bytecode_function_dispose(&output);
    mmay_mir_function_dispose(&input);

    MMAY_EXPECT(mmay_test_mir_bytecode_build_scalar(
        &input,
        mmay_system_allocator()
    ));
    ((MarimayMirInstruction *)mmay_vector_at(
        &((MarimayMirBlock *)mmay_vector_at(&input.blocks, 0))->instructions, 0
    ))->kind = (MarimayMirInstructionKind)999;
    memset(&output, 0, sizeof(output));
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&input, mmay_system_allocator(),
        options, &output) == MMAY_MIR_BYTECODE_LOWER_INVALID_MIR);
    mmay_mir_function_dispose(&input);
}

static void mmay_test_mir_bytecode_options_and_allocations(void) {
    MarimayMirFunction input;
    MarimayBytecodeFunction output;
    MarimayMirBytecodeLowerOptions options = mmay_test_mir_bytecode_options();
    size_t failure_index = 0;
    bool reached_success = false;

    MMAY_EXPECT(mmay_test_mir_bytecode_build_scalar(
        &input,
        mmay_system_allocator()
    ));
    memset(&output, 0, sizeof(output));
    options.source_count = 2;
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&input, mmay_system_allocator(),
        options, &output) == MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT);
    options = mmay_test_mir_bytecode_options();
    options.function_options.max_blocks = 0;
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&input, mmay_system_allocator(),
        options, &output) == MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT);
    options = mmay_test_mir_bytecode_options();
    options.function_options.max_blocks = 2;
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&input, mmay_system_allocator(),
        options, &output) == MMAY_MIR_BYTECODE_LOWER_LIMIT_REACHED);
    options = mmay_test_mir_bytecode_options();
    options.function_options.max_verify_work = 1;
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&input, mmay_system_allocator(),
        options, &output) == MMAY_MIR_BYTECODE_LOWER_LIMIT_REACHED);
    options = mmay_test_mir_bytecode_options();
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(NULL, mmay_system_allocator(),
        options, &output) == MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT);

    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&input, mmay_system_allocator(),
        options, &output) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(output.initialized);
    options.source_count = 2;
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&input, mmay_system_allocator(),
        options, &output) == MMAY_MIR_BYTECODE_LOWER_INVALID_ARGUMENT);
    MMAY_EXPECT(!output.initialized);
    options = mmay_test_mir_bytecode_options();

    for (failure_index = 0; failure_index < 256 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayMirBytecodeLowerStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = mmay_mir_lower_bytecode_function(
            &input, mmay_fail_allocator(&state), options, &output
        );
        if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
            reached_success = true;
            mmay_bytecode_function_dispose(&output);
        } else {
            MMAY_EXPECT(status == MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED);
            MMAY_EXPECT(!output.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
    mmay_mir_function_dispose(&input);

    {
        MarimayFailAllocator input_state;
        size_t input_live_allocations = 0;
        size_t input_live_bytes = 0;
        bool verifier_reached_success = false;
        mmay_fail_allocator_init(
            &input_state,
            mmay_system_allocator(),
            SIZE_MAX
        );
        MMAY_EXPECT(mmay_test_mir_bytecode_build_scalar(
            &input,
            mmay_fail_allocator(&input_state)
        ));
        input_live_allocations = input_state.live_allocations;
        input_live_bytes = input_state.live_bytes;
        for (failure_index = 0;
            failure_index < 32 && !verifier_reached_success;
            failure_index += 1) {
            MarimayMirBytecodeLowerStatus status;
            input_state.failure_index = input_state.attempts + failure_index;
            memset(&output, 0, sizeof(output));
            status = mmay_mir_lower_bytecode_function(
                &input,
                mmay_system_allocator(),
                options,
                &output
            );
            if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
                verifier_reached_success = true;
                mmay_bytecode_function_dispose(&output);
            } else {
                MMAY_EXPECT(
                    status == MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED
                );
                MMAY_EXPECT(!output.initialized);
            }
            MMAY_EXPECT(input_state.live_allocations == input_live_allocations);
            MMAY_EXPECT(input_state.live_bytes == input_live_bytes);
        }
        MMAY_EXPECT(verifier_reached_success);
        mmay_mir_function_dispose(&input);
        MMAY_EXPECT(input_state.live_allocations == 0);
        MMAY_EXPECT(input_state.live_bytes == 0);
    }
}

static bool mmay_test_mir_bytecode_build_arithmetic(
    MarimayMirFunction *function,
    MarimayMirInstructionKind kind
) {
    static const MarimayTypeId parameters[] = {MMAY_TYPE_I64, MMAY_TYPE_I64};
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    MarimayMirFunctionOptions options = mmay_mir_function_default_options();
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function,
            mmay_system_allocator(),
            (MarimaySymbolId){{0}},
            parameters,
            2,
            MMAY_TYPE_I64,
            options
        ) || !mmay_mir_add_register(function, MMAY_TYPE_I64, &result)
        || !mmay_mir_add_block(function, &block)) {
        mmay_mir_function_dispose(function);
        return false;
    }
    instruction = mmay_test_mir_bytecode_instruction(
        kind,
        result,
        0,
        1,
        0,
        false,
        30
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function,
            block,
            mmay_test_mir_bytecode_terminator(
                MMAY_MIR_RETURN,
                result,
                MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK,
                31
            )
        )) {
        mmay_mir_function_dispose(function);
        return false;
    }
    return true;
}

static MarimayMirInterpretStatus mmay_test_mir_bytecode_vm_status(
    MarimayBytecodeRunStatus status
) {
    if (status == MMAY_BYTECODE_RUN_OK) {
        return MMAY_MIR_INTERPRET_OK;
    }
    if (status == MMAY_BYTECODE_RUN_OVERFLOW) {
        return MMAY_MIR_INTERPRET_OVERFLOW;
    }
    if (status == MMAY_BYTECODE_RUN_DIVISION_BY_ZERO) {
        return MMAY_MIR_INTERPRET_DIVISION_BY_ZERO;
    }
    if (status == MMAY_BYTECODE_RUN_INVALID_SHIFT) {
        return MMAY_MIR_INTERPRET_INVALID_SHIFT;
    }
    return MMAY_MIR_INTERPRET_INVALID_MIR;
}

static MarimayOpcode mmay_test_mir_bytecode_expected_opcode(
    MarimayMirInstructionKind kind
) {
    switch (kind) {
        case MMAY_MIR_BITWISE_NOT_I64:
            return MMAY_OPCODE_I64_BITWISE_NOT;
        case MMAY_MIR_BITWISE_AND_I64:
            return MMAY_OPCODE_I64_BITWISE_AND;
        case MMAY_MIR_BITWISE_XOR_I64:
            return MMAY_OPCODE_I64_BITWISE_XOR;
        case MMAY_MIR_BITWISE_OR_I64:
            return MMAY_OPCODE_I64_BITWISE_OR;
        case MMAY_MIR_SHIFT_LEFT_CHECKED_I64:
            return MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED;
        case MMAY_MIR_SHIFT_RIGHT_I64:
            return MMAY_OPCODE_I64_SHIFT_RIGHT;
        default:
            return MMAY_OPCODE_INVALID;
    }
}

static void mmay_test_mir_bytecode_arithmetic_differential(void) {
    static const struct {
        MarimayMirInstructionKind kind;
        int64_t left;
        int64_t right;
    } cases[] = {
        {MMAY_MIR_ADD_CHECKED_I64, 2, 3},
        {MMAY_MIR_ADD_CHECKED_I64, INT64_MAX, 1},
        {MMAY_MIR_ADD_CHECKED_I64, INT64_MIN, -1},
        {MMAY_MIR_SUBTRACT_CHECKED_I64, 7, 3},
        {MMAY_MIR_SUBTRACT_CHECKED_I64, INT64_MIN, 1},
        {MMAY_MIR_SUBTRACT_CHECKED_I64, INT64_MAX, -1},
        {MMAY_MIR_MULTIPLY_CHECKED_I64, -7, 3},
        {MMAY_MIR_MULTIPLY_CHECKED_I64, INT64_MAX, 2},
        {MMAY_MIR_MULTIPLY_CHECKED_I64, INT64_MIN, -1},
        {MMAY_MIR_DIVIDE_CHECKED_I64, 7, 3},
        {MMAY_MIR_DIVIDE_CHECKED_I64, -7, 3},
        {MMAY_MIR_DIVIDE_CHECKED_I64, 7, -3},
        {MMAY_MIR_DIVIDE_CHECKED_I64, -7, -3},
        {MMAY_MIR_DIVIDE_CHECKED_I64, 1, 0},
        {MMAY_MIR_DIVIDE_CHECKED_I64, INT64_MIN, -1},
        {MMAY_MIR_REMAINDER_CHECKED_I64, 7, 3},
        {MMAY_MIR_REMAINDER_CHECKED_I64, -7, 3},
        {MMAY_MIR_REMAINDER_CHECKED_I64, 7, -3},
        {MMAY_MIR_REMAINDER_CHECKED_I64, -7, -3},
        {MMAY_MIR_REMAINDER_CHECKED_I64, 1, 0},
        {MMAY_MIR_REMAINDER_CHECKED_I64, INT64_MIN, -1},
        {MMAY_MIR_NEGATE_CHECKED_I64, 7, 0},
        {MMAY_MIR_NEGATE_CHECKED_I64, INT64_MIN, 0},
        {MMAY_MIR_BITWISE_NOT_I64, 0, 0},
        {MMAY_MIR_BITWISE_NOT_I64, -1, 0},
        {MMAY_MIR_BITWISE_AND_I64, -1, INT64_C(0x55)},
        {MMAY_MIR_BITWISE_XOR_I64, -1, INT64_C(0x55)},
        {MMAY_MIR_BITWISE_OR_I64, INT64_MIN, 1},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, -2, 1},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, 1, 62},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, -1, 63},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, -2, 62},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, -2, 63},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, 0, 63},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, INT64_MIN, 0},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, INT64_MAX, 0},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, INT64_MAX, 1},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, INT64_MIN, 1},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, 1, -1},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, 1, 64},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, 1, INT64_MIN},
        {MMAY_MIR_SHIFT_LEFT_CHECKED_I64, 1, INT64_MAX},
        {MMAY_MIR_SHIFT_RIGHT_I64, -7, 0},
        {MMAY_MIR_SHIFT_RIGHT_I64, -1, 1},
        {MMAY_MIR_SHIFT_RIGHT_I64, INT64_MIN, 63},
        {MMAY_MIR_SHIFT_RIGHT_I64, INT64_MAX, 62},
        {MMAY_MIR_SHIFT_RIGHT_I64, 1, 63},
        {MMAY_MIR_SHIFT_RIGHT_I64, 1, -1},
        {MMAY_MIR_SHIFT_RIGHT_I64, 1, 64},
        {MMAY_MIR_SHIFT_RIGHT_I64, 1, INT64_MIN},
        {MMAY_MIR_SHIFT_RIGHT_I64, 1, INT64_MAX}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayMirFunction mir;
        MarimayBytecodeFunction bytecode;
        MarimayBytecodeFunction decoded;
        MarimayMirValue mir_arguments[2];
        MarimayMirValue mir_result;
        MarimayBytecodeValue bytecode_arguments[2];
        MarimayBytecodeValue bytecode_result;
        MarimayBytecodeTrap trap;
        MarimayOwnedBytes encoded;
        MarimayBytecodeRunOptions run_options = mmay_bytecode_run_default_options();
        MarimayMirInterpretStatus mir_status;
        MarimayBytecodeRunStatus bytecode_status;
        memset(&bytecode, 0, sizeof(bytecode));
        memset(&decoded, 0, sizeof(decoded));
        memset(&encoded, 0, sizeof(encoded));
        memset(mir_arguments, 0, sizeof(mir_arguments));
        memset(&mir_result, 0, sizeof(mir_result));
        memset(bytecode_arguments, 0, sizeof(bytecode_arguments));
        memset(&bytecode_result, 0, sizeof(bytecode_result));
        memset(&trap, 0, sizeof(trap));
        MMAY_EXPECT(mmay_test_mir_bytecode_build_arithmetic(
            &mir,
            cases[index].kind
        ));
        MMAY_EXPECT(mmay_mir_lower_bytecode_function(
            &mir,
            mmay_system_allocator(),
            mmay_test_mir_bytecode_options(),
            &bytecode
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        if (mmay_test_mir_bytecode_expected_opcode(cases[index].kind)
            != MMAY_OPCODE_INVALID) {
            const MarimayBytecodeInstruction *instruction =
                mmay_test_mir_bytecode_at(&bytecode, 0, 0);
            MMAY_EXPECT(instruction != NULL);
            if (instruction != NULL) {
                MMAY_EXPECT(instruction->opcode
                    == mmay_test_mir_bytecode_expected_opcode(cases[index].kind));
                MMAY_EXPECT(instruction->span.source == 2);
                MMAY_EXPECT(instruction->span.start == 30);
                MMAY_EXPECT(instruction->span.end == 31);
            }
        }
        MMAY_EXPECT(mmay_bytecode_encode_function(
            &bytecode,
            mmay_system_allocator(),
            &encoded
        ) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_decode_function(
            encoded.data,
            encoded.length,
            mmay_system_allocator(),
            mmay_bytecode_function_default_options(),
            &decoded
        ) == MMAY_BYTECODE_CODEC_OK);
        mir_arguments[0].type = MMAY_TYPE_I64;
        mir_arguments[0].as.i64 = cases[index].left;
        mir_arguments[1].type = MMAY_TYPE_I64;
        mir_arguments[1].as.i64 = cases[index].right;
        bytecode_arguments[0].type = MMAY_BYTECODE_TYPE_I64;
        bytecode_arguments[0].as.i64 = cases[index].left;
        bytecode_arguments[1].type = MMAY_BYTECODE_TYPE_I64;
        bytecode_arguments[1].as.i64 = cases[index].right;
        mir_status = mmay_mir_interpret(
            &mir,
            mir_arguments,
            2,
            mmay_system_allocator(),
            mmay_mir_interpret_default_options(),
            &mir_result
        );
        run_options.trap = &trap;
        bytecode_status = mmay_bytecode_run(
            &decoded,
            bytecode_arguments,
            2,
            mmay_system_allocator(),
            run_options,
            &bytecode_result
        );
        MMAY_EXPECT(mir_status == mmay_test_mir_bytecode_vm_status(
            bytecode_status
        ));
        if (mir_status == MMAY_MIR_INTERPRET_OK) {
            MMAY_EXPECT(mir_result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(bytecode_result.type == MMAY_BYTECODE_TYPE_I64);
            MMAY_EXPECT(mir_result.as.i64 == bytecode_result.as.i64);
            MMAY_EXPECT(!trap.has_span);
        } else {
            MMAY_EXPECT(trap.has_span);
            MMAY_EXPECT(trap.span.source == 2);
            MMAY_EXPECT(trap.span.start == 30 && trap.span.end == 31);
        }
        mmay_bytecode_function_dispose(&decoded);
        mmay_owned_bytes_dispose(&encoded);
        mmay_bytecode_function_dispose(&bytecode);
        mmay_mir_function_dispose(&mir);
    }
}


void mmay_test_mir_bytecode_model(void) {
    mmay_test_mir_bytecode_scalar_mapping();
    mmay_test_mir_bytecode_void_and_failures();
    mmay_test_mir_bytecode_options_and_allocations();
    mmay_test_mir_bytecode_arithmetic_differential();
}
