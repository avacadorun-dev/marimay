#include "test_checker_support.h"
#include "test_native_c_emitter_support.h"

#include <string.h>

static const char ruda_test_generic_differential_source[] =
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
    "    case Some(value) => value;\n"
    "    case None => 0;\n"
    "  };\n"
    "}\n"
    "fn resultOrNine(result: Result<int, OutputError>): int {\n"
    "  return match result {\n"
    "    case Ok(value) => value;\n"
    "    case Err(_) => 9;\n"
    "  };\n"
    "}\n"
    "fn main(): int {\n"
    "  return resultOrNine(mappedValue()) + resultOrNine(mappedGood()) + optionValue();\n"
    "}\n";

static const RudaCheckedFunction *ruda_test_generic_entry(
    const RudaCheckResult *checked,
    size_t *index_result
) {
    size_t index = 0;
    if (checked == NULL || index_result == NULL) {
        return NULL;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
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

void ruda_test_native_generics_differential(void) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaMirFunction *functions[12];
    const RudaCheckedFunction *entry = NULL;
    RudaMirValue oracle_result;
    RudaCheckInterpretOptions oracle_options;
    RudaBytecodeProgram bytecode;
    RudaBytecodeProgram decoded;
    RudaOwnedBytes encoded;
    RudaBytecodeExecution execution;
    RudaBytecodeExecutionOptions execution_options;
    RudaBytecodeRunOptions run_options;
    RudaBytecodeValue vm_result;
    RudaMirBytecodeLowerStatus lower_status;
    RudaNativeCProgram native_program;
    RudaOwnedBytes generated;
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

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture, ruda_test_generic_differential_source
    ));
    if (!fixture.ast_initialized) {
        goto cleanup;
    }
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/native-generics-differential",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (!checked.initialized) {
        goto cleanup;
    }
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(checked.enums.length == 2);
    RUDA_EXPECT(checked.functions.length == 8);
    for (index = 0; index < checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked, index
        );
        RUDA_EXPECT(function != NULL);
        if (function == NULL || function_count >= sizeof(functions) / sizeof(functions[0])) {
            goto cleanup;
        }
        RUDA_EXPECT(ruda_mir_verify(&function->mir) == RUDA_MIR_VERIFY_OK);
        functions[function_count] = &function->mir;
        function_count += 1;
    }
    entry = ruda_test_generic_entry(&checked, &entry_index);
    RUDA_EXPECT(entry != NULL);
    if (entry == NULL) {
        goto cleanup;
    }

    oracle_options = ruda_check_interpret_default_options();
    RUDA_EXPECT(ruda_check_interpret_function(
        &checked,
        entry_index,
        NULL,
        0,
        ruda_system_allocator(),
        oracle_options,
        &oracle_result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(oracle_result.type == RUDA_TYPE_I64);
    RUDA_EXPECT(oracle_result.as.i64 == 20);

    lower_status = ruda_mir_lower_bytecode_program(
        functions,
        function_count,
        entry->mir.symbol,
        ruda_system_allocator(),
        ruda_mir_bytecode_program_lower_default_options(),
        &bytecode
    );
    RUDA_EXPECT(lower_status == RUDA_MIR_BYTECODE_LOWER_OK);
    bytecode_initialized = bytecode.initialized;
    RUDA_EXPECT(bytecode_initialized);
    RUDA_EXPECT(ruda_bytecode_program_verify(&bytecode)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_encode_program(
        &bytecode, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_program(
        encoded.data,
        encoded.length,
        ruda_system_allocator(),
        ruda_bytecode_program_default_options(),
        &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    decoded_initialized = decoded.initialized;
    RUDA_EXPECT(decoded_initialized);
    RUDA_EXPECT(ruda_bytecode_program_verify(&decoded)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    execution_options = ruda_bytecode_execution_default_options();
    execution_options.managed_heap.collect_before_allocation = true;
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(), execution_options
    ));
    execution_initialized = execution.initialized;
    RUDA_EXPECT(execution_initialized);
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run_program(
        &decoded,
        NULL,
        0,
        ruda_system_allocator(),
        run_options,
        &vm_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_I64);
    RUDA_EXPECT(vm_result.as.i64 == 20);

    native_program.functions = functions;
    native_program.function_count = function_count;
    native_program.entry_symbol = entry->mir.symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &native_program,
        ruda_system_allocator(),
        ruda_native_c_emit_default_options(),
        &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (generated.data != NULL) {
        ruda_test_native_compile_and_run(&generated, "ok:i64:20\n", 0);
    }
#endif

cleanup:
    ruda_owned_bytes_dispose(&generated);
    if (execution_initialized) {
        ruda_bytecode_execution_dispose(&execution);
    }
    if (decoded_initialized) {
        ruda_bytecode_program_dispose(&decoded);
    }
    ruda_owned_bytes_dispose(&encoded);
    if (bytecode_initialized) {
        ruda_bytecode_program_dispose(&bytecode);
    }
    ruda_check_result_dispose(&checked);
    ruda_checker_fixture_dispose(&fixture);
}
