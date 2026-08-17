#include "test_native_c_emitter_support.h"

#include "test_checker_imports_support.h"


static const char ruda_native_import_store_types[] =
    "public type User { public value: int; }\n"
    "public enum Level { Basic, Advanced(int), }\n";

static const char ruda_native_import_store_functions[] =
    "public fn makeUser(value: int): User {\n"
    "  return User { value: value };\n"
    "}\n"
    "public fn makeLevel(value: int): Level {\n"
    "  return Level.Advanced(value);\n"
    "}\n"
    "public fn score(user: User): int { return user.value; }\n"
    "public fn score(level: Level): int {\n"
    "  return match level {\n"
    "    case Basic => 0;\n"
    "    case Advanced(value) => value;\n"
    "  };\n"
    "}\n";

static const char ruda_native_import_app[] =
    "import { makeUser, makeLevel } from \"acme/store\";\n"
    "import { evaluateUser, evaluateLevel } from \"acme/eval\";\n"
    "fn main(): int {\n"
    "  return evaluateUser(makeUser(3)) + evaluateLevel(makeLevel(4));\n"
    "}\n";

static const char *const ruda_native_import_evaluators[] = {
    "import { User, Level } from \"acme/store\";\n"
    "interface Scored { score(): int; }\n"
    "fn through(value: Scored): int { return value.score(); }\n"
    "public fn evaluateUser(user: User): int {\n"
    "  return through(user) + user.score();\n"
    "}\n"
    "public fn evaluateLevel(level: Level): int {\n"
    "  return through(level) + level.score();\n"
    "}\n",
    "import { User, Level, score as namedScore } from \"acme/store\";\n"
    "interface Scored { score(): int; }\n"
    "fn through(value: Scored): int { return value.score(); }\n"
    "public fn evaluateUser(user: User): int {\n"
    "  return through(user) + namedScore(user);\n"
    "}\n"
    "public fn evaluateLevel(level: Level): int {\n"
    "  return through(level) + namedScore(level);\n"
    "}\n",
    "import * as store from \"acme/store\";\n"
    "interface Scored { score(): int; }\n"
    "fn through(value: Scored): int { return value.score(); }\n"
    "public fn evaluateUser(user: store.User): int {\n"
    "  return through(user) + store.score(user);\n"
    "}\n"
    "public fn evaluateLevel(level: store.Level): int {\n"
    "  return through(level) + store.score(level);\n"
    "}\n"
};

static void ruda_native_import_normalize_spans(
    RudaBytecodeProgram *program
) {
    size_t function_index = 0;
    for (function_index = 0; function_index < program->functions.length;
        function_index += 1) {
        RudaBytecodeProgramFunction *function = ruda_vector_at(
            &program->functions, function_index
        );
        size_t block_index = 0;
        if (function == NULL) {
            continue;
        }
        for (block_index = 0; block_index < function->body.blocks.length;
            block_index += 1) {
            RudaBytecodeBlock *block = ruda_vector_at(
                &function->body.blocks, block_index
            );
            size_t instruction_index = 0;
            for (instruction_index = 0; block != NULL
                && instruction_index < block->instructions.length;
                instruction_index += 1) {
                RudaBytecodeInstruction *instruction = ruda_vector_at(
                    &block->instructions, instruction_index
                );
                if (instruction != NULL) {
                    memset(&instruction->span, 0, sizeof(instruction->span));
                }
            }
        }
    }
}

static void ruda_test_native_import_form(
    const char *evaluator,
    RudaOwnedBytes *canonical_bytecode
) {
    RudaTestImportSource sources[] = {
        {"acme/store", "store/types.ruda", ruda_native_import_store_types},
        {"acme/app", "app/main.ruda", ruda_native_import_app},
        {"acme/store", "store/functions.ruda",
            ruda_native_import_store_functions},
        {"acme/eval", "eval/evaluate.ruda", evaluator}
    };
    RudaCheckerImportFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *entry = NULL;
    const RudaMirFunction *functions[16];
    RudaBytecodeProgram bytecode;
    RudaBytecodeExecution execution;
    RudaBytecodeExecutionOptions execution_options;
    RudaBytecodeRunOptions run_options;
    RudaBytecodeValue vm_result;
    RudaNativeCProgram native_program;
    RudaOwnedBytes generated;
    RudaOwnedBytes encoded;
    size_t entry_index = SIZE_MAX;
    size_t function_index = 0;
    RudaCheckStatus check_status;
    RudaMirBytecodeLowerStatus lower_status;
    bool bytecode_initialized = false;
    bool execution_initialized = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(functions, 0, sizeof(functions));
    memset(&bytecode, 0, sizeof(bytecode));
    memset(&execution, 0, sizeof(execution));
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&native_program, 0, sizeof(native_program));
    memset(&generated, 0, sizeof(generated));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_checker_import_fixture_init(
        &fixture, sources, sizeof(sources) / sizeof(sources[0])
    ));
    check_status = ruda_checker_import_fixture_check(
            &fixture, "acme/app", ruda_system_allocator(),
            ruda_check_graph_default_options(), &checked
        );
    if (check_status != RUDA_CHECK_OK || !checked.initialized) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(checked.functions.length <= 16);
    entry = ruda_checker_import_function(&checked, "main", &entry_index);
    RUDA_EXPECT(entry != NULL);
    if (entry == NULL || checked.functions.length > 16) {
        goto cleanup;
    }
    for (function_index = 0; function_index < checked.functions.length;
        function_index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked, function_index
        );
        if (function == NULL) {
            RUDA_EXPECT(false);
            goto cleanup;
        }
        RUDA_EXPECT(ruda_mir_verify(&function->mir) == RUDA_MIR_VERIFY_OK);
        functions[function_index] = &function->mir;
    }
    lower_status = ruda_mir_lower_bytecode_program(
        functions,
        checked.functions.length,
        entry->mir.symbol,
        ruda_system_allocator(),
        ruda_mir_bytecode_program_lower_default_options(),
        &bytecode
    );
    RUDA_EXPECT(lower_status == RUDA_MIR_BYTECODE_LOWER_OK);
    if (!bytecode.initialized) {
        goto cleanup;
    }
    bytecode_initialized = true;
    execution_options = ruda_bytecode_execution_default_options();
    execution_options.managed_heap.collect_before_allocation = true;
    if (!ruda_bytecode_execution_init(
            &execution, ruda_system_allocator(), execution_options
        )) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    execution_initialized = true;
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run_program(
        &bytecode, NULL, 0, ruda_system_allocator(), run_options, &vm_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_I64);
    RUDA_EXPECT(vm_result.as.i64 == 14);
    ruda_native_import_normalize_spans(&bytecode);
    RUDA_EXPECT(ruda_bytecode_encode_program(
        &bytecode, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    if (canonical_bytecode->data == NULL) {
        *canonical_bytecode = encoded;
        memset(&encoded, 0, sizeof(encoded));
    } else {
        RUDA_EXPECT(encoded.length == canonical_bytecode->length);
        if (encoded.length == canonical_bytecode->length) {
            RUDA_EXPECT(encoded.length == 0 || memcmp(
                encoded.data, canonical_bytecode->data, encoded.length
            ) == 0);
        }
    }
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
        ruda_test_native_compile_and_run(&generated, "ok:i64:14\n", 0);
    }
#endif
cleanup:
    ruda_owned_bytes_dispose(&encoded);
    ruda_owned_bytes_dispose(&generated);
    if (execution_initialized) {
        ruda_bytecode_execution_dispose(&execution);
    }
    if (bytecode_initialized) {
        ruda_bytecode_program_dispose(&bytecode);
    }
    if (checked.initialized) {
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_import_fixture_dispose(&fixture);
}

void ruda_test_native_import_differential(void) {
    size_t index = 0;
    RudaOwnedBytes canonical_bytecode;
    memset(&canonical_bytecode, 0, sizeof(canonical_bytecode));
    for (index = 0; index < sizeof(ruda_native_import_evaluators)
        / sizeof(ruda_native_import_evaluators[0]); index += 1) {
        ruda_test_native_import_form(
            ruda_native_import_evaluators[index], &canonical_bytecode
        );
    }
    ruda_owned_bytes_dispose(&canonical_bytecode);
}
