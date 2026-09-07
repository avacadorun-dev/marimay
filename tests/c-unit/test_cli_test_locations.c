#include "test_checker_support.h"
#include "test_native_c_emitter_support.h"
#include "cli_test_locations.h"

static const char location_source[] =
    "\xef\xbb\xbf// UTF-8 🙂\r\n"
    "fn same<T>(value: T, other: T): void {\r\n"
    "  const label = \"Я🙂\"; expect.equal(value, other);\r\n"
    "}\r\n"
    "@test fn fails(): void { same(1, 2); }\r\n";
static const char location_path[] = "tests/Мир с пробелом/helper_test.my";
static const char location_text[] = "  --> tests/Мир с пробелом/helper_test.my:3:23-3:49\n";

static void mmay_test_location_prepare_failures(const MarimayCheckResult *checked, bool valid) {
    size_t failure;
    bool complete = false;
    for (failure = 0; failure < 1024; failure += 1) {
        MarimayFailAllocator state;
        MarimayVector locations = {0};
        bool ready;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        ready = mmay_cli_test_locations_prepare(checked, mmay_fail_allocator(&state), &locations);
        complete = failure >= state.attempts;
        MMAY_EXPECT(ready == (complete && valid));
        if (ready) MMAY_EXPECT(failure == state.attempts && locations.length != 0);
        else MMAY_EXPECT(locations.data == NULL);
        mmay_vector_dispose(&locations);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (complete) break;
    }
    MMAY_EXPECT(complete);
}

static void mmay_test_location_emission_failures(const MarimayNativeCProgram *program,
    MarimayNativeCEmitOptions options) {
    size_t failure;
    bool complete = false;
    for (failure = 0; failure < 4096; failure += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes emitted = {0};
        MarimayNativeCEmitStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        status = mmay_native_c_emit_program(program, mmay_fail_allocator(&state), options, &emitted);
        if (status == MMAY_NATIVE_C_EMIT_OK) {
            MMAY_EXPECT(failure == state.attempts && emitted.length != 0);
            complete = true;
        } else {
            MMAY_EXPECT(status == MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED);
            MMAY_EXPECT(state.attempts > failure && emitted.data == NULL);
        }
        mmay_owned_bytes_dispose(&emitted);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (complete) break;
    }
    MMAY_EXPECT(complete);
}

static void mmay_test_location_bad_metadata(const MarimayNativeCProgram *program,
    MarimayNativeCEmitOptions options) {
    size_t test, failure;
    for (test = 0; test < 16; test += 1) {
        bool complete = false;
        for (failure = 0; failure < 128; failure += 1) {
            MarimayFailAllocator state;
            MarimayOwnedBytes emitted = {0};
            MarimayNativePanicLocation invalid[2] = {options.panic_locations[0], options.panic_locations[0]};
            MarimayNativeCEmitOptions candidate = options;
            MarimayNativeCEmitStatus status;
            candidate.panic_locations = invalid;
            candidate.panic_location_count = 1;
            switch (test) {
                case 0: candidate.panic_locations = NULL; break;
                case 1: candidate.panic_location_count = 0; break;
                case 2: candidate.executable_entry = false; break;
                case 3: invalid[0].path = NULL; break;
                case 4: invalid[0].path_length = 0; break;
                case 5: invalid[0].path = "\xff"; invalid[0].path_length = 1; break;
                case 6: invalid[0].path = "a\0b"; invalid[0].path_length = 3; break;
                case 7: invalid[0].end = invalid[0].start; break;
                case 8: invalid[0].line = 0; break;
                case 9: invalid[0].symbol = (MarimaySymbolId){{0}}; break;
                case 10: candidate.panic_location_count = 2; break;
                case 11: invalid[0].end_column = 0; break;
                case 12: invalid[0].end_line = invalid[0].line - 1; break;
                case 13: invalid[0].end_line = invalid[0].line; invalid[0].end_column = invalid[0].column; break;
                case 14:
                    candidate.panic_location_count = 2;
                    invalid[1].start += 1; invalid[1].end += 1;
                    invalid[1].path = "other"; invalid[1].path_length = 5;
                    break;
                case 15:
                    candidate.panic_location_count = 2;
                    invalid[0].start += 1; invalid[0].end += 1;
                    break;
            }
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
            status = mmay_native_c_emit_program(program, mmay_fail_allocator(&state), candidate, &emitted);
            complete = failure >= state.attempts;
            MMAY_EXPECT(status == (complete ? MMAY_NATIVE_C_EMIT_INVALID_ARGUMENT
                : MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED));
            MMAY_EXPECT(emitted.data == NULL);
            mmay_owned_bytes_dispose(&emitted);
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
            if (complete) break;
        }
        MMAY_EXPECT(complete);
    }
}

void mmay_test_cli_test_locations(void) {
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked = {0};
    MarimayFailAllocator tracked;
    MarimayAllocator allocator;
    MarimayVector locations = {0};
    MarimayOwnedBytes emitted = {0};
    MarimayNativeCEmitOptions options = mmay_native_c_emit_default_options();
    const MarimayMirFunction *functions[2] = {0};
    MarimayNativeCProgram program = {0};
    MarimayTestBuffer rendered;
    size_t index, start = (size_t)(strstr(location_source, "expect.equal(value, other)") - location_source);
    size_t end = start + strlen("expect.equal(value, other)");
    const MarimayNativePanicLocation *leaf = NULL;
    bool ready;
    mmay_fail_allocator_init(&tracked, mmay_system_allocator(), SIZE_MAX);
    allocator = mmay_fail_allocator(&tracked);
    MMAY_EXPECT(mmay_checker_fixture_init_path(&fixture, location_path, location_source));
    ready = mmay_check_ast(&fixture.ast, "test/locations", allocator,
        mmay_check_default_options(), &checked) == MMAY_CHECK_OK
        && checked.diagnostics.length == 0 && checked.functions.length == 2;
    MMAY_EXPECT(ready);
    if (!ready) goto cleanup;
    MMAY_EXPECT(mmay_cli_test_locations_prepare(&checked, allocator, &locations));
    for (index = 0; index < locations.length; index += 1) {
        const MarimayNativePanicLocation *location = mmay_vector_at_const(&locations, index);
        if (location->start == start && location->end == end) leaf = location;
    }
    MMAY_EXPECT(leaf != NULL);
    if (leaf == NULL) goto cleanup;
    mmay_test_buffer_init(&rendered);
    MMAY_EXPECT(mmay_cli_test_location_write(&locations, leaf->symbol, start, end,
        mmay_test_buffer_writer(&rendered)));
    MMAY_EXPECT(rendered.length == sizeof(location_text) - 1
        && memcmp(rendered.bytes, location_text, rendered.length) == 0);
    for (index = 0; index < rendered.calls; index += 1) {
        MarimayTestBuffer failing;
        mmay_test_buffer_init(&failing); failing.failure_index = index;
        MMAY_EXPECT(!mmay_cli_test_location_write(&locations, leaf->symbol, start, end,
            mmay_test_buffer_writer(&failing)));
    }
    MMAY_EXPECT(!mmay_cli_test_location_write(&locations, leaf->symbol, start, end + 1,
        mmay_test_buffer_writer(&rendered)));
    mmay_test_location_prepare_failures(&checked, true);
    {
        MarimayCheckedSymbol *fact = (MarimayCheckedSymbol *)mmay_checked_symbol_by_id(
            &checked, mmay_checked_function(&checked, 1)->mir.symbol);
        const MarimaySourceSnapshot *saved = fact->snapshot;
        fact->snapshot = NULL;
        mmay_test_location_prepare_failures(&checked, false);
        fact->snapshot = saved;
    }
    for (index = 0; index < 2; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(&checked, index);
        functions[index] = &function->mir;
        if (function->is_test) program.entry_symbol = function->mir.symbol;
    }
    program.functions = functions; program.function_count = 2;
    options.executable_entry = true;
    options.panic_locations = locations.data;
    options.panic_location_count = locations.length;
    mmay_test_location_emission_failures(&program, options);
    mmay_test_location_bad_metadata(&program, options);
    MMAY_EXPECT(mmay_native_c_emit_program(&program, allocator, options, &emitted) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (emitted.data != NULL) mmay_test_native_compile_and_run(&emitted,
        "panic[G4004]: expect.equal failed: values differ at root\n"
        "  --> tests/Мир с пробелом/helper_test.my:3:23-3:49\n", 1);
#endif
    /* A failed re-preparation must release a previously owned table. */
    MMAY_EXPECT(!mmay_cli_test_locations_prepare(NULL, allocator, &locations));
    MMAY_EXPECT(locations.data == NULL);
cleanup:
    mmay_owned_bytes_dispose(&emitted);
    mmay_vector_dispose(&locations);
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
    MMAY_EXPECT(tracked.live_allocations == 0 && tracked.live_bytes == 0);
}
