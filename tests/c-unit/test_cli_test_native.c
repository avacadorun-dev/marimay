#include "test_support.h"
#include "cli_test_native.h"

#include <stdio.h>
#include <string.h>

#include "mmay/runtime/program_arguments.h"
#include "mmay/cli.h"
#include "mmay/session.h"

static void mmay_test_native_plan_symbols(void) {
    MarimayCheckResult checked = {0};
    MarimayVector symbols = {0};
    MarimayCheckedFunction function = {0};
    MarimayFailAllocator state;
    MarimayAllocator allocator;
    char selector[MMAY_CLI_TEST_SELECTOR_BYTES];
    size_t index;
    bool success = false;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    allocator = mmay_fail_allocator(&state);
    checked.initialized = true;
    MMAY_EXPECT(mmay_vector_init(&checked.functions, allocator, sizeof(function)));
    for (index = 20; index > 0; index -= 1) {
        function.is_test = index != 10;
        function.mir.symbol = (MarimaySymbolId){{(unsigned char)index}};
        MMAY_EXPECT(mmay_vector_push(&checked.functions, &function));
    }
    /* Only the output table allocates after injection starts. */
    for (index = 0; index < 100; index += 1) {
        size_t position;
        state.attempts = 0;
        state.failure_index = index;
        success = mmay_cli_test_native_symbols(&checked, allocator, &symbols);
        if (!success) {
            MMAY_EXPECT(state.attempts > index && symbols.data == NULL);
            continue;
        }
        MMAY_EXPECT(index >= state.attempts && symbols.length == 19);
        for (position = 0; position < symbols.length; position += 1) {
            char expected[MMAY_CLI_TEST_SELECTOR_BYTES];
            const MarimaySymbolId *symbol = mmay_vector_at_const(&symbols, position);
            unsigned char expected_id = (unsigned char)(position < 9 ? position + 1 : position + 2);
            MMAY_EXPECT(symbol != NULL && symbol->bytes[0] == expected_id);
            (void)snprintf(expected, sizeof(expected), "%zu", position);
            MMAY_EXPECT(symbol != NULL && mmay_cli_test_native_selector(&symbols, *symbol, selector));
            MMAY_EXPECT(strcmp(expected, selector) == 0);
        }
        MMAY_EXPECT(!mmay_cli_test_native_selector(&symbols, (MarimaySymbolId){{10}}, selector));
        MMAY_EXPECT(!mmay_cli_test_native_selector(&symbols, (MarimaySymbolId){{99}}, selector));
        break;
    }
    MMAY_EXPECT(success);
    state.failure_index = SIZE_MAX;
    /* Duplicate symbols reject and release an already owned table. */
    function.is_test = true;
    MMAY_EXPECT(mmay_vector_push(&checked.functions, &function));
    MMAY_EXPECT(!mmay_cli_test_native_symbols(&checked, allocator, &symbols));
    MMAY_EXPECT(symbols.data == NULL);
    mmay_vector_dispose(&checked.functions);
    MMAY_EXPECT(mmay_vector_init(&checked.functions, allocator, sizeof(function)));
    MMAY_EXPECT(mmay_cli_test_native_symbols(&checked, allocator, &symbols));
    MMAY_EXPECT(symbols.length == 0 && symbols.data == NULL);
    MMAY_EXPECT(!mmay_cli_test_native_selector(&symbols, function.mir.symbol, selector));
    checked.initialized = false;
    MMAY_EXPECT(!mmay_cli_test_native_symbols(&checked, allocator, &symbols));
    MMAY_EXPECT(!mmay_cli_test_native_symbols(NULL, allocator, &symbols));
    mmay_vector_dispose(&symbols);
    mmay_vector_dispose(&checked.functions);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static void mmay_test_native_plan_arguments(void) {
    static const char *maximum[MMAY_PROGRAM_ARGUMENT_MAX_COUNT];
    static const char *const values[] = {"", "Мир", "two words", "--", "-dash", "C:\\trail\\"};
    static const char *const invalid[] = {"valid", NULL};
    size_t failure;
    size_t index;
    bool success = false;
    for (index = 0; index < MMAY_PROGRAM_ARGUMENT_MAX_COUNT; index += 1) maximum[index] = "";
    for (failure = 0; failure < 100; failure += 1) {
        MarimayFailAllocator state;
        MarimayVector arguments = {0};
        const char *const *argv;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        success = mmay_cli_test_native_arguments(mmay_fail_allocator(&state),
            "artifact", "17", values, sizeof(values) / sizeof(values[0]), &arguments);
        if (success) {
            MMAY_EXPECT(failure >= state.attempts && arguments.length == 9);
            argv = arguments.data;
            MMAY_EXPECT(strcmp(argv[0], "artifact") == 0 && strcmp(argv[1], "17") == 0);
            for (index = 0; index < 6; index += 1) MMAY_EXPECT(argv[index + 2] == values[index]);
            MMAY_EXPECT(argv[8] == NULL);
            state.failure_index = SIZE_MAX;
            MMAY_EXPECT(mmay_cli_test_native_arguments(mmay_fail_allocator(&state),
                "artifact", "0", maximum, MMAY_PROGRAM_ARGUMENT_MAX_COUNT, &arguments));
            MMAY_EXPECT(arguments.length == MMAY_PROGRAM_ARGUMENT_MAX_COUNT + 3);
            MMAY_EXPECT(!mmay_cli_test_native_arguments(mmay_fail_allocator(&state),
                "artifact", "0", maximum, MMAY_PROGRAM_ARGUMENT_MAX_COUNT + 1, &arguments));
            MMAY_EXPECT(arguments.data == NULL);
            MMAY_EXPECT(!mmay_cli_test_native_arguments(mmay_fail_allocator(&state),
                "artifact", "0", invalid, 2, &arguments));
            MMAY_EXPECT(arguments.data == NULL);
            MMAY_EXPECT(!mmay_cli_test_native_arguments(mmay_fail_allocator(&state),
                NULL, "0", NULL, 0, &arguments));
            MMAY_EXPECT(!mmay_cli_test_native_arguments(mmay_fail_allocator(&state),
                "artifact", NULL, NULL, 0, &arguments));
            MMAY_EXPECT(!mmay_cli_test_native_arguments(mmay_fail_allocator(&state),
                "artifact", "0", NULL, 1, &arguments));
            MMAY_EXPECT(mmay_cli_test_native_arguments(mmay_fail_allocator(&state),
                "artifact", "0", NULL, 0, &arguments));
            MMAY_EXPECT(arguments.length == 3);
        } else MMAY_EXPECT(state.attempts > failure && arguments.data == NULL);
        mmay_vector_dispose(&arguments);
        mmay_vector_dispose(&arguments);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (success) break;
    }
    MMAY_EXPECT(success);
}

static void mmay_test_native_boundary_processes(void) {
    static const char *arguments[MMAY_PROGRAM_ARGUMENT_MAX_COUNT + 6];
    static const char *const values[] = {"", "Мир", "two words", "--", "-dash", "C:\\trail\\"};
    static const char expected[] =
        "test local/native-tests.firstMutates ... ok\n"
        "test local/native-tests.noArguments ... ok\n"
        "test local/native-tests.lastHasFreshArguments ... ok\n"
        "test result: ok. 3 passed; 0 failed\n";
    MarimayFailAllocator state;
    MarimaySession session;
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    size_t run;
    size_t index;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    mmay_session_init(&session, mmay_fail_allocator(&state));
    arguments[0] = mmay_test_toolchain_executable();
    arguments[1] = "test";
    arguments[2] = "checkpoint11-native";
    arguments[3] = "--engine";
    arguments[4] = "both";
    arguments[5] = "--";
    for (run = 0; run < 3; run += 1) {
        size_t count = run == 1 ? MMAY_PROGRAM_ARGUMENT_MAX_COUNT : 6;
        for (index = 0; index < count; index += 1) arguments[index + 6] = run == 1 ? "" : values[index];
        mmay_test_buffer_init(&output);
        mmay_test_buffer_init(&error);
        if (run == 2) output.failure_index = 0;
        MMAY_EXPECT(mmay_cli_run(&session, (int)(count + 6), arguments,
            (MarimayReadStream){0}, mmay_test_buffer_writer(&output), mmay_test_buffer_writer(&error))
            == (run == 2 ? MMAY_EXIT_INTERNAL : MMAY_EXIT_SUCCESS));
        if (run != 2) MMAY_EXPECT(output.length == sizeof(expected) - 1
            && memcmp(output.bytes, expected, sizeof(expected) - 1) == 0);
        MMAY_EXPECT(error.length == 0);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    arguments[2] = "checkpoint11-alloc";
    mmay_test_buffer_init(&output);
    mmay_test_buffer_init(&error);
    MMAY_EXPECT(mmay_cli_run(&session, 5, arguments, (MarimayReadStream){0},
        mmay_test_buffer_writer(&output), mmay_test_buffer_writer(&error)) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strcmp(output.bytes, "test result: ok. 0 passed; 0 failed\n") == 0 && error.length == 0);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

void mmay_test_cli_test_native(void) {
    mmay_test_native_plan_symbols();
    mmay_test_native_plan_arguments();
    mmay_test_native_boundary_processes();
}
