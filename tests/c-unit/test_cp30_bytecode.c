#include "test_bytecode_support.h"

#include "mmay/bytecode/codec.h"
#include "mmay/bytecode/enum.h"
#include "mmay/bytecode/vm.h"

static bool mmay_test_cp30_bytecode_build(
    MarimayBytecodeFunction *function
) {
    MarimayBytecodeEnumVariant error_variants[4] = {
        {0, 0}, {0, 1}, {1, 0}, {1, 0}
    };
    MarimayBytecodeEnumPayload error_payload = {
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_NO_LAYOUT
    };
    MarimayBytecodeEnumVariant result_variants[2] = {
        {0, 1}, {1, 1}
    };
    MarimayBytecodeEnumPayload result_payloads[2];
    MarimaySymbolId error_symbol;
    MarimaySymbolId result_symbol;
    MarimayBytecodeEnumId error_layout = MMAY_BYTECODE_NO_ENUM;
    MarimayBytecodeEnumId result_layout = MMAY_BYTECODE_NO_ENUM;
    MarimayBytecodeStringId text_constant = MMAY_BYTECODE_NO_STRING;
    uint32_t text = UINT32_MAX;
    uint32_t radix = UINT32_MAX;
    uint32_t parsed = UINT32_MAX;
    uint32_t boolean = UINT32_MAX;
    uint32_t formatted = UINT32_MAX;
    uint32_t joined = UINT32_MAX;
    uint32_t unassigned_radix = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 0;
    memset(function, 0, sizeof(*function));
    memset(&error_symbol, 0, sizeof(error_symbol));
    memset(&result_symbol, 0, sizeof(result_symbol));
    error_symbol.bytes[0] = 1;
    result_symbol.bytes[0] = 2;
    result_payloads[0] = (MarimayBytecodeEnumPayload){
        MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_NO_LAYOUT
    };
    result_payloads[1] = (MarimayBytecodeEnumPayload){
        MMAY_BYTECODE_TYPE_ENUM, MMAY_BYTECODE_NO_LAYOUT
    };
    return mmay_bytecode_function_init(
            function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_STRING,
            1, NULL, 0, mmay_bytecode_function_default_options()
        ) && mmay_bytecode_add_enum_layout(
            function, error_symbol, error_variants, 4,
            &error_payload, 1, &error_layout
        ) && ((result_payloads[1].layout = error_layout), true)
        && mmay_bytecode_add_enum_layout(
            function, result_symbol, result_variants, 2,
            result_payloads, 2, &result_layout
        ) && mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_STRING, &text
        ) && mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &radix
        ) && mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ENUM, &parsed
        ) && mmay_bytecode_set_register_layout(function, parsed, result_layout)
        && mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_BOOL, &boolean
        ) && mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_STRING, &formatted
        ) && mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_STRING, &joined
        ) && mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &unassigned_radix
        ) && mmay_bytecode_add_string_constant(
            function, (const unsigned char *)"42", 2, &text_constant
        ) && mmay_bytecode_add_block(function, &block)
        && mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_CONST_STRING,
            text, text_constant, 0, &span
        ) && mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_CONST_I64,
            radix, 10, 0, &span
        ) && mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_PARSE_INTEGER,
            parsed, text, radix, &span
        ) && mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_CONST_BOOL,
            boolean, 1, 0, &span
        ) && mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_FORMAT_BOOL,
            formatted, boolean, 0, &span
        ) && mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_STRING_CONCAT,
            joined, text, formatted, &span
        ) && mmay_test_bytecode_append(
            function, block, MMAY_OPCODE_RETURN,
            joined, 0, 0, &span
        );
}

static void mmay_test_cp30_bytecode_execution(
    const MarimayBytecodeFunction *function
) {
    static const unsigned char expected[] = "42true";
    MarimayBytecodeExecution execution;
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue result;
    const unsigned char *bytes = NULL;
    size_t length = 0;
    memset(&execution, 0, sizeof(execution));
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(
        function, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_STRING);
    MMAY_EXPECT(mmay_bytecode_execution_string_bytes(
        &execution, result, &bytes, &length
    ));
    MMAY_EXPECT(length == sizeof(expected) - 1
        && memcmp(bytes, expected, sizeof(expected) - 1) == 0);
    mmay_bytecode_execution_dispose(&execution);
}

static void mmay_test_cp30_bytecode_resource_limit(
    const MarimayBytecodeFunction *function
) {
    MarimayBytecodeExecution execution;
    MarimayBytecodeExecutionOptions execution_options =
        mmay_bytecode_execution_default_options();
    MarimayBytecodeRunOptions run_options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue result;
    memset(&execution, 0, sizeof(execution));
    memset(&result, 0, sizeof(result));
    execution_options.max_total_string_bytes = 3;
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(), execution_options
    ));
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(
        function, NULL, 0, mmay_system_allocator(), run_options, &result
    ) == MMAY_BYTECODE_RUN_LIMIT_REACHED);
    mmay_bytecode_execution_dispose(&execution);
}

static void mmay_test_cp30_bytecode_allocation_failures(
    const MarimayBytecodeFunction *function
) {
    size_t failure_index;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 256; failure_index += 1U) {
        MarimayFailAllocator state;
        MarimayBytecodeExecution execution;
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        MarimayBytecodeValue result;
        MarimayBytecodeRunStatus status;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        if (!mmay_bytecode_execution_init(
                &execution, mmay_fail_allocator(&state),
                mmay_bytecode_execution_default_options()
            )) {
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            continue;
        }
        options.execution = &execution;
        status = mmay_bytecode_run(
            function, NULL, 0, mmay_fail_allocator(&state), options, &result
        );
        MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED
            || status == MMAY_BYTECODE_RUN_OK);
        reached_success = status == MMAY_BYTECODE_RUN_OK;
        mmay_bytecode_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_cp30_bytecode(void) {
    MarimayBytecodeFunction function;
    MarimayBytecodeFunction decoded;
    MarimayOwnedBytes encoded;
    MarimayBytecodeInstruction *parse;
    MarimayBytecodeInstruction *format;
    MarimayBytecodeInstruction *concat;
    MarimayBytecodeEnumVariant *invalid_digit;
    MarimayBytecodeEnumPayload *result_error_payload;
    uint64_t saved;
    uint32_t saved_layout;
    size_t saved_count;
    memset(&function, 0, sizeof(function));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_cp30_bytecode_build(&function));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    parse = mmay_test_bytecode_at(&function, 0, 2);
    format = mmay_test_bytecode_at(&function, 0, 4);
    concat = mmay_test_bytecode_at(&function, 0, 5);
    MMAY_EXPECT(parse != NULL && format != NULL && concat != NULL);
    if (parse != NULL && format != NULL && concat != NULL) {
        saved = parse->operands[2].unsigned_value;
        parse->operands[2].unsigned_value = 6;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
        parse->operands[2].unsigned_value = saved;
        saved = parse->operands[1].unsigned_value;
        parse->operands[1].unsigned_value = 1;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        parse->operands[1].unsigned_value = saved;
        saved = parse->operands[2].unsigned_value;
        parse->operands[2].unsigned_value = 0;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        parse->operands[2].unsigned_value = saved;
        saved_layout = mmay_bytecode_register_layout(&function, 2);
        MMAY_EXPECT(mmay_bytecode_set_register_layout(&function, 2, 0));
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        MMAY_EXPECT(mmay_bytecode_set_register_layout(
            &function, 2, saved_layout
        ));
        invalid_digit = mmay_vector_at(&function.enum_variants, 1);
        result_error_payload = mmay_vector_at(&function.enum_payloads, 2);
        MMAY_EXPECT(invalid_digit != NULL && result_error_payload != NULL);
        if (invalid_digit != NULL) {
            saved_count = invalid_digit->payload_count;
            invalid_digit->payload_count = 0;
            MMAY_EXPECT(mmay_bytecode_verify(&function)
                != MMAY_BYTECODE_VERIFY_OK);
            invalid_digit->payload_count = saved_count;
        }
        if (result_error_payload != NULL) {
            saved_layout = result_error_payload->layout;
            result_error_payload->layout = 1;
            MMAY_EXPECT(mmay_bytecode_verify(&function)
                != MMAY_BYTECODE_VERIFY_OK);
            result_error_payload->layout = saved_layout;
        }
        saved = format->operands[1].unsigned_value;
        format->operands[1].unsigned_value = 0;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        format->operands[1].unsigned_value = saved;
        format->opcode = MMAY_OPCODE_FORMAT_INTEGER;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        format->opcode = MMAY_OPCODE_FORMAT_BOOL;
        saved = concat->operands[2].unsigned_value;
        concat->operands[2].unsigned_value = 3;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        concat->operands[2].unsigned_value = saved;
        saved = concat->operands[2].unsigned_value;
        concat->operands[2].unsigned_value = 6;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
        concat->operands[2].unsigned_value = saved;
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    mmay_test_cp30_bytecode_execution(&function);
    mmay_test_cp30_bytecode_resource_limit(&function);
    mmay_test_cp30_bytecode_allocation_failures(&function);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &function, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_verify(&decoded) == MMAY_BYTECODE_VERIFY_OK);
    mmay_bytecode_function_dispose(&decoded);
    memset(&decoded, 0, sizeof(decoded));
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length - 1, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) != MMAY_BYTECODE_CODEC_OK);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&function);
}
