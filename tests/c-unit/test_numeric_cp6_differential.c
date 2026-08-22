#include "test_native_c_emitter_support.h"

#include "test_checker_support.h"

#include <stdio.h>
#include <string.h>

#ifndef MMAY_TEST_SOURCE_DIR
#define MMAY_TEST_SOURCE_DIR "."
#endif

static void mmay_test_numeric_cp6_format_differential(void);
static void mmay_test_numeric_cp6_policy_panics(
    const MarimayCheckResult *checked,
    const MarimayMirFunction *const *functions,
    size_t function_count
);

static bool mmay_test_numeric_cp6_source(char *source, size_t capacity) {
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
        path, sizeof(path), "%s/examples/numeric_cp6.my", MMAY_TEST_SOURCE_DIR
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

static const MarimayCheckedFunction *mmay_test_numeric_cp6_entry(
    const MarimayCheckResult *checked,
    size_t *result_index
) {
    size_t index = 0;
    if (checked == NULL || result_index == NULL) {
        return NULL;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checked, index
        );
        if (function != NULL && function->name.length == 4
            && memcmp(function->name.data, "main", 4) == 0) {
            *result_index = index;
            return function;
        }
    }
    return NULL;
}

void mmay_test_numeric_cp6_differential(void) {
    char source[16384];
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *entry = NULL;
    const MarimayMirFunction *functions[16];
    MarimayBytecodeProgram bytecode;
    MarimayBytecodeProgram decoded;
    MarimayOwnedBytes encoded;
    MarimayBytecodeExecution execution;
    MarimayBytecodeExecutionOptions execution_options;
    MarimayBytecodeRunOptions run_options;
    MarimayMirValue oracle_result;
    MarimayBytecodeValue vm_result;
    MarimayNativeCProgram native_program;
    MarimayOwnedBytes generated;
    MarimayMirBytecodeLowerStatus lower_status;
    size_t entry_index = SIZE_MAX;
    size_t index = 0;
    bool checked_ready = false;
    bool bytecode_ready = false;
    bool decoded_ready = false;
    bool execution_ready = false;
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

    if (!mmay_test_numeric_cp6_source(source, sizeof(source))) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    if (!mmay_checker_fixture_init(&fixture, source) || !fixture.ast_initialized) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    if (mmay_check_ast(
            &fixture.ast,
            "test/numeric-cp6-differential",
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
            &checked, index
        );
        if (diagnostic != NULL) {
            fprintf(
                stderr,
                "numeric checkpoint-6 diagnostic %s at %zu..%zu: %s\n",
                diagnostic->code,
                diagnostic->start,
                diagnostic->end,
                diagnostic->message
            );
        }
    }
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(checked.functions.length <= sizeof(functions) / sizeof(functions[0]));
    for (index = 0; index < checked.functions.length
        && index < sizeof(functions) / sizeof(functions[0]); index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked, index
        );
        MMAY_EXPECT(function != NULL);
        if (function == NULL) {
            goto cleanup;
        }
        MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
        functions[index] = &function->mir;
        {
            MarimayBytecodeFunction lowered_function;
            MarimayMirBytecodeLowerStatus function_status;
            memset(&lowered_function, 0, sizeof(lowered_function));
            function_status = mmay_mir_lower_bytecode_function(
                &function->mir,
                mmay_system_allocator(),
                mmay_mir_bytecode_lower_default_options(),
                &lowered_function
            );
            if (function_status != MMAY_MIR_BYTECODE_LOWER_OK) {
                fprintf(
                    stderr,
                    "numeric checkpoint-6 function %.*s lower status: %d\n",
                    (int)function->name.length,
                    function->name.data,
                    (int)function_status
                );
            }
            MMAY_EXPECT(function_status == MMAY_MIR_BYTECODE_LOWER_OK);
            mmay_bytecode_function_dispose(&lowered_function);
        }
    }
    entry = mmay_test_numeric_cp6_entry(&checked, &entry_index);
    MMAY_EXPECT(entry != NULL);
    if (entry == NULL) {
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
    if (oracle_result.type == MMAY_TYPE_I64 && oracle_result.as.i64 != 6) {
        fprintf(
            stderr,
            "numeric checkpoint-6 MIR result: %lld\n",
            (long long)oracle_result.as.i64
        );
    }
    MMAY_EXPECT(oracle_result.as.i64 == 6);

    lower_status = mmay_mir_lower_bytecode_program(
            functions,
            checked.functions.length,
            entry->mir.symbol,
            mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(),
            &bytecode
        );
    if (lower_status != MMAY_MIR_BYTECODE_LOWER_OK) {
        fprintf(
            stderr,
            "numeric checkpoint-6 bytecode lower status: %d\n",
            (int)lower_status
        );
        MMAY_EXPECT(false);
        goto cleanup;
    }
    bytecode_ready = true;
    MMAY_EXPECT(mmay_bytecode_program_verify(&bytecode)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    if (mmay_bytecode_encode_program(
            &bytecode, mmay_system_allocator(), &encoded
        ) != MMAY_BYTECODE_CODEC_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    if (mmay_bytecode_decode_program(
            encoded.data,
            encoded.length,
            mmay_system_allocator(),
            mmay_bytecode_program_default_options(),
            &decoded
        ) != MMAY_BYTECODE_CODEC_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    decoded_ready = true;
    MMAY_EXPECT(mmay_bytecode_program_verify(&decoded)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    execution_options = mmay_bytecode_execution_default_options();
    execution_options.managed_heap.collect_before_allocation = true;
    if (!mmay_bytecode_execution_init(
            &execution, mmay_system_allocator(), execution_options
        )) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    execution_ready = true;
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
    MMAY_EXPECT(vm_result.as.i64 == 6);

    native_program.functions = functions;
    native_program.function_count = checked.functions.length;
    native_program.entry_symbol = entry->mir.symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &native_program,
        mmay_system_allocator(),
        mmay_native_c_emit_default_options(),
        &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (generated.data != NULL) {
        mmay_test_native_compile_and_run(&generated, "ok:i64:6\n", 0);
    }
#endif
    mmay_test_numeric_cp6_policy_panics(
        &checked, functions, checked.functions.length
    );

cleanup:
    mmay_owned_bytes_dispose(&generated);
    if (execution_ready) {
        mmay_bytecode_execution_dispose(&execution);
    }
    if (decoded_ready) {
        mmay_bytecode_program_dispose(&decoded);
    }
    mmay_owned_bytes_dispose(&encoded);
    if (bytecode_ready) {
        mmay_bytecode_program_dispose(&bytecode);
    }
    if (checked_ready) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
    mmay_test_numeric_cp6_format_differential();
}

static const MarimayCheckedFunction *mmay_test_numeric_cp6_find(
    const MarimayCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checked, index
        );
        if (function != NULL && function->name.length == length
            && memcmp(function->name.data, name, length) == 0) {
            return function;
        }
    }
    return NULL;
}

static bool mmay_test_numeric_cp6_policy_span(
    const MarimayMirFunction *function,
    MarimayMirSpan *span
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && instruction->kind == MMAY_MIR_INTEGER_POLICY) {
                *span = instruction->span;
                return true;
            }
        }
    }
    return false;
}

static void mmay_test_numeric_cp6_policy_panics(
    const MarimayCheckResult *checked,
    const MarimayMirFunction *const *functions,
    size_t function_count
) {
    static const struct {
        const char *name;
        MarimayMirInterpretStatus mir_status;
        MarimayBytecodeRunStatus vm_status;
        int trap_code;
    } cases[] = {
        {
            "wrappingZeroDivisor", MMAY_MIR_INTERPRET_DIVISION_BY_ZERO,
            MMAY_BYTECODE_RUN_DIVISION_BY_ZERO, 2
        },
        {
            "saturatingInvalidShift", MMAY_MIR_INTERPRET_INVALID_SHIFT,
            MMAY_BYTECODE_RUN_INVALID_SHIFT, 3
        }
    };
    size_t case_index = 0;
    for (case_index = 0;
        case_index < sizeof(cases) / sizeof(cases[0]); case_index += 1) {
        const MarimayCheckedFunction *entry = mmay_test_numeric_cp6_find(
            checked, cases[case_index].name
        );
        MarimayMirSpan span = {0};
        MarimayMirValue mir_result;
        MarimayBytecodeProgram bytecode;
        MarimayBytecodeExecution execution;
        MarimayBytecodeRunOptions options;
        MarimayBytecodeTrap trap;
        MarimayBytecodeValue vm_result;
        MarimayNativeCProgram native_program;
        MarimayOwnedBytes generated;
        char expected[96];
        memset(&mir_result, 0, sizeof(mir_result));
        memset(&bytecode, 0, sizeof(bytecode));
        memset(&execution, 0, sizeof(execution));
        memset(&trap, 0, sizeof(trap));
        memset(&vm_result, 0, sizeof(vm_result));
        memset(&native_program, 0, sizeof(native_program));
        memset(&generated, 0, sizeof(generated));
        MMAY_EXPECT(entry != NULL);
        MMAY_EXPECT(entry != NULL
            && mmay_test_numeric_cp6_policy_span(&entry->mir, &span));
        if (entry == NULL) {
            continue;
        }
        MMAY_EXPECT(mmay_mir_interpret(
            &entry->mir, NULL, 0, mmay_system_allocator(),
            mmay_mir_interpret_default_options(), &mir_result
        ) == cases[case_index].mir_status);
        MMAY_EXPECT(mmay_mir_lower_bytecode_program(
            functions, function_count, entry->mir.symbol,
            mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(), &bytecode
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        MMAY_EXPECT(mmay_bytecode_execution_init(
            &execution, mmay_system_allocator(),
            mmay_bytecode_execution_default_options()
        ));
        options = mmay_bytecode_run_default_options();
        options.execution = &execution;
        options.trap = &trap;
        MMAY_EXPECT(mmay_bytecode_run_program(
            &bytecode, NULL, 0, mmay_system_allocator(), options, &vm_result
        ) == cases[case_index].vm_status);
        MMAY_EXPECT(trap.has_span && trap.span.start == span.start
            && trap.span.end == span.end);
        native_program.functions = functions;
        native_program.function_count = function_count;
        native_program.entry_symbol = entry->mir.symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &native_program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &generated
        ) == MMAY_NATIVE_C_EMIT_OK);
        (void)snprintf(
            expected, sizeof(expected), "trap:%d:%zu:%zu\n",
            cases[case_index].trap_code, span.start, span.end
        );
#if !defined(_WIN32)
        if (generated.data != NULL) {
            mmay_test_native_compile_and_run(&generated, expected, 1);
        }
#endif
        mmay_owned_bytes_dispose(&generated);
        mmay_bytecode_execution_dispose(&execution);
        mmay_bytecode_program_dispose(&bytecode);
    }
}

static bool mmay_test_numeric_cp6_build_format(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    bool binary32
) {
    MarimayTypeId parameter = MMAY_TYPE_F32;
    MarimayMirRegister source = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function, mmay_system_allocator(), symbol,
            binary32 ? &parameter : NULL, binary32 ? 1 : 0,
            MMAY_TYPE_STRING, mmay_mir_function_default_options()
        )) {
        return false;
    }
    if (binary32) {
        source = 0;
    } else if (!mmay_mir_add_register(function, MMAY_TYPE_F64, &source)) {
        goto failure;
    }
    if (!mmay_mir_add_register(function, MMAY_TYPE_STRING, &result)
        || !mmay_mir_add_block(function, &block)) {
        goto failure;
    }
    if (!binary32) {
        instruction = mmay_test_native_instruction(
            MMAY_MIR_CONSTANT_F64, source, MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_REGISTER, 0, false, 100
        );
        instruction.f64_bits = UINT64_C(0x4079f00000000000);
        if (!mmay_mir_append_instruction(function, block, &instruction)) {
            goto failure;
        }
    }
    instruction = mmay_test_native_instruction(
        binary32 ? MMAY_MIR_FORMAT_F32 : MMAY_MIR_FORMAT_F64,
        result, source, MMAY_MIR_NO_REGISTER, 0, false, 101
    );
    if (!mmay_mir_append_instruction(function, block, &instruction)
        || !mmay_mir_set_terminator(
            function, block,
            mmay_test_native_terminator(
                MMAY_MIR_RETURN, result, MMAY_MIR_NO_BLOCK,
                MMAY_MIR_NO_BLOCK, 102
            )
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_mir_function_dispose(function);
    return false;
}

static void mmay_test_numeric_cp6_execute_format(
    const MarimayMirFunction *function,
    const MarimayMirValue *mir_arguments,
    const MarimayBytecodeValue *bytecode_arguments,
    size_t argument_count,
    const char *expected
) {
    MarimayMirExecution mir_execution;
    MarimayMirInterpretOptions mir_options;
    MarimayMirValue mir_result;
    MarimayBytecodeFunction bytecode;
    MarimayBytecodeExecution bytecode_execution;
    MarimayBytecodeRunOptions bytecode_options;
    MarimayBytecodeValue bytecode_result;
    const unsigned char *bytes = NULL;
    size_t length = 0;
    memset(&mir_execution, 0, sizeof(mir_execution));
    memset(&mir_result, 0, sizeof(mir_result));
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&bytecode_execution, 0, sizeof(bytecode_execution));
    memset(&bytecode_result, 0, sizeof(bytecode_result));
    MMAY_EXPECT(mmay_mir_execution_init(
        &mir_execution, mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    mir_options = mmay_mir_interpret_default_options();
    mir_options.execution = &mir_execution;
    MMAY_EXPECT(mmay_mir_interpret(
        function, mir_arguments, argument_count, mmay_system_allocator(),
        mir_options, &mir_result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mmay_mir_execution_string_bytes(
        &mir_execution, mir_result, &bytes, &length
    ));
    MMAY_EXPECT(length == strlen(expected)
        && memcmp(bytes, expected, length) == 0);
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        function, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &bytecode
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &bytecode_execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    bytecode_options = mmay_bytecode_run_default_options();
    bytecode_options.execution = &bytecode_execution;
    MMAY_EXPECT(mmay_bytecode_run(
        &bytecode, bytecode_arguments, argument_count,
        mmay_system_allocator(), bytecode_options, &bytecode_result
    ) == MMAY_BYTECODE_RUN_OK);
    bytes = NULL;
    length = 0;
    MMAY_EXPECT(mmay_bytecode_execution_string_bytes(
        &bytecode_execution, bytecode_result, &bytes, &length
    ));
    MMAY_EXPECT(length == strlen(expected)
        && memcmp(bytes, expected, length) == 0);
    mmay_bytecode_execution_dispose(&bytecode_execution);
    mmay_bytecode_function_dispose(&bytecode);
    mmay_mir_execution_dispose(&mir_execution);
}

static void mmay_test_numeric_cp6_format_allocation_failures(
    const MarimayMirFunction *function
) {
    MarimayBytecodeFunction bytecode;
    size_t failure_index = 0;
    bool mir_success = false;
    bool bytecode_success = false;
    memset(&bytecode, 0, sizeof(bytecode));
    for (failure_index = 0; failure_index < 128 && !mir_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMirExecution execution;
        MarimayMirInterpretOptions options;
        MarimayMirValue result;
        MarimayMirInterpretStatus status = MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        allocator = mmay_fail_allocator(&state);
        if (mmay_mir_execution_init(
                &execution, allocator, mmay_mir_execution_default_options()
            )) {
            options = mmay_mir_interpret_default_options();
            options.execution = &execution;
            status = mmay_mir_interpret(
                function, NULL, 0, allocator, options, &result
            );
        }
        MMAY_EXPECT(status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED
            || status == MMAY_MIR_INTERPRET_OK);
        mir_success = status == MMAY_MIR_INTERPRET_OK;
        mmay_mir_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(mir_success);
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(
        function, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &bytecode
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    for (failure_index = 0; failure_index < 128 && !bytecode_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayBytecodeExecution execution;
        MarimayBytecodeRunOptions options;
        MarimayBytecodeValue result;
        MarimayBytecodeRunStatus status = MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        allocator = mmay_fail_allocator(&state);
        if (mmay_bytecode_execution_init(
                &execution, allocator,
                mmay_bytecode_execution_default_options()
            )) {
            options = mmay_bytecode_run_default_options();
            options.execution = &execution;
            status = mmay_bytecode_run(
                &bytecode, NULL, 0, allocator, options, &result
            );
        }
        MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED
            || status == MMAY_BYTECODE_RUN_OK);
        bytecode_success = status == MMAY_BYTECODE_RUN_OK;
        mmay_bytecode_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(bytecode_success);
    mmay_bytecode_function_dispose(&bytecode);
}

#if !defined(_WIN32)
static void mmay_test_numeric_cp6_native_f32(
    const MarimayOwnedBytes *generated
) {
    static const char prefix[] = "#define main mmay_g_unused_main\n";
    static const char suffix[] =
        "\n#undef main\n"
        "int main(void) {\n"
        "  size_t start = 0, end = 0; int status;\n"
        "  mmay_g_panic panic = {0}; mmay_g_execution execution = {0};\n"
        "  MarimayManagedHeapOptions options; mmay_g_string result = {0};\n"
        "  mmay_g_keep_assets();\n"
        "  execution.allocator = mmay_g_make_allocator(&execution.fail_allocator);\n"
        "  options = mmay_managed_heap_default_options();\n"
        "  if (!mmay_managed_heap_init(&execution.heap, execution.allocator, options)) return 1;\n"
        "  status = mmay_g_f_87000000000000000000000000000000("
        "&execution, &panic, UINT32_C(0x3f800000), &result, &start, &end);\n"
        "  if (status != 0) { mmay_managed_heap_dispose(&execution.heap); return 1; }\n"
        "  printf(\"ok:string:%zu:\", result.length);\n"
        "  mmay_g_print_hex(result.data, result.length); putchar('\\n');\n"
        "  mmay_managed_heap_dispose(&execution.heap); return 0;\n"
        "}\n";
    MarimayAllocator allocator = mmay_system_allocator();
    MarimayOwnedBytes harness;
    size_t total = 0;
    size_t with_suffix = 0;
    memset(&harness, 0, sizeof(harness));
    MMAY_EXPECT(generated != NULL && generated->data != NULL);
    if (generated == NULL || generated->data == NULL
        || !mmay_size_add(sizeof(prefix) - 1, generated->length, &total)
        || !mmay_size_add(total, sizeof(suffix) - 1, &with_suffix)
        || !mmay_size_add(with_suffix, 1, &total)) {
        MMAY_EXPECT(false);
        return;
    }
    harness.data = mmay_allocate(&allocator, total);
    MMAY_EXPECT(harness.data != NULL);
    if (harness.data == NULL) {
        return;
    }
    harness.allocator = allocator;
    harness.length = total - 1;
    memcpy(harness.data, prefix, sizeof(prefix) - 1);
    memcpy(harness.data + sizeof(prefix) - 1, generated->data,
        generated->length);
    memcpy(harness.data + sizeof(prefix) - 1 + generated->length,
        suffix, sizeof(suffix) - 1);
    harness.data[harness.length] = 0;
    mmay_test_native_compile_and_run(
        &harness, "ok:string:3:312e30\n", 0
    );
    mmay_owned_bytes_dispose(&harness);
}
#endif

static void mmay_test_numeric_cp6_format_differential(void) {
    const MarimaySymbolId f64_symbol = {{0x86}};
    const MarimaySymbolId f32_symbol = {{0x87}};
    MarimayMirFunction f64_function;
    MarimayMirFunction f32_function;
    MarimayMirValue mir_f32;
    MarimayBytecodeValue bytecode_f32;
    const MarimayMirFunction *functions[2];
    MarimayNativeCProgram native_program;
    MarimayOwnedBytes generated;
    memset(&f64_function, 0, sizeof(f64_function));
    memset(&f32_function, 0, sizeof(f32_function));
    memset(&mir_f32, 0, sizeof(mir_f32));
    memset(&bytecode_f32, 0, sizeof(bytecode_f32));
    memset(&native_program, 0, sizeof(native_program));
    memset(&generated, 0, sizeof(generated));
    MMAY_EXPECT(mmay_test_numeric_cp6_build_format(
        &f64_function, f64_symbol, false
    ));
    MMAY_EXPECT(mmay_test_numeric_cp6_build_format(
        &f32_function, f32_symbol, true
    ));
    MMAY_EXPECT(mmay_mir_verify(&f64_function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_verify(&f32_function) == MMAY_MIR_VERIFY_OK);
    mmay_test_numeric_cp6_execute_format(
        &f64_function, NULL, NULL, 0, "415.0"
    );
    mmay_test_numeric_cp6_format_allocation_failures(&f64_function);
    mir_f32.type = MMAY_TYPE_F32;
    mir_f32.as.integer_bits = UINT64_C(0x3f800000);
    bytecode_f32.type = MMAY_BYTECODE_TYPE_F32;
    bytecode_f32.as.integer_bits = UINT64_C(0x3f800000);
    mmay_test_numeric_cp6_execute_format(
        &f32_function, &mir_f32, &bytecode_f32, 1, "1.0"
    );
    functions[0] = &f64_function;
    functions[1] = &f32_function;
    native_program.functions = functions;
    native_program.function_count = 2;
    native_program.entry_symbol = f64_symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &native_program, mmay_system_allocator(),
        mmay_native_c_emit_default_options(), &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (generated.data != NULL) {
        mmay_test_native_compile_and_run(
            &generated, "ok:string:5:3431352e30\n", 0
        );
        mmay_test_numeric_cp6_native_f32(&generated);
    }
#endif
    mmay_owned_bytes_dispose(&generated);
    mmay_mir_function_dispose(&f32_function);
    mmay_mir_function_dispose(&f64_function);
}
