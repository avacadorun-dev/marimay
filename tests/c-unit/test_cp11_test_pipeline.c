#include "test_bytecode_support.h"

static bool mmay_test_cp11_bytecode_build(MarimayBytecodeFunction *function) {
    static const unsigned char expected[] = {'o', 'k', '\n'};
    uint32_t expected_register = UINT32_MAX;
    uint32_t marker = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t wrong = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint32_t string_id = UINT32_MAX;
    size_t span = 100;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(
            function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_VOID,
            1, NULL, 0, mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_STRING, &expected_register
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_BOOL, &marker
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_VOID, &result
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &wrong
        ) || !mmay_bytecode_add_string_constant(
            function, expected, sizeof(expected), &string_id
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_CONST_STRING,
            expected_register, string_id, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_CONST_I64,
            wrong, 1, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_TEST_OUTPUT_BEGIN,
            marker, expected_register, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_TEST_OUTPUT_END,
            result, marker, 0, &span
        ) || !mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RETURN_VOID, 0, 0, 0, &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

void mmay_test_cp11_test_pipeline(void) {
    MarimayBytecodeFunction function;
    MarimayBytecodeInstruction *begin = NULL;
    MarimayBytecodeInstruction *end = NULL;
    MMAY_EXPECT(mmay_test_cp11_bytecode_build(&function));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    begin = mmay_test_bytecode_at(&function, 0, 2);
    end = mmay_test_bytecode_at(&function, 0, 3);
    MMAY_EXPECT(begin != NULL && end != NULL);
    if (begin != NULL && end != NULL) {
        begin->operands[0].unsigned_value = 2;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        begin->operands[0].unsigned_value = 1;
        begin->operands[1].unsigned_value = 3;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        begin->operands[1].unsigned_value = 0;
        end->operands[0].unsigned_value = 1;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        end->operands[0].unsigned_value = 2;
        end->operands[1].unsigned_value = 0;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
    }
    mmay_bytecode_function_dispose(&function);
}
