#include "test_checker_imports_support.h"
#include "test_native_c_emitter_support.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/bytecode/codec.h"

typedef struct MarimayCp35PipelineFixture {
    MarimayCheckerImportFixture source;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *entry;
    const MarimayCheckedFunction *probe;
    size_t probe_index;
} MarimayCp35PipelineFixture;

static const char mmay_cp35_env_source[] =
    "public enum Error {\n"
    "  InvalidName, CapabilityDenied { name: string, },\n"
    "  InvalidUtf8 { offset: int, }, LimitExceeded { limit: int, },\n"
    "  ReadFailed,\n"
    "}\n"
    "extern \"c\" fn mmayEnvGet(name: string): Result<Option<string>, Error>;\n"
    "public fn state(name: string): int {\n"
    "  return match mmayEnvGet(name) {\n"
    "    Ok(Some(value)) => 1, Ok(None) => 2, Err(_) => 3,\n"
    "  };\n"
    "}\n";

static const char mmay_cp35_process_source[] =
    "public enum ContextError {\n"
    "  CurrentDirectoryUnavailable, ExecutablePathUnavailable,\n"
    "  InvalidUtf8 { offset: int, }, InvalidPath,\n"
    "}\n"
    "extern \"c\" fn mmayProcessCurrentDirectory(): Result<string, ContextError>;\n"
    "extern \"c\" fn mmayProcessExecutablePath(): Result<string, ContextError>;\n"
    "public fn state(): int {\n"
    "  const current = match mmayProcessCurrentDirectory() {\n"
    "    Ok(value) => 4, Err(_) => 16,\n"
    "  };\n"
    "  const executable = match mmayProcessExecutablePath() {\n"
    "    Ok(value) => 8, Err(_) => 32,\n"
    "  };\n"
    "  return current + executable;\n"
    "}\n";

static const char mmay_cp35_application_source[] =
    "import { state as envState } from \"std/env\";\n"
    "import { state as processState } from \"std/process\";\n"
    "public fn probe(): int {\n"
    "  return envState(\"MMAY_ALPHA_TEST\") + processState();\n"
    "}\n"
    "fn main(): void { const _ = probe(); }\n";

static bool mmay_cp35_pipeline_init(MarimayCp35PipelineFixture *fixture) {
    const MarimayTestImportSource sources[] = {
        {"test/cp35", "app/main.my", mmay_cp35_application_source},
        {"std/env", "std/env/env.my", mmay_cp35_env_source},
        {"std/process", "std/process/process.my", mmay_cp35_process_source}
    };
    memset(fixture, 0, sizeof(*fixture));
    if (!mmay_checker_import_fixture_init(
            &fixture->source, sources,
            sizeof(sources) / sizeof(sources[0])
        ) || mmay_checker_import_fixture_check(
            &fixture->source, "test/cp35", mmay_system_allocator(),
            mmay_check_graph_default_options(), &fixture->checked
        ) != MMAY_CHECK_OK
        || fixture->checked.diagnostics.length != 0) return false;
    fixture->entry = mmay_checker_import_function(
        &fixture->checked, "main", NULL
    );
    fixture->probe = mmay_checker_import_function(
        &fixture->checked, "probe", &fixture->probe_index
    );
    return fixture->entry != NULL && fixture->probe != NULL;
}

static void mmay_cp35_pipeline_dispose(
    MarimayCp35PipelineFixture *fixture
) {
    mmay_check_result_dispose(&fixture->checked);
    mmay_checker_import_fixture_dispose(&fixture->source);
    memset(fixture, 0, sizeof(*fixture));
}

static bool mmay_cp35_is_context_mir(
    const MarimayMirInstruction *instruction
) {
    return instruction != NULL && (instruction->kind == MMAY_MIR_ENV_READ
        || instruction->kind == MMAY_MIR_PROCESS_CONTEXT);
}

static size_t mmay_cp35_hostile_mir(MarimayMirFunction *function) {
    size_t block_index = 0;
    size_t count = 0;
    MMAY_EXPECT(mmay_mir_verify(function) == MMAY_MIR_VERIFY_OK);
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1U) {
        MarimayMirBlock *block = mmay_vector_at(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1U) {
            MarimayMirInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            MarimayMirContextOperation saved;
            if (!mmay_cp35_is_context_mir(instruction)) continue;
            saved = instruction->context_operation;
            instruction->context_operation = (MarimayMirContextOperation)99;
            MMAY_EXPECT(mmay_mir_verify(function)
                == (instruction->kind == MMAY_MIR_ENV_READ
                    ? MMAY_MIR_VERIFY_TYPE_MISMATCH
                    : MMAY_MIR_VERIFY_INVALID_ARGUMENT));
            instruction->context_operation = saved;
            if (instruction->kind == MMAY_MIR_ENV_READ) {
                MarimayMirRegister source = instruction->first;
                instruction->first = MMAY_MIR_NO_REGISTER;
                MMAY_EXPECT(mmay_mir_verify(function)
                    == MMAY_MIR_VERIFY_INVALID_REGISTER);
                instruction->first = source;
            }
            count += 1U;
        }
    }
    MMAY_EXPECT(mmay_mir_verify(function) == MMAY_MIR_VERIFY_OK);
    return count;
}

static bool mmay_cp35_is_context_opcode(MarimayOpcode opcode) {
    return opcode >= MMAY_OPCODE_ENV_GET
        && opcode <= MMAY_OPCODE_PROCESS_EXECUTABLE_PATH;
}

static size_t mmay_cp35_hostile_bytecode(
    MarimayBytecodeFunction *function
) {
    MarimayOwnedBytes encoded = {0};
    MarimayBytecodeFunction decoded;
    size_t block_index = 0;
    size_t count = 0;
    memset(&decoded, 0, sizeof(decoded));
    MMAY_EXPECT(mmay_bytecode_verify(function) == MMAY_BYTECODE_VERIFY_OK);
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1U) {
        MarimayBytecodeBlock *block = mmay_vector_at(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1U) {
            MarimayBytecodeInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            uint64_t saved_target;
            if (instruction == NULL
                || !mmay_cp35_is_context_opcode(instruction->opcode)) continue;
            saved_target = instruction->operands[0].unsigned_value;
            instruction->operands[0].unsigned_value =
                function->register_types.length;
            MMAY_EXPECT(mmay_bytecode_verify(function)
                == MMAY_BYTECODE_VERIFY_INVALID_REGISTER);
            instruction->operands[0].unsigned_value = saved_target;
            if (instruction->opcode == MMAY_OPCODE_ENV_GET) {
                uint64_t saved_source = instruction->operands[1].unsigned_value;
                instruction->operands[1].unsigned_value =
                    function->register_types.length;
                MMAY_EXPECT(mmay_bytecode_verify(function)
                    == MMAY_BYTECODE_VERIFY_INVALID_REGISTER);
                instruction->operands[1].unsigned_value = saved_source;
            }
            count += 1U;
        }
    }
    MMAY_EXPECT(mmay_bytecode_verify(function) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        function, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_verify(&decoded) == MMAY_BYTECODE_VERIFY_OK);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    return count;
}

static size_t mmay_cp35_collect_functions(
    MarimayCp35PipelineFixture *fixture,
    const MarimayMirFunction **functions,
    size_t capacity
) {
    size_t index = 0;
    size_t count = 0;
    for (index = 0; index < fixture->checked.functions.length;
        index += 1U) {
        MarimayCheckedFunction *checked = mmay_vector_at(
            &fixture->checked.functions, index
        );
        if (checked != NULL && !checked->is_external && count < capacity) {
            functions[count++] = &checked->mir;
        }
    }
    return count;
}

typedef struct MarimayCp35InjectedContext {
    const unsigned char *environment_value;
    size_t environment_length;
    const unsigned char *path;
    size_t path_length;
} MarimayCp35InjectedContext;

static MarimayHostEnvironmentStatus mmay_cp35_injected_environment(
    void *context,
    MarimayAllocator allocator,
    const char *name,
    size_t name_length,
    size_t maximum,
    MarimayOwnedBytes *value
) {
    MarimayCp35InjectedContext *injected = context;
    if (name_length != 15
        || memcmp(name, "MMAY_ALPHA_TEST", name_length) != 0) {
        return MMAY_HOST_ENVIRONMENT_MISSING;
    }
    if (injected->environment_length > maximum) {
        return MMAY_HOST_ENVIRONMENT_OUTPUT_LIMIT;
    }
    return mmay_owned_bytes_copy(
            value, allocator, injected->environment_value,
            injected->environment_length
        ) ? MMAY_HOST_ENVIRONMENT_OK
        : MMAY_HOST_ENVIRONMENT_ALLOCATION_FAILED;
}

static MarimayProcessContextSourceStatus mmay_cp35_injected_path(
    void *context,
    MarimayAllocator allocator,
    size_t maximum,
    MarimayOwnedBytes *path
) {
    MarimayCp35InjectedContext *injected = context;
    if (injected->path_length > maximum) {
        return MMAY_PROCESS_CONTEXT_SOURCE_LIMIT_EXCEEDED;
    }
    return mmay_owned_bytes_copy(
            path, allocator, injected->path, injected->path_length
        ) ? MMAY_PROCESS_CONTEXT_SOURCE_OK
        : MMAY_PROCESS_CONTEXT_SOURCE_ALLOCATION_FAILED;
}

static void mmay_cp35_injected_options(
    MarimayCp35InjectedContext *injected,
    MarimayEnvOptions *environment,
    MarimayProcessContextOptions *process
) {
    static const MarimayEnvNameView grant = {"MMAY_ALPHA_TEST", 15};
    memset(environment, 0, sizeof(*environment));
    environment->grants = &grant;
    environment->grant_count = 1;
    environment->host.context = injected;
    environment->host.lookup = mmay_cp35_injected_environment;
    memset(process, 0, sizeof(*process));
    process->flavor = MMAY_PATH_POSIX;
    process->current_directory_context = injected;
    process->current_directory = mmay_cp35_injected_path;
    process->executable_path_context = injected;
    process->executable_path = mmay_cp35_injected_path;
}

static void mmay_cp35_injected_oracles(
    MarimayCp35PipelineFixture *fixture,
    const MarimayMirFunction *const *functions,
    size_t function_count
) {
    static const unsigned char environment_value[] = "injected value \xC3\xBC";
    static const unsigned char path[] = "/tmp/cp35 context \xC3\xBC/tool";
    MarimayCp35InjectedContext injected = {
        environment_value, sizeof(environment_value) - 1U,
        path, sizeof(path) - 1U
    };
    MarimayEnvOptions environment;
    MarimayProcessContextOptions process;
    MarimayMirExecutionOptions mir_options =
        mmay_mir_execution_default_options();
    MarimayMirExecution mir_execution;
    MarimayCheckInterpretOptions interpret =
        mmay_check_interpret_default_options();
    MarimayMirValue mir_result = {0};
    MarimayBytecodeProgram program;
    MarimayBytecodeExecutionOptions vm_options =
        mmay_bytecode_execution_default_options();
    MarimayBytecodeExecution vm_execution;
    MarimayBytecodeRunOptions run = mmay_bytecode_run_default_options();
    MarimayBytecodeValue vm_result = {0};
    mmay_cp35_injected_options(&injected, &environment, &process);
    mir_options.env = &environment;
    mir_options.process_context = &process;
    memset(&mir_execution, 0, sizeof(mir_execution));
    MMAY_EXPECT(mmay_mir_execution_init(
        &mir_execution, mmay_system_allocator(), mir_options
    ));
    interpret.execution = &mir_execution;
    MMAY_EXPECT(mmay_check_interpret_function(
        &fixture->checked, fixture->probe_index, NULL, 0,
        mmay_system_allocator(), interpret, &mir_result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mir_result.type == MMAY_TYPE_I64);
    MMAY_EXPECT(mir_result.as.i64 == 13);
    mmay_mir_execution_dispose(&mir_execution);

    memset(&program, 0, sizeof(program));
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(
        functions, function_count, fixture->probe->mir.symbol,
        mmay_system_allocator(),
        mmay_mir_bytecode_program_lower_default_options(), &program
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    vm_options.env = &environment;
    vm_options.process_context = &process;
    memset(&vm_execution, 0, sizeof(vm_execution));
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &vm_execution, mmay_system_allocator(), vm_options
    ));
    run.execution = &vm_execution;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &program, NULL, 0, mmay_system_allocator(), run, &vm_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_I64);
    MMAY_EXPECT(vm_result.as.i64 == 13);
    mmay_bytecode_execution_dispose(&vm_execution);
    mmay_bytecode_program_dispose(&program);
}

static void mmay_cp35_oracle_allocation_failures(
    MarimayCp35PipelineFixture *fixture,
    const MarimayMirFunction *const *functions,
    size_t function_count
) {
    static const unsigned char environment_value[] = "injected";
    static const unsigned char path[] = "/tmp/cp35/tool";
    MarimayCp35InjectedContext injected = {
        environment_value, sizeof(environment_value) - 1U,
        path, sizeof(path) - 1U
    };
    MarimayEnvOptions environment;
    MarimayProcessContextOptions process;
    MarimayBytecodeProgram program;
    unsigned int engine = 0;
    mmay_cp35_injected_options(&injected, &environment, &process);
    memset(&program, 0, sizeof(program));
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(
        functions, function_count, fixture->probe->mir.symbol,
        mmay_system_allocator(),
        mmay_mir_bytecode_program_lower_default_options(), &program
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    for (engine = 0; engine < 2; engine += 1U) {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 512;
            failure_index += 1U) {
            MarimayFailAllocator state;
            MarimayAllocator allocator;
            mmay_fail_allocator_init(
                &state, mmay_system_allocator(), failure_index
            );
            allocator = mmay_fail_allocator(&state);
            if (engine == 0) {
                MarimayMirExecutionOptions execution_options =
                    mmay_mir_execution_default_options();
                MarimayMirExecution execution;
                MarimayCheckInterpretOptions interpret =
                    mmay_check_interpret_default_options();
                MarimayMirValue result = {0};
                bool initialized;
                execution_options.env = &environment;
                execution_options.process_context = &process;
                memset(&execution, 0, sizeof(execution));
                initialized = mmay_mir_execution_init(
                    &execution, allocator, execution_options
                );
                if (initialized) {
                    MarimayMirInterpretStatus status;
                    interpret.execution = &execution;
                    status = mmay_check_interpret_function(
                        &fixture->checked, fixture->probe_index, NULL, 0,
                        allocator, interpret, &result
                    );
                    reached_success = status == MMAY_MIR_INTERPRET_OK;
                    MMAY_EXPECT(reached_success
                        || status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
                    mmay_mir_execution_dispose(&execution);
                }
            } else {
                MarimayBytecodeExecutionOptions execution_options =
                    mmay_bytecode_execution_default_options();
                MarimayBytecodeExecution execution;
                MarimayBytecodeRunOptions run =
                    mmay_bytecode_run_default_options();
                MarimayBytecodeValue result = {0};
                bool initialized;
                execution_options.env = &environment;
                execution_options.process_context = &process;
                memset(&execution, 0, sizeof(execution));
                initialized = mmay_bytecode_execution_init(
                    &execution, allocator, execution_options
                );
                if (initialized) {
                    MarimayBytecodeRunStatus status;
                    run.execution = &execution;
                    status = mmay_bytecode_run_program(
                        &program, NULL, 0, allocator, run, &result
                    );
                    reached_success = status == MMAY_BYTECODE_RUN_OK;
                    MMAY_EXPECT(reached_success
                        || status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
                    mmay_bytecode_execution_dispose(&execution);
                }
            }
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            if (reached_success) break;
        }
        MMAY_EXPECT(reached_success);
    }
    mmay_bytecode_program_dispose(&program);
}

static void mmay_cp35_native_contract(
    MarimayCp35PipelineFixture *fixture,
    const MarimayMirFunction *const *functions,
    size_t function_count
) {
    static const MarimayEnvNameView grant = {"MMAY_ALPHA_TEST", 15};
    static const MarimayEnvNameView duplicate[] = {
        {"MMAY_ALPHA_TEST", 15}, {"MMAY_ALPHA_TEST", 15}
    };
    MarimayNativeCProgram program;
    MarimayNativeCEmitOptions options = mmay_native_c_emit_default_options();
    MarimayOwnedBytes first = {0};
    MarimayOwnedBytes second = {0};
    memset(&program, 0, sizeof(program));
    program.functions = functions;
    program.function_count = function_count;
    program.entry_symbol = fixture->entry->mir.symbol;
    options.env_grants = &grant;
    options.env_grant_count = 1;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program, mmay_system_allocator(), options, &first
    ) == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program, mmay_system_allocator(), options, &second
    ) == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(first.length != 0 && first.length == second.length);
    MMAY_EXPECT(first.length == second.length
        && memcmp(first.data, second.data, first.length) == 0);
    MMAY_EXPECT(mmay_test_native_contains(&first, "mmay_g_env_name_0"));
    MMAY_EXPECT(mmay_test_native_contains(&first, "mmay_g_env_get"));
    MMAY_EXPECT(mmay_test_native_contains(&first, "mmay_g_process_context"));
    MMAY_EXPECT(!mmay_test_native_contains(
        &first, "CP35_SECRET_MUST_NOT_BE_EMBEDDED"
    ));
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(&first, "ok:void\n", 0);
#endif
    mmay_owned_bytes_dispose(&second);
    mmay_owned_bytes_dispose(&first);

    options.env_grants = duplicate;
    options.env_grant_count = 2;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program, mmay_system_allocator(), options, &first
    ) == MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT);
    MMAY_EXPECT(first.data == NULL && first.length == 0);
}

static void mmay_cp35_native_allocation_failures(
    MarimayCp35PipelineFixture *fixture,
    const MarimayMirFunction *const *functions,
    size_t function_count
) {
    static const MarimayEnvNameView grant = {"MMAY_ALPHA_TEST", 15};
    MarimayNativeCProgram program;
    MarimayNativeCEmitOptions options = mmay_native_c_emit_default_options();
    size_t failure_index = 0;
    bool reached_success = false;
    memset(&program, 0, sizeof(program));
    program.functions = functions;
    program.function_count = function_count;
    program.entry_symbol = fixture->entry->mir.symbol;
    options.env_grants = &grant;
    options.env_grant_count = 1;
    for (failure_index = 0; failure_index < 512; failure_index += 1U) {
        MarimayFailAllocator state;
        MarimayOwnedBytes output = {0};
        MarimayNativeCEmitStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_native_c_emit_program(
            &program, mmay_fail_allocator(&state), options, &output
        );
        reached_success = status == MMAY_NATIVE_C_EMIT_OK;
        MMAY_EXPECT(reached_success
            || status == MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED);
        mmay_owned_bytes_dispose(&output);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_cp35_context_pipeline(void) {
    MarimayCp35PipelineFixture fixture;
    const MarimayMirFunction *functions[16];
    size_t function_count;
    size_t function_index = 0;
    size_t mir_count = 0;
    size_t bytecode_count = 0;
    memset(functions, 0, sizeof(functions));
    MMAY_EXPECT(mmay_cp35_pipeline_init(&fixture));
    function_count = mmay_cp35_collect_functions(
        &fixture, functions, sizeof(functions) / sizeof(functions[0])
    );
    MMAY_EXPECT(function_count == 4);
    for (function_index = 0; function_index < function_count;
        function_index += 1U) {
        MarimayBytecodeFunction lowered;
        size_t current = mmay_cp35_hostile_mir(
            (MarimayMirFunction *)functions[function_index]
        );
        mir_count += current;
        if (current == 0) continue;
        memset(&lowered, 0, sizeof(lowered));
        MMAY_EXPECT(mmay_mir_lower_bytecode_function(
            functions[function_index], mmay_system_allocator(),
            mmay_mir_bytecode_lower_default_options(), &lowered
        ) == MMAY_MIR_BYTECODE_LOWER_OK);
        bytecode_count += mmay_cp35_hostile_bytecode(&lowered);
        mmay_bytecode_function_dispose(&lowered);
    }
    MMAY_EXPECT(mir_count == 3);
    MMAY_EXPECT(bytecode_count == 3);
    mmay_cp35_injected_oracles(&fixture, functions, function_count);
    mmay_cp35_oracle_allocation_failures(
        &fixture, functions, function_count
    );
    mmay_cp35_native_contract(&fixture, functions, function_count);
    mmay_cp35_native_allocation_failures(
        &fixture, functions, function_count
    );
    mmay_cp35_pipeline_dispose(&fixture);
}
