#include "test_checker_imports_support.h"
#include "test_native_c_emitter_support.h"
#include "test_file_line_host.h"
#include "mmay/bytecode/codec.h"

static const char mmay_cp34_fs_source[] =
    "public enum Error {\n"
    "  CapabilityDenied { capability: string, path: string, },\n"
    "  InvalidLimit { limit: int, }, LimitExceeded { limit: int, },\n"
    "  NotFound, NotFile, NotDirectory, Symlink, Closed, OpenFailed,\n"
    "  ReadFailed, WriteFailed, SyncFailed, CloseFailed, ReplaceFailed,\n"
    "  InvalidUtf8 { offset: int, }, TooManyEntries { limit: int, },\n"
    "  DirectoryFailed,\n"
    "}\n"
    "extern \"c\" fn mmayFsReadLine(handle: int, limit: int): Result<Option<string>, Error>;\n"
    "extern \"c\" fn mmayFsCloseBestEffort(handle: int): void;\n"
    "fn close(): void { mmayFsCloseBestEffort(1); }\n"
    "fn main(): int {\n"
    "  defer close();\n"
    "  match mmayFsReadLine(1, -1) { Err(Error.InvalidLimit { limit }) => { if limit != -1 { return -1; } }, _ => { return -2; }, };\n"
    "  match mmayFsReadLine(0, 2) { Err(Error.Closed) => {}, _ => { return -3; }, };\n"
    "  let count = 0;\n"
    "  while true {\n"
    "    const line = match mmayFsReadLine(1, 2) {\n"
    "      Ok(value) => value,\n"
    "      Err(Error.InvalidUtf8 { offset }) => { return 10 + offset; },\n"
    "      Err(Error.ReadFailed) => { return 20; },\n"
    "      Err(_) => { return -4; },\n"
    "    };\n"
    "    if const text = line {\n"
    "      if count == 0 && text != \"\" { return -5; }\n"
    "      if count == 1 && text != \"é\" { return -6; }\n"
    "      if count == 2 && text != \"z\" { return -7; }\n"
    "      count += 1;\n"
    "    } else { break; }\n"
    "  }\n"
    "  return count;\n"
    "}\n";

static void run_failures(const MarimayCheckResult *checked, size_t entry_index,
    const MarimayBytecodeProgram *program, int64_t expected, unsigned scenario) {
    for (unsigned engine = 0; engine < 2; engine += 1) {
        bool completed = false;
        for (size_t failure = 0; failure < 4096 && !completed; failure += 1) {
            MarimayFailAllocator state;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
            MarimayAllocator allocator = mmay_fail_allocator(&state);
            MmayLineHost host = {0, scenario};
            if (engine == 0) {
                MarimayMirExecution execution = {0};
                MarimayMirValue result = {0};
                MarimayMirInterpretStatus status = MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
                if (mmay_mir_execution_init(&execution, allocator, mmay_mir_execution_default_options())) {
                    execution.fs_initialized = true;
                    if (mmay_line_host_init(&execution.fs, allocator, &host)) {
                        MarimayCheckInterpretOptions options = mmay_check_interpret_default_options();
                        options.execution = &execution;
                        status = mmay_check_interpret_function(checked, entry_index, NULL, 0, allocator, options, &result);
                    }
                }
                mmay_mir_execution_dispose(&execution);
                completed = status == MMAY_MIR_INTERPRET_OK;
                if (!completed && status != MMAY_MIR_INTERPRET_ALLOCATION_FAILED)
                    fprintf(stderr, "file-line MIR index %zu status %d\n",failure,(int)status);
                MMAY_EXPECT(completed || status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
                if (completed) MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == expected);
            } else {
                MarimayBytecodeExecution execution = {0};
                MarimayBytecodeExecutionOptions options = mmay_bytecode_execution_default_options();
                options.managed_heap.collect_before_allocation = true;
                if (mmay_bytecode_execution_init(&execution, allocator, options)) {
                    execution.fs_initialized = true;
                    if (mmay_line_host_init(&execution.fs, allocator, &host)) {
                    MarimayBytecodeRunOptions run = mmay_bytecode_run_default_options();
                    MarimayBytecodeValue result = {0};
                    run.execution = &execution;
                    MarimayBytecodeRunStatus status = mmay_bytecode_run_program(program, NULL, 0,
                        allocator, run, &result);
                    completed = status == MMAY_BYTECODE_RUN_OK;
                    if (!completed && status != MMAY_BYTECODE_RUN_ALLOCATION_FAILED)
                        fprintf(stderr, "file-line VM index %zu status %d\n",failure,(int)status);
                    MMAY_EXPECT(completed || status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
                    if (completed) MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == expected);
                }
                }
                mmay_bytecode_execution_dispose(&execution);
            }
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        }
        MMAY_EXPECT(completed);
    }
}

static void wire_failures(const MarimayMirFunction *const *functions, size_t count,
    MarimaySymbolId entry, const MarimayBytecodeProgram *program, const MarimayOwnedBytes *encoded) {
    for (unsigned phase = 0; phase < 3; phase += 1) {
        bool completed = false;
        for (size_t failure = 0; failure < 8192 && !completed; failure += 1) {
            MarimayFailAllocator state;
            MarimayBytecodeProgram temporary = {0};
            MarimayOwnedBytes bytes = {0};
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
            MarimayAllocator allocator = mmay_fail_allocator(&state);
            if (phase == 0) {
                MarimayMirBytecodeLowerStatus status = mmay_mir_lower_bytecode_program(functions, count,
                    entry, allocator, mmay_mir_bytecode_program_lower_default_options(), &temporary);
                completed = status == MMAY_MIR_BYTECODE_LOWER_OK;
                MMAY_EXPECT(completed || status == MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED);
            } else {
                MarimayBytecodeCodecStatus status = phase == 1
                    ? mmay_bytecode_encode_program(program, allocator, &bytes)
                    : mmay_bytecode_decode_program(encoded->data, encoded->length, allocator,
                        mmay_bytecode_program_default_options(), &temporary);
                completed = status == MMAY_BYTECODE_CODEC_OK;
                MMAY_EXPECT(completed || status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            }
            mmay_owned_bytes_dispose(&bytes);
            mmay_bytecode_program_dispose(&temporary);
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        }
        MMAY_EXPECT(completed);
    }
}

static void native_failures(const MarimayOwnedBytes *generated, MarimaySymbolId entry, int64_t expected, unsigned scenario) {
#if !defined(_WIN32)
    static const char prefix[] = "#include \"" MMAY_TEST_SOURCE_DIR "/tests/c-unit/test_file_line_host.h\"\n#define main mmay_callable_original_main\n";
    static const char harness[] =
        "\n#undef main\n"
        "int main(void) {\n"
        "  bool completed = false; (void)&mmay_callable_original_main; mmay_g_keep_assets();\n"
        "  for (size_t failure = 0; failure < 2048 && !completed; failure += 1) {\n"
        "    MarimayFailAllocator state; mmay_g_execution x = {0};\n"
        "    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();\n"
        "    mmay_g_panic panic = {0}; mmay_g_frame frames[MMAY_G_MAX_FRAMES];\n"
        "    mmay_g_suppressed_panic suppressed[MMAY_G_MAX_SUPPRESSED];\n"
        "    size_t start = 0, end = 0; int64_t value = 0; int status = 5;\n"
        "    MmayLineHost host = {0, %u};\n"
        "    mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);\n"
        "    x.allocator = mmay_fail_allocator(&state); options.collect_before_allocation = true;\n"
        "    panic.frames = frames; panic.frame_capacity = MMAY_G_MAX_FRAMES;\n"
        "    panic.suppressed = suppressed; panic.suppressed_capacity = MMAY_G_MAX_SUPPRESSED;\n"
        "    if (mmay_managed_heap_init(&x.heap, x.allocator, options)) {\n"
        "      status = mmay_g_managed_status(mmay_g_register_immediate_enums(&x.heap));\n"
        "      if (status == 0) { x.fs_initialized = true; if (!mmay_line_host_init(&x.fs, x.allocator, &host)) status = 5; }\n"
        "      if (status == 0) { status = mmay_g_f_%s(&x, &panic, &value, &start, &end); }\n"
        "    }\n"
        "    completed = status == 0;\n"
        "    bool ok = completed ? value == %lld : status == mmay_g_managed_status(MMAY_MANAGED_ALLOCATION_FAILED);\n"
        "    mmay_g_execution_dispose(&x);\n"
        "    if (!ok || state.live_allocations != 0 || state.live_bytes != 0) {\n"
        "      fprintf(stderr, \"file-line failure=%%zu status=%%d value=%%lld\\n\", failure, status, (long long)value);\n"
        "      return 5;\n"
        "    }\n"
        "  }\n"
        "  if (!completed) { return 6; }\n"
        "  puts(\"file-line lifetime: ok\"); return 0;\n"
        "}\n";
    char symbol[33], suffix[4096];
    MarimayOwnedBytes combined = {0};
    MMAY_EXPECT(mmay_test_native_hex_bytes(entry.bytes, 16, symbol, sizeof(symbol)));
    int written = snprintf(suffix, sizeof(suffix), harness, scenario, symbol, (long long)expected);
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(suffix));
    if (written <= 0 || (size_t)written >= sizeof(suffix)) return;
    combined.allocator = mmay_system_allocator();
    combined.length = sizeof(prefix) - 1 + generated->length + (size_t)written;
    combined.data = mmay_allocate(&combined.allocator, combined.length + 1);
    MMAY_EXPECT(combined.data != NULL);
    if (combined.data == NULL) return;
    memcpy(combined.data, prefix, sizeof(prefix) - 1);
    memcpy(combined.data + sizeof(prefix) - 1, generated->data, generated->length);
    memcpy(combined.data + sizeof(prefix) - 1 + generated->length, suffix, (size_t)written + 1);
    mmay_test_native_compile_and_run(&combined, "file-line lifetime: ok\n", 0);
    mmay_owned_bytes_dispose(&combined);
#else
    (void)generated; (void)entry; (void)expected; (void)scenario;
#endif
}

void mmay_test_file_line_engines(void) {
    const MarimayTestImportSource input = {"std/fs", "std/fs/fs.my", mmay_cp34_fs_source};
    MarimayCheckerImportFixture fixture = {0};
    MarimayCheckResult checked = {0};
    MarimayBytecodeProgram program = {0}, decoded = {0};
    MarimayOwnedBytes encoded = {0}, generated = {0};
    const MarimayMirFunction *functions[8];
    size_t entry_index = SIZE_MAX, count = 0;
    MMAY_EXPECT(mmay_checker_import_fixture_init(&fixture, &input, 1));
    MMAY_EXPECT(mmay_checker_import_fixture_check(&fixture, "std/fs", mmay_system_allocator(),
        mmay_check_graph_default_options(), &checked) == MMAY_CHECK_OK);
    for (size_t d = 0; d < checked.diagnostics.length; d += 1) {
        const MarimayCheckDiagnostic *diagnostic = mmay_vector_at_const(&checked.diagnostics, d);
        fprintf(stderr, "file lines: %s %s\n", diagnostic->code, diagnostic->message);
    }
    MMAY_EXPECT(checked.diagnostics.length == 0);
    const MarimayCheckedFunction *entry = mmay_checker_import_function(&checked, "main", &entry_index);
    MMAY_EXPECT(entry != NULL && checked.functions.length <= 8);
    if (entry == NULL || checked.functions.length > 8) goto cleanup;
    for (size_t i = 0; i < checked.functions.length; i += 1) {
        const MarimayCheckedFunction *function = mmay_vector_at_const(&checked.functions, i);
        if (!function->is_external) functions[count++] = &function->mir;
    }
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(functions, count, entry->mir.symbol,
        mmay_system_allocator(), mmay_mir_bytecode_program_lower_default_options(), &program) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_encode_program(&program, mmay_system_allocator(), &encoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_program(encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_program_default_options(), &decoded) == MMAY_BYTECODE_CODEC_OK);
    wire_failures(functions, count, entry->mir.symbol, &program, &encoded);
    MarimayNativeCProgram native = {0};
    native.functions = functions; native.function_count = count; native.entry_symbol = entry->mir.symbol;
    bool completed = false;
    for (size_t failure = 0; failure < 8192 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes output = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MarimayNativeCEmitStatus status = mmay_native_c_emit_program(&native, mmay_fail_allocator(&state),
            mmay_native_c_emit_default_options(), &output);
        completed = status == MMAY_NATIVE_C_EMIT_OK;
        MMAY_EXPECT(completed || status == MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED);
        mmay_owned_bytes_dispose(&output);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    MMAY_EXPECT(mmay_native_c_emit_program(&native, mmay_system_allocator(),
        mmay_native_c_emit_default_options(), &generated) == MMAY_NATIVE_C_EMIT_OK);
    const int64_t expected[] = {3, 11, 20};
    for (unsigned scenario = 0; scenario < 3; scenario += 1) {
        run_failures(&checked, entry_index, &decoded, expected[scenario], scenario);
        native_failures(&generated, entry->mir.symbol, expected[scenario], scenario);
    }
cleanup:
    mmay_owned_bytes_dispose(&generated);
    mmay_bytecode_program_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&program);
    mmay_check_result_dispose(&checked);
    mmay_checker_import_fixture_dispose(&fixture);
}
