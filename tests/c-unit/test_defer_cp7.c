#include "test_native_c_emitter_support.h"

#include "test_checker_support.h"
#include "test_mir_support.h"

#include <stdio.h>
#include <string.h>

#ifndef RUDA_TEST_SOURCE_DIR
#define RUDA_TEST_SOURCE_DIR "."
#endif

static bool ruda_test_defer_source(char *source, size_t capacity) {
    char path[1024];
    FILE *file = NULL;
    size_t count = 0;
    int written = snprintf(
        path, sizeof(path), "%s/examples/defer_cp7.ruda", RUDA_TEST_SOURCE_DIR
    );
    if (source == NULL || capacity < 2 || written < 0
        || (size_t)written >= sizeof(path)) {
        return false;
    }
    file = fopen(path, "rb");
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

static const RudaCheckedFunction *ruda_test_defer_find(
    const RudaCheckResult *checked,
    const char *name,
    size_t *result_index
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; index < checked->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
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

static void ruda_test_defer_source_semantics(void) {
    char source[16384];
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue result;
    size_t main_index = SIZE_MAX;
    size_t index = 0;
    bool reached_success = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_test_defer_source(source, sizeof(source)));
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    if (!fixture.ast_initialized) {
        goto cleanup;
    }
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/defer-cp7", ruda_system_allocator(),
        ruda_check_default_options(), &checked
    ) == RUDA_CHECK_OK);
    if (!checked.initialized) {
        goto cleanup;
    }
    for (index = 0; index < checked.diagnostics.length; index += 1) {
        const RudaCheckDiagnostic *diagnostic = ruda_check_diagnostic(
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
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(ruda_test_defer_find(&checked, "main", &main_index) != NULL);
    if (main_index != SIZE_MAX) {
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, main_index, NULL, 0, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64);
        RUDA_EXPECT(result.as.i64 == INT64_C(2136541923789));
    }
    for (index = 0; index < 128 && main_index != SIZE_MAX
        && !reached_success; index += 1) {
        RudaFailAllocator state;
        RudaMirInterpretStatus status;
        memset(&result, 0, sizeof(result));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), index);
        status = ruda_check_interpret_function(
            &checked, main_index, NULL, 0, ruda_fail_allocator(&state),
            ruda_check_interpret_default_options(), &result
        );
        RUDA_EXPECT(status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED
            || status == RUDA_MIR_INTERPRET_OK);
        if (status == RUDA_MIR_INTERPRET_OK) {
            reached_success = true;
            RUDA_EXPECT(result.as.i64 == INT64_C(2136541923789));
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

cleanup:
    if (checked.initialized) {
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_defer_engine_differential(void) {
    char source[16384];
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *entry = NULL;
    const RudaMirFunction *functions[32];
    RudaBytecodeProgram bytecode;
    RudaBytecodeProgram decoded;
    RudaOwnedBytes encoded;
    RudaBytecodeExecution execution;
    RudaBytecodeRunOptions run_options;
    RudaBytecodeValue vm_result;
    RudaNativeCProgram native_program;
    RudaNativeCEmitStatus native_status;
    RudaOwnedBytes generated;
    size_t entry_index = SIZE_MAX;
    size_t index = 0;
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
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&native_program, 0, sizeof(native_program));
    memset(&generated, 0, sizeof(generated));
    if (!ruda_test_defer_source(source, sizeof(source))
        || !ruda_checker_fixture_init(&fixture, source)
        || !fixture.ast_initialized
        || ruda_check_ast(
            &fixture.ast, "test/defer-cp7-differential",
            ruda_system_allocator(), ruda_check_default_options(), &checked
        ) != RUDA_CHECK_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(checked.functions.length
        <= sizeof(functions) / sizeof(functions[0]));
    for (index = 0; index < checked.functions.length
        && index < sizeof(functions) / sizeof(functions[0]); index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked, index
        );
        if (function == NULL) {
            RUDA_EXPECT(false);
            goto cleanup;
        }
        RUDA_EXPECT(ruda_mir_verify(&function->mir) == RUDA_MIR_VERIFY_OK);
        functions[index] = &function->mir;
    }
    entry = ruda_test_defer_find(&checked, "main", &entry_index);
    RUDA_EXPECT(entry != NULL);
    if (entry == NULL || ruda_mir_lower_bytecode_program(
            functions, checked.functions.length, entry->mir.symbol,
            ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(), &bytecode
        ) != RUDA_MIR_BYTECODE_LOWER_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    bytecode_ready = true;
    RUDA_EXPECT(ruda_bytecode_program_verify(&bytecode)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    if (ruda_bytecode_encode_program(
            &bytecode, ruda_system_allocator(), &encoded
        ) != RUDA_BYTECODE_CODEC_OK
        || ruda_bytecode_decode_program(
            encoded.data, encoded.length, ruda_system_allocator(),
            ruda_bytecode_program_default_options(), &decoded
        ) != RUDA_BYTECODE_CODEC_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    decoded_ready = true;
    RUDA_EXPECT(ruda_bytecode_program_verify(&decoded)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()
    ));
    if (!execution.initialized) {
        goto cleanup;
    }
    execution_ready = true;
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run_program(
        &decoded, NULL, 0, ruda_system_allocator(), run_options, &vm_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_I64);
    RUDA_EXPECT(vm_result.as.i64 == INT64_C(2136541923789));

    native_program.functions = functions;
    native_program.function_count = checked.functions.length;
    native_program.entry_symbol = entry->mir.symbol;
    native_status = ruda_native_c_emit_program(
        &native_program, ruda_system_allocator(),
        ruda_native_c_emit_default_options(), &generated
    );
    if (native_status != RUDA_NATIVE_C_EMIT_OK) {
        fprintf(stderr, "defer native emit status: %d\n", (int)native_status);
    }
    RUDA_EXPECT(native_status == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (generated.data != NULL) {
        ruda_test_native_compile_and_run(
            &generated, "ok:i64:2136541923789\n", 0
        );
    }
#endif

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
    if (checked.initialized) {
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_defer_checker_negatives(void) {
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
        RudaCheckerFixture fixture;
        RudaCheckResult checked;
        const RudaCheckDiagnostic *diagnostic = NULL;
        memset(&fixture, 0, sizeof(fixture));
        memset(&checked, 0, sizeof(checked));
        RUDA_EXPECT(ruda_checker_fixture_init(&fixture, sources[source_index]));
        if (fixture.ast_initialized) {
            RUDA_EXPECT(ruda_check_ast(
                &fixture.ast, "test/defer-negative", ruda_system_allocator(),
                ruda_check_default_options(), &checked
            ) == RUDA_CHECK_OK);
        }
        RUDA_EXPECT(checked.initialized);
        RUDA_EXPECT(checked.diagnostics.length == 1);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        RUDA_EXPECT(diagnostic != NULL
            && strcmp(diagnostic->code, RUDA_DIAGNOSTIC_TYPE_MISMATCH) == 0);
        RUDA_EXPECT(diagnostic != NULL
            && strstr(diagnostic->message, "must return void") != NULL);
        if (checked.initialized) {
            ruda_check_result_dispose(&checked);
        }
        ruda_checker_fixture_dispose(&fixture);
    }
}

static void ruda_test_defer_native_cycle(void) {
    static const char source[] =
        "fn recursive(): void { defer recursive(); }\n"
        "fn main(): void { recursive(); }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaMirFunction *functions[2];
    const RudaCheckedFunction *entry = NULL;
    RudaNativeCProgram program;
    RudaOwnedBytes generated;
    size_t index = 0;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(functions, 0, sizeof(functions));
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    if (!fixture.ast_initialized || ruda_check_ast(
            &fixture.ast, "test/defer-cycle", ruda_system_allocator(),
            ruda_check_default_options(), &checked
        ) != RUDA_CHECK_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(checked.functions.length == 2);
    for (index = 0; index < checked.functions.length && index < 2;
        index += 1) {
        functions[index] = &ruda_checked_function(&checked, index)->mir;
    }
    entry = ruda_test_defer_find(&checked, "main", NULL);
    RUDA_EXPECT(entry != NULL);
    if (entry != NULL && checked.functions.length == 2) {
        program.functions = functions;
        program.function_count = 2;
        program.entry_symbol = entry->mir.symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(
            &program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &generated
        ) == RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL);
    }

cleanup:
    ruda_owned_bytes_dispose(&generated);
    if (checked.initialized) {
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_defer_mir_invariants(void) {
    static const char source[] =
        "fn cleanup(): void {}\n"
        "fn branch(flag: bool): void { if flag { defer cleanup(); } }\n"
        "fn main(): void { defer cleanup(); defer cleanup(); }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckedFunction *entry = NULL;
    RudaMirInstruction *first_cleanup = NULL;
    RudaMirCallId other_call = RUDA_MIR_NO_CALL;
    RudaMirCallId original_call = RUDA_MIR_NO_CALL;
    RudaMirBlock *cleanup_block = NULL;
    size_t cleanup_index = SIZE_MAX;
    size_t entry_index = SIZE_MAX;
    size_t block_index = 0;
    size_t failure_index = 0;
    bool reached_success = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    if (!fixture.ast_initialized || ruda_check_ast(
            &fixture.ast, "test/defer-mir", ruda_system_allocator(),
            ruda_check_default_options(), &checked
        ) != RUDA_CHECK_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    (void)ruda_test_defer_find(&checked, "main", &entry_index);
    entry = entry_index < checked.functions.length
        ? ruda_vector_at(&checked.functions, entry_index)
        : NULL;
    RUDA_EXPECT(entry != NULL);
    if (entry == NULL) {
        goto cleanup;
    }
    for (block_index = 0; block_index < entry->mir.blocks.length;
        block_index += 1) {
        RudaMirBlock *block = ruda_vector_at(&entry->mir.blocks, block_index);
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            RudaMirInstruction *instruction = ruda_vector_at(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && instruction->kind == RUDA_MIR_DEFER_REGISTER
                && other_call == RUDA_MIR_NO_CALL) {
                other_call = instruction->call;
            }
            if (instruction != NULL
                && instruction->kind == RUDA_MIR_DEFER_CALL
                && first_cleanup == NULL) {
                first_cleanup = instruction;
                cleanup_block = block;
                cleanup_index = instruction_index;
            }
        }
    }
    RUDA_EXPECT(first_cleanup != NULL && cleanup_block != NULL);
    RUDA_EXPECT(other_call != RUDA_MIR_NO_CALL);
    if (first_cleanup != NULL && other_call == first_cleanup->call) {
        other_call = (RudaMirCallId)(other_call == 0 ? 1 : 0);
    }
    if (first_cleanup != NULL && other_call != RUDA_MIR_NO_CALL) {
        original_call = first_cleanup->call;
        first_cleanup->call = other_call;
        RUDA_EXPECT(ruda_mir_verify(&entry->mir)
            == RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW);
        first_cleanup->call = original_call;
    }
    if (cleanup_block != NULL && cleanup_index != SIZE_MAX) {
        size_t old_length = cleanup_block->instructions.length;
        size_t removed = old_length - cleanup_index;
        cleanup_block->instructions.length = cleanup_index;
        entry->mir.instruction_count -= removed;
        RUDA_EXPECT(ruda_mir_verify(&entry->mir)
            == RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW);
        cleanup_block->instructions.length = old_length;
        entry->mir.instruction_count += removed;
    }
    RUDA_EXPECT(ruda_mir_verify(&entry->mir) == RUDA_MIR_VERIFY_OK);
    {
        RudaCheckedFunction *branch = NULL;
        RudaMirInstruction *merge_cleanup = NULL;
        size_t branch_index = SIZE_MAX;
        branch = (RudaCheckedFunction *)ruda_test_defer_find(
            &checked, "branch", &branch_index);
        for (block_index = 0; branch != NULL
            && block_index < branch->mir.blocks.length; block_index += 1) {
            RudaMirBlock *block = ruda_vector_at(
                &branch->mir.blocks, block_index);
            size_t instruction_index = 0;
            for (instruction_index = 0; block != NULL
                && instruction_index < block->instructions.length;
                instruction_index += 1) {
                RudaMirInstruction *instruction = ruda_vector_at(
                    &block->instructions, instruction_index
                );
                if (instruction != NULL
                    && instruction->kind == RUDA_MIR_DEFER_CALL) {
                    merge_cleanup = instruction;
                }
            }
        }
        RUDA_EXPECT(merge_cleanup != NULL);
        if (merge_cleanup != NULL) {
            merge_cleanup->kind = RUDA_MIR_DEFER_REGISTER;
            RUDA_EXPECT(ruda_mir_verify(&branch->mir)
                == RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW);
            merge_cleanup->kind = RUDA_MIR_DEFER_CALL;
            RUDA_EXPECT(ruda_mir_verify(&branch->mir) == RUDA_MIR_VERIFY_OK);
        }
    }
    for (failure_index = 0; failure_index < 64 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaMirFunction shallow = entry->mir;
        RudaMirVerifyStatus status;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        shallow.allocator = ruda_fail_allocator(&state);
        status = ruda_mir_verify(&shallow);
        RUDA_EXPECT(status == RUDA_MIR_VERIFY_ALLOCATION_FAILED
            || status == RUDA_MIR_VERIFY_OK);
        reached_success = status == RUDA_MIR_VERIFY_OK;
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

cleanup:
    if (checked.initialized) {
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_defer_bytecode_invariants(void) {
    static const char source[] =
        "fn cleanup(): void {}\n"
        "fn branch(flag: bool): void { if flag { defer cleanup(); } }\n"
        "fn main(): void { defer cleanup(); defer cleanup(); }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *entry = NULL;
    RudaBytecodeFunction lowered;
    RudaBytecodeProgram program;
    const RudaMirFunction *functions[3];
    RudaBytecodeInstruction *cleanup = NULL;
    RudaBytecodeInstruction *return_instruction = NULL;
    RudaBytecodeBlock *cleanup_block = NULL;
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
    memset(functions, 0, sizeof(functions));
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    if (!fixture.ast_initialized || ruda_check_ast(
            &fixture.ast, "test/defer-bytecode", ruda_system_allocator(),
            ruda_check_default_options(), &checked
        ) != RUDA_CHECK_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    entry = ruda_test_defer_find(&checked, "main", &entry_index);
    RUDA_EXPECT(entry != NULL);
    if (entry == NULL || ruda_mir_lower_bytecode_function(
            &entry->mir, ruda_system_allocator(),
            ruda_mir_bytecode_lower_default_options(), &lowered
        ) != RUDA_MIR_BYTECODE_LOWER_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    lowered_ready = true;
    RUDA_EXPECT(ruda_bytecode_verify(&lowered) == RUDA_BYTECODE_VERIFY_OK);
    {
        const RudaCheckedFunction *branch = ruda_test_defer_find(
            &checked, "branch", NULL);
        RudaBytecodeFunction branch_lowered;
        RudaBytecodeInstruction *merge_cleanup = NULL;
        memset(&branch_lowered, 0, sizeof(branch_lowered));
        RUDA_EXPECT(branch != NULL && ruda_mir_lower_bytecode_function(
            &branch->mir, ruda_system_allocator(),
            ruda_mir_bytecode_lower_default_options(), &branch_lowered
        ) == RUDA_MIR_BYTECODE_LOWER_OK);
        for (block_index = 0; block_index < branch_lowered.blocks.length;
            block_index += 1) {
            RudaBytecodeBlock *block = ruda_vector_at(
                &branch_lowered.blocks, block_index
            );
            size_t instruction_index = 0;
            for (instruction_index = 0; block != NULL
                && instruction_index < block->instructions.length;
                instruction_index += 1) {
                RudaBytecodeInstruction *instruction = ruda_vector_at(
                    &block->instructions, instruction_index
                );
                if (instruction != NULL
                    && instruction->opcode == RUDA_OPCODE_DEFER_CALL) {
                    merge_cleanup = instruction;
                }
            }
        }
        RUDA_EXPECT(merge_cleanup != NULL);
        if (merge_cleanup != NULL) {
            merge_cleanup->opcode = RUDA_OPCODE_DEFER_REGISTER;
            RUDA_EXPECT(ruda_bytecode_verify(&branch_lowered)
                == RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
            merge_cleanup->opcode = RUDA_OPCODE_DEFER_CALL;
            RUDA_EXPECT(ruda_bytecode_verify(&branch_lowered)
                == RUDA_BYTECODE_VERIFY_OK);
        }
        ruda_bytecode_function_dispose(&branch_lowered);
    }
    for (block_index = 0; block_index < lowered.blocks.length;
        block_index += 1) {
        RudaBytecodeBlock *block = ruda_vector_at(
            &lowered.blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            RudaBytecodeInstruction *instruction = ruda_vector_at(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && instruction->opcode == RUDA_OPCODE_DEFER_CALL) {
                cleanup = instruction;
                cleanup_block = block;
                cleanup_index = instruction_index;
            }
        }
    }
    RUDA_EXPECT(cleanup != NULL && cleanup_block != NULL);
    if (cleanup != NULL && lowered.calls.length > 1) {
        original_call = cleanup->operands[1].unsigned_value;
        cleanup->operands[1].unsigned_value = original_call == 0 ? 1 : 0;
        RUDA_EXPECT(ruda_bytecode_verify(&lowered)
            == RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
        cleanup->operands[1].unsigned_value = original_call;
    }
    if (cleanup_block != NULL
        && cleanup_index + 2 == cleanup_block->instructions.length) {
        return_instruction = ruda_vector_at(
            &cleanup_block->instructions, cleanup_index + 1
        );
        if (return_instruction != NULL) {
            RudaBytecodeInstruction saved_cleanup = *cleanup;
            *cleanup = *return_instruction;
            cleanup_block->instructions.length -= 1;
            lowered.instruction_count -= 1;
            RUDA_EXPECT(ruda_bytecode_verify(&lowered)
                == RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
            cleanup_block->instructions.length += 1;
            lowered.instruction_count += 1;
            *cleanup = saved_cleanup;
        }
    } else {
        RUDA_EXPECT(false);
    }
    RUDA_EXPECT(ruda_bytecode_verify(&lowered) == RUDA_BYTECODE_VERIFY_OK);
    for (failure_index = 0; failure_index < 64 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeFunction shallow = lowered;
        RudaBytecodeVerifyStatus status;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        shallow.allocator = ruda_fail_allocator(&state);
        status = ruda_bytecode_verify(&shallow);
        RUDA_EXPECT(status == RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED
            || status == RUDA_BYTECODE_VERIFY_OK);
        reached_success = status == RUDA_BYTECODE_VERIFY_OK;
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    if (checked.functions.length == 3) {
        for (block_index = 0; block_index < 3; block_index += 1) {
            functions[block_index] = &ruda_checked_function(
                &checked, block_index
            )->mir;
        }
        RUDA_EXPECT(ruda_mir_lower_bytecode_program(
            functions, 3, entry->mir.symbol, ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(), &program
        ) == RUDA_MIR_BYTECODE_LOWER_OK);
        program_ready = program.initialized;
    } else {
        RUDA_EXPECT(false);
    }
    reached_success = false;
    for (failure_index = 0; failure_index < 128
        && program_ready && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeValue result;
        RudaBytecodeRunStatus status;
        memset(&result, 0, sizeof(result));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_bytecode_run_program(
            &program, NULL, 0, ruda_fail_allocator(&state),
            ruda_bytecode_run_default_options(), &result
        );
        RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED
            || status == RUDA_BYTECODE_RUN_OK);
        reached_success = status == RUDA_BYTECODE_RUN_OK;
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

cleanup:
    if (program_ready) {
        ruda_bytecode_program_dispose(&program);
    }
    if (lowered_ready) {
        ruda_bytecode_function_dispose(&lowered);
    }
    if (checked.initialized) {
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static bool ruda_test_defer_append_hex(
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

static bool ruda_test_defer_panic_output(
    char *output,
    size_t capacity,
    RudaSymbolId primary,
    RudaMirSpan primary_span,
    const char *message,
    const RudaSymbolId *parent,
    RudaMirSpan parent_span,
    const RudaSymbolId *suppressed,
    RudaMirSpan suppressed_span,
    const char *suppressed_message
) {
    size_t length = 0;
    size_t message_length = strlen(message);
    int written = snprintf(output, capacity, "panic:G4000:");
    if (written < 0 || (size_t)written >= capacity) {
        return false;
    }
    length = (size_t)written;
    if (!ruda_test_defer_append_hex(
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
    if (!ruda_test_defer_append_hex(
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
    if (!ruda_test_defer_append_hex(
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
        if (!ruda_test_defer_append_hex(
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
        if (!ruda_test_defer_append_hex(
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
        if (!ruda_test_defer_append_hex(
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
        if (!ruda_test_defer_append_hex(
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

static bool ruda_test_defer_find_panic_span(
    const RudaMirFunction *function,
    RudaMirSpan *span
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks, block_index
        );
        if (block != NULL && block->terminator.kind == RUDA_MIR_PANIC) {
            *span = block->terminator.span;
            return true;
        }
    }
    return false;
}

static bool ruda_test_defer_find_call_span(
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
                && instruction->kind == RUDA_MIR_DEFER_CALL) {
                *span = instruction->span;
                return true;
            }
        }
    }
    return false;
}

static void ruda_test_defer_panic_case(
    const RudaMirFunction *const *functions,
    size_t function_count,
    const RudaMirFunction *entry,
    const RudaMirFunction *primary,
    RudaMirSpan primary_span,
    const char *message,
    const RudaMirSpan *parent_span,
    const RudaMirFunction *suppressed,
    RudaMirSpan suppressed_span,
    const char *suppressed_message
) {
    RudaBytecodeProgram bytecode;
    RudaBytecodeExecution execution;
    RudaBytecodePanic panic;
    RudaBytecodeRunOptions options;
    RudaBytecodeValue result;
    RudaNativeCProgram native_program;
    RudaOwnedBytes generated;
    char expected[1024];
    size_t failure_index = 0;
    bool reached_failure_success = false;
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&result, 0, sizeof(result));
    memset(&native_program, 0, sizeof(native_program));
    memset(&generated, 0, sizeof(generated));
    RUDA_EXPECT(ruda_mir_lower_bytecode_program(
        functions, function_count, entry->symbol, ruda_system_allocator(),
        ruda_mir_bytecode_program_lower_default_options(), &bytecode
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
    options = ruda_bytecode_run_default_options();
    options.execution = &execution;
    options.panic = &panic;
    RUDA_EXPECT(ruda_bytecode_run_program(
        &bytecode, NULL, 0, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_PANIC);
    RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
    RUDA_EXPECT(panic.message.length == strlen(message));
    RUDA_EXPECT(panic.message.data != NULL && memcmp(
        panic.message.data, message, strlen(message)
    ) == 0);
    RUDA_EXPECT(ruda_symbol_id_equal(panic.symbol, primary->symbol));
    RUDA_EXPECT(panic.span.start == primary_span.start
        && panic.span.end == primary_span.end);
    RUDA_EXPECT(panic.frames.length == (parent_span == NULL ? 1 : 2));
    RUDA_EXPECT(panic.suppressed.length == (suppressed == NULL ? 0 : 1));
    if (suppressed != NULL) {
        const RudaBytecodeSuppressedPanic *observed =
            ruda_bytecode_panic_suppressed(&panic, 0);
        RUDA_EXPECT(observed != NULL);
        RUDA_EXPECT(observed != NULL
            && ruda_symbol_id_equal(observed->symbol, suppressed->symbol));
        RUDA_EXPECT(observed != NULL && observed->span.start
            == suppressed_span.start && observed->span.end
            == suppressed_span.end);
        RUDA_EXPECT(observed != NULL && observed->message.length
            == strlen(suppressed_message));
        RUDA_EXPECT(observed != NULL && observed->message.data != NULL
            && memcmp(
                observed->message.data, suppressed_message,
                strlen(suppressed_message)
            ) == 0);
    }
    for (failure_index = 0; failure_index < 256
        && !reached_failure_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaBytecodeExecution attempted_execution;
        RudaBytecodePanic attempted_panic;
        RudaBytecodeRunOptions attempted_options;
        RudaBytecodeValue attempted_result;
        RudaBytecodeRunStatus attempted_status =
            RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
        memset(&attempted_execution, 0, sizeof(attempted_execution));
        memset(&attempted_panic, 0, sizeof(attempted_panic));
        memset(&attempted_result, 0, sizeof(attempted_result));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        allocator = ruda_fail_allocator(&state);
        if (ruda_bytecode_execution_init(
                &attempted_execution, allocator,
                ruda_bytecode_execution_default_options()
            ) && ruda_bytecode_panic_init(&attempted_panic, allocator)) {
            attempted_options = ruda_bytecode_run_default_options();
            attempted_options.execution = &attempted_execution;
            attempted_options.panic = &attempted_panic;
            attempted_status = ruda_bytecode_run_program(
                &bytecode, NULL, 0, allocator, attempted_options,
                &attempted_result
            );
        }
        RUDA_EXPECT(attempted_status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED
            || attempted_status == RUDA_BYTECODE_RUN_PANIC);
        reached_failure_success = attempted_status == RUDA_BYTECODE_RUN_PANIC
            && attempted_panic.suppressed.length
                == (suppressed == NULL ? 0 : 1);
        ruda_bytecode_panic_dispose(&attempted_panic);
        ruda_bytecode_execution_dispose(&attempted_execution);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_failure_success);

    native_program.functions = functions;
    native_program.function_count = function_count;
    native_program.entry_symbol = entry->symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &native_program, ruda_system_allocator(),
        ruda_native_c_emit_default_options(), &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(ruda_test_defer_panic_output(
        expected, sizeof(expected), primary->symbol, primary_span, message,
        parent_span != NULL ? &entry->symbol : NULL,
        parent_span != NULL ? *parent_span : (RudaMirSpan){0},
        suppressed != NULL ? &suppressed->symbol : NULL,
        suppressed_span, suppressed_message
    ));
#if !defined(_WIN32)
    if (generated.data != NULL) {
        ruda_test_native_compile_and_run(&generated, expected, 1);
    }
#endif
    ruda_owned_bytes_dispose(&generated);
    ruda_bytecode_panic_dispose(&panic);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_program_dispose(&bytecode);
}

static void ruda_test_defer_panic_policy(void) {
    char source[16384];
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaMirFunction *functions[32];
    const RudaCheckedFunction *cleanup = NULL;
    const RudaCheckedFunction *earlier = NULL;
    const RudaCheckedFunction *body = NULL;
    const RudaCheckedFunction *replacement = NULL;
    const RudaCheckedFunction *two = NULL;
    RudaMirSpan cleanup_span = {0};
    RudaMirSpan earlier_span = {0};
    RudaMirSpan body_span = {0};
    RudaMirSpan replacement_call = {0};
    RudaMirSpan two_call = {0};
    size_t index = 0;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(functions, 0, sizeof(functions));
    if (!ruda_test_defer_source(source, sizeof(source))
        || !ruda_checker_fixture_init(&fixture, source)
        || !fixture.ast_initialized
        || ruda_check_ast(
            &fixture.ast, "test/defer-panic", ruda_system_allocator(),
            ruda_check_default_options(), &checked
        ) != RUDA_CHECK_OK) {
        RUDA_EXPECT(false);
        goto cleanup_result;
    }
    for (index = 0; index < checked.functions.length
        && index < sizeof(functions) / sizeof(functions[0]); index += 1) {
        functions[index] = &ruda_checked_function(&checked, index)->mir;
    }
    cleanup = ruda_test_defer_find(&checked, "cleanupPanics", NULL);
    earlier = ruda_test_defer_find(&checked, "earlierCleanupPanics", NULL);
    body = ruda_test_defer_find(&checked, "bodyPanic", NULL);
    replacement = ruda_test_defer_find(
        &checked, "cleanupPanicReplacesReturn", NULL
    );
    two = ruda_test_defer_find(&checked, "twoCleanupPanics", NULL);
    RUDA_EXPECT(cleanup != NULL && earlier != NULL && body != NULL
        && replacement != NULL && two != NULL);
    RUDA_EXPECT(cleanup != NULL
        && ruda_test_defer_find_panic_span(&cleanup->mir, &cleanup_span));
    RUDA_EXPECT(earlier != NULL
        && ruda_test_defer_find_panic_span(&earlier->mir, &earlier_span));
    RUDA_EXPECT(body != NULL
        && ruda_test_defer_find_panic_span(&body->mir, &body_span));
    RUDA_EXPECT(replacement != NULL
        && ruda_test_defer_find_call_span(
            &replacement->mir, &replacement_call
        ));
    RUDA_EXPECT(two != NULL
        && ruda_test_defer_find_call_span(&two->mir, &two_call));
    if (cleanup != NULL && body != NULL && replacement != NULL) {
        ruda_test_defer_panic_case(
            functions, checked.functions.length, &body->mir, &body->mir,
            body_span, "body", NULL, &cleanup->mir, cleanup_span, "cleanup"
        );
        ruda_test_defer_panic_case(
            functions, checked.functions.length, &replacement->mir,
            &cleanup->mir, cleanup_span, "cleanup", &replacement_call,
            NULL, (RudaMirSpan){0}, NULL
        );
    }
    if (cleanup != NULL && earlier != NULL && two != NULL) {
        ruda_test_defer_panic_case(
            functions, checked.functions.length, &two->mir, &cleanup->mir,
            cleanup_span, "cleanup", &two_call, &earlier->mir, earlier_span,
            "earlier"
        );
    }

cleanup_result:
    if (checked.initialized) {
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

typedef struct RudaTestDeferCallLog {
    RudaSymbolId first;
    RudaSymbolId second;
    RudaSymbolId observed[4];
    size_t count;
} RudaTestDeferCallLog;

static RudaMirInterpretStatus ruda_test_defer_call_handler(
    void *opaque,
    RudaSymbolId callee,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
) {
    RudaTestDeferCallLog *log = opaque;
    (void)arguments;
    if (log == NULL || result == NULL || argument_count != 0
        || log->count >= sizeof(log->observed) / sizeof(log->observed[0])) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    log->observed[log->count] = callee;
    log->count += 1;
    memset(result, 0, sizeof(*result));
    result->type = RUDA_TYPE_VOID;
    return ruda_symbol_id_equal(callee, log->second)
        ? RUDA_MIR_INTERPRET_PANIC
        : RUDA_MIR_INTERPRET_OK;
}

static bool ruda_test_defer_append_mir(
    RudaMirFunction *function,
    RudaMirBlockId block,
    RudaMirInstructionKind kind,
    RudaMirRegister target,
    RudaMirCallId call
) {
    RudaMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.third = RUDA_MIR_NO_REGISTER;
    instruction.call = call;
    return ruda_mir_append_instruction(function, block, &instruction);
}

static void ruda_test_defer_cleanup_panic_continues(void) {
    RudaMirFunction function;
    RudaMirExecution execution;
    RudaMirInterpretOptions options;
    RudaMirPanic panic;
    RudaMirValue result;
    RudaSymbolId function_symbol;
    RudaTestDeferCallLog log;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirRegister message = RUDA_MIR_NO_REGISTER;
    RudaMirRegister first_marker = RUDA_MIR_NO_REGISTER;
    RudaMirRegister second_marker = RUDA_MIR_NO_REGISTER;
    RudaMirStringId text = RUDA_MIR_NO_STRING;
    RudaMirCallId first_call = RUDA_MIR_NO_CALL;
    RudaMirCallId second_call = RUDA_MIR_NO_CALL;
    RudaMirInstruction constant;
    size_t failure_index = 0;
    bool reached_failure_success = false;
    memset(&function, 0, sizeof(function));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&result, 0, sizeof(result));
    memset(&log, 0, sizeof(log));
    RUDA_EXPECT(ruda_symbol_id_from_parts(
        "test/defer", "panic.ruda", "entry", 0, &function_symbol
    ));
    RUDA_EXPECT(ruda_symbol_id_from_parts(
        "test/defer", "panic.ruda", "first", 0, &log.first
    ));
    RUDA_EXPECT(ruda_symbol_id_from_parts(
        "test/defer", "panic.ruda", "second", 0, &log.second
    ));
    RUDA_EXPECT(ruda_mir_function_init(
        &function, ruda_system_allocator(), function_symbol, NULL, 0,
        RUDA_TYPE_VOID, ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&function, RUDA_TYPE_STRING, &message));
    RUDA_EXPECT(ruda_mir_add_register(
        &function, RUDA_TYPE_BOOL, &first_marker
    ));
    RUDA_EXPECT(ruda_mir_add_register(
        &function, RUDA_TYPE_BOOL, &second_marker
    ));
    RUDA_EXPECT(ruda_mir_add_string_constant(
        &function, (const unsigned char *)"body", 4, &text
    ));
    RUDA_EXPECT(ruda_mir_add_call(
        &function, log.first, RUDA_TYPE_VOID, NULL, 0, &first_call
    ));
    RUDA_EXPECT(ruda_mir_add_call(
        &function, log.second, RUDA_TYPE_VOID, NULL, 0, &second_call
    ));
    RUDA_EXPECT(ruda_mir_add_block(&function, &block));
    memset(&constant, 0, sizeof(constant));
    constant.kind = RUDA_MIR_CONSTANT_STRING;
    constant.target = message;
    constant.first = RUDA_MIR_NO_REGISTER;
    constant.second = RUDA_MIR_NO_REGISTER;
    constant.string_constant = text;
    RUDA_EXPECT(ruda_mir_append_instruction(&function, block, &constant));
    RUDA_EXPECT(ruda_test_defer_append_mir(
        &function, block, RUDA_MIR_DEFER_REGISTER,
        first_marker, first_call
    ));
    RUDA_EXPECT(ruda_test_defer_append_mir(
        &function, block, RUDA_MIR_DEFER_REGISTER,
        second_marker, second_call
    ));
    RUDA_EXPECT(ruda_mir_set_terminator(
        &function, block, ruda_test_mir_panic(message, 10, 14)
    ));
    RUDA_EXPECT(ruda_mir_verify(&function) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_execution_init(
        &execution, ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    options = ruda_mir_interpret_default_options();
    options.execution = &execution;
    options.panic = &panic;
    options.call = ruda_test_defer_call_handler;
    options.call_context = &log;
    RUDA_EXPECT(ruda_mir_interpret(
        &function, NULL, 0, ruda_system_allocator(), options, &result
    ) == RUDA_MIR_INTERPRET_PANIC);
    RUDA_EXPECT(log.count == 2);
    RUDA_EXPECT(ruda_symbol_id_equal(log.observed[0], log.second));
    RUDA_EXPECT(ruda_symbol_id_equal(log.observed[1], log.first));
    RUDA_EXPECT(panic.message.length == 4
        && memcmp(panic.message.data, "body", 4) == 0);
    RUDA_EXPECT(panic.suppressed.length == 1);
    ruda_mir_panic_dispose(&panic);
    ruda_mir_execution_dispose(&execution);
    for (failure_index = 0; failure_index < 128
        && !reached_failure_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaMirExecution attempted_execution;
        RudaMirPanic attempted_panic;
        RudaMirValue attempted_result;
        RudaMirInterpretStatus attempted_status =
            RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
        memset(&attempted_execution, 0, sizeof(attempted_execution));
        memset(&attempted_panic, 0, sizeof(attempted_panic));
        memset(&attempted_result, 0, sizeof(attempted_result));
        log.count = 0;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        allocator = ruda_fail_allocator(&state);
        if (ruda_mir_execution_init(
                &attempted_execution, allocator,
                ruda_mir_execution_default_options()
            )) {
            options = ruda_mir_interpret_default_options();
            options.execution = &attempted_execution;
            options.panic = &attempted_panic;
            options.call = ruda_test_defer_call_handler;
            options.call_context = &log;
            attempted_status = ruda_mir_interpret(
                &function, NULL, 0, allocator, options, &attempted_result
            );
        }
        RUDA_EXPECT(attempted_status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED
            || attempted_status == RUDA_MIR_INTERPRET_PANIC);
        reached_failure_success = attempted_status == RUDA_MIR_INTERPRET_PANIC
            && attempted_panic.suppressed.length == 1;
        if (reached_failure_success) {
            RUDA_EXPECT(log.count == 2);
        }
        ruda_mir_panic_dispose(&attempted_panic);
        ruda_mir_execution_dispose(&attempted_execution);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_failure_success);
    ruda_mir_function_dispose(&function);
}

void ruda_test_defer_cp7(void) {
    ruda_test_defer_source_semantics();
    ruda_test_defer_engine_differential();
    ruda_test_defer_checker_negatives();
    ruda_test_defer_native_cycle();
    ruda_test_defer_mir_invariants();
    ruda_test_defer_bytecode_invariants();
    ruda_test_defer_panic_policy();
    ruda_test_defer_cleanup_panic_continues();
}
