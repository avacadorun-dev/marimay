#include "test_checker_support.h"
#include "test_native_c_emitter_support.h"

/* Public test output is terse; independently check the complete panic
 * location and call frames retained by the execution engines. */
void mmay_test_native_test_helpers(void) {
    static const char source[] =
        "fn require(value: bool): void { expect.true(value); }\n"
        "@test fn failure(): void { require(false); }\n";
    static const char message[] = "expect.true failed: value is false";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked = {0};
    MarimayFailAllocator tracked;
    MarimayAllocator allocator;
    MarimayBytecodeProgram bytecode = {0};
    MarimayBytecodeExecution execution = {0};
    MarimayBytecodePanic panic = {0};
    MarimayOwnedBytes emitted = {0};
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue result = {0};
    const MarimayMirFunction *functions[2];
    MarimayNativeCProgram program;
    size_t start = (size_t)(strstr(source, "expect.true(value)") - source);
    size_t end = start + strlen("expect.true(value)");
    size_t caller_start = (size_t)(strstr(source, "require(false)") - source);
    size_t caller_end = caller_start + strlen("require(false)");
    bool ready;
    mmay_fail_allocator_init(&tracked, mmay_system_allocator(), SIZE_MAX);
    allocator = mmay_fail_allocator(&tracked);
    MMAY_EXPECT(mmay_checker_fixture_init_path(&fixture, "helper-location_test.my", source));
    ready = mmay_check_ast(&fixture.ast, "test/helper-location", allocator,
        mmay_check_default_options(), &checked) == MMAY_CHECK_OK
        && checked.diagnostics.length == 0 && checked.functions.length == 2;
    MMAY_EXPECT(ready);
    if (!ready) goto cleanup;
    functions[0] = &mmay_checked_function(&checked, 0)->mir;
    functions[1] = &mmay_checked_function(&checked, 1)->mir;
    program = (MarimayNativeCProgram){functions, 2, functions[1]->symbol};
    ready = mmay_mir_lower_bytecode_program(functions, 2, functions[1]->symbol,
        allocator, mmay_mir_bytecode_program_lower_default_options(), &bytecode)
            == MMAY_MIR_BYTECODE_LOWER_OK
        && mmay_bytecode_execution_init(&execution, allocator,
            mmay_bytecode_execution_default_options())
        && mmay_bytecode_panic_init(&panic, allocator);
    MMAY_EXPECT(ready);
    if (!ready) goto cleanup;
    options.execution = &execution;
    options.panic = &panic;
    MMAY_EXPECT(mmay_bytecode_run_program(&bytecode, NULL, 0, allocator,
        options, &result) == MMAY_BYTECODE_RUN_PANIC);
    MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4004") == 0
        && panic.message.length == sizeof(message) - 1 && panic.message.data != NULL
        && memcmp(panic.message.data, message, sizeof(message) - 1) == 0);
    MMAY_EXPECT(mmay_symbol_id_equal(panic.symbol, functions[0]->symbol)
        && panic.span.start == start && panic.span.end == end && panic.frames.length == 2);
    {
        const MarimayBytecodePanicFrame *leaf = mmay_bytecode_panic_frame(&panic, 0);
        const MarimayBytecodePanicFrame *caller = mmay_bytecode_panic_frame(&panic, 1);
        MMAY_EXPECT(leaf != NULL && mmay_symbol_id_equal(leaf->symbol, functions[0]->symbol)
            && leaf->call_span.start == start && leaf->call_span.end == end);
        MMAY_EXPECT(caller != NULL && mmay_symbol_id_equal(caller->symbol, functions[1]->symbol)
            && caller->call_span.start == caller_start && caller->call_span.end == caller_end);
    }
    MMAY_EXPECT(mmay_native_c_emit_program(&program, allocator,
        mmay_native_c_emit_default_options(), &emitted) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (emitted.data != NULL) {
        char helper_symbol[33], caller_symbol[33], message_hex[2 * sizeof(message)];
        char expected[512];
        int length;
        MMAY_EXPECT(mmay_test_native_hex_bytes(functions[0]->symbol.bytes, 16,
            helper_symbol, sizeof(helper_symbol)));
        MMAY_EXPECT(mmay_test_native_hex_bytes(functions[1]->symbol.bytes, 16,
            caller_symbol, sizeof(caller_symbol)));
        MMAY_EXPECT(mmay_test_native_hex_bytes((const unsigned char *)message,
            sizeof(message) - 1, message_hex, sizeof(message_hex)));
        length = snprintf(expected, sizeof(expected),
            "panic:G4004:%s:%zu:%zu:%zu:%s:2:%s:%zu:%zu:%s:%zu:%zu\n",
            helper_symbol, start, end, sizeof(message) - 1, message_hex,
            helper_symbol, start, end, caller_symbol, caller_start, caller_end);
        MMAY_EXPECT(length > 0 && (size_t)length < sizeof(expected));
        if (length > 0 && (size_t)length < sizeof(expected)) {
            mmay_test_native_compile_and_run(&emitted, expected, 1);
        }
    }
#endif
cleanup:
    mmay_owned_bytes_dispose(&emitted);
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_program_dispose(&bytecode);
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
    MMAY_EXPECT(tracked.live_allocations == 0 && tracked.live_bytes == 0);
}
