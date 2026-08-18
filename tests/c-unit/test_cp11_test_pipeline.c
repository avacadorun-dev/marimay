#include "test_bytecode_support.h"

static bool ruda_test_cp11_bytecode_build(RudaBytecodeFunction *function) {
    static const unsigned char expected[] = {'o', 'k', '\n'};
    uint32_t expected_register = UINT32_MAX;
    uint32_t marker = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t wrong = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint32_t string_id = UINT32_MAX;
    size_t span = 100;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(
            function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_VOID,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_STRING, &expected_register
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_BOOL, &marker
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_VOID, &result
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &wrong
        ) || !ruda_bytecode_add_string_constant(
            function, expected, sizeof(expected), &string_id
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_CONST_STRING,
            expected_register, string_id, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_CONST_I64,
            wrong, 1, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_TEST_OUTPUT_BEGIN,
            marker, expected_register, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_TEST_OUTPUT_END,
            result, marker, 0, &span
        ) || !ruda_test_bytecode_append(
            function, block, RUDA_OPCODE_RETURN_VOID, 0, 0, 0, &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

void ruda_test_cp11_test_pipeline(void) {
    RudaBytecodeFunction function;
    RudaBytecodeInstruction *begin = NULL;
    RudaBytecodeInstruction *end = NULL;
    RUDA_EXPECT(ruda_test_cp11_bytecode_build(&function));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    begin = ruda_test_bytecode_at(&function, 0, 2);
    end = ruda_test_bytecode_at(&function, 0, 3);
    RUDA_EXPECT(begin != NULL && end != NULL);
    if (begin != NULL && end != NULL) {
        begin->operands[0].unsigned_value = 2;
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
        begin->operands[0].unsigned_value = 1;
        begin->operands[1].unsigned_value = 3;
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
        begin->operands[1].unsigned_value = 0;
        end->operands[0].unsigned_value = 1;
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
        end->operands[0].unsigned_value = 2;
        end->operands[1].unsigned_value = 0;
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
    }
    ruda_bytecode_function_dispose(&function);
}
