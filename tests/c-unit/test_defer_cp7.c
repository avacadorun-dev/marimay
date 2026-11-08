#include "test_native_c_emitter_support.h"

#include "test_checker_support.h"
#include "test_mir_support.h"

#include <stdio.h>
#include <string.h>

#ifndef MMAY_TEST_SOURCE_DIR
#define MMAY_TEST_SOURCE_DIR "."
#endif

static bool mmay_test_defer_source(char *source, size_t capacity) {
    char path[1024];
    FILE *file = NULL;
    size_t count = 0;
    int written = snprintf(
        path, sizeof(path), "%s/examples/defer_cp7.my", MMAY_TEST_SOURCE_DIR
    );
    if (source == NULL || capacity < 2 || written < 0
        || (size_t)written >= sizeof(path)) {
        return false;
    }
    file = mmay_test_open_file(path, "rb");
    if (file == NULL) {
        return false;
    }
    count = fread(source, 1, capacity - 1, file);
    if (ferror(file) != 0 || feof(file) == 0 || fclose(file) != 0) {
        return false;
    }
    source[count] = '\0';
    return true;
}

static const MarimayCheckedFunction *mmay_test_defer_find(
    const MarimayCheckResult *checked,
    const char *name,
    size_t *result_index
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checked, index
        );
        if (function != NULL && function->name.length == length
            && memcmp(function->name.data, name, length) == 0) {
            if (result_index != NULL) {
                *result_index = index;
            }
            return function;
        }
    }
    return NULL;
}

static void mmay_test_defer_source_semantics(void) {
    char source[16384];
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirValue result;
    size_t main_index = SIZE_MAX;
    size_t index = 0;
    bool reached_success = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_test_defer_source(source, sizeof(source)));
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    if (!fixture.ast_initialized) {
        goto cleanup;
    }
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/defer-cp7", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (!checked.initialized) {
        goto cleanup;
    }
    for (index = 0; index < checked.diagnostics.length; index += 1) {
        const MarimayCheckDiagnostic *diagnostic = mmay_check_diagnostic(
            &checked, index
        );
        if (diagnostic != NULL) {
            fprintf(
                stderr, "defer diagnostic %s at %zu..%zu: %s\n",
                diagnostic->code, diagnostic->start, diagnostic->end,
                diagnostic->message
            );
        }
    }
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(mmay_test_defer_find(&checked, "main", &main_index) != NULL);
    if (main_index != SIZE_MAX) {
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, main_index, NULL, 0, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_I64);
        MMAY_EXPECT(result.as.i64 == INT64_C(2136541923789));
    }
    for (index = 0; index < 128 && main_index != SIZE_MAX
        && !reached_success; index += 1) {
        MarimayFailAllocator state;
        MarimayMirInterpretStatus status;
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        status = mmay_check_interpret_function(
            &checked, main_index, NULL, 0, mmay_fail_allocator(&state),
            mmay_check_interpret_default_options(), &result
        );
        MMAY_EXPECT(status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED
            || status == MMAY_MIR_INTERPRET_OK);
        if (status == MMAY_MIR_INTERPRET_OK) {
            reached_success = true;
            MMAY_EXPECT(result.as.i64 == INT64_C(2136541923789));
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);

cleanup:
    if (checked.initialized) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_defer_engine_differential(void) {
    char source[16384];
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *entry = NULL;
    const MarimayMirFunction *functions[32] = {0};
    MarimayBytecodeProgram bytecode;
    MarimayBytecodeProgram decoded;
    MarimayOwnedBytes encoded;
    MarimayBytecodeExecution execution;
    MarimayBytecodeRunOptions run_options;
    MarimayBytecodeValue vm_result;
    MarimayNativeCProgram native_program;
    MarimayNativeCEmitStatus native_status;
    MarimayOwnedBytes generated;
    size_t entry_index = SIZE_MAX;
    size_t index = 0;
    bool bytecode_ready = false;
    bool decoded_ready = false;
    bool execution_ready = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&execution, 0, sizeof(execution));
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&native_program, 0, sizeof(native_program));
    memset(&generated, 0, sizeof(generated));
    if (!mmay_test_defer_source(source, sizeof(source))
        || !mmay_checker_fixture_init(&fixture, source)
        || !fixture.ast_initialized
        || mmay_check_ast(
            &fixture.ast, "test/defer-cp7-differential",
            mmay_system_allocator(), mmay_check_default_options(), &checked
        ) != MMAY_CHECK_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(checked.functions.length
        <= sizeof(functions) / sizeof(functions[0]));
    for (index = 0; index < checked.functions.length
        && index < sizeof(functions) / sizeof(functions[0]); index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked, index
        );
        if (function == NULL) {
            MMAY_EXPECT(false);
            goto cleanup;
        }
        MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
        functions[index] = &function->mir;
    }
    entry = mmay_test_defer_find(&checked, "main", &entry_index);
    MMAY_EXPECT(entry != NULL);
    if (entry == NULL || mmay_mir_lower_bytecode_program(
            functions, checked.functions.length, entry->mir.symbol,
            mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(), &bytecode
        ) != MMAY_MIR_BYTECODE_LOWER_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    bytecode_ready = true;
    MMAY_EXPECT(mmay_bytecode_program_verify(&bytecode)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    if (mmay_bytecode_encode_program(
            &bytecode, mmay_system_allocator(), &encoded
        ) != MMAY_BYTECODE_CODEC_OK
        || mmay_bytecode_decode_program(
            encoded.data, encoded.length, mmay_system_allocator(),
            mmay_bytecode_program_default_options(), &decoded
        ) != MMAY_BYTECODE_CODEC_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    decoded_ready = true;
    MMAY_EXPECT(mmay_bytecode_program_verify(&decoded)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    if (!execution.initialized) {
        goto cleanup;
    }
    execution_ready = true;
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &decoded, NULL, 0, mmay_system_allocator(), run_options, &vm_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_I64);
    MMAY_EXPECT(vm_result.as.i64 == INT64_C(2136541923789));

    native_program.functions = functions;
    native_program.function_count = checked.functions.length;
    native_program.entry_symbol = entry->mir.symbol;
    native_status = mmay_native_c_emit_program(
        &native_program, mmay_system_allocator(),
        mmay_native_c_emit_default_options(), &generated
    );
    if (native_status != MMAY_NATIVE_C_EMIT_OK) {
        fprintf(stderr, "defer native emit status: %d\n", (int)native_status);
    }
    MMAY_EXPECT(native_status == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (generated.data != NULL) {
        mmay_test_native_compile_and_run(
            &generated, "ok:i64:2136541923789\n", 0
        );
    }
#endif

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
    if (checked.initialized) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_defer_checker_negatives(void) {
    static const char *const sources[] = {
        "fn value(): int { return 1; }\n"
        "fn main(): void { defer value(); }\n",
        "enum CleanupError { Failed }\n"
        "fn fallible(): Result<int, CleanupError> { return Ok(1); }\n"
        "fn main(): void { defer fallible(); }\n"
    };
    size_t source_index = 0;
    for (source_index = 0;
        source_index < sizeof(sources) / sizeof(sources[0]);
        source_index += 1) {
        MarimayCheckerFixture fixture;
        MarimayCheckResult checked;
        const MarimayCheckDiagnostic *diagnostic = NULL;
        memset(&fixture, 0, sizeof(fixture));
        memset(&checked, 0, sizeof(checked));
        MMAY_EXPECT(mmay_checker_fixture_init(&fixture, sources[source_index]));
        if (fixture.ast_initialized) {
            MMAY_EXPECT(mmay_check_ast(
                &fixture.ast, "test/defer-negative", mmay_system_allocator(),
                mmay_check_default_options(), &checked
            ) == MMAY_CHECK_OK);
        }
        MMAY_EXPECT(checked.initialized);
        MMAY_EXPECT(checked.diagnostics.length == 1);
        diagnostic = mmay_check_diagnostic(&checked, 0);
        MMAY_EXPECT(diagnostic != NULL);
        MMAY_EXPECT(diagnostic != NULL
            && strcmp(diagnostic->code, MMAY_DIAGNOSTIC_TYPE_MISMATCH) == 0);
        MMAY_EXPECT(diagnostic != NULL
            && strstr(diagnostic->message, "must return void") != NULL);
        if (checked.initialized) {
            mmay_check_result_dispose(&checked);
        }
        mmay_checker_fixture_dispose(&fixture);
    }
}

static void mmay_test_defer_native_cycle(void) {
    static const char source[] =
        "fn recursive(): void { defer recursive(); }\n"
        "fn main(): void { recursive(); }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayMirFunction *functions[2] = {0};
    const MarimayCheckedFunction *entry = NULL;
    MarimayNativeCProgram program;
    MarimayOwnedBytes generated;
    size_t index = 0;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    if (!fixture.ast_initialized || mmay_check_ast(
            &fixture.ast, "test/defer-cycle", mmay_system_allocator(),
            mmay_check_default_options(), &checked
        ) != MMAY_CHECK_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(checked.functions.length == 2);
    for (index = 0; index < checked.functions.length && index < 2;
        index += 1) {
        functions[index] = &mmay_checked_function(&checked, index)->mir;
    }
    entry = mmay_test_defer_find(&checked, "main", NULL);
    MMAY_EXPECT(entry != NULL);
    if (entry != NULL && checked.functions.length == 2) {
        program.functions = functions;
        program.function_count = 2;
        program.entry_symbol = entry->mir.symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &generated
        ) == MMAY_NATIVE_C_EMIT_UNSUPPORTED_CALL);
    }

cleanup:
    mmay_owned_bytes_dispose(&generated);
    if (checked.initialized) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_defer_mir_invariants(void) {
    static const char source[] =
        "fn cleanup(): void {}\n"
        "fn branch(flag: bool): void { if flag { defer cleanup(); } }\n"
        "fn main(): void { defer cleanup(); defer cleanup(); }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckedFunction *entry = NULL;
    MarimayMirInstruction *first_cleanup = NULL;
    MarimayMirCallId other_call = MMAY_MIR_NO_CALL;
    MarimayMirCallId original_call = MMAY_MIR_NO_CALL;
    MarimayMirBlock *cleanup_block = NULL;
    size_t cleanup_index = SIZE_MAX;
    size_t entry_index = SIZE_MAX;
    size_t block_index = 0;
    size_t failure_index = 0;
    bool reached_success = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    if (!fixture.ast_initialized || mmay_check_ast(
            &fixture.ast, "test/defer-mir", mmay_system_allocator(),
            mmay_check_default_options(), &checked
        ) != MMAY_CHECK_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    (void)mmay_test_defer_find(&checked, "main", &entry_index);
    entry = entry_index < checked.functions.length
        ? mmay_vector_at(&checked.functions, entry_index)
        : NULL;
    MMAY_EXPECT(entry != NULL);
    if (entry == NULL) {
        goto cleanup;
    }
    for (block_index = 0; block_index < entry->mir.blocks.length;
        block_index += 1) {
        MarimayMirBlock *block = mmay_vector_at(&entry->mir.blocks, block_index);
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            MarimayMirInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && instruction->kind == MMAY_MIR_DEFER_REGISTER
                && other_call == MMAY_MIR_NO_CALL) {
                other_call = instruction->call;
            }
            if (instruction != NULL
                && instruction->kind == MMAY_MIR_DEFER_CALL
                && first_cleanup == NULL) {
                first_cleanup = instruction;
                cleanup_block = block;
                cleanup_index = instruction_index;
            }
        }
    }
    MMAY_EXPECT(first_cleanup != NULL && cleanup_block != NULL);
    MMAY_EXPECT(other_call != MMAY_MIR_NO_CALL);
    if (first_cleanup != NULL && other_call == first_cleanup->call) {
        other_call = (MarimayMirCallId)(other_call == 0 ? 1 : 0);
    }
    if (first_cleanup != NULL && other_call != MMAY_MIR_NO_CALL) {
        original_call = first_cleanup->call;
        first_cleanup->call = other_call;
        MMAY_EXPECT(mmay_mir_verify(&entry->mir)
            == MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW);
        first_cleanup->call = original_call;
    }
    if (cleanup_block != NULL && cleanup_index != SIZE_MAX) {
        size_t old_length = cleanup_block->instructions.length;
        size_t removed = old_length - cleanup_index;
        cleanup_block->instructions.length = cleanup_index;
        entry->mir.instruction_count -= removed;
        MMAY_EXPECT(mmay_mir_verify(&entry->mir)
            == MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW);
        cleanup_block->instructions.length = old_length;
        entry->mir.instruction_count += removed;
    }
    MMAY_EXPECT(mmay_mir_verify(&entry->mir) == MMAY_MIR_VERIFY_OK);
    {
        MarimayCheckedFunction *branch = NULL;
        MarimayMirInstruction *merge_cleanup = NULL;
        size_t branch_index = SIZE_MAX;
        branch = (MarimayCheckedFunction *)mmay_test_defer_find(
            &checked, "branch", &branch_index);
        for (block_index = 0; branch != NULL
            && block_index < branch->mir.blocks.length; block_index += 1) {
            MarimayMirBlock *block = mmay_vector_at(
                &branch->mir.blocks, block_index);
            size_t instruction_index = 0;
            for (instruction_index = 0; block != NULL
                && instruction_index < block->instructions.length;
                instruction_index += 1) {
                MarimayMirInstruction *instruction = mmay_vector_at(
                    &block->instructions, instruction_index
                );
                if (instruction != NULL
                    && instruction->kind == MMAY_MIR_DEFER_CALL) {
                    merge_cleanup = instruction;
                }
            }
        }
        MMAY_EXPECT(merge_cleanup != NULL);
        if (merge_cleanup != NULL) {
            merge_cleanup->kind = MMAY_MIR_DEFER_REGISTER;
            MMAY_EXPECT(mmay_mir_verify(&branch->mir)
                == MMAY_MIR_VERIFY_INVALID_CONTROL_FLOW);
            merge_cleanup->kind = MMAY_MIR_DEFER_CALL;
            MMAY_EXPECT(mmay_mir_verify(&branch->mir) == MMAY_MIR_VERIFY_OK);
        }
    }
    for (failure_index = 0; failure_index < 64 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayMirFunction shallow = entry->mir;
        MarimayMirVerifyStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        shallow.allocator = mmay_fail_allocator(&state);
        status = mmay_mir_verify(&shallow);
        MMAY_EXPECT(status == MMAY_MIR_VERIFY_ALLOCATION_FAILED
            || status == MMAY_MIR_VERIFY_OK);
        reached_success = status == MMAY_MIR_VERIFY_OK;
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);

cleanup:
    if (checked.initialized) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_defer_bytecode_invariants(void) {
    static const char source[] =
        "fn cleanup(): void {}\n"
        "fn branch(flag: bool): void { if flag { defer cleanup(); } }\n"
        "fn main(): void { defer cleanup(); defer cleanup(); }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *entry = NULL;
    MarimayBytecodeFunction lowered;
    MarimayBytecodeProgram program;
    const MarimayMirFunction *functions[3] = {0};
    MarimayBytecodeInstruction *cleanup = NULL;
    MarimayBytecodeInstruction *return_instruction = NULL;
    MarimayBytecodeBlock *cleanup_block = NULL;
    uint64_t original_call = 0;
    size_t entry_index = SIZE_MAX;
    size_t block_index = 0;
    size_t cleanup_index = SIZE_MAX;
    size_t failure_index = 0;
    bool lowered_ready = false;
    bool program_ready = false;
    bool reached_success = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&lowered, 0, sizeof(lowered));
    memset(&program, 0, sizeof(program));
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    if (!fixture.ast_initialized || mmay_check_ast(
            &fixture.ast, "test/defer-bytecode", mmay_system_allocator(),
            mmay_check_default_options(), &checked
        ) != MMAY_CHECK_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    entry = mmay_test_defer_find(&checked, "main", &entry_index);
    MMAY_EXPECT(entry != NULL);
    if (entry == NULL || mmay_mir_lower_bytecode_function(
            &entry->mir, mmay_system_allocator(),
            mmay_mir_bytecode_lower_default_options(), &lowered
        ) != MMAY_MIR_BYTECODE_LOWER_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    lowered_ready = true;
    MMAY_EXPECT(mmay_bytecode_verify(&lowered) == MMAY_BYTECODE_VERIFY_OK);
    {
        const MarimayCheckedFunction *branch = mmay_test_defer_find(
            &checked, "branch", NULL);
        MarimayBytecodeFunction branch_lowered;
        MarimayBytecodeInstruction *merge_cleanup = NULL;
        memset(&branch_lowered, 0, sizeof(branch_lowered));
        MMAY_EXPECT(branch != NULL && mmay_mir_lower_bytecode_function(
            &branch->mir, mmay_system_allocator(),
            mmay_mir_bytecode_lower_default_options(), &branch_lowered
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        for (block_index = 0; block_index < branch_lowered.blocks.length;
            block_index += 1) {
            MarimayBytecodeBlock *block = mmay_vector_at(
                &branch_lowered.blocks, block_index
            );
            size_t instruction_index = 0;
            for (instruction_index = 0; block != NULL
                && instruction_index < block->instructions.length;
                instruction_index += 1) {
                MarimayBytecodeInstruction *instruction = mmay_vector_at(
                    &block->instructions, instruction_index
                );
                if (instruction != NULL
                    && instruction->opcode == MMAY_OPCODE_DEFER_CALL) {
                    merge_cleanup = instruction;
                }
            }
        }
        MMAY_EXPECT(merge_cleanup != NULL);
        if (merge_cleanup != NULL) {
            merge_cleanup->opcode = MMAY_OPCODE_DEFER_REGISTER;
            MMAY_EXPECT(mmay_bytecode_verify(&branch_lowered)
                == MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
            merge_cleanup->opcode = MMAY_OPCODE_DEFER_CALL;
            MMAY_EXPECT(mmay_bytecode_verify(&branch_lowered)
                == MMAY_BYTECODE_VERIFY_OK);
        }
        mmay_bytecode_function_dispose(&branch_lowered);
    }
    for (block_index = 0; block_index < lowered.blocks.length;
        block_index += 1) {
        MarimayBytecodeBlock *block = mmay_vector_at(
            &lowered.blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            MarimayBytecodeInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && instruction->opcode == MMAY_OPCODE_DEFER_CALL) {
                cleanup = instruction;
                cleanup_block = block;
                cleanup_index = instruction_index;
            }
        }
    }
    MMAY_EXPECT(cleanup != NULL && cleanup_block != NULL);
    if (cleanup != NULL && lowered.calls.length > 1) {
        original_call = cleanup->operands[1].unsigned_value;
        cleanup->operands[1].unsigned_value = original_call == 0 ? 1 : 0;
        MMAY_EXPECT(mmay_bytecode_verify(&lowered)
            == MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
        cleanup->operands[1].unsigned_value = original_call;
    }
    if (cleanup_block != NULL
        && cleanup_index + 2 == cleanup_block->instructions.length) {
        return_instruction = mmay_vector_at(
            &cleanup_block->instructions, cleanup_index + 1
        );
        if (return_instruction != NULL) {
            MarimayBytecodeInstruction saved_cleanup = *cleanup;
            *cleanup = *return_instruction;
            cleanup_block->instructions.length -= 1;
            lowered.instruction_count -= 1;
            MMAY_EXPECT(mmay_bytecode_verify(&lowered)
                == MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
            cleanup_block->instructions.length += 1;
            lowered.instruction_count += 1;
            *cleanup = saved_cleanup;
        }
    } else {
        MMAY_EXPECT(false);
    }
    MMAY_EXPECT(mmay_bytecode_verify(&lowered) == MMAY_BYTECODE_VERIFY_OK);
    for (failure_index = 0; failure_index < 64 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeFunction shallow = lowered;
        MarimayBytecodeVerifyStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        shallow.allocator = mmay_fail_allocator(&state);
        status = mmay_bytecode_verify(&shallow);
        MMAY_EXPECT(status == MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED
            || status == MMAY_BYTECODE_VERIFY_OK);
        reached_success = status == MMAY_BYTECODE_VERIFY_OK;
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
    if (checked.functions.length == 3) {
        for (block_index = 0; block_index < 3; block_index += 1) {
            functions[block_index] = &mmay_checked_function(
                &checked, block_index
            )->mir;
        }
        MMAY_EXPECT(mmay_mir_lower_bytecode_program(
            functions, 3, entry->mir.symbol, mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(), &program
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        program_ready = program.initialized;
    } else {
        MMAY_EXPECT(false);
    }
    reached_success = false;
    for (failure_index = 0; failure_index < 128
        && program_ready && !reached_success; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeValue result;
        MarimayBytecodeRunStatus status;
        memset(&result, 0, sizeof(result));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_bytecode_run_program(
            &program, NULL, 0, mmay_fail_allocator(&state),
            mmay_bytecode_run_default_options(), &result
        );
        MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED
            || status == MMAY_BYTECODE_RUN_OK);
        reached_success = status == MMAY_BYTECODE_RUN_OK;
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);

cleanup:
    if (program_ready) {
        mmay_bytecode_program_dispose(&program);
    }
    if (lowered_ready) {
        mmay_bytecode_function_dispose(&lowered);
    }
    if (checked.initialized) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

static bool mmay_test_defer_append_hex(
    char *output,
    size_t capacity,
    size_t *length,
    const unsigned char *bytes,
    size_t count
) {
    static const char digits[] = "0123456789abcdef";
    size_t index = 0;
    if (*length > capacity || count > (capacity - *length) / 2) {
        return false;
    }
    for (index = 0; index < count; index += 1) {
        output[*length] = digits[bytes[index] >> 4];
        output[*length + 1] = digits[bytes[index] & 0x0fU];
        *length += 2;
    }
    return true;
}

static bool mmay_test_defer_panic_output(
    char *output,
    size_t capacity,
    MarimaySymbolId primary,
    MarimayMirSpan primary_span,
    const char *message,
    const MarimaySymbolId *parent,
    MarimayMirSpan parent_span,
    const MarimaySymbolId *suppressed,
    MarimayMirSpan suppressed_span,
    const char *suppressed_message
) {
    size_t length = 0;
    size_t message_length = strlen(message);
    int written = snprintf(output, capacity, "panic:G4000:");
    if (written < 0 || (size_t)written >= capacity) {
        return false;
    }
    length = (size_t)written;
    if (!mmay_test_defer_append_hex(
            output, capacity, &length, primary.bytes, sizeof(primary.bytes)
        )) {
        return false;
    }
    written = snprintf(
        output + length, capacity - length, ":%zu:%zu:%zu:",
        primary_span.start, primary_span.end, message_length
    );
    if (written < 0 || (size_t)written >= capacity - length) {
        return false;
    }
    length += (size_t)written;
    if (!mmay_test_defer_append_hex(
            output, capacity, &length,
            (const unsigned char *)message, message_length
        )) {
        return false;
    }
    written = snprintf(
        output + length, capacity - length, ":%d:", parent == NULL ? 1 : 2
    );
    if (written < 0 || (size_t)written >= capacity - length) {
        return false;
    }
    length += (size_t)written;
    if (!mmay_test_defer_append_hex(
            output, capacity, &length, primary.bytes, sizeof(primary.bytes)
        )) {
        return false;
    }
    written = snprintf(
        output + length, capacity - length, ":%zu:%zu",
        primary_span.start, primary_span.end
    );
    if (written < 0 || (size_t)written >= capacity - length) {
        return false;
    }
    length += (size_t)written;
    if (parent != NULL) {
        if (length + 1 >= capacity) {
            return false;
        }
        output[length] = ':';
        length += 1;
        if (!mmay_test_defer_append_hex(
                output, capacity, &length,
                parent->bytes, sizeof(parent->bytes)
            )) {
            return false;
        }
        written = snprintf(
            output + length, capacity - length, ":%zu:%zu",
            parent_span.start, parent_span.end
        );
        if (written < 0 || (size_t)written >= capacity - length) {
            return false;
        }
        length += (size_t)written;
    }
    if (suppressed != NULL && suppressed_message != NULL) {
        size_t suppressed_length = strlen(suppressed_message);
        written = snprintf(
            output + length, capacity - length,
            ":suppressed:1:G4000:"
        );
        if (written < 0 || (size_t)written >= capacity - length) {
            return false;
        }
        length += (size_t)written;
        if (!mmay_test_defer_append_hex(
                output, capacity, &length,
                suppressed->bytes, sizeof(suppressed->bytes)
            )) {
            return false;
        }
        written = snprintf(
            output + length, capacity - length, ":%zu:%zu:%zu:",
            suppressed_span.start, suppressed_span.end, suppressed_length
        );
        if (written < 0 || (size_t)written >= capacity - length) {
            return false;
        }
        length += (size_t)written;
        if (!mmay_test_defer_append_hex(
                output, capacity, &length,
                (const unsigned char *)suppressed_message,
                suppressed_length
            )) {
            return false;
        }
        written = snprintf(output + length, capacity - length, ":1:");
        if (written < 0 || (size_t)written >= capacity - length) {
            return false;
        }
        length += (size_t)written;
        if (!mmay_test_defer_append_hex(
                output, capacity, &length,
                suppressed->bytes, sizeof(suppressed->bytes)
            )) {
            return false;
        }
        written = snprintf(
            output + length, capacity - length, ":%zu:%zu",
            suppressed_span.start, suppressed_span.end
        );
        if (written < 0 || (size_t)written >= capacity - length) {
            return false;
        }
        length += (size_t)written;
    }
    if (length + 2 > capacity) {
        return false;
    }
    output[length] = '\n';
    output[length + 1] = '\0';
    return true;
}

static bool mmay_test_defer_find_panic_span(
    const MarimayMirFunction *function,
    MarimayMirSpan *span
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        if (block != NULL && block->terminator.kind == MMAY_MIR_PANIC) {
            *span = block->terminator.span;
            return true;
        }
    }
    return false;
}

static bool mmay_test_defer_find_call_span(
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
                && instruction->kind == MMAY_MIR_DEFER_CALL) {
                *span = instruction->span;
                return true;
            }
        }
    }
    return false;
}

static void mmay_test_defer_panic_case(
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayMirFunction *entry,
    const MarimayMirFunction *primary,
    MarimayMirSpan primary_span,
    const char *message,
    const MarimayMirSpan *parent_span,
    const MarimayMirFunction *suppressed,
    MarimayMirSpan suppressed_span,
    const char *suppressed_message
) {
    MarimayBytecodeProgram bytecode;
    MarimayBytecodeExecution execution;
    MarimayBytecodePanic panic;
    MarimayBytecodeRunOptions options;
    MarimayBytecodeValue result;
    MarimayNativeCProgram native_program;
    MarimayOwnedBytes generated;
    char expected[1024];
    size_t failure_index = 0;
    bool reached_failure_success = false;
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&result, 0, sizeof(result));
    memset(&native_program, 0, sizeof(native_program));
    memset(&generated, 0, sizeof(generated));
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(
        functions, function_count, entry->symbol, mmay_system_allocator(),
        mmay_mir_bytecode_program_lower_default_options(), &bytecode
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_panic_init(&panic, mmay_system_allocator()));
    options = mmay_bytecode_run_default_options();
    options.execution = &execution;
    options.panic = &panic;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &bytecode, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_PANIC);
    MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
    MMAY_EXPECT(panic.message.length == strlen(message));
    MMAY_EXPECT(panic.message.data != NULL && memcmp(
        panic.message.data, message, strlen(message)
    ) == 0);
    MMAY_EXPECT(mmay_symbol_id_equal(panic.symbol, primary->symbol));
    MMAY_EXPECT(panic.span.start == primary_span.start
        && panic.span.end == primary_span.end);
    MMAY_EXPECT(panic.frames.length == (parent_span == NULL ? 1 : 2));
    MMAY_EXPECT(panic.suppressed.length == (suppressed == NULL ? 0 : 1));
    if (suppressed != NULL) {
        const MarimayBytecodeSuppressedPanic *observed =
            mmay_bytecode_panic_suppressed(&panic, 0);
        MMAY_EXPECT(observed != NULL);
        MMAY_EXPECT(observed != NULL
            && mmay_symbol_id_equal(observed->symbol, suppressed->symbol));
        MMAY_EXPECT(observed != NULL && observed->span.start
            == suppressed_span.start && observed->span.end
            == suppressed_span.end);
        MMAY_EXPECT(observed != NULL && observed->message.length
            == strlen(suppressed_message));
        MMAY_EXPECT(observed != NULL && observed->message.data != NULL
            && memcmp(
                observed->message.data, suppressed_message,
                strlen(suppressed_message)
            ) == 0);
    }
    for (failure_index = 0; failure_index < 256
        && !reached_failure_success; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayBytecodeExecution attempted_execution;
        MarimayBytecodePanic attempted_panic;
        MarimayBytecodeRunOptions attempted_options;
        MarimayBytecodeValue attempted_result;
        MarimayBytecodeRunStatus attempted_status =
            MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        memset(&attempted_execution, 0, sizeof(attempted_execution));
        memset(&attempted_panic, 0, sizeof(attempted_panic));
        memset(&attempted_result, 0, sizeof(attempted_result));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        allocator = mmay_fail_allocator(&state);
        if (mmay_bytecode_execution_init(
                &attempted_execution, allocator,
                mmay_bytecode_execution_default_options()
            ) && mmay_bytecode_panic_init(&attempted_panic, allocator)) {
            attempted_options = mmay_bytecode_run_default_options();
            attempted_options.execution = &attempted_execution;
            attempted_options.panic = &attempted_panic;
            attempted_status = mmay_bytecode_run_program(
                &bytecode, NULL, 0, allocator, attempted_options,
                &attempted_result
            );
        }
        MMAY_EXPECT(attempted_status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED
            || attempted_status == MMAY_BYTECODE_RUN_PANIC);
        reached_failure_success = attempted_status == MMAY_BYTECODE_RUN_PANIC
            && attempted_panic.suppressed.length
                == (suppressed == NULL ? 0 : 1);
        mmay_bytecode_panic_dispose(&attempted_panic);
        mmay_bytecode_execution_dispose(&attempted_execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_failure_success);

    native_program.functions = functions;
    native_program.function_count = function_count;
    native_program.entry_symbol = entry->symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &native_program, mmay_system_allocator(),
        mmay_native_c_emit_default_options(), &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(mmay_test_defer_panic_output(
        expected, sizeof(expected), primary->symbol, primary_span, message,
        parent_span != NULL ? &entry->symbol : NULL,
        parent_span != NULL ? *parent_span : (MarimayMirSpan){0},
        suppressed != NULL ? &suppressed->symbol : NULL,
        suppressed_span, suppressed_message
    ));
#if !defined(_WIN32)
    if (generated.data != NULL) {
        mmay_test_native_compile_and_run(&generated, expected, 1);
    }
#endif
    mmay_owned_bytes_dispose(&generated);
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_program_dispose(&bytecode);
}

static void mmay_test_defer_panic_policy(void) {
    char source[16384];
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayMirFunction *functions[32] = {0};
    const MarimayCheckedFunction *cleanup = NULL;
    const MarimayCheckedFunction *earlier = NULL;
    const MarimayCheckedFunction *body = NULL;
    const MarimayCheckedFunction *replacement = NULL;
    const MarimayCheckedFunction *two = NULL;
    MarimayMirSpan cleanup_span = {0};
    MarimayMirSpan earlier_span = {0};
    MarimayMirSpan body_span = {0};
    MarimayMirSpan replacement_call = {0};
    MarimayMirSpan two_call = {0};
    size_t index = 0;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    if (!mmay_test_defer_source(source, sizeof(source))
        || !mmay_checker_fixture_init(&fixture, source)
        || !fixture.ast_initialized
        || mmay_check_ast(
            &fixture.ast, "test/defer-panic", mmay_system_allocator(),
            mmay_check_default_options(), &checked
        ) != MMAY_CHECK_OK) {
        MMAY_EXPECT(false);
        goto cleanup_result;
    }
    for (index = 0; index < checked.functions.length
        && index < sizeof(functions) / sizeof(functions[0]); index += 1) {
        functions[index] = &mmay_checked_function(&checked, index)->mir;
    }
    cleanup = mmay_test_defer_find(&checked, "cleanupPanics", NULL);
    earlier = mmay_test_defer_find(&checked, "earlierCleanupPanics", NULL);
    body = mmay_test_defer_find(&checked, "bodyPanic", NULL);
    replacement = mmay_test_defer_find(
        &checked, "cleanupPanicReplacesReturn", NULL
    );
    two = mmay_test_defer_find(&checked, "twoCleanupPanics", NULL);
    MMAY_EXPECT(cleanup != NULL && earlier != NULL && body != NULL
        && replacement != NULL && two != NULL);
    MMAY_EXPECT(cleanup != NULL
        && mmay_test_defer_find_panic_span(&cleanup->mir, &cleanup_span));
    MMAY_EXPECT(earlier != NULL
        && mmay_test_defer_find_panic_span(&earlier->mir, &earlier_span));
    MMAY_EXPECT(body != NULL
        && mmay_test_defer_find_panic_span(&body->mir, &body_span));
    MMAY_EXPECT(replacement != NULL
        && mmay_test_defer_find_call_span(
            &replacement->mir, &replacement_call
        ));
    MMAY_EXPECT(two != NULL
        && mmay_test_defer_find_call_span(&two->mir, &two_call));
    if (cleanup != NULL && body != NULL && replacement != NULL) {
        mmay_test_defer_panic_case(
            functions, checked.functions.length, &body->mir, &body->mir,
            body_span, "body", NULL, &cleanup->mir, cleanup_span, "cleanup"
        );
        mmay_test_defer_panic_case(
            functions, checked.functions.length, &replacement->mir,
            &cleanup->mir, cleanup_span, "cleanup", &replacement_call,
            NULL, (MarimayMirSpan){0}, NULL
        );
    }
    if (cleanup != NULL && earlier != NULL && two != NULL) {
        mmay_test_defer_panic_case(
            functions, checked.functions.length, &two->mir, &cleanup->mir,
            cleanup_span, "cleanup", &two_call, &earlier->mir, earlier_span,
            "earlier"
        );
    }

cleanup_result:
    if (checked.initialized) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

typedef struct MarimayTestDeferCallLog {
    MarimaySymbolId first;
    MarimaySymbolId second;
    MarimaySymbolId observed[4];
    size_t count;
} MarimayTestDeferCallLog;

static MarimayMirInterpretStatus mmay_test_defer_call_handler(
    void *opaque,
    MarimaySymbolId callee,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayMirValue *result
) {
    MarimayTestDeferCallLog *log = opaque;
    (void)arguments;
    if (log == NULL || result == NULL || argument_count != 0
        || log->count >= sizeof(log->observed) / sizeof(log->observed[0])) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    log->observed[log->count] = callee;
    log->count += 1;
    memset(result, 0, sizeof(*result));
    result->type = MMAY_TYPE_VOID;
    return mmay_symbol_id_equal(callee, log->second)
        ? MMAY_MIR_INTERPRET_PANIC
        : MMAY_MIR_INTERPRET_OK;
}

static bool mmay_test_defer_append_mir(
    MarimayMirFunction *function,
    MarimayMirBlockId block,
    MarimayMirInstructionKind kind,
    MarimayMirRegister target,
    MarimayMirCallId call
) {
    MarimayMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.call = call;
    return mmay_mir_append_instruction(function, block, &instruction);
}

static void mmay_test_defer_cleanup_panic_continues(void) {
    MarimayMirFunction function;
    MarimayMirExecution execution;
    MarimayMirInterpretOptions options;
    MarimayMirPanic panic;
    MarimayMirValue result;
    MarimaySymbolId function_symbol;
    MarimayTestDeferCallLog log;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirRegister message = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister first_marker = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister second_marker = MMAY_MIR_NO_REGISTER;
    MarimayMirStringId text = MMAY_MIR_NO_STRING;
    MarimayMirCallId first_call = MMAY_MIR_NO_CALL;
    MarimayMirCallId second_call = MMAY_MIR_NO_CALL;
    MarimayMirInstruction constant;
    size_t failure_index = 0;
    bool reached_failure_success = false;
    memset(&function, 0, sizeof(function));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&result, 0, sizeof(result));
    memset(&log, 0, sizeof(log));
    MMAY_EXPECT(mmay_symbol_id_from_parts(
        "test/defer", "panic.my", "entry", 0, &function_symbol
    ));
    MMAY_EXPECT(mmay_symbol_id_from_parts(
        "test/defer", "panic.my", "first", 0, &log.first
    ));
    MMAY_EXPECT(mmay_symbol_id_from_parts(
        "test/defer", "panic.my", "second", 0, &log.second
    ));
    MMAY_EXPECT(mmay_mir_function_init(
        &function, mmay_system_allocator(), function_symbol, NULL, 0,
        MMAY_TYPE_VOID, mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&function, MMAY_TYPE_STRING, &message));
    MMAY_EXPECT(mmay_mir_add_register(
        &function, MMAY_TYPE_BOOL, &first_marker
    ));
    MMAY_EXPECT(mmay_mir_add_register(
        &function, MMAY_TYPE_BOOL, &second_marker
    ));
    MMAY_EXPECT(mmay_mir_add_string_constant(
        &function, (const unsigned char *)"body", 4, &text
    ));
    MMAY_EXPECT(mmay_mir_add_call(
        &function, log.first, MMAY_TYPE_VOID, NULL, 0, &first_call
    ));
    MMAY_EXPECT(mmay_mir_add_call(
        &function, log.second, MMAY_TYPE_VOID, NULL, 0, &second_call
    ));
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    memset(&constant, 0, sizeof(constant));
    constant.kind = MMAY_MIR_CONSTANT_STRING;
    constant.target = message;
    constant.first = MMAY_MIR_NO_REGISTER;
    constant.second = MMAY_MIR_NO_REGISTER;
    constant.string_constant = text;
    MMAY_EXPECT(mmay_mir_append_instruction(&function, block, &constant));
    MMAY_EXPECT(mmay_test_defer_append_mir(
        &function, block, MMAY_MIR_DEFER_REGISTER,
        first_marker, first_call
    ));
    MMAY_EXPECT(mmay_test_defer_append_mir(
        &function, block, MMAY_MIR_DEFER_REGISTER,
        second_marker, second_call
    ));
    MMAY_EXPECT(mmay_mir_set_terminator(
        &function, block, mmay_test_mir_panic(message, 10, 14)
    ));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution, mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    options = mmay_mir_interpret_default_options();
    options.execution = &execution;
    options.panic = &panic;
    options.call = mmay_test_defer_call_handler;
    options.call_context = &log;
    MMAY_EXPECT(mmay_mir_interpret(
        &function, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_MIR_INTERPRET_PANIC);
    MMAY_EXPECT(log.count == 2);
    MMAY_EXPECT(mmay_symbol_id_equal(log.observed[0], log.second));
    MMAY_EXPECT(mmay_symbol_id_equal(log.observed[1], log.first));
    MMAY_EXPECT(panic.message.length == 4
        && memcmp(panic.message.data, "body", 4) == 0);
    MMAY_EXPECT(panic.suppressed.length == 1);
    mmay_mir_panic_dispose(&panic);
    mmay_mir_execution_dispose(&execution);
    for (failure_index = 0; failure_index < 128
        && !reached_failure_success; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMirExecution attempted_execution;
        MarimayMirPanic attempted_panic;
        MarimayMirValue attempted_result;
        MarimayMirInterpretStatus attempted_status =
            MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
        memset(&attempted_execution, 0, sizeof(attempted_execution));
        memset(&attempted_panic, 0, sizeof(attempted_panic));
        memset(&attempted_result, 0, sizeof(attempted_result));
        log.count = 0;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        allocator = mmay_fail_allocator(&state);
        if (mmay_mir_execution_init(
                &attempted_execution, allocator,
                mmay_mir_execution_default_options()
            )) {
            options = mmay_mir_interpret_default_options();
            options.execution = &attempted_execution;
            options.panic = &attempted_panic;
            options.call = mmay_test_defer_call_handler;
            options.call_context = &log;
            attempted_status = mmay_mir_interpret(
                &function, NULL, 0, allocator, options, &attempted_result
            );
        }
        MMAY_EXPECT(attempted_status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED
            || attempted_status == MMAY_MIR_INTERPRET_PANIC);
        reached_failure_success = attempted_status == MMAY_MIR_INTERPRET_PANIC
            && attempted_panic.suppressed.length == 1;
        if (reached_failure_success) {
            MMAY_EXPECT(log.count == 2);
        }
        mmay_mir_panic_dispose(&attempted_panic);
        mmay_mir_execution_dispose(&attempted_execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_failure_success);
    mmay_mir_function_dispose(&function);
}

void mmay_test_defer_cp7(void) {
    mmay_test_defer_source_semantics();
    mmay_test_defer_engine_differential();
    mmay_test_defer_checker_negatives();
    mmay_test_defer_native_cycle();
    mmay_test_defer_mir_invariants();
    mmay_test_defer_bytecode_invariants();
    mmay_test_defer_panic_policy();
    mmay_test_defer_cleanup_panic_continues();
}
