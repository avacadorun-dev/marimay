#include "test_checker_imports_support.h"
#include "test_native_c_emitter_support.h"

#include <stdio.h>
#include <string.h>

#include "mmay/bytecode/codec.h"
#include "mmay/check/interpret.h"

static const int64_t mmay_cursor_inputs[] = {-1, 0, 1, 2, 3, 4, INT64_MAX};

static bool mmay_cursor_expected(bool is_set, unsigned int protocol,
    int64_t cursor, int64_t *value) {
    bool valid = protocol == 0 ? cursor == 1 || cursor == 3
        : protocol == 1 ? cursor == 0 || cursor == 1 : cursor >= 0 && cursor <= 3;
    *value = 0;
    if (valid) *value = protocol == 0 ? (cursor == 1 ? 10 : 30)
        : protocol == 1 ? (cursor == 0 ? 10 : 30)
        : cursor == 3 ? -1 : cursor == 0 ? 10 : 30;
    if (!is_set && *value > 0) *value = *value * 2 + 100;
    return valid;
}

#if !defined(_WIN32)
/* One build per protocol/kind and optimization level; each value still runs
 * in a fresh process, including native trap paths and sanitizer teardown. */
static void mmay_cursor_native_batch(const MarimayOwnedBytes *source,
    MarimaySymbolId probe, bool is_set, unsigned int protocol) {
    static const char prefix[] =
        "#include \"mmay/platform/process.h\"\n"
        "#define main mmay_cursor_original_main\n";
    static const char harness[] =
        "\n#undef main\n"
        "static int cursor_run(int64_t cursor) {\n"
        "  MarimayFailAllocator state; mmay_g_execution x = {0};\n"
        "  MarimayManagedHeapOptions options = mmay_managed_heap_default_options();\n"
        "  mmay_g_panic panic = {0}; mmay_g_frame frames[MMAY_G_MAX_FRAMES];\n"
        "  mmay_g_suppressed_panic suppressed[MMAY_G_MAX_SUPPRESSED];\n"
        "  size_t start = 0, end = 0; int64_t value = 0; int status = 5;\n"
        "  mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);\n"
        "  x.allocator = mmay_fail_allocator(&state);\n"
        "  options.collect_before_allocation = true;\n"
        "  panic.frames = frames; panic.frame_capacity = MMAY_G_MAX_FRAMES;\n"
        "  panic.suppressed = suppressed; panic.suppressed_capacity = MMAY_G_MAX_SUPPRESSED;\n"
        "  if (mmay_managed_heap_init(&x.heap, x.allocator, options)) {\n"
        "    status = mmay_g_managed_status(mmay_g_register_immediate_enums(&x.heap));\n"
        "    if (status == 0) status = mmay_g_f_%s(&x, &panic, cursor, true, &value, &start, &end);\n"
        "  }\n"
        "  mmay_g_execution_dispose(&x);\n"
        "  if (state.live_allocations != 0 || state.live_bytes != 0) return 2;\n"
        "  if (status == 0) printf(\"ok:i64:%%lld\\n\", (long long)value);\n"
        "  else printf(\"trap:%%d:%%zu:%%zu\\n\", status, start, end);\n"
        "  return status == 0 ? 0 : 1;\n"
        "}\n"
        "int main(int argc, char **argv) {\n"
        "  static const int64_t inputs[] = {-1, 0, 1, 2, 3, 4, INT64_MAX};\n"
        "  static const struct { uint32_t status; const char *text; } cases[] = {%s};\n"
        "  size_t i; (void)&mmay_cursor_original_main; mmay_g_keep_assets();\n"
        "  if (argc == 2 && argv[1][0] >= '0' && argv[1][0] <= '6' && argv[1][1] == 0)\n"
        "    return cursor_run(inputs[(size_t)(argv[1][0] - '0')]);\n"
        "  if (argc != 1) return 3;\n"
        "  for (i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i += 1) {\n"
        "    char selector[2] = {(char)('0' + i), 0};\n"
        "    const char *args[] = {argv[0], selector, NULL};\n"
        "    MarimayProcessResult child = {0};\n"
        "    bool ok = mmay_process_run(mmay_system_allocator(), args, 2, 16384, &child) == MMAY_PROCESS_OK\n"
        "      && !child.signaled && child.exit_code == cases[i].status\n"
        "      && child.output.length == strlen(cases[i].text)\n"
        "      && memcmp(child.output.data, cases[i].text, child.output.length) == 0;\n"
        "    if (!ok) { fprintf(stderr, \"cursor %%zu failed: exit=%%u signal=%%d\\n\",\n"
        "      i, child.exit_code, child.signaled);\n"
        "      if (child.output.data != NULL) fwrite(child.output.data, 1, child.output.length, stderr); }\n"
        "    mmay_process_result_dispose(&child); if (!ok) return 4;\n"
        "  }\n"
        "  puts(\"cursor cases: 7\"); return 0;\n"
        "}\n";
    char symbol[33], cases[1024], suffix[8192];
    MarimayOwnedBytes generated = {0};
    size_t used = 0, i, length = 0, bytes = 0;
    int written;
    MMAY_EXPECT(mmay_test_native_hex_bytes(probe.bytes, sizeof(probe.bytes),
        symbol, sizeof(symbol)));
    for (i = 0; i < sizeof(mmay_cursor_inputs) / sizeof(mmay_cursor_inputs[0]); i += 1U) {
        int64_t value;
        bool valid = mmay_cursor_expected(is_set, protocol, mmay_cursor_inputs[i], &value);
        written = valid ? snprintf(cases + used, sizeof(cases) - used,
            "{0, \"ok:i64:%lld\\n\"},", (long long)value)
            : snprintf(cases + used, sizeof(cases) - used, "{1, \"trap:7:0:0\\n\"},");
        MMAY_EXPECT(written > 0 && (size_t)written < sizeof(cases) - used);
        if (written <= 0 || (size_t)written >= sizeof(cases) - used) return;
        used += (size_t)written;
    }
    written = snprintf(suffix, sizeof(suffix), harness, symbol, cases);
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(suffix));
    if (written <= 0 || (size_t)written >= sizeof(suffix)) return;
    MMAY_EXPECT(mmay_size_add(sizeof(prefix) - 1U, source->length, &length)
        && mmay_size_add(length, (size_t)written, &length)
        && mmay_size_add(length, 1, &bytes));
    if (bytes == 0) return;
    generated.allocator = mmay_system_allocator();
    generated.data = mmay_allocate(&generated.allocator, bytes);
    MMAY_EXPECT(generated.data != NULL);
    if (generated.data == NULL) return;
    generated.length = length;
    memcpy(generated.data, prefix, sizeof(prefix) - 1U);
    memcpy(generated.data + sizeof(prefix) - 1U, source->data, source->length);
    memcpy(generated.data + sizeof(prefix) - 1U + source->length, suffix, (size_t)written + 1U);
    mmay_test_native_compile_and_run(&generated, "cursor cases: 7\n", 0);
    mmay_owned_bytes_dispose(&generated);
}
#endif

/* Construct valid source-owned layouts, then forge only the internal cursor
 * operation's operand. The signed input remains correctly defined and typed. */
static void mmay_cursor_wire_case(bool is_set, unsigned int protocol, int64_t cursor) {
    char source[1600];
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckedFunction *probe;
    const MarimayCheckedFunction *entry;
    const MarimayMirFunction *functions[8];
    MarimayBytecodeProgram program = {0}, decoded = {0};
    MarimayOwnedBytes encoded = {0};
    MarimayMirValue oracle = {0};
    MarimayBytecodeValue result = {0};
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodeRunOptions run = mmay_bytecode_run_default_options();
    MarimayMirInterpretStatus mir_status;
    MarimayBytecodeRunStatus vm_status;
    size_t entry_index = 0, probe_index = 0, count = 0, i, b;
    int64_t expected_key = 0;
    bool valid;
    int written = snprintf(source, sizeof(source),
        "fn probe(cursor: int, flag: bool): int {\n"
        " if !flag { return -10; }\n"
        " const values: %s = %s;\n%s\n"
        " for item in values { return %s; } return -1;\n}\n"
        "fn main(): int { return probe(%lld, true); }\n",
        is_set ? "Set<int>" : "Map<int, int>",
        is_set ? "setOf<int>()" : "mapOf<int, int>()",
        is_set ? "const _ = values.add(10); const _ = values.add(20);"
            "const _ = values.add(30); const _ = values.remove(20);"
            : "const _ = values.put(10, 110); const _ = values.put(20, 120);"
              "const _ = values.put(30, 130); const _ = values.remove(20);",
        is_set ? "item" : "match item { (key, value) => key + value, }",
        (long long)cursor);
    const MarimayTestImportSource input = {"test/cursor", "app/main.my", source};
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(source));
    if (!mmay_checker_import_fixture_init(&fixture, &input, 1)) return;
    MMAY_EXPECT(mmay_checker_import_fixture_check(&fixture, "test/cursor",
        mmay_system_allocator(), mmay_check_graph_default_options(), &checked)
        == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.diagnostics.length == 0);
    (void)mmay_checker_import_function(&checked, "probe", &probe_index);
    probe = mmay_vector_at(&checked.functions, probe_index);
    entry = mmay_checker_import_function(&checked, "main", &entry_index);
    MMAY_EXPECT(probe != NULL && entry != NULL);
    if (probe == NULL || entry == NULL) goto cleanup;
    for (b = 0; b < probe->mir.blocks.length; b += 1U) {
        MarimayMirBlock *block = mmay_vector_at(&probe->mir.blocks, b);
        for (i = 0; i < block->instructions.length; i += 1U) {
            MarimayMirInstruction *op = mmay_vector_at(&block->instructions, i);
            if (op->kind < MMAY_MIR_COLLECTION_NEXT) continue;
            {
                MarimayMirRegister saved = op->second, target = op->target;
                MarimayMirInstructionKind kind = op->kind;
                op->second = 1; /* Independently initialized bool parameter. */
                MMAY_EXPECT(mmay_mir_verify(&probe->mir) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
                op->second = saved;
                op->target = 1;
                MMAY_EXPECT(mmay_mir_verify(&probe->mir) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
                op->target = target;
                if (is_set && kind == MMAY_MIR_COLLECTION_CURSOR_KEY) {
                    op->kind = MMAY_MIR_COLLECTION_CURSOR_VALUE;
                    MMAY_EXPECT(mmay_mir_verify(&probe->mir) == MMAY_MIR_VERIFY_TYPE_MISMATCH);
                    op->kind = kind;
                }
            }
            if (protocol == 2) {
                if (op->kind == MMAY_MIR_COLLECTION_NEXT) op->second = 0;
            } else if (op->kind != MMAY_MIR_COLLECTION_NEXT) {
                op->second = 0;
                if (protocol == 1) op->kind = op->kind == MMAY_MIR_COLLECTION_CURSOR_KEY
                    ? MMAY_MIR_COLLECTION_ENTRY_KEY : MMAY_MIR_COLLECTION_ENTRY_VALUE;
            }
            op->span.start = 0;
            op->span.end = 0;
        }
    }
    MMAY_EXPECT(mmay_mir_verify(&probe->mir) == MMAY_MIR_VERIFY_OK);
    valid = mmay_cursor_expected(is_set, protocol, cursor, &expected_key);
    mir_status = mmay_check_interpret_function(&checked, entry_index, NULL, 0,
        mmay_system_allocator(), mmay_check_interpret_default_options(), &oracle);
    MMAY_EXPECT(mir_status == (valid ? MMAY_MIR_INTERPRET_OK : MMAY_MIR_INTERPRET_INVALID_MIR));
    if (valid) MMAY_EXPECT(oracle.as.i64 == expected_key);
    for (i = 0; i < checked.functions.length; i += 1U) {
        const MarimayCheckedFunction *f = mmay_vector_at_const(&checked.functions, i);
        if (!f->is_external) {
            MMAY_EXPECT(count < 8);
            if (count < 8) functions[count++] = &f->mir;
        }
    }
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(functions, count, entry->mir.symbol,
        mmay_system_allocator(), mmay_mir_bytecode_program_lower_default_options(),
        &program) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_encode_program(&program, mmay_system_allocator(), &encoded)
        == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_program(encoded.data, encoded.length,
        mmay_system_allocator(), mmay_bytecode_program_default_options(), &decoded)
        == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_program_verify(&decoded) == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    for (i = 0; i < decoded.functions.length; i += 1U) {
        MarimayBytecodeProgramFunction *f = mmay_vector_at(&decoded.functions, i);
        for (b = 0; b < f->body.blocks.length; b += 1U) {
            MarimayBytecodeBlock *block = mmay_vector_at(&f->body.blocks, b);
            size_t n;
            for (n = 0; n < block->instructions.length; n += 1U) {
                MarimayBytecodeInstruction *op = mmay_vector_at(&block->instructions, n);
                uint64_t saved;
                if (op->opcode < MMAY_OPCODE_COLLECTION_NEXT) continue;
                saved = op->operands[2].unsigned_value;
                op->operands[2].unsigned_value = 1;
                MMAY_EXPECT(mmay_bytecode_verify(&f->body) == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
                op->operands[2].unsigned_value = saved;
                saved = op->operands[0].unsigned_value;
                op->operands[0].unsigned_value = 1;
                MMAY_EXPECT(mmay_bytecode_verify(&f->body) == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
                op->operands[0].unsigned_value = saved;
                if (is_set && op->opcode == MMAY_OPCODE_COLLECTION_CURSOR_KEY) {
                    op->opcode = MMAY_OPCODE_COLLECTION_CURSOR_VALUE;
                    MMAY_EXPECT(mmay_bytecode_verify(&f->body) == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
                    op->opcode = MMAY_OPCODE_COLLECTION_CURSOR_KEY;
                }
            }
        }
    }
    MMAY_EXPECT(mmay_bytecode_execution_init(&execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()));
    run.execution = &execution;
    vm_status = mmay_bytecode_run_program(&decoded, NULL, 0, mmay_system_allocator(), run, &result);
    MMAY_EXPECT(valid ? vm_status == MMAY_BYTECODE_RUN_OK
        : vm_status == (protocol != 1 ? MMAY_BYTECODE_RUN_INVALID_REFERENCE
            : cursor >= 0 ? MMAY_BYTECODE_RUN_PANIC : MMAY_BYTECODE_RUN_INVALID_BYTECODE));
    if (valid) MMAY_EXPECT(result.as.i64 == expected_key);
#if !defined(_WIN32)
    if (cursor == 0) {
        MarimayNativeCProgram native = {0};
        MarimayOwnedBytes generated = {0};
        native.functions = functions;
        native.function_count = count;
        native.entry_symbol = entry->mir.symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(&native, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &generated) == MMAY_NATIVE_C_EMIT_OK);
        mmay_cursor_native_batch(&generated, probe->mir.symbol, is_set, protocol);
        mmay_owned_bytes_dispose(&generated);
    }
#endif
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_program_dispose(&decoded);
    mmay_bytecode_program_dispose(&program);
    mmay_owned_bytes_dispose(&encoded);
cleanup:
    mmay_check_result_dispose(&checked);
    mmay_checker_import_fixture_dispose(&fixture);
}

void mmay_test_collection_cursor(void) {
    size_t kind, protocol, index;
    for (kind = 0; kind < 2; kind += 1U) {
        for (protocol = 0; protocol < 3; protocol += 1U) {
            for (index = 0; index < sizeof(mmay_cursor_inputs) / sizeof(mmay_cursor_inputs[0]); index += 1U) {
                mmay_cursor_wire_case(kind != 0, (unsigned int)protocol, mmay_cursor_inputs[index]);
            }
        }
    }
}
