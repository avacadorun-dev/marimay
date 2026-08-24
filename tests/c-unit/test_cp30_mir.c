#include "test_mir_support.h"

#include <string.h>

#include "mmay/base/allocator.h"

static bool mmay_test_cp30_mir_symbol(
    const char *package_name,
    const char *source_path,
    const char *name,
    MarimaySymbolId *symbol
) {
    return mmay_symbol_id_from_parts(
        package_name, source_path, name, 0, symbol
    );
}

static bool mmay_test_cp30_mir_build(MarimayMirFunction *function) {
    const MarimayTypeId error_type = MMAY_TYPE_FIRST_DYNAMIC + 1;
    const MarimayTypeId result_type = MMAY_TYPE_FIRST_DYNAMIC + 2;
    MarimayMirEnumVariant error_variants[4] = {
        {MMAY_MIR_ENUM_VARIANT_UNIT, 0, 0},
        {MMAY_MIR_ENUM_VARIANT_NAMED, 0, 1},
        {MMAY_MIR_ENUM_VARIANT_UNIT, 1, 0},
        {MMAY_MIR_ENUM_VARIANT_UNIT, 1, 0}
    };
    MarimayMirEnumPayload error_payload = {MMAY_TYPE_I64};
    MarimayMirEnumVariant result_variants[2] = {
        {MMAY_MIR_ENUM_VARIANT_POSITIONAL, 0, 1},
        {MMAY_MIR_ENUM_VARIANT_POSITIONAL, 1, 1}
    };
    MarimayMirEnumPayload result_payloads[2] = {
        {MMAY_TYPE_I64}, {error_type}
    };
    MarimaySymbolId function_symbol;
    MarimaySymbolId error_symbol;
    MarimaySymbolId result_symbol;
    MarimayMirEnumId error_layout = MMAY_MIR_NO_ENUM;
    MarimayMirEnumId result_layout = MMAY_MIR_NO_ENUM;
    MarimayMirStringId text_constant = MMAY_MIR_NO_STRING;
    MarimayMirRegister text = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister radix = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister boolean = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister formatted = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister joined = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister parsed = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_test_cp30_mir_symbol(
            "test", "test/cp30-mir", "entry", &function_symbol
        ) || !mmay_test_cp30_mir_symbol(
            "std/convert", "std/convert/convert.my", "ParseError",
            &error_symbol
        ) || !mmay_test_cp30_mir_symbol(
            "std", "prelude", "Result", &result_symbol
        ) || !mmay_mir_function_init(
            function, mmay_system_allocator(), function_symbol,
            NULL, 0, result_type, mmay_mir_function_default_options()
        ) || !mmay_mir_add_enum_with_symbol(
            function, error_symbol, error_type,
            error_variants, 4, &error_payload, 1, &error_layout
        ) || !mmay_mir_add_enum_with_symbol(
            function, result_symbol, result_type,
            result_variants, 2, result_payloads, 2, &result_layout
        ) || !mmay_mir_add_register(function, MMAY_TYPE_STRING, &text)
        || !mmay_mir_add_register(function, MMAY_TYPE_I64, &radix)
        || !mmay_mir_add_register(function, MMAY_TYPE_BOOL, &boolean)
        || !mmay_mir_add_register(function, MMAY_TYPE_STRING, &formatted)
        || !mmay_mir_add_register(function, MMAY_TYPE_STRING, &joined)
        || !mmay_mir_add_register(function, result_type, &parsed)
        || !mmay_mir_add_string_constant(
            function, (const unsigned char *)"12x", 3, &text_constant
        ) || !mmay_mir_add_block(function, &block)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_STRING;
    instruction.target = text;
    instruction.string_constant = text_constant;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    if (!mmay_mir_append_instruction(function, block, &instruction)) return false;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = radix;
    instruction.integer = 10;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    if (!mmay_mir_append_instruction(function, block, &instruction)) return false;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_BOOL;
    instruction.target = boolean;
    instruction.boolean = true;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    if (!mmay_mir_append_instruction(function, block, &instruction)) return false;
    instruction = mmay_test_mir_binary(
        MMAY_MIR_FORMAT_BOOL, formatted, boolean, MMAY_MIR_NO_REGISTER
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) return false;
    instruction = mmay_test_mir_binary(
        MMAY_MIR_STRING_CONCAT, joined, text, formatted
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)) return false;
    instruction = mmay_test_mir_binary(
        MMAY_MIR_PARSE_INTEGER, parsed, text, radix
    );
    instruction.enum_id = result_layout;
    return mmay_mir_append_instruction(function, block, &instruction)
        && mmay_mir_set_terminator(
            function, block, mmay_test_mir_return(parsed)
        );
}

static void mmay_test_cp30_mir_execute(
    const MarimayMirFunction *function
) {
    MarimayMirExecution execution;
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    MarimayMirValue result;
    MarimayMirValue error;
    MarimayMirValue offset;
    size_t variant = SIZE_MAX;
    size_t payload_count = SIZE_MAX;
    memset(&execution, 0, sizeof(execution));
    memset(&result, 0, sizeof(result));
    memset(&error, 0, sizeof(error));
    memset(&offset, 0, sizeof(offset));
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution, mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    options.execution = &execution;
    MMAY_EXPECT(mmay_mir_interpret(
        function, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(execution.strings.length == 3);
    MMAY_EXPECT(mmay_mir_execution_inspect_enum(
        &execution, result, &variant, &payload_count
    ));
    MMAY_EXPECT(variant == 1 && payload_count == 1);
    MMAY_EXPECT(mmay_mir_execution_enum_payload(
        &execution, result, 0, &error
    ));
    MMAY_EXPECT(mmay_mir_execution_inspect_enum(
        &execution, error, &variant, &payload_count
    ));
    MMAY_EXPECT(variant == 1 && payload_count == 1);
    MMAY_EXPECT(mmay_mir_execution_enum_payload(
        &execution, error, 0, &offset
    ));
    MMAY_EXPECT(offset.type == MMAY_TYPE_I64 && offset.as.i64 == 2);
    mmay_mir_execution_dispose(&execution);
}

static void mmay_test_cp30_mir_resource_limit(
    const MarimayMirFunction *function
) {
    MarimayMirExecution execution;
    MarimayMirExecutionOptions execution_options =
        mmay_mir_execution_default_options();
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    MarimayMirValue result;
    memset(&execution, 0, sizeof(execution));
    execution_options.max_total_string_bytes = 7;
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution, mmay_system_allocator(), execution_options
    ));
    options.execution = &execution;
    MMAY_EXPECT(mmay_mir_interpret(
        function, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_MIR_INTERPRET_OBJECT_LIMIT);
    MMAY_EXPECT(execution.strings.length == 2);
    MMAY_EXPECT(execution.total_string_bytes == 7);
    mmay_mir_execution_dispose(&execution);
}

static void mmay_test_cp30_mir_allocation_failures(
    const MarimayMirFunction *function
) {
    size_t failure_index;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 128; failure_index += 1U) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMirExecution execution;
        MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
        MarimayMirValue result;
        MarimayMirInterpretStatus status;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        allocator = mmay_fail_allocator(&state);
        if (!mmay_mir_execution_init(
                &execution, allocator, mmay_mir_execution_default_options()
            )) {
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            continue;
        }
        options.execution = &execution;
        status = mmay_mir_interpret(
            function, NULL, 0, allocator, options, &result
        );
        MMAY_EXPECT(status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED
            || status == MMAY_MIR_INTERPRET_OK);
        reached_success = status == MMAY_MIR_INTERPRET_OK;
        mmay_mir_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_cp30_mir_hostile(MarimayMirFunction *function) {
    MarimayMirBlock *block = mmay_vector_at(&function->blocks, 0);
    MarimayMirInstruction *format = block != NULL
        ? mmay_vector_at(&block->instructions, 3) : NULL;
    MarimayMirInstruction *concat = block != NULL
        ? mmay_vector_at(&block->instructions, 4) : NULL;
    MarimayMirInstruction *parse = block != NULL
        ? mmay_vector_at(&block->instructions, 5) : NULL;
    MarimayMirEnum *result_layout = mmay_vector_at(&function->enums, 1);
    MarimayMirRegister saved_register;
    unsigned char saved_symbol;
    MMAY_EXPECT(format != NULL && concat != NULL && parse != NULL
        && result_layout != NULL);
    if (format != NULL) {
        saved_register = format->first;
        format->first = 0;
        MMAY_EXPECT(mmay_mir_verify(function)
            == MMAY_MIR_VERIFY_TYPE_MISMATCH);
        format->first = saved_register;
    }
    if (concat != NULL) {
        saved_register = concat->second;
        concat->second = 2;
        MMAY_EXPECT(mmay_mir_verify(function)
            == MMAY_MIR_VERIFY_TYPE_MISMATCH);
        concat->second = saved_register;
    }
    if (parse != NULL) {
        saved_register = parse->second;
        parse->second = 0;
        MMAY_EXPECT(mmay_mir_verify(function)
            == MMAY_MIR_VERIFY_TYPE_MISMATCH);
        parse->second = saved_register;
    }
    if (result_layout != NULL) {
        saved_symbol = result_layout->symbol.bytes[0];
        result_layout->symbol.bytes[0] ^= 0xffU;
        MMAY_EXPECT(mmay_mir_verify(function)
            == MMAY_MIR_VERIFY_TYPE_MISMATCH);
        result_layout->symbol.bytes[0] = saved_symbol;
    }
    MMAY_EXPECT(mmay_mir_verify(function) == MMAY_MIR_VERIFY_OK);
}

void mmay_test_cp30_mir(void) {
    MarimayMirFunction function;
    MMAY_EXPECT(mmay_test_cp30_mir_build(&function));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    mmay_test_cp30_mir_hostile(&function);
    mmay_test_cp30_mir_execute(&function);
    mmay_test_cp30_mir_resource_limit(&function);
    mmay_test_cp30_mir_allocation_failures(&function);
    mmay_mir_function_dispose(&function);
}
