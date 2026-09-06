#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_check_targets.h"
#include "cli_standard_packages.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/vm.h"
#include "mmay/bytecode/vm_arguments.h"
#include "mmay/check/service.h"

enum { MMAY_JSON_FUZZ_MAX_INPUT = 4096 };

static MarimayBytecodeProgram mmay_json_fuzz_program;
static bool mmay_json_fuzz_initialized = false;

static const unsigned char mmay_json_fuzz_source[] =
    "import * as bytes from \"std/bytes\";\n"
    "import * as json from \"std/json\";\n"
    "type Envelope { public notes: [][]string, }\n"
    "public fn fuzz(arguments: []string): int {\n"
    "  const input = arguments[0];\n"
    "  const typed = json.decodeWithLimits<Envelope>(bytes.encodeUtf8(input),\n"
    "    json.limits(4096, 4096, 4096, 128, 64, 512));\n"
    "  const _ = match typed {\n"
    "    Ok(_) => true,\n"
    "    Err(error) => {\n"
    "      const display = json.pathText(error);\n"
    "      if bytes.length(bytes.encodeUtf8(display)) > 4096 { panic(\"JSON path display bound\"); }\n"
    "      const path = json.path(error);\n"
    "      if path.length() > 3 { panic(\"JSON path exceeds schema depth\"); }\n"
    "      true\n"
    "    },\n"
    "  };\n"
    "  const decoder = match json.newDecoder(\n"
    "    bytes.encodeUtf8(input),\n"
    "    json.limits(4096, 4096, 4096, 128, 64, 512),\n"
    "  ) {\n"
    "    Ok(value) => value,\n"
    "    Err(_) => { return 1; },\n"
    "  };\n"
    "  return match json.skipValue(decoder) {\n"
    "    Err(_) => 2,\n"
    "    Ok(_) => match json.finishDecoder(decoder) {\n"
    "      Ok(_) => 0,\n"
    "      Err(_) => 3,\n"
    "    },\n"
    "  };\n"
    "}\n";

static bool mmay_json_fuzz_target_init(MarimayCliCheckTarget *target) {
    MarimayCompilerOptions options = mmay_compiler_default_options();
    memset(target, 0, sizeof(*target));
    target->allocator = mmay_system_allocator();
    options.max_documents = MMAY_CLI_CHECK_MAX_FILES;
    options.max_path_bytes = MMAY_CLI_CHECK_MAX_PATH_BYTES;
    options.max_source_bytes = MMAY_CLI_CHECK_MAX_SOURCE_BYTES;
    if (mmay_compiler_service_init(
            &target->service, target->allocator, options
        ) != MMAY_COMPILER_OK) {
        return false;
    }
    target->service_initialized = true;
    if (!mmay_vector_init(
            &target->packages,
            target->allocator,
            sizeof(MarimayCliCheckPackage)
        )) {
        return false;
    }
    target->packages_initialized = true;
    return true;
}

static const MarimayCheckedFunction *mmay_json_fuzz_entry(
    const MarimayCheckResult *checked
) {
    size_t index = 0;
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checked, index
        );
        if (function != NULL && function->name.length == 4
            && memcmp(function->name.data, "fuzz", 4) == 0) {
            return function;
        }
    }
    return NULL;
}

static const MarimayCheckedFunction *mmay_json_fuzz_find_symbol(
    const MarimayCheckResult *checked,
    MarimaySymbolId symbol
) {
    size_t index = 0;
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checked, index
        );
        if (function != NULL && !function->is_external
            && mmay_symbol_id_equal(function->mir.symbol, symbol)) {
            return function;
        }
    }
    return NULL;
}

static bool mmay_json_fuzz_has_function(
    const MarimayMirFunction *const *functions,
    size_t function_count,
    MarimaySymbolId symbol
) {
    size_t index = 0;
    for (index = 0; index < function_count; index += 1) {
        if (mmay_symbol_id_equal(functions[index]->symbol, symbol)) {
            return true;
        }
    }
    return false;
}

static bool mmay_json_fuzz_collect_reachable(
    const MarimayCheckResult *checked,
    const MarimayCheckedFunction *entry,
    const MarimayMirFunction **functions,
    size_t capacity,
    size_t *function_count
) {
    size_t function_index = 0;
    *function_count = 1;
    functions[0] = &entry->mir;
    for (function_index = 0; function_index < *function_count;
        function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        size_t call_index = 0;
        for (call_index = 0; call_index < function->calls.length;
            call_index += 1) {
            const MarimayMirCall *call = mmay_vector_at_const(
                &function->calls, call_index
            );
            const MarimayCheckedFunction *callee = call == NULL ? NULL
                : mmay_json_fuzz_find_symbol(checked, call->callee);
            if (call == NULL) return false;
            if (callee == NULL || mmay_json_fuzz_has_function(
                    functions, *function_count, call->callee
                )) {
                continue;
            }
            if (*function_count >= capacity) return false;
            functions[(*function_count)++] = &callee->mir;
        }
    }
    return true;
}

static bool mmay_json_fuzz_compile(void) {
    static const char import_path[] = "fuzz/json";
    static const char source_path[] = "tests/fuzz/json_input.my";
    MarimayCliCheckTarget target;
    MarimayCompilerResult compiler;
    MarimayCompilerPackage *packages = NULL;
    const MarimayMirFunction **functions = NULL;
    const MarimayCheckedFunction *entry = NULL;
    MarimayFileId input_file = 0;
    MarimayCompilerStatus compiler_status;
    size_t package_count = 0;
    size_t function_count = 0;
    size_t index = 0;
    bool accepted = false;
    memset(&compiler, 0, sizeof(compiler));
    if (!mmay_json_fuzz_target_init(&target)
        || mmay_cli_standard_packages_add(&target)
            != MMAY_CLI_CHECK_TARGET_OK
        || mmay_compiler_service_add_document(
            &target.service,
            source_path,
            sizeof(source_path) - 1,
            1,
            mmay_json_fuzz_source,
            sizeof(mmay_json_fuzz_source) - 1,
            &input_file
        ) != MMAY_COMPILER_OK) {
        goto cleanup;
    }
    package_count = target.packages.length + 1;
    packages = mmay_allocate(
        &target.allocator, package_count * sizeof(*packages)
    );
    if (packages == NULL) goto cleanup;
    for (index = 0; index < target.packages.length; index += 1) {
        const MarimayCliCheckPackage *package = mmay_vector_at_const(
            &target.packages, index
        );
        if (package == NULL) goto cleanup;
        packages[index].import_path = (const char *)package->import_path.data;
        packages[index].import_path_length = package->import_path.length;
        packages[index].file_ids = package->file_ids.data;
        packages[index].file_count = package->file_ids.length;
    }
    packages[package_count - 1].import_path = import_path;
    packages[package_count - 1].import_path_length = sizeof(import_path) - 1;
    packages[package_count - 1].file_ids = &input_file;
    packages[package_count - 1].file_count = 1;
    compiler_status = mmay_compiler_service_check_graph(
        &target.service,
        packages,
        package_count,
        import_path,
        sizeof(import_path) - 1,
        &compiler
    );
    if (compiler_status != MMAY_COMPILER_OK
        || compiler.has_frontend_errors
        || compiler.checked.diagnostics.length != 0) {
        goto cleanup;
    }
    entry = mmay_json_fuzz_entry(&compiler.checked);
    if (entry == NULL) goto cleanup;
    functions = mmay_allocate(
        &target.allocator,
        compiler.checked.functions.length * sizeof(*functions)
    );
    if (functions == NULL) goto cleanup;
    if (!mmay_json_fuzz_collect_reachable(
            &compiler.checked,
            entry,
            functions,
            compiler.checked.functions.length,
            &function_count
        )) {
        goto cleanup;
    }
    accepted = mmay_mir_lower_bytecode_program(
        functions,
        function_count,
        entry->mir.symbol,
        target.allocator,
        mmay_mir_bytecode_program_lower_default_options(),
        &mmay_json_fuzz_program
    ) == MMAY_MIR_BYTECODE_LOWER_OK;
cleanup:
    mmay_deallocate(
        &target.allocator,
        functions,
        compiler.checked.functions.length * sizeof(*functions)
    );
    mmay_deallocate(
        &target.allocator, packages, package_count * sizeof(*packages)
    );
    mmay_compiler_result_dispose(&compiler);
    mmay_cli_check_target_dispose(&target);
    return accepted;
}

int LLVMFuzzerInitialize(int *argument_count, char ***arguments) {
    (void)argument_count;
    (void)arguments;
    if (!mmay_json_fuzz_compile()) abort();
    mmay_json_fuzz_initialized = true;
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    unsigned char input[MMAY_JSON_FUZZ_MAX_INPUT];
    MarimayBytecodeExecution execution;
    MarimayProgramArgument program_argument;
    MarimayProgramArgumentList program_arguments;
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue argument = {0};
    MarimayBytecodeValue result = {0};
    MarimayBytecodeRunStatus status;
    size_t index = 0;
    if (!mmay_json_fuzz_initialized || size > sizeof(input)) return 0;
    for (index = 0; index < size; index += 1) {
        input[index] = data[index] > 0 && data[index] < 128
            ? data[index] : (unsigned char)' ';
    }
    memset(&execution, 0, sizeof(execution));
    if (!mmay_bytecode_execution_init(
            &execution,
            mmay_system_allocator(),
            mmay_bytecode_execution_default_options()
        )) {
        abort();
    }
    program_argument.data = input;
    program_argument.length = size;
    program_arguments.values = &program_argument;
    program_arguments.count = 1;
    if (mmay_bytecode_execution_program_arguments(
            &execution,
            &mmay_bytecode_program_function(
                &mmay_json_fuzz_program,
                mmay_json_fuzz_program.entry_function
            )->body,
            program_arguments,
            &argument
        ) != MMAY_BYTECODE_RUN_OK) {
        mmay_bytecode_execution_dispose(&execution);
        abort();
    }
    options.execution = &execution;
    status = mmay_bytecode_run_program(
        &mmay_json_fuzz_program,
        &argument,
        1,
        mmay_system_allocator(),
        options,
        &result
    );
    if (status != MMAY_BYTECODE_RUN_OK
        || result.type != MMAY_BYTECODE_TYPE_I64
        || result.as.i64 < 0 || result.as.i64 > 3) {
        (void)fprintf(
            stderr,
            "unexpected JSON fuzz VM result: status=%d type=%u value=%lld\n",
            (int)status,
            (unsigned int)result.type,
            (long long)result.as.i64
        );
        mmay_bytecode_execution_dispose(&execution);
        abort();
    }
    mmay_bytecode_execution_dispose(&execution);
    return 0;
}
