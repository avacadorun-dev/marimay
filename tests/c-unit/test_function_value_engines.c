#include "test_checker_imports_support.h"
#include "test_native_c_emitter_support.h"
#include "mmay/bytecode/codec.h"
#include "cli_test_locations.h"

static const char source[] =
    "fn unused(f:(bool)=>string):string{return f(true); }\n"
    "fn factory():()=>[]()=>int {let n=1;return ():[]()=>int=>[\n"
    " ():int=>{n+=1;return n;},():int=>n];}\n"
    "fn main():int {const a=factory()();const x=a[0]();const y=a[1]();\n"
    " const step=():void=>{const _=a[0]();};step();\n"
    " let f:()=>int=():int=>0;const cycle=():int=>{const _=f;return 0;};f=cycle;\n"
    " return x*100+y*10+a[1]()+f();}\n";

static void run_failures(const MarimayCheckResult *checked, size_t entry_index,
    const MarimayBytecodeProgram *program) {
    for (unsigned engine = 0; engine < 2; engine += 1) {
        bool completed = false;
        for (size_t failure = 0; failure < 4096 && !completed; failure += 1) {
            MarimayFailAllocator state;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
            MarimayAllocator allocator = mmay_fail_allocator(&state);
            if (engine == 0) {
                MarimayMirValue result = {0};
                MarimayMirInterpretStatus status = mmay_check_interpret_function(checked, entry_index,
                    NULL, 0, allocator, mmay_check_interpret_default_options(), &result);
                completed = status == MMAY_MIR_INTERPRET_OK;
                if (!completed && status != MMAY_MIR_INTERPRET_ALLOCATION_FAILED)
                    fprintf(stderr, "callable MIR index %zu status %d\n",failure,(int)status);
                MMAY_EXPECT(completed || status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
                if (completed) MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 223);
            } else {
                MarimayBytecodeExecution execution = {0};
                MarimayBytecodeExecutionOptions options = mmay_bytecode_execution_default_options();
                options.managed_heap.collect_before_allocation = true;
                if (mmay_bytecode_execution_init(&execution, allocator, options)) {
                    MarimayBytecodeRunOptions run = mmay_bytecode_run_default_options();
                    MarimayBytecodeValue result = {0};
                    run.execution = &execution;
                    MarimayBytecodeRunStatus status = mmay_bytecode_run_program(program, NULL, 0,
                        allocator, run, &result);
                    completed = status == MMAY_BYTECODE_RUN_OK;
                    if (!completed && status != MMAY_BYTECODE_RUN_ALLOCATION_FAILED)
                        fprintf(stderr, "callable VM index %zu status %d\n",failure,(int)status);
                    MMAY_EXPECT(completed || status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
                    if (completed) MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 223);
                }
                mmay_bytecode_execution_dispose(&execution);
            }
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        }
        MMAY_EXPECT(completed);
    }
}

static void source_location_failures(const MarimayCheckResult *checked) {
    bool completed = false;
    for (size_t failure = 0; failure < 1024 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayVector locations = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        completed = mmay_cli_test_locations_prepare(checked, mmay_fail_allocator(&state), &locations);
        if (completed) {
            size_t synthetic = 0;
            for (size_t i = 0; i < locations.length; i += 1) {
                const MarimayNativePanicLocation *location = mmay_vector_at_const(&locations, i);
                if (mmay_checked_symbol_by_id(checked, location->symbol) == NULL) {
                    synthetic += 1;
                    MMAY_EXPECT(location->line != 0 && location->column != 0);
                    MMAY_EXPECT(location->path_length == strlen("app/main.my")
                        && memcmp(location->path, "app/main.my", location->path_length) == 0);
                }
            }
            MMAY_EXPECT(synthetic > 0);
        }
        mmay_vector_dispose(&locations);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
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

static void native_failures(const MarimayOwnedBytes *generated, MarimaySymbolId entry) {
#if !defined(_WIN32)
    static const char prefix[] = "#define main mmay_callable_original_main\n";
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
        "    mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);\n"
        "    x.allocator = mmay_fail_allocator(&state); options.collect_before_allocation = true;\n"
        "    panic.frames = frames; panic.frame_capacity = MMAY_G_MAX_FRAMES;\n"
        "    panic.suppressed = suppressed; panic.suppressed_capacity = MMAY_G_MAX_SUPPRESSED;\n"
        "    if (mmay_managed_heap_init(&x.heap, x.allocator, options)) {\n"
        "      status = mmay_g_managed_status(mmay_g_register_immediate_enums(&x.heap));\n"
        "      if (status == 0) { status = mmay_g_f_%s(&x, &panic, &value, &start, &end); }\n"
        "    }\n"
        "    completed = status == 0;\n"
        "    bool ok = completed ? value == 223 : status == mmay_g_managed_status(MMAY_MANAGED_ALLOCATION_FAILED);\n"
        "    mmay_g_execution_dispose(&x);\n"
        "    if (!ok || state.live_allocations != 0 || state.live_bytes != 0) {\n"
        "      fprintf(stderr, \"callable failure=%%zu status=%%d value=%%lld\\n\", failure, status, (long long)value);\n"
        "      return 5;\n"
        "    }\n"
        "  }\n"
        "  if (!completed) { return 6; }\n"
        "  puts(\"callable lifetime: ok\"); return 0;\n"
        "}\n";
    char symbol[33], suffix[4096];
    MarimayOwnedBytes combined = {0};
    MMAY_EXPECT(mmay_test_native_hex_bytes(entry.bytes, 16, symbol, sizeof(symbol)));
    int written = snprintf(suffix, sizeof(suffix), harness, symbol);
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
    mmay_test_native_compile_and_run(&combined, "callable lifetime: ok\n", 0);
    mmay_owned_bytes_dispose(&combined);
#else
    (void)generated; (void)entry;
#endif
}

void mmay_test_function_value_engines(void) {
    const MarimayTestImportSource input = {"test/callable-engines", "app/main.my", source};
    MarimayCheckerImportFixture fixture = {0};
    MarimayCheckResult checked = {0};
    MarimayBytecodeProgram program = {0}, decoded = {0};
    MarimayOwnedBytes encoded = {0}, generated = {0};
    const MarimayMirFunction *functions[64];
    size_t entry_index = SIZE_MAX;
    MMAY_EXPECT(mmay_checker_import_fixture_init(&fixture, &input, 1));
    MMAY_EXPECT(mmay_checker_import_fixture_check(&fixture, "test/callable-engines",
        mmay_system_allocator(), mmay_check_graph_default_options(), &checked) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.diagnostics.length == 0);
    const MarimayCheckedFunction *entry = mmay_checker_import_function(&checked, "main", &entry_index);
    MMAY_EXPECT(entry != NULL && checked.functions.length <= 64);
    if (entry == NULL || checked.functions.length > 64) goto cleanup;
    size_t indirect = 0, captures = 0;
    for (size_t i = 0; i < checked.functions.length; i += 1) {
        MarimayCheckedFunction *function = mmay_vector_at(&checked.functions, i);
        functions[i] = &function->mir;
        MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
        for (size_t n = 0; n < function->mir.calls.length; n += 1) {
            MarimayMirCall *call = mmay_vector_at(&function->mir.calls, n);
            if (call->interface_slot == UINT32_MAX) continue;
            uint32_t saved = call->interface_slot;
            call->interface_slot = 1;
            MMAY_EXPECT(mmay_mir_verify(&function->mir) != MMAY_MIR_VERIFY_OK);
            call->interface_slot = saved; indirect += 1;
        }
        for (size_t n = 0; n < function->mir.interface_constructions.length; n += 1) {
            MarimayMirInterfaceConstruction *construction = mmay_vector_at(&function->mir.interface_constructions, n);
            MarimayMirRegister saved = construction->payload;
            construction->payload = MMAY_MIR_NO_REGISTER;
            MMAY_EXPECT(mmay_mir_verify(&function->mir) != MMAY_MIR_VERIFY_OK);
            construction->payload = saved; captures += 1;
        }
    }
    MMAY_EXPECT(indirect >= 5 && captures >= 5);
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(functions, checked.functions.length, entry->mir.symbol,
        mmay_system_allocator(), mmay_mir_bytecode_program_lower_default_options(), &program) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_encode_program(&program, mmay_system_allocator(), &encoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_program(encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_program_default_options(), &decoded) == MMAY_BYTECODE_CODEC_OK);
    source_location_failures(&checked);
    wire_failures(functions, checked.functions.length, entry->mir.symbol, &program, &encoded);
    for (size_t i = 0; i < decoded.functions.length; i += 1) {
        MarimayBytecodeProgramFunction *function = mmay_vector_at(&decoded.functions, i);
        for (size_t c = 0; c < function->body.calls.length; c += 1) {
            MarimayBytecodeCall *call = mmay_vector_at(&function->body.calls, c);
            if (call->interface_slot == UINT32_MAX) continue;
            call->interface_slot = 1;
            MMAY_EXPECT(mmay_bytecode_program_verify(&decoded) != MMAY_BYTECODE_PROGRAM_VERIFY_OK);
            call->interface_slot = 0;
        }
    }
    run_failures(&checked, entry_index, &decoded);
    MarimayNativeCProgram native = {0};
    native.functions = functions; native.function_count = checked.functions.length; native.entry_symbol = entry->mir.symbol;
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
#if !defined(_WIN32)
    mmay_test_native_compile_and_run(&generated, "ok:i64:223\n", 0);
#endif
    native_failures(&generated, entry->mir.symbol);
cleanup:
    mmay_owned_bytes_dispose(&generated);
    mmay_bytecode_program_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&program);
    mmay_check_result_dispose(&checked);
    mmay_checker_import_fixture_dispose(&fixture);
}
