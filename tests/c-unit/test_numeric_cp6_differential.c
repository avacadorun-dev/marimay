#include "test_native_c_emitter_support.h"

#include "test_checker_support.h"

#include <stdio.h>
#include <string.h>

#ifndef RUDA_TEST_SOURCE_DIR
#define RUDA_TEST_SOURCE_DIR "."
#endif

static void ruda_test_numeric_cp6_format_differential(void);
static void ruda_test_numeric_cp6_policy_panics(
    const RudaCheckResult *checked,
    const RudaMirFunction *const *functions,
    size_t function_count
);

static bool ruda_test_numeric_cp6_source(char *source, size_t capacity) {
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
        path, sizeof(path), "%s/examples/numeric_cp6.ruda", RUDA_TEST_SOURCE_DIR
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

static const RudaCheckedFunction *ruda_test_numeric_cp6_entry(
    const RudaCheckResult *checked,
    size_t *result_index
) {
    size_t index = 0;
    if (checked == NULL || result_index == NULL) {
        return NULL;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
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

void ruda_test_numeric_cp6_differential(void) {
    char source[16384];
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *entry = NULL;
    const RudaMirFunction *functions[16];
    RudaBytecodeProgram bytecode;
    RudaBytecodeProgram decoded;
    RudaOwnedBytes encoded;
    RudaBytecodeExecution execution;
    RudaBytecodeExecutionOptions execution_options;
    RudaBytecodeRunOptions run_options;
    RudaMirValue oracle_result;
    RudaBytecodeValue vm_result;
    RudaNativeCProgram native_program;
    RudaOwnedBytes generated;
    RudaMirBytecodeLowerStatus lower_status;
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

    if (!ruda_test_numeric_cp6_source(source, sizeof(source))) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    if (!ruda_checker_fixture_init(&fixture, source) || !fixture.ast_initialized) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    if (ruda_check_ast(
            &fixture.ast,
            "test/numeric-cp6-differential",
            ruda_system_allocator(),
            ruda_check_default_options(),
            &checked
        ) != RUDA_CHECK_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    checked_ready = true;
    for (index = 0; index < checked.diagnostics.length; index += 1) {
        const RudaCheckDiagnostic *diagnostic = ruda_check_diagnostic(
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
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(checked.functions.length <= sizeof(functions) / sizeof(functions[0]));
    for (index = 0; index < checked.functions.length
        && index < sizeof(functions) / sizeof(functions[0]); index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked, index
        );
        RUDA_EXPECT(function != NULL);
        if (function == NULL) {
            goto cleanup;
        }
        RUDA_EXPECT(ruda_mir_verify(&function->mir) == RUDA_MIR_VERIFY_OK);
        functions[index] = &function->mir;
        {
            RudaBytecodeFunction lowered_function;
            RudaMirBytecodeLowerStatus function_status;
            memset(&lowered_function, 0, sizeof(lowered_function));
            function_status = ruda_mir_lower_bytecode_function(
                &function->mir,
                ruda_system_allocator(),
                ruda_mir_bytecode_lower_default_options(),
                &lowered_function
            );
            if (function_status != RUDA_MIR_BYTECODE_LOWER_OK) {
                fprintf(
                    stderr,
                    "numeric checkpoint-6 function %.*s lower status: %d\n",
                    (int)function->name.length,
                    function->name.data,
                    (int)function_status
                );
            }
            RUDA_EXPECT(function_status == RUDA_MIR_BYTECODE_LOWER_OK);
            ruda_bytecode_function_dispose(&lowered_function);
        }
    }
    entry = ruda_test_numeric_cp6_entry(&checked, &entry_index);
    RUDA_EXPECT(entry != NULL);
    if (entry == NULL) {
        goto cleanup;
    }
    RUDA_EXPECT(ruda_check_interpret_function(
        &checked,
        entry_index,
        NULL,
        0,
        ruda_system_allocator(),
        ruda_check_interpret_default_options(),
        &oracle_result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(oracle_result.type == RUDA_TYPE_I64);
    if (oracle_result.type == RUDA_TYPE_I64 && oracle_result.as.i64 != 6) {
        fprintf(
            stderr,
            "numeric checkpoint-6 MIR result: %lld\n",
            (long long)oracle_result.as.i64
        );
    }
    RUDA_EXPECT(oracle_result.as.i64 == 6);

    lower_status = ruda_mir_lower_bytecode_program(
            functions,
            checked.functions.length,
            entry->mir.symbol,
            ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(),
            &bytecode
        );
    if (lower_status != RUDA_MIR_BYTECODE_LOWER_OK) {
        fprintf(
            stderr,
            "numeric checkpoint-6 bytecode lower status: %d\n",
            (int)lower_status
        );
        RUDA_EXPECT(false);
        goto cleanup;
    }
    bytecode_ready = true;
    RUDA_EXPECT(ruda_bytecode_program_verify(&bytecode)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    if (ruda_bytecode_encode_program(
            &bytecode, ruda_system_allocator(), &encoded
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
    decoded_ready = true;
    RUDA_EXPECT(ruda_bytecode_program_verify(&decoded)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    execution_options = ruda_bytecode_execution_default_options();
    execution_options.managed_heap.collect_before_allocation = true;
    if (!ruda_bytecode_execution_init(
            &execution, ruda_system_allocator(), execution_options
        )) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    execution_ready = true;
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
    RUDA_EXPECT(vm_result.as.i64 == 6);

    native_program.functions = functions;
    native_program.function_count = checked.functions.length;
    native_program.entry_symbol = entry->mir.symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &native_program,
        ruda_system_allocator(),
        ruda_native_c_emit_default_options(),
        &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (generated.data != NULL) {
        ruda_test_native_compile_and_run(&generated, "ok:i64:6\n", 0);
    }
#endif
    ruda_test_numeric_cp6_policy_panics(
        &checked, functions, checked.functions.length
    );

cleanup:
    ruda_owned_bytes_dispose(&generated);
    if (execution_ready) {
        ruda_bytecode_execution_dispose(&execution);
    }
    if (decoded_ready) {
        ruda_bytecode_program_dispose(&decoded);
    }
    ruda_owned_bytes_dispose(&encoded);
    if (bytecode_ready) {
        ruda_bytecode_program_dispose(&bytecode);
    }
    if (checked_ready) {
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
    ruda_test_numeric_cp6_format_differential();
}

static const RudaCheckedFunction *ruda_test_numeric_cp6_find(
    const RudaCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; index < checked->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            checked, index
        );
        if (function != NULL && function->name.length == length
            && memcmp(function->name.data, name, length) == 0) {
            return function;
        }
    }
    return NULL;
}

static bool ruda_test_numeric_cp6_policy_span(
    const RudaMirFunction *function,
    RudaMirSpan *span
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && instruction->kind == RUDA_MIR_INTEGER_POLICY) {
                *span = instruction->span;
                return true;
            }
        }
    }
    return false;
}

static void ruda_test_numeric_cp6_policy_panics(
    const RudaCheckResult *checked,
    const RudaMirFunction *const *functions,
    size_t function_count
) {
    static const struct {
        const char *name;
        RudaMirInterpretStatus mir_status;
        RudaBytecodeRunStatus vm_status;
        int trap_code;
    } cases[] = {
        {
            "wrappingZeroDivisor", RUDA_MIR_INTERPRET_DIVISION_BY_ZERO,
            RUDA_BYTECODE_RUN_DIVISION_BY_ZERO, 2
        },
        {
            "saturatingInvalidShift", RUDA_MIR_INTERPRET_INVALID_SHIFT,
            RUDA_BYTECODE_RUN_INVALID_SHIFT, 3
        }
    };
    size_t case_index = 0;
    for (case_index = 0;
        case_index < sizeof(cases) / sizeof(cases[0]); case_index += 1) {
        const RudaCheckedFunction *entry = ruda_test_numeric_cp6_find(
            checked, cases[case_index].name
        );
        RudaMirSpan span = {0};
        RudaMirValue mir_result;
        RudaBytecodeProgram bytecode;
        RudaBytecodeExecution execution;
        RudaBytecodeRunOptions options;
        RudaBytecodeTrap trap;
        RudaBytecodeValue vm_result;
        RudaNativeCProgram native_program;
        RudaOwnedBytes generated;
        char expected[96];
        memset(&mir_result, 0, sizeof(mir_result));
        memset(&bytecode, 0, sizeof(bytecode));
        memset(&execution, 0, sizeof(execution));
        memset(&trap, 0, sizeof(trap));
        memset(&vm_result, 0, sizeof(vm_result));
        memset(&native_program, 0, sizeof(native_program));
        memset(&generated, 0, sizeof(generated));
        RUDA_EXPECT(entry != NULL);
        RUDA_EXPECT(entry != NULL
            && ruda_test_numeric_cp6_policy_span(&entry->mir, &span));
        if (entry == NULL) {
            continue;
        }
        RUDA_EXPECT(ruda_mir_interpret(
            &entry->mir, NULL, 0, ruda_system_allocator(),
            ruda_mir_interpret_default_options(), &mir_result
        ) == cases[case_index].mir_status);
        RUDA_EXPECT(ruda_mir_lower_bytecode_program(
            functions, function_count, entry->mir.symbol,
            ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(), &bytecode
        ) == RUDA_MIR_BYTECODE_LOWER_OK);
        RUDA_EXPECT(ruda_bytecode_execution_init(
            &execution, ruda_system_allocator(),
            ruda_bytecode_execution_default_options()
        ));
        options = ruda_bytecode_run_default_options();
        options.execution = &execution;
        options.trap = &trap;
        RUDA_EXPECT(ruda_bytecode_run_program(
            &bytecode, NULL, 0, ruda_system_allocator(), options, &vm_result
        ) == cases[case_index].vm_status);
        RUDA_EXPECT(trap.has_span && trap.span.start == span.start
            && trap.span.end == span.end);
        native_program.functions = functions;
        native_program.function_count = function_count;
        native_program.entry_symbol = entry->mir.symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(
            &native_program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &generated
        ) == RUDA_NATIVE_C_EMIT_OK);
        (void)snprintf(
            expected, sizeof(expected), "trap:%d:%zu:%zu\n",
            cases[case_index].trap_code, span.start, span.end
        );
#if !defined(_WIN32)
        if (generated.data != NULL) {
            ruda_test_native_compile_and_run(&generated, expected, 1);
        }
#endif
        ruda_owned_bytes_dispose(&generated);
        ruda_bytecode_execution_dispose(&execution);
        ruda_bytecode_program_dispose(&bytecode);
    }
}

static bool ruda_test_numeric_cp6_build_format(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    bool binary32
) {
    RudaTypeId parameter = RUDA_TYPE_F32;
    RudaMirRegister source = RUDA_MIR_NO_REGISTER;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function, ruda_system_allocator(), symbol,
            binary32 ? &parameter : NULL, binary32 ? 1 : 0,
            RUDA_TYPE_STRING, ruda_mir_function_default_options()
        )) {
        return false;
    }
    if (binary32) {
        source = 0;
    } else if (!ruda_mir_add_register(function, RUDA_TYPE_F64, &source)) {
        goto failure;
    }
    if (!ruda_mir_add_register(function, RUDA_TYPE_STRING, &result)
        || !ruda_mir_add_block(function, &block)) {
        goto failure;
    }
    if (!binary32) {
        instruction = ruda_test_native_instruction(
            RUDA_MIR_CONSTANT_F64, source, RUDA_MIR_NO_REGISTER,
            RUDA_MIR_NO_REGISTER, 0, false, 100
        );
        instruction.f64_bits = UINT64_C(0x4079f00000000000);
        if (!ruda_mir_append_instruction(function, block, &instruction)) {
            goto failure;
        }
    }
    instruction = ruda_test_native_instruction(
        binary32 ? RUDA_MIR_FORMAT_F32 : RUDA_MIR_FORMAT_F64,
        result, source, RUDA_MIR_NO_REGISTER, 0, false, 101
    );
    if (!ruda_mir_append_instruction(function, block, &instruction)
        || !ruda_mir_set_terminator(
            function, block,
            ruda_test_native_terminator(
                RUDA_MIR_RETURN, result, RUDA_MIR_NO_BLOCK,
                RUDA_MIR_NO_BLOCK, 102
            )
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_mir_function_dispose(function);
    return false;
}

static void ruda_test_numeric_cp6_execute_format(
    const RudaMirFunction *function,
    const RudaMirValue *mir_arguments,
    const RudaBytecodeValue *bytecode_arguments,
    size_t argument_count,
    const char *expected
) {
    RudaMirExecution mir_execution;
    RudaMirInterpretOptions mir_options;
    RudaMirValue mir_result;
    RudaBytecodeFunction bytecode;
    RudaBytecodeExecution bytecode_execution;
    RudaBytecodeRunOptions bytecode_options;
    RudaBytecodeValue bytecode_result;
    const unsigned char *bytes = NULL;
    size_t length = 0;
    memset(&mir_execution, 0, sizeof(mir_execution));
    memset(&mir_result, 0, sizeof(mir_result));
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&bytecode_execution, 0, sizeof(bytecode_execution));
    memset(&bytecode_result, 0, sizeof(bytecode_result));
    RUDA_EXPECT(ruda_mir_execution_init(
        &mir_execution, ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    mir_options = ruda_mir_interpret_default_options();
    mir_options.execution = &mir_execution;
    RUDA_EXPECT(ruda_mir_interpret(
        function, mir_arguments, argument_count, ruda_system_allocator(),
        mir_options, &mir_result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(ruda_mir_execution_string_bytes(
        &mir_execution, mir_result, &bytes, &length
    ));
    RUDA_EXPECT(length == strlen(expected)
        && memcmp(bytes, expected, length) == 0);
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        function, ruda_system_allocator(),
        ruda_mir_bytecode_lower_default_options(), &bytecode
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &bytecode_execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()
    ));
    bytecode_options = ruda_bytecode_run_default_options();
    bytecode_options.execution = &bytecode_execution;
    RUDA_EXPECT(ruda_bytecode_run(
        &bytecode, bytecode_arguments, argument_count,
        ruda_system_allocator(), bytecode_options, &bytecode_result
    ) == RUDA_BYTECODE_RUN_OK);
    bytes = NULL;
    length = 0;
    RUDA_EXPECT(ruda_bytecode_execution_string_bytes(
        &bytecode_execution, bytecode_result, &bytes, &length
    ));
    RUDA_EXPECT(length == strlen(expected)
        && memcmp(bytes, expected, length) == 0);
    ruda_bytecode_execution_dispose(&bytecode_execution);
    ruda_bytecode_function_dispose(&bytecode);
    ruda_mir_execution_dispose(&mir_execution);
}

static void ruda_test_numeric_cp6_format_allocation_failures(
    const RudaMirFunction *function
) {
    RudaBytecodeFunction bytecode;
    size_t failure_index = 0;
    bool mir_success = false;
    bool bytecode_success = false;
    memset(&bytecode, 0, sizeof(bytecode));
    for (failure_index = 0; failure_index < 128 && !mir_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaMirExecution execution;
        RudaMirInterpretOptions options;
        RudaMirValue result;
        RudaMirInterpretStatus status = RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        allocator = ruda_fail_allocator(&state);
        if (ruda_mir_execution_init(
                &execution, allocator, ruda_mir_execution_default_options()
            )) {
            options = ruda_mir_interpret_default_options();
            options.execution = &execution;
            status = ruda_mir_interpret(
                function, NULL, 0, allocator, options, &result
            );
        }
        RUDA_EXPECT(status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED
            || status == RUDA_MIR_INTERPRET_OK);
        mir_success = status == RUDA_MIR_INTERPRET_OK;
        ruda_mir_execution_dispose(&execution);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(mir_success);
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(
        function, ruda_system_allocator(),
        ruda_mir_bytecode_lower_default_options(), &bytecode
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    for (failure_index = 0; failure_index < 128 && !bytecode_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaBytecodeExecution execution;
        RudaBytecodeRunOptions options;
        RudaBytecodeValue result;
        RudaBytecodeRunStatus status = RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        allocator = ruda_fail_allocator(&state);
        if (ruda_bytecode_execution_init(
                &execution, allocator,
                ruda_bytecode_execution_default_options()
            )) {
            options = ruda_bytecode_run_default_options();
            options.execution = &execution;
            status = ruda_bytecode_run(
                &bytecode, NULL, 0, allocator, options, &result
            );
        }
        RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED
            || status == RUDA_BYTECODE_RUN_OK);
        bytecode_success = status == RUDA_BYTECODE_RUN_OK;
        ruda_bytecode_execution_dispose(&execution);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(bytecode_success);
    ruda_bytecode_function_dispose(&bytecode);
}

#if !defined(_WIN32)
static void ruda_test_numeric_cp6_native_f32(
    const RudaOwnedBytes *generated
) {
    static const char prefix[] = "#define main ruda_g_unused_main\n";
    static const char suffix[] =
        "\n#undef main\n"
        "int main(void) {\n"
        "  size_t start = 0, end = 0; int status;\n"
        "  ruda_g_panic panic = {0}; ruda_g_execution execution = {0};\n"
        "  RudaManagedHeapOptions options; ruda_g_string result = {0};\n"
        "  ruda_g_keep_assets();\n"
        "  execution.allocator = ruda_g_make_allocator(&execution.fail_allocator);\n"
        "  options = ruda_managed_heap_default_options();\n"
        "  if (!ruda_managed_heap_init(&execution.heap, execution.allocator, options)) return 1;\n"
        "  status = ruda_g_f_87000000000000000000000000000000("
        "&execution, &panic, UINT32_C(0x3f800000), &result, &start, &end);\n"
        "  if (status != 0) { ruda_managed_heap_dispose(&execution.heap); return 1; }\n"
        "  printf(\"ok:string:%zu:\", result.length);\n"
        "  ruda_g_print_hex(result.data, result.length); putchar('\\n');\n"
        "  ruda_managed_heap_dispose(&execution.heap); return 0;\n"
        "}\n";
    RudaAllocator allocator = ruda_system_allocator();
    RudaOwnedBytes harness;
    size_t total = 0;
    size_t with_suffix = 0;
    memset(&harness, 0, sizeof(harness));
    RUDA_EXPECT(generated != NULL && generated->data != NULL);
    if (generated == NULL || generated->data == NULL
        || !ruda_size_add(sizeof(prefix) - 1, generated->length, &total)
        || !ruda_size_add(total, sizeof(suffix) - 1, &with_suffix)
        || !ruda_size_add(with_suffix, 1, &total)) {
        RUDA_EXPECT(false);
        return;
    }
    harness.data = ruda_allocate(&allocator, total);
    RUDA_EXPECT(harness.data != NULL);
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
    ruda_test_native_compile_and_run(
        &harness, "ok:string:3:312e30\n", 0
    );
    ruda_owned_bytes_dispose(&harness);
}
#endif

static void ruda_test_numeric_cp6_format_differential(void) {
    const RudaSymbolId f64_symbol = {{0x86}};
    const RudaSymbolId f32_symbol = {{0x87}};
    RudaMirFunction f64_function;
    RudaMirFunction f32_function;
    RudaMirValue mir_f32;
    RudaBytecodeValue bytecode_f32;
    const RudaMirFunction *functions[2];
    RudaNativeCProgram native_program;
    RudaOwnedBytes generated;
    memset(&f64_function, 0, sizeof(f64_function));
    memset(&f32_function, 0, sizeof(f32_function));
    memset(&mir_f32, 0, sizeof(mir_f32));
    memset(&bytecode_f32, 0, sizeof(bytecode_f32));
    memset(&native_program, 0, sizeof(native_program));
    memset(&generated, 0, sizeof(generated));
    RUDA_EXPECT(ruda_test_numeric_cp6_build_format(
        &f64_function, f64_symbol, false
    ));
    RUDA_EXPECT(ruda_test_numeric_cp6_build_format(
        &f32_function, f32_symbol, true
    ));
    RUDA_EXPECT(ruda_mir_verify(&f64_function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_verify(&f32_function) == RUDA_MIR_VERIFY_OK);
    ruda_test_numeric_cp6_execute_format(
        &f64_function, NULL, NULL, 0, "415.0"
    );
    ruda_test_numeric_cp6_format_allocation_failures(&f64_function);
    mir_f32.type = RUDA_TYPE_F32;
    mir_f32.as.integer_bits = UINT64_C(0x3f800000);
    bytecode_f32.type = RUDA_BYTECODE_TYPE_F32;
    bytecode_f32.as.integer_bits = UINT64_C(0x3f800000);
    ruda_test_numeric_cp6_execute_format(
        &f32_function, &mir_f32, &bytecode_f32, 1, "1.0"
    );
    functions[0] = &f64_function;
    functions[1] = &f32_function;
    native_program.functions = functions;
    native_program.function_count = 2;
    native_program.entry_symbol = f64_symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &native_program, ruda_system_allocator(),
        ruda_native_c_emit_default_options(), &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (generated.data != NULL) {
        ruda_test_native_compile_and_run(
            &generated, "ok:string:5:3431352e30\n", 0
        );
        ruda_test_numeric_cp6_native_f32(&generated);
    }
#endif
    ruda_owned_bytes_dispose(&generated);
    ruda_mir_function_dispose(&f32_function);
    ruda_mir_function_dispose(&f64_function);
}
