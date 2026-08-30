#include "test_checker_imports_support.h"
#include "test_formatter_support.h"
#include "test_native_c_emitter_support.h"

#include <stdio.h>
#include <string.h>

#include "mmay/bytecode/codec.h"
#include "mmay/check/interpret.h"

typedef struct MarimayCp36Fixture {
    MarimayCheckerImportFixture source;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *probe;
    const MarimayCheckedFunction *map_mutation;
    const MarimayCheckedFunction *set_mutation;
    const MarimayCheckedFunction *set_duplicate;
    size_t probe_index;
    size_t map_mutation_index;
    size_t set_mutation_index;
    size_t set_duplicate_index;
} MarimayCp36Fixture;

static const char mmay_cp36_source[] =
    "@derive(Eq, Hash)\n"
    "type Key { id: int, label: string, }\n"
    "@derive(Eq, Hash)\n"
    "enum Token { Index { value: int }, Name { value: string }, }\n"
    "public fn probe(): int {\n"
    "  const values: Map<Key, int> = mapOf<Key, int>();\n"
    "  const first = Key { id: 1, label: \"a\" };\n"
    "  const second = Key { id: 2, label: \"b\" };\n"
    "  const _ = values.put(first, 3);\n"
    "  const _ = values.put(second, 4);\n"
    "  const _ = values.put(Key { id: 1, label: \"a\" }, 5);\n"
    "  const removed = match values.remove(second) {\n"
    "    Some(value) => value, None => 0,\n"
    "  };\n"
    "  const _ = values.put(second, 6);\n"
    "  let mapCount = 0;\n"
    "  for entry in values { const _ = entry; mapCount += 1; }\n"
    "  const selected = match values.get(Key { id: 1, label: \"a\" }) {\n"
    "    Some(value) => value, None => 0,\n"
    "  };\n"
    "  const seen: Set<int> = setOf<int>();\n"
    "  const _ = seen.add(2); const _ = seen.add(2);\n"
    "  const _ = seen.add(3);\n"
    "  if !seen.remove(2) { return -1; }\n"
    "  const _ = seen.add(2);\n"
    "  let setSum = 0;\n"
    "  for item in seen { setSum += item; }\n"
    "  const cleared: Set<int> = setOf<int>();\n"
    "  const _ = cleared.add(99); cleared.clear();\n"
    "  const tokens: Set<Token> = setOf<Token>(); tokens.clear();\n"
    "  if !values.containsKey(first) || !seen.contains(2)\n"
    "      || cleared.length() != 0 { return -2; }\n"
    "  return selected + removed + mapCount + setSum;\n"
    "}\n"
    "public fn mapMutation(): int {\n"
    "  const values: Map<string, int> = mapOf<string, int>();\n"
    "  const _ = values.put(\"a\", 1);\n"
    "  for entry in values {\n"
    "    const _ = entry; const _ = values.put(\"b\", 2);\n"
    "  }\n"
    "  return 0;\n"
    "}\n"
    "public fn setMutation(): int {\n"
    "  const values: Set<int> = setOf<int>(); const _ = values.add(1);\n"
    "  for item in values { const _ = item; const _ = values.add(2); }\n"
    "  return 0;\n"
    "}\n"
    "public fn setDuplicate(): int {\n"
    "  const values: Set<int> = setOf<int>(); const _ = values.add(1);\n"
    "  let count = 0;\n"
    "  for item in values { count += item; const _ = values.add(1); }\n"
    "  return count;\n"
    "}\n"
    "fn main(): void { const _ = probe(); }\n";

static bool mmay_cp36_fixture_init(MarimayCp36Fixture *fixture) {
    const MarimayTestImportSource sources[] = {
        {"test/cp36", "app/main.my", mmay_cp36_source}
    };
    memset(fixture, 0, sizeof(*fixture));
    if (!mmay_checker_import_fixture_init(
            &fixture->source, sources, 1
        ) || mmay_checker_import_fixture_check(
            &fixture->source, "test/cp36", mmay_system_allocator(),
            mmay_check_graph_default_options(), &fixture->checked
        ) != MMAY_CHECK_OK || fixture->checked.diagnostics.length != 0) {
        return false;
    }
    fixture->probe = mmay_checker_import_function(
        &fixture->checked, "probe", &fixture->probe_index
    );
    fixture->map_mutation = mmay_checker_import_function(
        &fixture->checked, "mapMutation", &fixture->map_mutation_index
    );
    fixture->set_mutation = mmay_checker_import_function(
        &fixture->checked, "setMutation", &fixture->set_mutation_index
    );
    fixture->set_duplicate = mmay_checker_import_function(
        &fixture->checked, "setDuplicate", &fixture->set_duplicate_index
    );
    return fixture->probe != NULL && fixture->map_mutation != NULL
        && fixture->set_mutation != NULL && fixture->set_duplicate != NULL;
}

static void mmay_cp36_fixture_dispose(MarimayCp36Fixture *fixture) {
    mmay_check_result_dispose(&fixture->checked);
    mmay_checker_import_fixture_dispose(&fixture->source);
    memset(fixture, 0, sizeof(*fixture));
}

static size_t mmay_cp36_functions(
    MarimayCp36Fixture *fixture,
    const MarimayMirFunction **functions,
    size_t capacity
) {
    size_t index = 0;
    size_t count = 0;
    for (index = 0; index < fixture->checked.functions.length; index += 1) {
        MarimayCheckedFunction *function = mmay_vector_at(
            &fixture->checked.functions, index
        );
        if (function != NULL && !function->is_external && count < capacity) {
            functions[count++] = &function->mir;
        }
    }
    return count;
}

static bool mmay_cp36_collection_kind(MarimayMirInstructionKind kind) {
    return kind >= MMAY_MIR_COLLECTION_NEW
        && kind <= MMAY_MIR_COLLECTION_ENTRY_VALUE;
}

static void mmay_cp36_mir_contract(MarimayCp36Fixture *fixture) {
    bool seen[MMAY_MIR_COLLECTION_ENTRY_VALUE + 1];
    size_t function_index = 0;
    size_t hostile = 0;
    memset(seen, 0, sizeof(seen));
    for (function_index = 0;
        function_index < fixture->checked.functions.length;
        function_index += 1) {
        MarimayCheckedFunction *checked = mmay_vector_at(
            &fixture->checked.functions, function_index
        );
        MarimayMirFunction *function = checked != NULL ? &checked->mir : NULL;
        size_t block_index = 0;
        if (function == NULL || checked->is_external) continue;
        MMAY_EXPECT(mmay_mir_verify(function) == MMAY_MIR_VERIFY_OK);
        for (block_index = 0; block_index < function->blocks.length;
            block_index += 1) {
            MarimayMirBlock *block = mmay_vector_at(
                &function->blocks, block_index
            );
            size_t instruction_index = 0;
            for (instruction_index = 0; block != NULL
                && instruction_index < block->instructions.length;
                instruction_index += 1) {
                MarimayMirInstruction *instruction = mmay_vector_at(
                    &block->instructions, instruction_index
                );
                MarimayMirCollectionId saved;
                if (instruction == NULL
                    || !mmay_cp36_collection_kind(instruction->kind)) continue;
                seen[instruction->kind] = true;
                saved = instruction->collection;
                instruction->collection = MMAY_MIR_NO_COLLECTION;
                MMAY_EXPECT(mmay_mir_verify(function)
                    == MMAY_MIR_VERIFY_INVALID_ARGUMENT);
                instruction->collection = saved;
                hostile += 1;
            }
        }
        MMAY_EXPECT(mmay_mir_verify(function) == MMAY_MIR_VERIFY_OK);
    }
    MMAY_EXPECT(hostile >= 30);
    for (function_index = MMAY_MIR_COLLECTION_NEW;
        function_index <= MMAY_MIR_COLLECTION_ENTRY_VALUE;
        function_index += 1) {
        MMAY_EXPECT(seen[function_index]);
    }
}

static MarimayMirInterpretStatus mmay_cp36_mir_run(
    MarimayCp36Fixture *fixture,
    size_t function_index,
    MarimayAllocator allocator,
    MarimayMirExecutionOptions execution_options,
    MarimayMirPanic *panic,
    MarimayMirValue *result
) {
    MarimayMirExecution execution;
    MarimayCheckInterpretOptions options =
        mmay_check_interpret_default_options();
    MarimayMirInterpretStatus status;
    memset(&execution, 0, sizeof(execution));
    if (!mmay_mir_execution_init(&execution, allocator, execution_options)) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    options.execution = &execution;
    options.panic = panic;
    status = mmay_check_interpret_function(
        &fixture->checked, function_index, NULL, 0,
        allocator, options, result
    );
    mmay_mir_execution_dispose(&execution);
    return status;
}

static void mmay_cp36_mir_oracle(MarimayCp36Fixture *fixture) {
    MarimayMirExecutionOptions options = mmay_mir_execution_default_options();
    MarimayMirValue result = {0};
    MarimayMirPanic panic;
    memset(&panic, 0, sizeof(panic));
    MMAY_EXPECT(mmay_cp36_mir_run(
        fixture, fixture->probe_index, mmay_system_allocator(),
        options, NULL, &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 16);
    MMAY_EXPECT(mmay_cp36_mir_run(
        fixture, fixture->set_duplicate_index, mmay_system_allocator(),
        options, NULL, &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 1);
    MMAY_EXPECT(mmay_cp36_mir_run(
        fixture, fixture->map_mutation_index, mmay_system_allocator(),
        options, &panic, &result
    ) == MMAY_MIR_INTERPRET_PANIC);
    MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4005") == 0);
    MMAY_EXPECT(panic.message.length == 36
        && memcmp(panic.message.data,
            "map iterator invalidated by mutation", 36) == 0);
    mmay_mir_panic_dispose(&panic);
    memset(&panic, 0, sizeof(panic));
    MMAY_EXPECT(mmay_cp36_mir_run(
        fixture, fixture->set_mutation_index, mmay_system_allocator(),
        options, &panic, &result
    ) == MMAY_MIR_INTERPRET_PANIC);
    MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4005") == 0);
    MMAY_EXPECT(panic.message.length == 36
        && memcmp(panic.message.data,
            "set iterator invalidated by mutation", 36) == 0);
    mmay_mir_panic_dispose(&panic);
    options.max_collection_length = 1;
    MMAY_EXPECT(mmay_cp36_mir_run(
        fixture, fixture->probe_index, mmay_system_allocator(),
        options, NULL, &result
    ) == MMAY_MIR_INTERPRET_OBJECT_LIMIT);
}

static bool mmay_cp36_opcode(MarimayOpcode opcode) {
    return opcode >= MMAY_OPCODE_COLLECTION_NEW
        && opcode <= MMAY_OPCODE_COLLECTION_ENTRY_VALUE;
}

static size_t mmay_cp36_hostile_bytecode(MarimayBytecodeFunction *function) {
    size_t block_index = 0;
    size_t count = 0;
    MMAY_EXPECT(mmay_bytecode_verify(function) == MMAY_BYTECODE_VERIFY_OK);
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        MarimayBytecodeBlock *block = mmay_vector_at(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1) {
            MarimayBytecodeInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            uint64_t collection_register;
            uint32_t *layout;
            uint32_t saved;
            if (instruction == NULL || !mmay_cp36_opcode(
                    instruction->opcode
                )) continue;
            collection_register = instruction->opcode
                    == MMAY_OPCODE_COLLECTION_NEW
                ? instruction->operands[0].unsigned_value
                : ((instruction->opcode == MMAY_OPCODE_COLLECTION_PUT
                        || instruction->opcode == MMAY_OPCODE_COLLECTION_ADD
                        || instruction->opcode == MMAY_OPCODE_COLLECTION_CLEAR)
                    ? instruction->operands[0].unsigned_value
                    : instruction->operands[1].unsigned_value);
            layout = collection_register < function->register_layouts.length
                ? mmay_vector_at(
                    &function->register_layouts,
                    (size_t)collection_register
                ) : NULL;
            MMAY_EXPECT(layout != NULL);
            if (layout == NULL) continue;
            saved = *layout;
            *layout = MMAY_BYTECODE_NO_LAYOUT;
            MMAY_EXPECT(mmay_bytecode_verify(function)
                == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
            *layout = saved;
            count += 1;
        }
    }
    MMAY_EXPECT(mmay_bytecode_verify(function) == MMAY_BYTECODE_VERIFY_OK);
    return count;
}

static void mmay_cp36_bytecode_program(
    MarimayCp36Fixture *fixture,
    const MarimayMirFunction *const *functions,
    size_t function_count
) {
    MarimayBytecodeProgram program;
    MarimayBytecodeProgram decoded;
    MarimayOwnedBytes encoded = {0};
    MarimayBytecodeExecution execution;
    MarimayBytecodeRunOptions run = mmay_bytecode_run_default_options();
    MarimayBytecodeValue result = {0};
    size_t function_index = 0;
    size_t hostile = 0;
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(
        functions, function_count, fixture->probe->mir.symbol,
        mmay_system_allocator(),
        mmay_mir_bytecode_program_lower_default_options(), &program
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    for (function_index = 0; function_index < program.functions.length;
        function_index += 1) {
        MarimayBytecodeProgramFunction *function = mmay_vector_at(
            &program.functions, function_index
        );
        if (function != NULL) hostile += mmay_cp36_hostile_bytecode(
            &function->body
        );
    }
    MMAY_EXPECT(hostile >= 30);
    MMAY_EXPECT(mmay_bytecode_encode_program(
        &program, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_program(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_program_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(decoded.functions.length == program.functions.length);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    run.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &decoded, NULL, 0, mmay_system_allocator(), run, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 16);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_program_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&program);
}

static void mmay_cp36_vm_panic(
    MarimayCp36Fixture *fixture,
    const MarimayMirFunction *const *functions,
    size_t function_count,
    const MarimayCheckedFunction *entry,
    const char *message
) {
    MarimayBytecodeProgram program;
    MarimayBytecodeExecution execution;
    MarimayBytecodePanic panic;
    MarimayBytecodeRunOptions run = mmay_bytecode_run_default_options();
    MarimayBytecodeValue result = {0};
    memset(&program, 0, sizeof(program));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    (void)fixture;
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(
        functions, function_count, entry->mir.symbol,
        mmay_system_allocator(),
        mmay_mir_bytecode_program_lower_default_options(), &program
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_panic_init(&panic, mmay_system_allocator()));
    run.execution = &execution;
    run.panic = &panic;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &program, NULL, 0, mmay_system_allocator(), run, &result
    ) == MMAY_BYTECODE_RUN_PANIC);
    MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4005") == 0);
    MMAY_EXPECT(panic.message.length == strlen(message)
        && memcmp(panic.message.data, message, strlen(message)) == 0);
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_program_dispose(&program);
}

static void mmay_cp36_native(
    MarimayCp36Fixture *fixture,
    const MarimayMirFunction *const *functions,
    size_t function_count
) {
    const MarimayCheckedFunction *entries[3] = {
        fixture->probe, fixture->map_mutation, fixture->set_mutation
    };
    const char *messages[3] = {
        NULL, "map iterator invalidated by mutation",
        "set iterator invalidated by mutation"
    };
    size_t index = 0;
    for (index = 0; index < 3; index += 1) {
        MarimayNativeCProgram program;
        MarimayOwnedBytes source = {0};
        char expected[512];
        memset(&program, 0, sizeof(program));
        program.functions = functions;
        program.function_count = function_count;
        program.entry_symbol = entries[index]->mir.symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &source
        ) == MMAY_NATIVE_C_EMIT_OK);
        MMAY_EXPECT(mmay_test_native_contains(
            &source, "MarimayManagedMapDescriptor"
        ));
        if (index == 0) {
#if !defined(_WIN32)
            mmay_test_native_compile_and_run(&source, "ok:i64:16\n", 0);
#endif
        } else {
            const MarimayMirInstruction *require =
                mmay_test_mir_instruction_kind_at(
                    &entries[index]->mir,
                    MMAY_MIR_COLLECTION_REQUIRE_GENERATION, 0
                );
#if !defined(_WIN32)
            char symbol_hex[33];
            char message_hex[96];
            MMAY_EXPECT(require != NULL);
            MMAY_EXPECT(mmay_test_native_hex_bytes(
                entries[index]->mir.symbol.bytes,
                sizeof(entries[index]->mir.symbol.bytes),
                symbol_hex, sizeof(symbol_hex)
            ));
            MMAY_EXPECT(mmay_test_native_hex_bytes(
                (const unsigned char *)messages[index],
                strlen(messages[index]), message_hex, sizeof(message_hex)
            ));
            if (require != NULL) {
                int written = snprintf(
                    expected, sizeof(expected),
                    "panic:G4005:%s:%zu:%zu:%zu:%s:1:%s:%zu:%zu\n",
                    symbol_hex, require->span.start, require->span.end,
                    strlen(messages[index]), message_hex, symbol_hex,
                    require->span.start, require->span.end
                );
                MMAY_EXPECT(written > 0 && (size_t)written < sizeof(expected));
                if (written > 0 && (size_t)written < sizeof(expected)) {
                    mmay_test_native_compile_and_run(&source, expected, 1);
                }
            }
#endif
        }
        mmay_owned_bytes_dispose(&source);
    }
}

static void mmay_cp36_checker_diagnostics(void) {
    mmay_checker_expect_one_diagnostic(
        "fn bad(): void { const value: Set<f64> = setOf<f64>(); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH, "setOf<f64>()", 0
    );
    mmay_checker_expect_one_diagnostic(
        "type Key { value: int, }\n"
        "fn bad(): void { const value: Set<Key> = setOf<Key>(); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH, "setOf<Key>()", 0
    );
    mmay_checker_expect_one_diagnostic(
        "@derive(Eq, Hash) type Key { mutable value: int, }\n"
        "fn bad(): void { const value: Set<Key> = setOf<Key>(); }\n",
        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE, "mutable value: int", 0
    );
    mmay_checker_expect_one_diagnostic(
        "@derive(Eq) type Key { value: int, }\n"
        "fn bad(): void { const value: Set<Key> = setOf<Key>(); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "setOf<Key>()", 0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad<T>(): void { const value: Set<T> = setOf<T>(); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH, "Set<T>", 0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(): void { const value = mapOf(); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH, "mapOf()", 0
    );
}

static void mmay_cp36_formatter(void) {
    static const char input[] =
        "@derive(Eq,Hash)\n"
        "type Pair{left:int,right:int,}\n"
        "fn collect(): Map<Pair, Set<string>> {\n"
        "const values: Map<Pair, Set<string>> = mapOf<Pair, Set<string>>();\n"
        "for entry in values { const _ = entry; }\n"
        "return values;\n"
        "}\n";
    MarimayFormatterFixture fixture;
    MarimayFormatResult result;
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_formatter_fixture_init(
        &fixture, "cp36-collections.my",
        (const unsigned char *)input, sizeof(input) - 1
    ));
    MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
    MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_format_source(
        &fixture.lexed, &fixture.parsed, mmay_system_allocator(),
        mmay_format_default_options(), &result
    ) == MMAY_FORMAT_OK);
    if (result.initialized) {
        MMAY_EXPECT_SNAPSHOT(
            "formatter-cp36-collections.my", (const char *)result.output.data
        );
        mmay_format_result_dispose(&result);
    }
    mmay_formatter_fixture_dispose(&fixture);
}

static void mmay_cp36_checker_allocation_failures(void) {
    static const char source[] =
        "@derive(Eq, Hash) type Key { value: int, }\n"
        "fn values(): int {\n"
        " const map: Map<Key, int> = mapOf<Key, int>();\n"
        " const _ = map.put(Key { value: 1 }, 2);\n"
        " const set: Set<int> = setOf<int>(); const _ = set.add(3);\n"
        " for entry in map { const _ = entry; }\n"
        " for item in set { const _ = item; } return map.length();\n"
        "}\n";
    MarimayCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_check_ast(
            &fixture.ast, "test/cp36-allocation",
            mmay_fail_allocator(&state), mmay_check_default_options(),
            &checked
        );
        reached_success = status == MMAY_CHECK_OK;
        MMAY_EXPECT(reached_success || status == MMAY_CHECK_ALLOCATION_FAILED);
        if (reached_success) {
            MMAY_EXPECT(checked.initialized && checked.diagnostics.length == 0);
            mmay_check_result_dispose(&checked);
        } else {
            MMAY_EXPECT(!checked.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_cp36_execution_allocation_failures(
    MarimayCp36Fixture *fixture,
    const MarimayMirFunction *const *functions,
    size_t function_count
) {
    MarimayBytecodeProgram program;
    unsigned int engine = 0;
    memset(&program, 0, sizeof(program));
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(
        functions, function_count, fixture->probe->mir.symbol,
        mmay_system_allocator(),
        mmay_mir_bytecode_program_lower_default_options(), &program
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    for (engine = 0; engine < 2; engine += 1) {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 1024; failure_index += 1) {
            MarimayFailAllocator state;
            MarimayAllocator allocator;
            mmay_fail_allocator_init(
                &state, mmay_system_allocator(), failure_index
            );
            allocator = mmay_fail_allocator(&state);
            if (engine == 0) {
                MarimayMirValue result = {0};
                MarimayMirInterpretStatus status = mmay_cp36_mir_run(
                    fixture, fixture->probe_index, allocator,
                    mmay_mir_execution_default_options(), NULL, &result
                );
                reached_success = status == MMAY_MIR_INTERPRET_OK;
                MMAY_EXPECT(reached_success
                    || status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
            } else {
                MarimayBytecodeExecution execution;
                MarimayBytecodeRunOptions run =
                    mmay_bytecode_run_default_options();
                MarimayBytecodeValue result = {0};
                memset(&execution, 0, sizeof(execution));
                if (mmay_bytecode_execution_init(
                        &execution, allocator,
                        mmay_bytecode_execution_default_options()
                    )) {
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

static void mmay_cp36_native_allocation_failures(
    MarimayCp36Fixture *fixture,
    const MarimayMirFunction *const *functions,
    size_t function_count
) {
    MarimayNativeCProgram program;
    size_t failure_index = 0;
    bool reached_success = false;
    memset(&program, 0, sizeof(program));
    program.functions = functions;
    program.function_count = function_count;
    program.entry_symbol = fixture->probe->mir.symbol;
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes output = {0};
        MarimayNativeCEmitStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_native_c_emit_program(
            &program, mmay_fail_allocator(&state),
            mmay_native_c_emit_default_options(), &output
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

void mmay_test_cp36_collections(void) {
    MarimayCp36Fixture fixture;
    const MarimayMirFunction *functions[16];
    size_t function_count;
    memset(functions, 0, sizeof(functions));
    MMAY_EXPECT(mmay_cp36_fixture_init(&fixture));
    if (fixture.probe == NULL) {
        mmay_cp36_fixture_dispose(&fixture);
        return;
    }
    function_count = mmay_cp36_functions(
        &fixture, functions, sizeof(functions) / sizeof(functions[0])
    );
    MMAY_EXPECT(function_count == 5);
    mmay_cp36_mir_contract(&fixture);
    mmay_cp36_mir_oracle(&fixture);
    mmay_cp36_bytecode_program(&fixture, functions, function_count);
    mmay_cp36_vm_panic(
        &fixture, functions, function_count, fixture.map_mutation,
        "map iterator invalidated by mutation"
    );
    mmay_cp36_vm_panic(
        &fixture, functions, function_count, fixture.set_mutation,
        "set iterator invalidated by mutation"
    );
    mmay_cp36_native(&fixture, functions, function_count);
    mmay_cp36_execution_allocation_failures(
        &fixture, functions, function_count
    );
    mmay_cp36_native_allocation_failures(
        &fixture, functions, function_count
    );
    mmay_cp36_fixture_dispose(&fixture);
    mmay_cp36_checker_diagnostics();
    mmay_cp36_checker_allocation_failures();
    mmay_cp36_formatter();
}
