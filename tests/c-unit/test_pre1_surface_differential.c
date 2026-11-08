#include "test_native_c_emitter_support.h"

#include "test_checker_support.h"

#include <stdio.h>
#include <string.h>

#ifndef MMAY_TEST_SOURCE_DIR
#define MMAY_TEST_SOURCE_DIR "."
#endif

static bool mmay_test_pre1_surface_source(
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
        "%s/tests/conformance/pre1-surface.my",
        MMAY_TEST_SOURCE_DIR
    );
    if (written < 0 || (size_t)written >= sizeof(path)) {
        return false;
    }
    file = mmay_test_open_file(path, "rb");
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

static const MarimayCheckedFunction *mmay_test_pre1_surface_entry(
    const MarimayCheckResult *checked,
    size_t *result_index
) {
    size_t index = 0;
    if (checked == NULL || result_index == NULL) {
        return NULL;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checked,
            index
        );
        if (function != NULL && function->name.length == 4
            && memcmp(function->name.data, "main", 4) == 0) {
            *result_index = index;
            return function;
        }
    }
    return NULL;
}

static void mmay_test_pre1_surface_ignored_result(void) {
    mmay_checker_expect_one_diagnostic(
        "enum Failure { Bad, }\n"
        "fn operation(): Result<int, Failure> { return Err(Failure.Bad); }\n"
        "fn main(): void { operation(); }\n",
        MMAY_DIAGNOSTIC_IGNORED_RESULT,
        "operation()",
        1
    );
}

static void mmay_test_pre1_surface_range_value(void) {
    mmay_checker_expect_one_diagnostic(
        "fn main(): void { const values = 0..3; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "0..3",
        0
    );
}

void mmay_test_pre1_surface_differential(void) {
    static const int64_t expected = 61;
    char source[16384];
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *entry = NULL;
    const MarimayMirFunction *functions[32] = {0};
    MarimayMirValue oracle_result;
    MarimayBytecodeProgram bytecode;
    MarimayBytecodeExecution execution;
    MarimayBytecodeRunOptions run_options;
    MarimayBytecodeValue vm_result;
    MarimayNativeCProgram native_program;
    MarimayOwnedBytes generated;
    size_t function_count = 0;
    size_t entry_index = SIZE_MAX;
    size_t index = 0;
    bool checked_ready = false;
    bool bytecode_ready = false;
    bool execution_ready = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&oracle_result, 0, sizeof(oracle_result));
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&execution, 0, sizeof(execution));
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&native_program, 0, sizeof(native_program));
    memset(&generated, 0, sizeof(generated));

    if (!mmay_test_pre1_surface_source(source, sizeof(source))
        || !mmay_checker_fixture_init(&fixture, source)
        || !fixture.ast_initialized) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    if (mmay_check_ast(
            &fixture.ast,
            "test/pre1-surface-differential",
            mmay_system_allocator(),
            mmay_check_default_options(),
            &checked
        ) != MMAY_CHECK_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    checked_ready = true;
    for (index = 0; index < checked.diagnostics.length; index += 1) {
        const MarimayCheckDiagnostic *diagnostic = mmay_check_diagnostic(
            &checked,
            index
        );
        if (diagnostic != NULL) {
            fprintf(
                stderr,
                "pre-1.0 surface diagnostic %s at %zu..%zu: %s\n",
                diagnostic->code,
                diagnostic->start,
                diagnostic->end,
                diagnostic->message
            );
        }
    }
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(
        checked.functions.length
            <= sizeof(functions) / sizeof(functions[0])
    );
    for (index = 0; index < checked.functions.length
        && index < sizeof(functions) / sizeof(functions[0]); index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked,
            index
        );
        if (function == NULL || function->is_external) {
            MMAY_EXPECT(false);
            goto cleanup;
        }
        MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
        functions[function_count] = &function->mir;
        function_count += 1;
    }
    entry = mmay_test_pre1_surface_entry(&checked, &entry_index);
    MMAY_EXPECT(entry != NULL);
    if (entry == NULL || function_count != checked.functions.length) {
        goto cleanup;
    }
    MMAY_EXPECT(mmay_check_interpret_function(
        &checked,
        entry_index,
        NULL,
        0,
        mmay_system_allocator(),
        mmay_check_interpret_default_options(),
        &oracle_result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(oracle_result.type == MMAY_TYPE_I64);
    MMAY_EXPECT(oracle_result.as.i64 == expected);

    if (mmay_mir_lower_bytecode_program(
            functions,
            function_count,
            entry->mir.symbol,
            mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(),
            &bytecode
        ) != MMAY_MIR_BYTECODE_LOWER_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    bytecode_ready = true;
    MMAY_EXPECT(mmay_bytecode_program_verify(&bytecode)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    if (!mmay_bytecode_execution_init(
            &execution,
            mmay_system_allocator(),
            mmay_bytecode_execution_default_options()
        )) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    execution_ready = true;
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &bytecode,
        NULL,
        0,
        mmay_system_allocator(),
        run_options,
        &vm_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_I64);
    MMAY_EXPECT(vm_result.as.i64 == expected);

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
        mmay_test_native_compile_and_run(&generated, "ok:i64:61\n", 0);
    }
#endif

cleanup:
    mmay_owned_bytes_dispose(&generated);
    if (execution_ready) {
        mmay_bytecode_execution_dispose(&execution);
    }
    if (bytecode_ready) {
        mmay_bytecode_program_dispose(&bytecode);
    }
    if (checked_ready) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
    mmay_test_pre1_surface_ignored_result();
    mmay_test_pre1_surface_range_value();
}
