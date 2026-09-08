#include "test_native_c_emitter_support.h"
#include "test_bytecode_vm_support.h"
#include "test_checker_imports_support.h"

#include <stdio.h>
#include "mmay/bytecode/codec.h"

#if !defined(_WIN32)
/* The caller retains the original array across an injected failure. This
 * checks source lowering's Some-before-mutation boundary in generated C,
 * including reference payloads and copy roots at every allocation. */
static void mmay_edit_native_atomicity(void) {
    static const char source[] =
        "fn make(): [][]int { return [[7], [8], [9]]; }\n"
        "fn edit(values: [][]int): int { const saved = values.copy();\n"
        " if const removed = values.removeAt(1) {\n"
        "   return removed[0] * 100 + values.length() * 10 + saved.length(); }\n"
        " return -1; }\n"
        "fn main(): int { return edit(make()); }\n";
    static const char prefix[] = "#define main mmay_edit_original_main\n";
    static const char harness[] =
        "\n#undef main\n"
        "int main(void) {\n"
        "  size_t failure; bool completed = false;\n"
        "  (void)&mmay_edit_original_main; mmay_g_keep_assets();\n"
        "  for (failure = 0; failure < 512 && !completed; failure += 1) {\n"
        "    MarimayFailAllocator state; mmay_g_execution x = {0};\n"
        "    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();\n"
        "    mmay_g_panic panic = {0}; mmay_g_frame frames[MMAY_G_MAX_FRAMES];\n"
        "    mmay_g_suppressed_panic suppressed[MMAY_G_MAX_SUPPRESSED];\n"
        "    MarimayManagedReference input = mmay_managed_invalid_reference();\n"
        "    MarimayManagedReference *locations[] = {&input};\n"
        "    MarimayManagedRootFrame roots = {0}; MarimayManagedCell before[3], cell;\n"
        "    size_t start = 0, end = 0, length = 0; uint64_t generation = 0;\n"
        "    int64_t value = 0; int status = 5; bool ok = true;\n"
        "    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);\n"
        "    x.allocator = mmay_fail_allocator(&state); options.collect_before_allocation = true;\n"
        "    panic.frames = frames; panic.frame_capacity = MMAY_G_MAX_FRAMES;\n"
        "    panic.suppressed = suppressed; panic.suppressed_capacity = MMAY_G_MAX_SUPPRESSED;\n"
        "    if (!mmay_managed_heap_init(&x.heap, x.allocator, options)) return 2;\n"
        "    status = mmay_g_managed_status(mmay_g_register_immediate_enums(&x.heap));\n"
        "    if (status == 0) status = mmay_g_f_%s(&x, &panic, &input, &start, &end);\n"
        "    if (status != 0 || mmay_managed_root_push(&x.heap, &roots, locations, 1) != MMAY_MANAGED_OK) return 3;\n"
        "    for (size_t i = 0; i < 3; i += 1)\n"
        "      if (mmay_managed_array_get(&x.heap, input, (int64_t)i, &before[i], &length) != MMAY_MANAGED_OK) return 4;\n"
        "    state.failure_index = state.attempts + failure;\n"
        "    status = mmay_g_f_%s(&x, &panic, input, &value, &start, &end);\n"
        "    completed = status == 0;\n"
        "    ok = (completed ? value == 823 : status == mmay_g_managed_status(MMAY_MANAGED_ALLOCATION_FAILED));\n"
        "    ok = ok && mmay_managed_array_length(&x.heap, input, &length) == MMAY_MANAGED_OK\n"
        "      && length == (completed ? 2U : 3U);\n"
        "    ok = ok && mmay_managed_array_mutation_generation(&x.heap, input, &generation) == MMAY_MANAGED_OK\n"
        "      && generation == (completed ? 1U : 0U);\n"
        "    for (size_t i = 0; ok && i < length; i += 1) {\n"
        "      size_t expected = completed && i == 1 ? 2 : i;\n"
        "      ok = mmay_managed_array_get(&x.heap, input, (int64_t)i, &cell, &length) == MMAY_MANAGED_OK\n"
        "        && memcmp(&cell.reference, &before[expected].reference, sizeof(cell.reference)) == 0;\n"
        "    }\n"
        "    ok = mmay_managed_root_pop(&x.heap, &roots) == MMAY_MANAGED_OK && ok;\n"
        "    mmay_g_execution_dispose(&x);\n"
        "    if (!ok || state.live_allocations != 0 || state.live_bytes != 0) {\n"
        "      fprintf(stderr, \"failure=%%zu status=%%d value=%%lld length=%%zu generation=%%llu\\n\",\n"
        "        failure, status, (long long)value, length, (unsigned long long)generation); return 5; }\n"
        "  }\n"
        "  if (!completed) { return 6; }\n"
        "  puts(\"array edit atomicity: ok\"); return 0;\n"
        "}\n";
    const MarimayTestImportSource input = {"test/edit-atomicity", "app/main.my", source};
    MarimayCheckerImportFixture fixture = {0};
    MarimayCheckResult checked = {0};
    const MarimayCheckedFunction *make, *edit, *entry;
    const MarimayMirFunction *functions[8];
    MarimayNativeCProgram program = {0};
    MarimayOwnedBytes generated = {0}, combined = {0};
    char make_symbol[33], edit_symbol[33], suffix[8192];
    size_t index = 0, length;
    int written;
    MMAY_EXPECT(mmay_checker_import_fixture_init(&fixture, &input, 1));
    MMAY_EXPECT(mmay_checker_import_fixture_check(&fixture, "test/edit-atomicity",
        mmay_system_allocator(), mmay_check_graph_default_options(), &checked) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.diagnostics.length == 0);
    make = mmay_checker_import_function(&checked, "make", &index);
    edit = mmay_checker_import_function(&checked, "edit", &index);
    entry = mmay_checker_import_function(&checked, "main", &index);
    MMAY_EXPECT(make != NULL && edit != NULL && entry != NULL && checked.functions.length <= 8);
    if (make == NULL || edit == NULL || entry == NULL || checked.functions.length > 8) goto cleanup;
    for (index = 0; index < checked.functions.length; index += 1)
        functions[index] = &mmay_checked_function(&checked, index)->mir;
    program.functions = functions; program.function_count = checked.functions.length;
    program.entry_symbol = entry->mir.symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(&program, mmay_system_allocator(),
        mmay_native_c_emit_default_options(), &generated) == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(mmay_test_native_hex_bytes(make->mir.symbol.bytes, sizeof(make->mir.symbol.bytes), make_symbol, sizeof(make_symbol)));
    MMAY_EXPECT(mmay_test_native_hex_bytes(edit->mir.symbol.bytes, sizeof(edit->mir.symbol.bytes), edit_symbol, sizeof(edit_symbol)));
    written = snprintf(suffix, sizeof(suffix), harness, make_symbol, edit_symbol);
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(suffix));
    if (written <= 0 || (size_t)written >= sizeof(suffix)) goto cleanup;
    length = sizeof(prefix) - 1 + generated.length + (size_t)written;
    combined.allocator = mmay_system_allocator();
    combined.data = mmay_allocate(&combined.allocator, length + 1);
    MMAY_EXPECT(combined.data != NULL);
    if (combined.data == NULL) goto cleanup;
    combined.length = length;
    memcpy(combined.data, prefix, sizeof(prefix) - 1);
    memcpy(combined.data + sizeof(prefix) - 1, generated.data, generated.length);
    memcpy(combined.data + sizeof(prefix) - 1 + generated.length, suffix, (size_t)written + 1);
    mmay_test_native_compile_and_run(&combined, "array edit atomicity: ok\n", 0);
cleanup:
    mmay_owned_bytes_dispose(&combined);
    mmay_owned_bytes_dispose(&generated);
    mmay_check_result_dispose(&checked);
    mmay_checker_import_fixture_dispose(&fixture);
}
#endif

static bool mmay_edit_append(MarimayMirFunction *function, MarimayMirInstructionKind kind,
    unsigned target, unsigned first, unsigned second, int64_t value) {
    MarimayMirInstruction instruction = mmay_test_native_instruction(
        kind, target, first, second, value, false, 100);
    instruction.array = 0;
    instruction.array_construction = 0;
    return mmay_mir_append_instruction(function, 0, &instruction);
}

static bool mmay_edit_build(MarimayMirFunction *function, int64_t index, bool proof) {
    const MarimaySymbolId symbol = {{0xed}};
    const MarimayTypeId array_type = MMAY_TYPE_FIRST_DYNAMIC + 10;
    const MarimayTypeId types[] = {MMAY_TYPE_I64, array_type, MMAY_TYPE_I64,
        array_type, array_type, MMAY_TYPE_I64, MMAY_TYPE_I64, MMAY_TYPE_I64,
        MMAY_TYPE_I64, MMAY_TYPE_I64, array_type, MMAY_TYPE_I64};
    MarimayMirArrayId array;
    MarimayMirArrayConstructionId construction;
    MarimayMirBlockId block;
    MarimayMirArrayArgument elements[] = {{.register_id = 0, .type = MMAY_TYPE_I64},
        {.register_id = 0, .type = MMAY_TYPE_I64}, {.register_id = 0, .type = MMAY_TYPE_I64}};
    if (!mmay_mir_function_init(function, mmay_system_allocator(), symbol, NULL, 0,
            MMAY_TYPE_I64, mmay_mir_function_default_options())) return false;
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i += 1) {
        MarimayMirRegister stored;
        if (!mmay_mir_add_register(function, types[i], &stored)) return false;
        MMAY_EXPECT(stored == i);
    }
    return mmay_mir_add_array(function, array_type, MMAY_TYPE_I64, &array)
        && mmay_mir_add_array_construction(function, array, elements, 3, &construction)
        && mmay_mir_add_block(function, &block)
        && mmay_edit_append(function, MMAY_MIR_CONSTANT_I64, 0, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 7)
        && mmay_edit_append(function, MMAY_MIR_ARRAY_NEW, 1, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 0)
        && mmay_edit_append(function, MMAY_MIR_CONSTANT_I64, 2, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, index)
        && (!proof || mmay_edit_append(function, MMAY_MIR_ARRAY_REQUIRE_INDEX, 10, 1, 2, 0))
        && mmay_edit_append(function, MMAY_MIR_ARRAY_COPY, 3, 1, MMAY_MIR_NO_REGISTER, 0)
        && mmay_edit_append(function, MMAY_MIR_ARRAY_REMOVE_AT, 4, 3, 2, 0)
        && mmay_edit_append(function, MMAY_MIR_ARRAY_LENGTH, 5, 1, MMAY_MIR_NO_REGISTER, 0)
        && mmay_edit_append(function, MMAY_MIR_ARRAY_LENGTH, 6, 4, MMAY_MIR_NO_REGISTER, 0)
        && mmay_edit_append(function, MMAY_MIR_CONSTANT_I64, 7, MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER, 10)
        && mmay_edit_append(function, MMAY_MIR_MULTIPLY_CHECKED_I64, 8, 5, 7, 0)
        && mmay_edit_append(function, MMAY_MIR_ADD_CHECKED_I64, 9, 8, 6, 0)
        && (!proof || mmay_edit_append(function, MMAY_MIR_ARRAY_GET_IN_BOUNDS, 11, 10, 2, 0))
        && mmay_mir_set_terminator(function, block, mmay_test_native_terminator(
            MMAY_MIR_RETURN, proof ? 11 : 9, MMAY_MIR_NO_BLOCK, MMAY_MIR_NO_BLOCK, 101));
}

static void mmay_edit_mir_failures(const MarimayMirFunction *function, bool valid) {
    bool completed = false;
    for (size_t failure = 0; failure < 512 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayMirExecution execution = {0};
        MarimayMirPanic panic = {0};
        MarimayMirValue result;
        MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
        MarimayMirInterpretStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MMAY_EXPECT(mmay_mir_execution_init(&execution, mmay_fail_allocator(&state),
            mmay_mir_execution_default_options()));
        options.execution = &execution; options.panic = &panic;
        status = mmay_mir_interpret(function, NULL, 0, mmay_fail_allocator(&state), options, &result);
        completed = status == (valid ? MMAY_MIR_INTERPRET_OK : MMAY_MIR_INTERPRET_PANIC);
        MMAY_EXPECT(completed || status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
        if (completed && valid) MMAY_EXPECT(result.as.i64 == 32);
        if (completed && !valid) {
            MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
            MMAY_EXPECT(panic.span.start == 100 && panic.span.end == 101);
        }
        mmay_mir_panic_dispose(&panic);
        mmay_mir_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
}

static void mmay_edit_vm_failures(const MarimayBytecodeFunction *function, bool valid) {
    bool completed = false;
    for (size_t failure = 0; failure < 1024 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeExecution execution = {0};
        MarimayBytecodePanic panic = {0};
        MarimayBytecodeValue result;
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        MarimayBytecodeExecutionOptions execution_options = mmay_bytecode_execution_default_options();
        MarimayBytecodeRunStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MMAY_EXPECT(mmay_bytecode_execution_init(&execution, mmay_fail_allocator(&state), execution_options));
        execution.managed_heap.options.collect_before_allocation = true;
        MMAY_EXPECT(mmay_bytecode_panic_init(&panic, mmay_fail_allocator(&state)));
        options.execution = &execution; options.panic = &panic;
        status = mmay_bytecode_run(function, NULL, 0, mmay_fail_allocator(&state), options, &result);
        completed = status == (valid ? MMAY_BYTECODE_RUN_OK : MMAY_BYTECODE_RUN_PANIC);
        MMAY_EXPECT(completed || status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
        if (completed && valid) MMAY_EXPECT(result.as.i64 == 32);
        if (completed && !valid) {
            MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
            MMAY_EXPECT(panic.span.start == 100 && panic.span.end == 101);
        }
        mmay_bytecode_panic_dispose(&panic);
        mmay_bytecode_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
}

static void mmay_edit_compile_failures(const MarimayMirFunction *function) {
    bool completed = false;
    for (size_t failure = 0; failure < 4096 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeFunction bytecode = {0};
        MarimayMirBytecodeLowerStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        status = mmay_mir_lower_bytecode_function(function, mmay_fail_allocator(&state),
            mmay_mir_bytecode_lower_default_options(), &bytecode);
        completed = status == MMAY_MIR_BYTECODE_LOWER_OK;
        MMAY_EXPECT(completed || status == MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED);
        mmay_bytecode_function_dispose(&bytecode);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    completed = false;
    for (size_t failure = 0; failure < 4096 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes source = {0};
        MarimayNativeCProgram program = {.functions = &function, .function_count = 1,
            .entry_symbol = function->symbol};
        MarimayNativeCEmitStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        status = mmay_native_c_emit_program(&program, mmay_fail_allocator(&state),
            mmay_native_c_emit_default_options(), &source);
        completed = status == MMAY_NATIVE_C_EMIT_OK;
        MMAY_EXPECT(completed || status == MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED);
        mmay_owned_bytes_dispose(&source);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
}

static void mmay_edit_hostile(void) {
    MarimayMirFunction function = {0};
    MarimayBytecodeFunction bytecode = {0};
    MarimayMirBlock *block;
    MarimayMirInstruction *copy, *remove;
    MMAY_EXPECT(mmay_edit_build(&function, 1, false));
    block = mmay_vector_at(&function.blocks, 0);
    copy = mmay_vector_at(&block->instructions, 3);
    remove = mmay_vector_at(&block->instructions, 4);
    copy->target = 0;
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
    copy->target = 3;
    remove->second = 3;
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
    remove->second = 2;
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&function, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &bytecode) == MMAY_MIR_BYTECODE_LOWER_OK);
    {
        MarimayBytecodeInstruction *wire_copy = mmay_test_vm_at(&bytecode, 0, 3);
        MarimayBytecodeInstruction *wire_remove = mmay_test_vm_at(&bytecode, 0, 4);
        MMAY_EXPECT(wire_copy->opcode == MMAY_OPCODE_ARRAY_COPY);
        MMAY_EXPECT(wire_remove->opcode == MMAY_OPCODE_ARRAY_REMOVE_AT);
        wire_copy->operands[0].unsigned_value = 0;
        MMAY_EXPECT(mmay_bytecode_verify(&bytecode) == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        wire_copy->operands[0].unsigned_value = 3;
        wire_copy->operands[1].unsigned_value = 4;
        MMAY_EXPECT(mmay_bytecode_verify(&bytecode) == MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
        wire_copy->operands[1].unsigned_value = 1;
        wire_remove->operands[2].unsigned_value = 3;
        MMAY_EXPECT(mmay_bytecode_verify(&bytecode) == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        wire_remove->operands[2].unsigned_value = UINT32_MAX;
        MMAY_EXPECT(mmay_bytecode_verify(&bytecode) != MMAY_BYTECODE_VERIFY_OK);
    }
    mmay_bytecode_function_dispose(&bytecode);
    mmay_mir_function_dispose(&function);
    memset(&function, 0, sizeof(function));
    MMAY_EXPECT(mmay_edit_build(&function, 1, true));
    MMAY_EXPECT(mmay_mir_verify(&function) != MMAY_MIR_VERIFY_OK);
    block = mmay_vector_at(&function.blocks, 0);
    remove = mmay_vector_at(&block->instructions, 5);
    remove->kind = MMAY_MIR_COPY;
    remove->second = MMAY_MIR_NO_REGISTER;
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_lower_bytecode_function(&function, mmay_system_allocator(),
        mmay_mir_bytecode_lower_default_options(), &bytecode) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_verify(&bytecode) == MMAY_BYTECODE_VERIFY_OK);
    {
        MarimayBytecodeInstruction *wire = mmay_test_vm_at(&bytecode, 0, 5);
        wire->opcode = MMAY_OPCODE_ARRAY_REMOVE_AT;
        wire->operands[2].unsigned_value = 2;
        MMAY_EXPECT(mmay_bytecode_verify(&bytecode) != MMAY_BYTECODE_VERIFY_OK);
    }
    mmay_bytecode_function_dispose(&bytecode);
    mmay_mir_function_dispose(&function);
}

void mmay_test_array_edit_engines(void) {
    static const int64_t indices[] = {0, 1, 2, -1, 3, INT64_MIN, INT64_MAX};
    mmay_edit_hostile();
#if !defined(_WIN32)
    mmay_edit_native_atomicity();
#endif
    for (size_t index = 0; index < sizeof(indices) / sizeof(indices[0]); index += 1) {
        MarimayMirFunction function = {0};
        MarimayBytecodeFunction bytecode = {0}, decoded = {0};
        MarimayOwnedBytes encoded = {0}, generated = {0};
        const MarimayMirFunction *functions[] = {&function};
        MarimayNativeCProgram program = {0};
        bool valid = index < 3;
        MMAY_EXPECT(mmay_edit_build(&function, indices[index], false));
        MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
        mmay_edit_mir_failures(&function, valid);
        MMAY_EXPECT(mmay_mir_lower_bytecode_function(&function, mmay_system_allocator(),
            mmay_mir_bytecode_lower_default_options(), &bytecode) == MMAY_MIR_BYTECODE_LOWER_OK);
        MMAY_EXPECT(mmay_bytecode_encode_function(&bytecode, mmay_system_allocator(), &encoded)
            == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_decode_function(encoded.data, encoded.length,
            mmay_system_allocator(), mmay_bytecode_function_default_options(), &decoded)
            == MMAY_BYTECODE_CODEC_OK);
        mmay_edit_vm_failures(&decoded, valid);
        program.functions = functions; program.function_count = 1; program.entry_symbol = function.symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(&program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &generated) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
        if (valid) {
            mmay_test_native_compile_and_run(&generated, "ok:i64:32\n", 0);
        } else {
            char message[100], hex[201], expected[600], symbol[33];
            int length = snprintf(message, sizeof(message), "array index %lld is outside length 3",
                (long long)indices[index]);
            MMAY_EXPECT(length > 0 && (size_t)length < sizeof(message));
            MMAY_EXPECT(mmay_test_native_hex_bytes((const unsigned char *)message, (size_t)length, hex, sizeof(hex)));
            MMAY_EXPECT(mmay_test_native_hex_bytes(function.symbol.bytes, sizeof(function.symbol.bytes), symbol, sizeof(symbol)));
            (void)snprintf(expected, sizeof(expected), "panic:G4002:%s:100:101:%d:%s:1:%s:100:101\n",
                symbol, length, hex, symbol);
            mmay_test_native_compile_and_run(&generated, expected, 1);
        }
#endif
        if (index == 0) mmay_edit_compile_failures(&function);
        mmay_owned_bytes_dispose(&generated);
        mmay_owned_bytes_dispose(&encoded);
        mmay_bytecode_function_dispose(&decoded);
        mmay_bytecode_function_dispose(&bytecode);
        mmay_mir_function_dispose(&function);
    }
}
