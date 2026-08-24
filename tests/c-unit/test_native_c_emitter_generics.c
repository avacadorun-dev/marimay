#include "test_checker_support.h"
#include "test_native_c_emitter_support.h"

#include <string.h>

static const char mmay_test_generic_differential_source[] =
    "enum InputError { Bad, }\n"
    "enum OutputError { Config, }\n"
    "fn bad(): Result<int, InputError> { return Err(InputError.Bad); }\n"
    "fn configError(error: InputError): OutputError {\n"
    "  return OutputError.Config;\n"
    "}\n"
    "fn mappedValue(): Result<int, OutputError> {\n"
    "  const value = bad().mapError(configError)?;\n"
    "  return Ok(value);\n"
    "}\n"
    "fn good(): Result<int, InputError> { return Ok(4); }\n"
    "fn mappedGood(): Result<int, OutputError> {\n"
    "  const value = good().mapError(configError)?;\n"
    "  return Ok(value);\n"
    "}\n"
    "fn optionValue(): int {\n"
    "  const option: Option<int> = Some(7);\n"
    "  return match option {\n"
    "    Some(value) => value,\n"
    "    None => 0,\n"
    "  };\n"
    "}\n"
    "fn resultOrNine(result: Result<int, OutputError>): int {\n"
    "  return match result {\n"
    "    Ok(value) => value,\n"
    "    Err(_) => 9,\n"
    "  };\n"
    "}\n"
    "fn main(): int {\n"
    "  return resultOrNine(mappedValue()) + resultOrNine(mappedGood()) + optionValue();\n"
    "}\n";

static const MarimayCheckedFunction *mmay_test_generic_entry(
    const MarimayCheckResult *checked,
    size_t *index_result
) {
    size_t index = 0;
    if (checked == NULL || index_result == NULL) {
        return NULL;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checked, index
        );
        if (function != NULL && function->name.length == sizeof("main") - 1
            && memcmp(function->name.data, "main", sizeof("main") - 1) == 0) {
            *index_result = index;
            return function;
        }
    }
    return NULL;
}

void mmay_test_native_generics_differential(void) {
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayMirFunction *functions[12];
    const MarimayCheckedFunction *entry = NULL;
    MarimayMirValue oracle_result;
    MarimayCheckInterpretOptions oracle_options;
    MarimayBytecodeProgram bytecode;
    MarimayBytecodeProgram decoded;
    MarimayOwnedBytes encoded;
    MarimayBytecodeExecution execution;
    MarimayBytecodeExecutionOptions execution_options;
    MarimayBytecodeRunOptions run_options;
    MarimayBytecodeValue vm_result;
    MarimayMirBytecodeLowerStatus lower_status;
    MarimayNativeCProgram native_program;
    MarimayOwnedBytes generated;
    size_t entry_index = SIZE_MAX;
    size_t function_count = 0;
    size_t index = 0;
    bool execution_initialized = false;
    bool bytecode_initialized = false;
    bool decoded_initialized = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(functions, 0, sizeof(functions));
    memset(&oracle_result, 0, sizeof(oracle_result));
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&execution, 0, sizeof(execution));
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&native_program, 0, sizeof(native_program));
    memset(&generated, 0, sizeof(generated));

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture, mmay_test_generic_differential_source
    ));
    if (!fixture.ast_initialized) {
        goto cleanup;
    }
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/native-generics-differential",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (!checked.initialized) {
        goto cleanup;
    }
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(checked.enums.length == 2);
    MMAY_EXPECT(checked.functions.length == 8);
    for (index = 0; index < checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked, index
        );
        MMAY_EXPECT(function != NULL);
        if (function == NULL || function_count >= sizeof(functions) / sizeof(functions[0])) {
            goto cleanup;
        }
        MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
        functions[function_count] = &function->mir;
        function_count += 1;
    }
    entry = mmay_test_generic_entry(&checked, &entry_index);
    MMAY_EXPECT(entry != NULL);
    if (entry == NULL) {
        goto cleanup;
    }

    oracle_options = mmay_check_interpret_default_options();
    MMAY_EXPECT(mmay_check_interpret_function(
        &checked,
        entry_index,
        NULL,
        0,
        mmay_system_allocator(),
        oracle_options,
        &oracle_result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(oracle_result.type == MMAY_TYPE_I64);
    MMAY_EXPECT(oracle_result.as.i64 == 20);

    lower_status = mmay_mir_lower_bytecode_program(
        functions,
        function_count,
        entry->mir.symbol,
        mmay_system_allocator(),
        mmay_mir_bytecode_program_lower_default_options(),
        &bytecode
    );
    MMAY_EXPECT(lower_status == MMAY_MIR_BYTECODE_LOWER_OK);
    bytecode_initialized = bytecode.initialized;
    MMAY_EXPECT(bytecode_initialized);
    MMAY_EXPECT(mmay_bytecode_program_verify(&bytecode)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_encode_program(
        &bytecode, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_program(
        encoded.data,
        encoded.length,
        mmay_system_allocator(),
        mmay_bytecode_program_default_options(),
        &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    decoded_initialized = decoded.initialized;
    MMAY_EXPECT(decoded_initialized);
    MMAY_EXPECT(mmay_bytecode_program_verify(&decoded)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    execution_options = mmay_bytecode_execution_default_options();
    execution_options.managed_heap.collect_before_allocation = true;
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(), execution_options
    ));
    execution_initialized = execution.initialized;
    MMAY_EXPECT(execution_initialized);
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &decoded,
        NULL,
        0,
        mmay_system_allocator(),
        run_options,
        &vm_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_I64);
    MMAY_EXPECT(vm_result.as.i64 == 20);

    native_program.functions = functions;
    native_program.function_count = function_count;
    native_program.entry_symbol = entry->mir.symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &native_program,
        mmay_system_allocator(),
        mmay_native_c_emit_default_options(),
        &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (generated.data != NULL) {
        mmay_test_native_compile_and_run(&generated, "ok:i64:20\n", 0);
    }
#endif

cleanup:
    mmay_owned_bytes_dispose(&generated);
    if (execution_initialized) {
        mmay_bytecode_execution_dispose(&execution);
    }
    if (decoded_initialized) {
        mmay_bytecode_program_dispose(&decoded);
    }
    mmay_owned_bytes_dispose(&encoded);
    if (bytecode_initialized) {
        mmay_bytecode_program_dispose(&bytecode);
    }
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}
