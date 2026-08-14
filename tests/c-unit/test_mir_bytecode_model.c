#include "test_mir_bytecode_support.h"

#include <limits.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"

static bool ruda_test_mir_bytecode_build_scalar(
    RudaMirFunction *function,
    RudaAllocator allocator
) {
    static const RudaTypeId parameters[] = {RUDA_TYPE_I64};
    RudaMirFunctionOptions options = ruda_mir_function_default_options();
    RudaMirBlockId entry = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId when_true = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId when_false = RUDA_MIR_NO_BLOCK;
    RudaMirRegister constant = RUDA_MIR_NO_REGISTER;
    RudaMirRegister copied = RUDA_MIR_NO_REGISTER;
    RudaMirRegister arithmetic = RUDA_MIR_NO_REGISTER;
    RudaMirRegister condition = RUDA_MIR_NO_REGISTER;
    RudaMirRegister negated = RUDA_MIR_NO_REGISTER;
    RudaMirRegister equality = RUDA_MIR_NO_REGISTER;
    RudaMirRegister bool_equality = RUDA_MIR_NO_REGISTER;
    RudaMirRegister boolean_constant = RUDA_MIR_NO_REGISTER;
    RudaMirRegister boolean_not = RUDA_MIR_NO_REGISTER;
    RudaMirRegister subtraction = RUDA_MIR_NO_REGISTER;
    RudaMirRegister multiplication = RUDA_MIR_NO_REGISTER;
    RudaMirRegister division = RUDA_MIR_NO_REGISTER;
    RudaMirRegister remainder = RUDA_MIR_NO_REGISTER;
    RudaMirRegister less_equal = RUDA_MIR_NO_REGISTER;
    RudaMirRegister bitwise_not = RUDA_MIR_NO_REGISTER;
    RudaMirRegister bitwise_and = RUDA_MIR_NO_REGISTER;
    RudaMirRegister bitwise_xor = RUDA_MIR_NO_REGISTER;
    RudaMirRegister bitwise_or = RUDA_MIR_NO_REGISTER;
    RudaMirRegister shift_left = RUDA_MIR_NO_REGISTER;
    RudaMirRegister shift_right = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;

    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, allocator, (RudaSymbolId){{0}}, parameters,
            1, RUDA_TYPE_I64, options
        ) || !ruda_mir_add_register(function, RUDA_TYPE_I64, &constant)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &copied)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &arithmetic)
        || !ruda_mir_add_register(function, RUDA_TYPE_BOOL, &condition)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &negated)
        || !ruda_mir_add_register(function, RUDA_TYPE_BOOL, &equality)
        || !ruda_mir_add_register(function, RUDA_TYPE_BOOL, &bool_equality)
        || !ruda_mir_add_register(function, RUDA_TYPE_BOOL, &boolean_constant)
        || !ruda_mir_add_register(function, RUDA_TYPE_BOOL, &boolean_not)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &subtraction)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &multiplication)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &division)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &remainder)
        || !ruda_mir_add_register(function, RUDA_TYPE_BOOL, &less_equal)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &bitwise_not)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &bitwise_and)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &bitwise_xor)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &bitwise_or)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &shift_left)
        || !ruda_mir_add_register(function, RUDA_TYPE_I64, &shift_right)
        || !ruda_mir_add_block(function, &entry)
        || !ruda_mir_add_block(function, &when_true)
        || !ruda_mir_add_block(function, &when_false)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_CONSTANT_I64, constant, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 7, false, 10
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_COPY, copied, constant, RUDA_MIR_NO_REGISTER, 0, false, 11
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_ADD_CHECKED_I64, arithmetic, copied, 0, 0, false, 12
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_LESS_I64, condition, 0, arithmetic, 0, false, 13
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_NEGATE_CHECKED_I64, negated, arithmetic,
        RUDA_MIR_NO_REGISTER, 0, false, 14
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_EQUAL_I64, equality, negated, arithmetic, 0, false, 15
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_EQUAL_BOOL, bool_equality, condition, equality, 0, false, 16
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_CONSTANT_BOOL, boolean_constant, RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER, 0, true, 17
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_NOT_BOOL, boolean_not, boolean_constant,
        RUDA_MIR_NO_REGISTER, 0, false, 18
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_SUBTRACT_CHECKED_I64, subtraction, arithmetic, copied,
        0, false, 19
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_MULTIPLY_CHECKED_I64, multiplication, copied, copied,
        0, false, 20
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_DIVIDE_CHECKED_I64, division, multiplication, copied,
        0, false, 21
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_REMAINDER_CHECKED_I64, remainder, multiplication, copied,
        0, false, 22
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_LESS_EQUAL_I64, less_equal, copied, arithmetic,
        0, false, 23
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_BITWISE_NOT_I64, bitwise_not, copied,
        RUDA_MIR_NO_REGISTER, 0, false, 24
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_BITWISE_AND_I64, bitwise_and, copied, constant,
        0, false, 25
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_BITWISE_XOR_I64, bitwise_xor, copied, constant,
        0, false, 26
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_BITWISE_OR_I64, bitwise_or, copied, constant,
        0, false, 27
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_SHIFT_LEFT_CHECKED_I64, shift_left, copied, constant,
        0, false, 28
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)) {
        goto failure;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        RUDA_MIR_SHIFT_RIGHT_I64, shift_right, copied, constant,
        0, false, 29
    );
    if (!ruda_mir_append_instruction(function, entry, &instruction)
        || !ruda_mir_set_terminator(function, entry,
            ruda_test_mir_bytecode_terminator(
                RUDA_MIR_BRANCH, bool_equality, when_true, when_false, 30
            ))
        || !ruda_mir_set_terminator(function, when_true,
            ruda_test_mir_bytecode_terminator(
                RUDA_MIR_RETURN, arithmetic, RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK, 31
            ))
        || !ruda_mir_set_terminator(function, when_false,
            ruda_test_mir_bytecode_terminator(
                RUDA_MIR_GOTO, RUDA_MIR_NO_REGISTER, when_true,
                RUDA_MIR_NO_BLOCK, 32
            ))) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static void ruda_test_mir_bytecode_scalar_mapping(void) {
    RudaMirFunction input;
    RudaBytecodeFunction output;
    RudaBytecodeFunction decoded;
    RudaOwnedBytes encoded;
    const RudaBytecodeInstruction *instruction;
    RudaMirBytecodeLowerOptions options = ruda_test_mir_bytecode_options();
    size_t index = 0;

    memset(&output, 0, sizeof(output));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_test_mir_bytecode_build_scalar(
        &input,
        ruda_system_allocator()
    ));
    RUDA_EXPECT(ruda_mir_verify(&input) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        &input, ruda_system_allocator(), options, &output
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(output.result_type == RUDA_BYTECODE_TYPE_I64);
    RUDA_EXPECT(output.source_count == 3);
    RUDA_EXPECT(output.parameter_count == 1);
    RUDA_EXPECT(output.register_types.length == input.register_types.length);
    RUDA_EXPECT(output.blocks.length == input.blocks.length);
    for (index = 0; index < output.register_types.length; index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &output.register_types, index
        );
        RUDA_EXPECT(type != NULL);
        if (type != NULL) {
            if (index == 4 || index == 6 || index == 7 || index == 8
                || index == 9 || index == 14) {
                RUDA_EXPECT(*type == RUDA_BYTECODE_TYPE_BOOL);
            } else {
                RUDA_EXPECT(*type == RUDA_BYTECODE_TYPE_I64);
            }
        }
    }
    instruction = ruda_test_mir_bytecode_at(&output, 0, 0);
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_CONST_I64);
    RUDA_EXPECT(instruction != NULL && instruction->operands[0].unsigned_value == 1);
    RUDA_EXPECT(instruction != NULL && instruction->operands[1].signed_value == 7);
    RUDA_EXPECT(instruction != NULL && instruction->span.source == 2
        && instruction->span.start == 10 && instruction->span.end == 11);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 1);
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_COPY);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 2);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_ADD_CHECKED);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 3);
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_I64_LESS);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 4);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_NEGATE_CHECKED);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 5);
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_I64_EQUAL);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 6);
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_BOOL_EQUAL);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 7);
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_CONST_BOOL);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 8);
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_BOOL_NOT);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 9);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_SUBTRACT_CHECKED);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 10);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_MULTIPLY_CHECKED);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 11);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_DIVIDE_CHECKED);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 12);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_REMAINDER_CHECKED);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 13);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_LESS_EQUAL);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 14);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_BITWISE_NOT);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 15);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_BITWISE_AND);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 16);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_BITWISE_XOR);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 17);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_BITWISE_OR);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 18);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 19);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_I64_SHIFT_RIGHT);
    instruction = ruda_test_mir_bytecode_at(&output, 0, 20);
    RUDA_EXPECT(instruction != NULL
        && instruction->opcode == RUDA_OPCODE_BRANCH_BOOL
        && instruction->operands[0].unsigned_value == 7
        && instruction->operands[1].unsigned_value == 1
        && instruction->operands[2].unsigned_value == 2);
    instruction = ruda_test_mir_bytecode_at(&output, 1, 0);
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_RETURN);
    instruction = ruda_test_mir_bytecode_at(&output, 2, 0);
    RUDA_EXPECT(instruction != NULL && instruction->opcode == RUDA_OPCODE_JUMP);
    RUDA_EXPECT(ruda_bytecode_verify(&output) == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &output,
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
    {
        static const int64_t values[] = {-7, 5};
        size_t value_index = 0;
        for (value_index = 0;
            value_index < sizeof(values) / sizeof(values[0]);
            value_index += 1) {
            RudaMirValue mir_argument;
            RudaMirValue mir_result;
            RudaBytecodeValue bytecode_argument;
            RudaBytecodeValue bytecode_result;
            memset(&mir_argument, 0, sizeof(mir_argument));
            memset(&mir_result, 0, sizeof(mir_result));
            memset(&bytecode_argument, 0, sizeof(bytecode_argument));
            memset(&bytecode_result, 0, sizeof(bytecode_result));
            mir_argument.type = RUDA_TYPE_I64;
            mir_argument.as.i64 = values[value_index];
            bytecode_argument.type = RUDA_BYTECODE_TYPE_I64;
            bytecode_argument.as.i64 = values[value_index];
            RUDA_EXPECT(ruda_mir_interpret(
                &input,
                &mir_argument,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &mir_result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(ruda_bytecode_run(
                &decoded,
                &bytecode_argument,
                1,
                ruda_system_allocator(),
                ruda_bytecode_run_default_options(),
                &bytecode_result
            ) == RUDA_BYTECODE_RUN_OK);
            RUDA_EXPECT(mir_result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(bytecode_result.type == RUDA_BYTECODE_TYPE_I64);
            RUDA_EXPECT(mir_result.as.i64 == bytecode_result.as.i64);
            {
                size_t exact_steps = values[value_index] == -7 ? 22 : 23;
                RudaMirInterpretOptions mir_options =
                    ruda_mir_interpret_default_options();
                RudaBytecodeRunOptions bytecode_options =
                    ruda_bytecode_run_default_options();
                mir_options.max_steps = exact_steps;
                bytecode_options.max_steps = exact_steps;
                RUDA_EXPECT(ruda_mir_interpret(
                    &input,
                    &mir_argument,
                    1,
                    ruda_system_allocator(),
                    mir_options,
                    &mir_result
                ) == RUDA_MIR_INTERPRET_OK);
                RUDA_EXPECT(ruda_bytecode_run(
                    &decoded,
                    &bytecode_argument,
                    1,
                    ruda_system_allocator(),
                    bytecode_options,
                    &bytecode_result
                ) == RUDA_BYTECODE_RUN_OK);
                mir_options.max_steps -= 1;
                bytecode_options.max_steps -= 1;
                RUDA_EXPECT(ruda_mir_interpret(
                    &input,
                    &mir_argument,
                    1,
                    ruda_system_allocator(),
                    mir_options,
                    &mir_result
                ) == RUDA_MIR_INTERPRET_STEP_LIMIT);
                RUDA_EXPECT(ruda_bytecode_run(
                    &decoded,
                    &bytecode_argument,
                    1,
                    ruda_system_allocator(),
                    bytecode_options,
                    &bytecode_result
                ) == RUDA_BYTECODE_RUN_STEP_LIMIT);
                {
                    size_t mir_shared_steps = exact_steps;
                    size_t bytecode_shared_steps = exact_steps;
                    mir_options = ruda_mir_interpret_default_options();
                    bytecode_options = ruda_bytecode_run_default_options();
                    mir_options.remaining_shared_steps = &mir_shared_steps;
                    bytecode_options.remaining_shared_steps =
                        &bytecode_shared_steps;
                    RUDA_EXPECT(ruda_mir_interpret(
                        &input,
                        &mir_argument,
                        1,
                        ruda_system_allocator(),
                        mir_options,
                        &mir_result
                    ) == RUDA_MIR_INTERPRET_OK);
                    RUDA_EXPECT(ruda_bytecode_run(
                        &decoded,
                        &bytecode_argument,
                        1,
                        ruda_system_allocator(),
                        bytecode_options,
                        &bytecode_result
                    ) == RUDA_BYTECODE_RUN_OK);
                    RUDA_EXPECT(mir_shared_steps == 0);
                    RUDA_EXPECT(bytecode_shared_steps == 0);

                    mir_shared_steps = exact_steps - 1;
                    bytecode_shared_steps = exact_steps - 1;
                    RUDA_EXPECT(ruda_mir_interpret(
                        &input,
                        &mir_argument,
                        1,
                        ruda_system_allocator(),
                        mir_options,
                        &mir_result
                    ) == RUDA_MIR_INTERPRET_STEP_LIMIT);
                    RUDA_EXPECT(ruda_bytecode_run(
                        &decoded,
                        &bytecode_argument,
                        1,
                        ruda_system_allocator(),
                        bytecode_options,
                        &bytecode_result
                    ) == RUDA_BYTECODE_RUN_STEP_LIMIT);
                    RUDA_EXPECT(mir_shared_steps == 0);
                    RUDA_EXPECT(bytecode_shared_steps == 0);
                }
            }
        }
    }
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&output);
    ruda_mir_function_dispose(&input);
}

static void ruda_test_mir_bytecode_void_and_failures(void) {
    RudaMirFunction input;
    RudaBytecodeFunction output;
    RudaMirFunctionOptions mir_options = ruda_mir_function_default_options();
    RudaMirBytecodeLowerOptions options = ruda_test_mir_bytecode_options();
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirRegister string_register = RUDA_MIR_NO_REGISTER;
    RudaMirStringId string_id = RUDA_MIR_NO_STRING;
    RudaMirCallId call_id = RUDA_MIR_NO_CALL;
    RudaMirInstruction instruction;

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    RUDA_EXPECT(ruda_mir_function_init(&input, ruda_system_allocator(),
        (RudaSymbolId){{0}}, NULL, 0, RUDA_TYPE_VOID, mir_options));
    RUDA_EXPECT(ruda_mir_add_block(&input, &block));
    RUDA_EXPECT(ruda_mir_set_terminator(&input, block,
        ruda_test_mir_bytecode_terminator(RUDA_MIR_RETURN, RUDA_MIR_NO_REGISTER,
            RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 20)));
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input, ruda_system_allocator(),
        options, &output) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_test_mir_bytecode_at(&output, 0, 0)->opcode
        == RUDA_OPCODE_RETURN_VOID);
    ruda_bytecode_function_dispose(&output);
    ruda_mir_function_dispose(&input);

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    RUDA_EXPECT(ruda_mir_function_init(&input, ruda_system_allocator(),
        (RudaSymbolId){{0}}, NULL, 0, RUDA_TYPE_VOID, mir_options));
    RUDA_EXPECT(ruda_mir_add_register(&input, RUDA_TYPE_STRING,
        &string_register));
    RUDA_EXPECT(ruda_mir_add_block(&input, &block));
    RUDA_EXPECT(ruda_mir_add_string_constant(&input,
        (const unsigned char *)"x", 1, &string_id));
    instruction = ruda_test_mir_bytecode_instruction(RUDA_MIR_CONSTANT_STRING,
        string_register, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_REGISTER,
        0, false, 21);
    instruction.string_constant = string_id;
    RUDA_EXPECT(ruda_mir_append_instruction(&input, block, &instruction));
    RUDA_EXPECT(ruda_mir_set_terminator(&input, block,
        ruda_test_mir_bytecode_terminator(RUDA_MIR_RETURN, RUDA_MIR_NO_REGISTER,
            RUDA_MIR_NO_BLOCK, RUDA_MIR_NO_BLOCK, 22)));
    RUDA_EXPECT(ruda_mir_verify(&input) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input, ruda_system_allocator(),
        options, &output) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(output.string_constants.length == 1
        && output.string_bytes == 1);
    {
        const RudaBytecodeInstruction *bytecode_instruction =
            ruda_test_mir_bytecode_at(&output, 0, 0);
        const RudaBytecodeStringConstant *constant =
            ruda_bytecode_string_constant(&output, 0);
        RUDA_EXPECT(bytecode_instruction != NULL && constant != NULL);
        if (bytecode_instruction != NULL) {
            RUDA_EXPECT(bytecode_instruction->opcode == RUDA_OPCODE_CONST_STRING);
            RUDA_EXPECT(bytecode_instruction->operands[0].unsigned_value == 0);
            RUDA_EXPECT(bytecode_instruction->operands[1].unsigned_value == 0);
            RUDA_EXPECT(bytecode_instruction->span.source == 2);
            RUDA_EXPECT(bytecode_instruction->span.start == 21);
            RUDA_EXPECT(bytecode_instruction->span.end == 22);
        }
        if (constant != NULL) {
            RUDA_EXPECT(constant->value.length == 1
                && constant->value.data[0] == 'x');
        }
    }
    ruda_bytecode_function_dispose(&output);
    ruda_mir_function_dispose(&input);

    {
        static const RudaTypeId parameters[] = {RUDA_TYPE_I64};
        RudaMirRegister result = RUDA_MIR_NO_REGISTER;
        memset(&input, 0, sizeof(input));
        RUDA_EXPECT(ruda_mir_function_init(
            &input,
            ruda_system_allocator(),
            (RudaSymbolId){{0}},
            parameters,
            1,
            RUDA_TYPE_I64,
            mir_options
        ));
        RUDA_EXPECT(ruda_mir_add_register(&input, RUDA_TYPE_I64, &result));
        RUDA_EXPECT(ruda_mir_add_block(&input, &block));
        instruction = ruda_test_mir_bytecode_instruction(
            RUDA_MIR_BITWISE_NOT_I64,
            result,
            0,
            RUDA_MIR_NO_REGISTER,
            0,
            false,
            21
        );
        RUDA_EXPECT(ruda_mir_append_instruction(&input, block, &instruction));
        RUDA_EXPECT(ruda_mir_set_terminator(
            &input,
            block,
            ruda_test_mir_bytecode_terminator(
                RUDA_MIR_RETURN,
                result,
                RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK,
                22
            )
        ));
    }
    RUDA_EXPECT(ruda_mir_verify(&input) == RUDA_MIR_VERIFY_OK);
    memset(&output, 0, sizeof(output));
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input, ruda_system_allocator(),
        options, &output) == RUDA_MIR_BYTECODE_LOWER_OK);
    {
        const RudaBytecodeInstruction *bytecode_instruction =
            ruda_test_mir_bytecode_at(&output, 0, 0);
        RUDA_EXPECT(bytecode_instruction != NULL);
        if (bytecode_instruction != NULL) {
            RUDA_EXPECT(bytecode_instruction->opcode == RUDA_OPCODE_I64_BITWISE_NOT);
            RUDA_EXPECT(bytecode_instruction->operands[0].unsigned_value == 1);
            RUDA_EXPECT(bytecode_instruction->operands[1].unsigned_value == 0);
            RUDA_EXPECT(bytecode_instruction->span.source == 2);
            RUDA_EXPECT(bytecode_instruction->span.start == 21);
            RUDA_EXPECT(bytecode_instruction->span.end == 22);
        }
    }
    ruda_bytecode_function_dispose(&output);
    ruda_mir_function_dispose(&input);

    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));
    RUDA_EXPECT(ruda_mir_function_init(&input, ruda_system_allocator(),
        (RudaSymbolId){{0}}, NULL, 0, RUDA_TYPE_I64, mir_options));
    RUDA_EXPECT(ruda_mir_add_block(&input, &block));
    RUDA_EXPECT(ruda_mir_add_call(&input, (RudaSymbolId){{1}}, RUDA_TYPE_NEVER,
        NULL, 0, &call_id));
    {
        RudaMirTerminator terminator = ruda_test_mir_bytecode_terminator(
            RUDA_MIR_CALL_NEVER, RUDA_MIR_NO_REGISTER, RUDA_MIR_NO_BLOCK,
            RUDA_MIR_NO_BLOCK, 23
        );
        terminator.call = call_id;
        RUDA_EXPECT(ruda_mir_set_terminator(&input, block, terminator));
    }
    RUDA_EXPECT(ruda_mir_verify(&input) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input, ruda_system_allocator(),
        options, &output) == RUDA_MIR_BYTECODE_LOWER_OK);
    {
        const RudaBytecodeInstruction *bytecode_instruction =
            ruda_test_mir_bytecode_at(&output, 0, 0);
        RUDA_EXPECT(bytecode_instruction != NULL);
        if (bytecode_instruction != NULL) {
            RUDA_EXPECT(bytecode_instruction->opcode
                == RUDA_OPCODE_CALL_DIRECT_NEVER);
            RUDA_EXPECT(bytecode_instruction->operands[0].unsigned_value == 0);
            RUDA_EXPECT(bytecode_instruction->span.source == 2);
            RUDA_EXPECT(bytecode_instruction->span.start == 23);
            RUDA_EXPECT(bytecode_instruction->span.end == 24);
        }
    }
    ruda_bytecode_function_dispose(&output);
    ruda_mir_function_dispose(&input);

    RUDA_EXPECT(ruda_test_mir_bytecode_build_scalar(
        &input,
        ruda_system_allocator()
    ));
    ((RudaMirInstruction *)ruda_vector_at(
        &((RudaMirBlock *)ruda_vector_at(&input.blocks, 0))->instructions, 0
    ))->kind = (RudaMirInstructionKind)999;
    memset(&output, 0, sizeof(output));
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input, ruda_system_allocator(),
        options, &output) == RUDA_MIR_BYTECODE_LOWER_INVALID_MIR);
    ruda_mir_function_dispose(&input);
}

static void ruda_test_mir_bytecode_options_and_allocations(void) {
    RudaMirFunction input;
    RudaBytecodeFunction output;
    RudaMirBytecodeLowerOptions options = ruda_test_mir_bytecode_options();
    size_t failure_index = 0;
    bool reached_success = false;

    RUDA_EXPECT(ruda_test_mir_bytecode_build_scalar(
        &input,
        ruda_system_allocator()
    ));
    memset(&output, 0, sizeof(output));
    options.source_count = 2;
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input, ruda_system_allocator(),
        options, &output) == RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT);
    options = ruda_test_mir_bytecode_options();
    options.function_options.max_blocks = 0;
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input, ruda_system_allocator(),
        options, &output) == RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT);
    options = ruda_test_mir_bytecode_options();
    options.function_options.max_blocks = 2;
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input, ruda_system_allocator(),
        options, &output) == RUDA_MIR_BYTECODE_LOWER_LIMIT_REACHED);
    options = ruda_test_mir_bytecode_options();
    options.function_options.max_verify_work = 1;
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input, ruda_system_allocator(),
        options, &output) == RUDA_MIR_BYTECODE_LOWER_LIMIT_REACHED);
    options = ruda_test_mir_bytecode_options();
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(NULL, ruda_system_allocator(),
        options, &output) == RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT);

    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input, ruda_system_allocator(),
        options, &output) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(output.initialized);
    options.source_count = 2;
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(&input, ruda_system_allocator(),
        options, &output) == RUDA_MIR_BYTECODE_LOWER_INVALID_ARGUMENT);
    RUDA_EXPECT(!output.initialized);
    options = ruda_test_mir_bytecode_options();

    for (failure_index = 0; failure_index < 256 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaMirBytecodeLowerStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = ruda_mir_lower_bytecode_function(
            &input, ruda_fail_allocator(&state), options, &output
        );
        if (status == RUDA_MIR_BYTECODE_LOWER_OK) {
            reached_success = true;
            ruda_bytecode_function_dispose(&output);
        } else {
            RUDA_EXPECT(status == RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED);
            RUDA_EXPECT(!output.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_mir_function_dispose(&input);

    {
        RudaFailAllocator input_state;
        size_t input_live_allocations = 0;
        size_t input_live_bytes = 0;
        bool verifier_reached_success = false;
        ruda_fail_allocator_init(
            &input_state,
            ruda_system_allocator(),
            SIZE_MAX
        );
        RUDA_EXPECT(ruda_test_mir_bytecode_build_scalar(
            &input,
            ruda_fail_allocator(&input_state)
        ));
        input_live_allocations = input_state.live_allocations;
        input_live_bytes = input_state.live_bytes;
        for (failure_index = 0;
            failure_index < 32 && !verifier_reached_success;
            failure_index += 1) {
            RudaMirBytecodeLowerStatus status;
            input_state.failure_index = input_state.attempts + failure_index;
            memset(&output, 0, sizeof(output));
            status = ruda_mir_lower_bytecode_function(
                &input,
                ruda_system_allocator(),
                options,
                &output
            );
            if (status == RUDA_MIR_BYTECODE_LOWER_OK) {
                verifier_reached_success = true;
                ruda_bytecode_function_dispose(&output);
            } else {
                RUDA_EXPECT(
                    status == RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED
                );
                RUDA_EXPECT(!output.initialized);
            }
            RUDA_EXPECT(input_state.live_allocations == input_live_allocations);
            RUDA_EXPECT(input_state.live_bytes == input_live_bytes);
        }
        RUDA_EXPECT(verifier_reached_success);
        ruda_mir_function_dispose(&input);
        RUDA_EXPECT(input_state.live_allocations == 0);
        RUDA_EXPECT(input_state.live_bytes == 0);
    }
}

static bool ruda_test_mir_bytecode_build_arithmetic(
    RudaMirFunction *function,
    RudaMirInstructionKind kind
) {
    static const RudaTypeId parameters[] = {RUDA_TYPE_I64, RUDA_TYPE_I64};
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    RudaMirFunctionOptions options = ruda_mir_function_default_options();
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function,
            ruda_system_allocator(),
            (RudaSymbolId){{0}},
            parameters,
            2,
            RUDA_TYPE_I64,
            options
        ) || !ruda_mir_add_register(function, RUDA_TYPE_I64, &result)
        || !ruda_mir_add_block(function, &block)) {
        ruda_mir_function_dispose(function);
        return false;
    }
    instruction = ruda_test_mir_bytecode_instruction(
        kind,
        result,
        0,
        1,
        0,
        false,
        30
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function,
            block,
            ruda_test_mir_bytecode_terminator(
                RUDA_MIR_RETURN,
                result,
                RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK,
                31
            )
        )) {
        ruda_mir_function_dispose(function);
        return false;
    }
    return true;
}

static RudaMirInterpretStatus ruda_test_mir_bytecode_vm_status(
    RudaBytecodeRunStatus status
) {
    if (status == RUDA_BYTECODE_RUN_OK) {
        return RUDA_MIR_INTERPRET_OK;
    }
    if (status == RUDA_BYTECODE_RUN_OVERFLOW) {
        return RUDA_MIR_INTERPRET_OVERFLOW;
    }
    if (status == RUDA_BYTECODE_RUN_DIVISION_BY_ZERO) {
        return RUDA_MIR_INTERPRET_DIVISION_BY_ZERO;
    }
    if (status == RUDA_BYTECODE_RUN_INVALID_SHIFT) {
        return RUDA_MIR_INTERPRET_INVALID_SHIFT;
    }
    return RUDA_MIR_INTERPRET_INVALID_MIR;
}

static RudaOpcode ruda_test_mir_bytecode_expected_opcode(
    RudaMirInstructionKind kind
) {
    switch (kind) {
        case RUDA_MIR_BITWISE_NOT_I64:
            return RUDA_OPCODE_I64_BITWISE_NOT;
        case RUDA_MIR_BITWISE_AND_I64:
            return RUDA_OPCODE_I64_BITWISE_AND;
        case RUDA_MIR_BITWISE_XOR_I64:
            return RUDA_OPCODE_I64_BITWISE_XOR;
        case RUDA_MIR_BITWISE_OR_I64:
            return RUDA_OPCODE_I64_BITWISE_OR;
        case RUDA_MIR_SHIFT_LEFT_CHECKED_I64:
            return RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED;
        case RUDA_MIR_SHIFT_RIGHT_I64:
            return RUDA_OPCODE_I64_SHIFT_RIGHT;
        default:
            return RUDA_OPCODE_INVALID;
    }
}

static void ruda_test_mir_bytecode_arithmetic_differential(void) {
    static const struct {
        RudaMirInstructionKind kind;
        int64_t left;
        int64_t right;
    } cases[] = {
        {RUDA_MIR_ADD_CHECKED_I64, 2, 3},
        {RUDA_MIR_ADD_CHECKED_I64, INT64_MAX, 1},
        {RUDA_MIR_ADD_CHECKED_I64, INT64_MIN, -1},
        {RUDA_MIR_SUBTRACT_CHECKED_I64, 7, 3},
        {RUDA_MIR_SUBTRACT_CHECKED_I64, INT64_MIN, 1},
        {RUDA_MIR_SUBTRACT_CHECKED_I64, INT64_MAX, -1},
        {RUDA_MIR_MULTIPLY_CHECKED_I64, -7, 3},
        {RUDA_MIR_MULTIPLY_CHECKED_I64, INT64_MAX, 2},
        {RUDA_MIR_MULTIPLY_CHECKED_I64, INT64_MIN, -1},
        {RUDA_MIR_DIVIDE_CHECKED_I64, 7, 3},
        {RUDA_MIR_DIVIDE_CHECKED_I64, -7, 3},
        {RUDA_MIR_DIVIDE_CHECKED_I64, 7, -3},
        {RUDA_MIR_DIVIDE_CHECKED_I64, -7, -3},
        {RUDA_MIR_DIVIDE_CHECKED_I64, 1, 0},
        {RUDA_MIR_DIVIDE_CHECKED_I64, INT64_MIN, -1},
        {RUDA_MIR_REMAINDER_CHECKED_I64, 7, 3},
        {RUDA_MIR_REMAINDER_CHECKED_I64, -7, 3},
        {RUDA_MIR_REMAINDER_CHECKED_I64, 7, -3},
        {RUDA_MIR_REMAINDER_CHECKED_I64, -7, -3},
        {RUDA_MIR_REMAINDER_CHECKED_I64, 1, 0},
        {RUDA_MIR_REMAINDER_CHECKED_I64, INT64_MIN, -1},
        {RUDA_MIR_NEGATE_CHECKED_I64, 7, 0},
        {RUDA_MIR_NEGATE_CHECKED_I64, INT64_MIN, 0},
        {RUDA_MIR_BITWISE_NOT_I64, 0, 0},
        {RUDA_MIR_BITWISE_NOT_I64, -1, 0},
        {RUDA_MIR_BITWISE_AND_I64, -1, INT64_C(0x55)},
        {RUDA_MIR_BITWISE_XOR_I64, -1, INT64_C(0x55)},
        {RUDA_MIR_BITWISE_OR_I64, INT64_MIN, 1},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, -2, 1},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, 62},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, -1, 63},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, -2, 62},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, -2, 63},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 0, 63},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, INT64_MIN, 0},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, INT64_MAX, 0},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, INT64_MAX, 1},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, INT64_MIN, 1},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, -1},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, 64},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, INT64_MIN},
        {RUDA_MIR_SHIFT_LEFT_CHECKED_I64, 1, INT64_MAX},
        {RUDA_MIR_SHIFT_RIGHT_I64, -7, 0},
        {RUDA_MIR_SHIFT_RIGHT_I64, -1, 1},
        {RUDA_MIR_SHIFT_RIGHT_I64, INT64_MIN, 63},
        {RUDA_MIR_SHIFT_RIGHT_I64, INT64_MAX, 62},
        {RUDA_MIR_SHIFT_RIGHT_I64, 1, 63},
        {RUDA_MIR_SHIFT_RIGHT_I64, 1, -1},
        {RUDA_MIR_SHIFT_RIGHT_I64, 1, 64},
        {RUDA_MIR_SHIFT_RIGHT_I64, 1, INT64_MIN},
        {RUDA_MIR_SHIFT_RIGHT_I64, 1, INT64_MAX}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaMirFunction mir;
        RudaBytecodeFunction bytecode;
        RudaBytecodeFunction decoded;
        RudaMirValue mir_arguments[2];
        RudaMirValue mir_result;
        RudaBytecodeValue bytecode_arguments[2];
        RudaBytecodeValue bytecode_result;
        RudaBytecodeTrap trap;
        RudaOwnedBytes encoded;
        RudaBytecodeRunOptions run_options = ruda_bytecode_run_default_options();
        RudaMirInterpretStatus mir_status;
        RudaBytecodeRunStatus bytecode_status;
        memset(&bytecode, 0, sizeof(bytecode));
        memset(&decoded, 0, sizeof(decoded));
        memset(&encoded, 0, sizeof(encoded));
        memset(mir_arguments, 0, sizeof(mir_arguments));
        memset(&mir_result, 0, sizeof(mir_result));
        memset(bytecode_arguments, 0, sizeof(bytecode_arguments));
        memset(&bytecode_result, 0, sizeof(bytecode_result));
        memset(&trap, 0, sizeof(trap));
        RUDA_EXPECT(ruda_test_mir_bytecode_build_arithmetic(
            &mir,
            cases[index].kind
        ));
        RUDA_EXPECT(ruda_mir_lower_bytecode_function(
            &mir,
            ruda_system_allocator(),
            ruda_test_mir_bytecode_options(),
            &bytecode
        ) == RUDA_MIR_BYTECODE_LOWER_OK);
        if (ruda_test_mir_bytecode_expected_opcode(cases[index].kind)
            != RUDA_OPCODE_INVALID) {
            const RudaBytecodeInstruction *instruction =
                ruda_test_mir_bytecode_at(&bytecode, 0, 0);
            RUDA_EXPECT(instruction != NULL);
            if (instruction != NULL) {
                RUDA_EXPECT(instruction->opcode
                    == ruda_test_mir_bytecode_expected_opcode(cases[index].kind));
                RUDA_EXPECT(instruction->span.source == 2);
                RUDA_EXPECT(instruction->span.start == 30);
                RUDA_EXPECT(instruction->span.end == 31);
            }
        }
        RUDA_EXPECT(ruda_bytecode_encode_function(
            &bytecode,
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
        mir_arguments[0].type = RUDA_TYPE_I64;
        mir_arguments[0].as.i64 = cases[index].left;
        mir_arguments[1].type = RUDA_TYPE_I64;
        mir_arguments[1].as.i64 = cases[index].right;
        bytecode_arguments[0].type = RUDA_BYTECODE_TYPE_I64;
        bytecode_arguments[0].as.i64 = cases[index].left;
        bytecode_arguments[1].type = RUDA_BYTECODE_TYPE_I64;
        bytecode_arguments[1].as.i64 = cases[index].right;
        mir_status = ruda_mir_interpret(
            &mir,
            mir_arguments,
            2,
            ruda_system_allocator(),
            ruda_mir_interpret_default_options(),
            &mir_result
        );
        run_options.trap = &trap;
        bytecode_status = ruda_bytecode_run(
            &decoded,
            bytecode_arguments,
            2,
            ruda_system_allocator(),
            run_options,
            &bytecode_result
        );
        RUDA_EXPECT(mir_status == ruda_test_mir_bytecode_vm_status(
            bytecode_status
        ));
        if (mir_status == RUDA_MIR_INTERPRET_OK) {
            RUDA_EXPECT(mir_result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(bytecode_result.type == RUDA_BYTECODE_TYPE_I64);
            RUDA_EXPECT(mir_result.as.i64 == bytecode_result.as.i64);
            RUDA_EXPECT(!trap.has_span);
        } else {
            RUDA_EXPECT(trap.has_span);
            RUDA_EXPECT(trap.span.source == 2);
            RUDA_EXPECT(trap.span.start == 30 && trap.span.end == 31);
        }
        ruda_bytecode_function_dispose(&decoded);
        ruda_owned_bytes_dispose(&encoded);
        ruda_bytecode_function_dispose(&bytecode);
        ruda_mir_function_dispose(&mir);
    }
}


void ruda_test_mir_bytecode_model(void) {
    ruda_test_mir_bytecode_scalar_mapping();
    ruda_test_mir_bytecode_void_and_failures();
    ruda_test_mir_bytecode_options_and_allocations();
    ruda_test_mir_bytecode_arithmetic_differential();
}
