#include "test_native_c_emitter_support.h"

#include "test_checker_support.h"

#include <stdio.h>
#include <string.h>

#ifndef RUDA_TEST_SOURCE_DIR
#define RUDA_TEST_SOURCE_DIR "."
#endif

static bool ruda_test_native_nominal_read_example(
    char *source,
    size_t capacity
) {
    char path[1024];
    FILE *file = NULL;
    size_t count = 0;
    int written = 0;
    bool read_ok = false;
    bool close_ok = false;
    if (source == NULL || capacity < 2) {
        return false;
    }
    written = snprintf(
        path,
        sizeof(path),
        "%s/examples/nominal_cp1.ruda",
        RUDA_TEST_SOURCE_DIR
    );
    if (written < 0 || (size_t)written >= sizeof(path)) {
        return false;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    count = fread(source, 1, capacity - 1, file);
    read_ok = ferror(file) == 0 && feof(file) != 0;
    close_ok = fclose(file) == 0;
    if (!read_ok || !close_ok) {
        return false;
    }
    source[count] = '\0';
    return true;
}

static const RudaCheckedFunction *ruda_test_native_nominal_differential_entry(
    const RudaCheckResult *checked,
    size_t *result_index
) {
    size_t index = 0;
    if (checked == NULL || result_index == NULL) {
        return NULL;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            checked,
            index
        );
        if (function != NULL && function->name.length == sizeof("main") - 1
            && memcmp(
                function->name.data,
                "main",
                sizeof("main") - 1
            ) == 0) {
            *result_index = index;
            return function;
        }
    }
    return NULL;
}

static void ruda_test_native_nominal_source_differential(void) {
    static const int64_t expected = 7;
    char source[16384];
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *entry = NULL;
    const RudaMirFunction *functions[20];
    RudaBytecodeProgram bytecode;
    RudaBytecodeProgram decoded;
    RudaOwnedBytes encoded;
    RudaBytecodeExecution execution;
    RudaBytecodeExecutionOptions execution_options;
    RudaBytecodeRunOptions run_options;
    RudaCheckInterpretOptions oracle_options;
    RudaMirValue oracle_result;
    RudaBytecodeValue vm_result;
    RudaNativeCProgram native_program;
    RudaOwnedBytes generated;
    size_t function_count = 0;
    size_t entry_index = SIZE_MAX;
    size_t index = 0;
    bool checked_initialized = false;
    bool bytecode_initialized = false;
    bool decoded_initialized = false;
    bool execution_initialized = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(functions, 0, sizeof(functions));
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&execution, 0, sizeof(execution));
    memset(&oracle_result, 0, sizeof(oracle_result));
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&native_program, 0, sizeof(native_program));
    memset(&generated, 0, sizeof(generated));

    if (!ruda_test_native_nominal_read_example(source, sizeof(source))) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    if (!ruda_checker_fixture_init(&fixture, source)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    if (!fixture.ast_initialized) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    if (ruda_check_ast(
            &fixture.ast,
            "test/native-nominal-differential",
            ruda_system_allocator(),
            ruda_check_default_options(),
            &checked
        ) != RUDA_CHECK_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    checked_initialized = true;
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(checked.records.length == 2);
    RUDA_EXPECT(checked.enums.length == 3);
    RUDA_EXPECT(checked.interfaces.length == 1);
    RUDA_EXPECT(checked.functions.length <= sizeof(functions) / sizeof(functions[0]));
    for (index = 0; index < checked.functions.length
        && index < sizeof(functions) / sizeof(functions[0]); index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked,
            index
        );
        if (function == NULL) {
            RUDA_EXPECT(false);
            goto cleanup;
        }
        RUDA_EXPECT(ruda_mir_verify(&function->mir) == RUDA_MIR_VERIFY_OK);
        functions[function_count] = &function->mir;
        function_count += 1;
    }
    entry = ruda_test_native_nominal_differential_entry(
        &checked,
        &entry_index
    );
    RUDA_EXPECT(entry != NULL);
    if (entry == NULL || function_count != checked.functions.length) {
        goto cleanup;
    }

    oracle_options = ruda_check_interpret_default_options();
    if (ruda_check_interpret_function(
            &checked,
            entry_index,
            NULL,
            0,
            ruda_system_allocator(),
            oracle_options,
            &oracle_result
        ) != RUDA_MIR_INTERPRET_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    RUDA_EXPECT(oracle_result.type == RUDA_TYPE_I64);
    RUDA_EXPECT(oracle_result.as.i64 == expected);

    if (ruda_mir_lower_bytecode_program(
            functions,
            function_count,
            entry->mir.symbol,
            ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(),
            &bytecode
        ) != RUDA_MIR_BYTECODE_LOWER_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    bytecode_initialized = true;
    RUDA_EXPECT(ruda_bytecode_program_verify(&bytecode)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    if (ruda_bytecode_encode_program(
            &bytecode,
            ruda_system_allocator(),
            &encoded
        ) != RUDA_BYTECODE_CODEC_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    if (ruda_bytecode_decode_program(
            encoded.data,
            encoded.length,
            ruda_system_allocator(),
            ruda_bytecode_program_default_options(),
            &decoded
        ) != RUDA_BYTECODE_CODEC_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    decoded_initialized = true;
    RUDA_EXPECT(ruda_bytecode_program_verify(&decoded)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    execution_options = ruda_bytecode_execution_default_options();
    execution_options.managed_heap.collect_before_allocation = true;
    if (!ruda_bytecode_execution_init(
            &execution,
            ruda_system_allocator(),
            execution_options
        )) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    execution_initialized = true;
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    if (ruda_bytecode_run_program(
            &decoded,
            NULL,
            0,
            ruda_system_allocator(),
            run_options,
            &vm_result
        ) != RUDA_BYTECODE_RUN_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_I64);
    RUDA_EXPECT(vm_result.as.i64 == expected);

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
        ruda_test_native_compile_and_run(&generated, "ok:i64:7\n", 0);
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
    if (checked_initialized) {
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_native_nominal_differential(void) {
    ruda_test_native_nominal_source_differential();
}
