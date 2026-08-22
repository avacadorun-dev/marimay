#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/cli.h"
#include "mmay/platform/file.h"
#include "mmay/platform/process.h"
#include "mmay/session.h"

static int mmay_cli_heap_run(
    MarimaySession *session,
    int count,
    const char *const arguments[],
    MarimayTestBuffer *output,
    MarimayTestBuffer *error
) {
    mmay_test_buffer_init(output);
    mmay_test_buffer_init(error);
    return mmay_cli_run(
        session, count, arguments, (MarimayReadStream){0},
        mmay_test_buffer_writer(output), mmay_test_buffer_writer(error)
    );
}

static void mmay_cli_heap_invalid_values(MarimaySession *session) {
    static const char *const values[] = {
        "0", "-1", "+1", " 1", "1 ", "one",
        "9223372036854775808", "18446744073709551616"
    };
    const char *arguments[] = {
        "mmay", "run", "cp19-resource-limits", "--max-heap-bytes", NULL
    };
    const char *empty[] = {
        "mmay", "run", "cp19-resource-limits", "--max-heap-bytes="
    };
    const char *missing[] = {
        "mmay", "run", "cp19-resource-limits", "--max-heap-bytes"
    };
    const char *repeated[] = {
        "mmay", "run", "cp19-resource-limits",
        "--max-heap-bytes=1", "--max-heap-bytes", "2"
    };
    const char *json[] = {
        "mmay", "run", "cp19-resource-limits", "--diagnostic", "json",
        "--max-heap-bytes", "-1"
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    size_t index = 0;
    for (index = 0; index < sizeof(values) / sizeof(values[0]); index += 1) {
        arguments[4] = values[index];
        MMAY_EXPECT(mmay_cli_heap_run(
            session, 5, arguments, &output, &error
        ) == MMAY_EXIT_USAGE);
        MMAY_EXPECT(output.length == 0);
        MMAY_EXPECT(strcmp(
            error.bytes,
            "error[U0005]: --max-heap-bytes expects a decimal byte count from 1 to 9223372036854775807\n"
        ) == 0);
    }
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 4, empty, &output, &error
    ) == MMAY_EXIT_USAGE);
    MMAY_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 4, missing, &output, &error
    ) == MMAY_EXIT_USAGE);
    MMAY_EXPECT(strcmp(
        error.bytes,
        "error[U0004]: option '--max-heap-bytes' requires a value\n"
    ) == 0);
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 6, repeated, &output, &error
    ) == MMAY_EXIT_USAGE);
    MMAY_EXPECT(strcmp(
        error.bytes,
        "error[U0005]: --max-heap-bytes may be specified only once\n"
    ) == 0);
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 7, json, &output, &error
    ) == MMAY_EXIT_USAGE);
    MMAY_EXPECT(strstr(error.bytes, "\"code\":\"U0005\"") != NULL);
    MMAY_EXPECT(strstr(
        error.bytes, "\"kind\":\"command-line\",\"argument\":6"
    ) != NULL);
}

static void mmay_cli_heap_help_and_success(MarimaySession *session) {
    const char *help[] = {"mmay", "help", "run"};
    const char *run[] = {
        "mmay", "run", "cp19-resource-limits",
        "--max-heap-bytes", "1048576"
    };
    const char *test[] = {
        "mmay", "test", "cp19-resource-limits", "--engine", "both",
        "--max-heap-bytes=1024"
    };
    const char *maximum[] = {
        "mmay", "run", "cp19-resource-limits",
        "--max-heap-bytes=9223372036854775807"
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 3, help, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strstr(output.bytes, "--max-heap-bytes bytes") != NULL);
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 5, run, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strcmp(output.bytes, "cp19 resource limits ok\n") == 0);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 6, test, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strstr(output.bytes, "2 passed; 0 failed") != NULL);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 4, maximum, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strcmp(output.bytes, "cp19 resource limits ok\n") == 0);
    MMAY_EXPECT(error.length == 0);
}

static void mmay_cli_heap_budget_diagnostics(MarimaySession *session) {
    static const char expected[] =
        "error[I0002]: managed heap budget exceeded: live=0 bytes, requested=328 bytes, limit=1 bytes\n";
    static const char expected_json[] =
        "{\"schema\":\"marimay.diagnostic/1\",\"code\":\"I0002\",\"severity\":\"error\","
        "\"message\":\"managed heap budget exceeded: live=0 bytes, requested=328 bytes, limit=1 bytes\","
        "\"primary\":null,\"labels\":[],\"notes\":[],\"fixes\":[],"
        "\"context\":{\"kind\":\"runtime-resource\",\"resource\":\"managed-heap\","
        "\"classification\":\"budget-exceeded\",\"liveBytes\":0,\"requestedBytes\":328,\"limitBytes\":1}}\n";
    const char *run[] = {
        "mmay", "run", "cp19-resource-limits", "--max-heap-bytes", "1"
    };
    const char *json[] = {
        "mmay", "run", "cp19-resource-limits", "--max-heap-bytes=1",
        "--diagnostic", "json"
    };
    const char *test[] = {
        "mmay", "test", "cp19-resource-limits", "--engine", "both",
        "--max-heap-bytes", "1"
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 5, run, &output, &error
    ) == MMAY_EXIT_INTERNAL);
    MMAY_EXPECT(output.length == 0 && strcmp(error.bytes, expected) == 0);
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 6, json, &output, &error
    ) == MMAY_EXIT_INTERNAL);
    MMAY_EXPECT(output.length == 0 && strcmp(error.bytes, expected_json) == 0);
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 7, test, &output, &error
    ) == MMAY_EXIT_INTERNAL);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strcmp(
        error.bytes,
        "error[I0002]: managed heap budget exceeded: live=0 bytes, requested=328 bytes, limit=1 bytes\n"
    ) == 0);
}

static void mmay_cli_heap_native_and_embedding(MarimaySession *session) {
    static const char expected[] =
        "error[I0002]: managed heap budget exceeded: live=0 bytes, requested=328 bytes, limit=1 bytes\n";
    const char *build[] = {
        "mmay", "build", "cp19-resource-limits", "--max-heap-bytes", "1",
        "--output", "cp19-resource-artifact"
    };
    const char *emit_default[] = {
        "mmay", "build", "run/cp9/void.my", "--emit", "c",
        "--output", "cp19-default.c"
    };
    const char *emit_selected[] = {
        "mmay", "build", "run/cp9/void.my", "--emit", "c",
        "--max-heap-bytes=1048576", "--output", "cp19-selected.c"
    };
    const char *native_arguments[] = {"./cp19-resource-artifact", NULL};
    const char *cache_default[] = {
        "mmay", "build", "run/cp9/void.my"
    };
    const char *cache_selected[] = {
        "mmay", "build", "run/cp9/void.my",
        "--max-heap-bytes", "1048576"
    };
    MarimayTestBuffer output;
    MarimayTestBuffer default_path;
    MarimayTestBuffer selected_path;
    MarimayTestBuffer error;
    MarimayOwnedBytes default_source = {0};
    MarimayOwnedBytes selected_source = {0};
    MarimayProcessCaptureResult capture;
    (void)remove("cp19-resource-artifact");
    (void)remove("cp19-default.c");
    (void)remove("cp19-selected.c");
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 7, build, &output, &error
    ) == MMAY_EXIT_SUCCESS && error.length == 0);
    memset(&capture, 0, sizeof(capture));
    MMAY_EXPECT(mmay_process_run_capture(
        mmay_system_allocator(), native_arguments, 1, 4096, 4096, &capture
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!capture.signaled && capture.exit_code == MMAY_EXIT_INTERNAL);
    MMAY_EXPECT(capture.standard_output.length == 0);
    MMAY_EXPECT(capture.standard_error.length == sizeof(expected) - 1
        && memcmp(capture.standard_error.data, expected, sizeof(expected) - 1)
            == 0);
    mmay_process_capture_result_dispose(&capture);
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 7, emit_default, &output, &error
    ) == MMAY_EXIT_SUCCESS && error.length == 0);
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 8, emit_selected, &output, &error
    ) == MMAY_EXIT_SUCCESS && error.length == 0);
    MMAY_EXPECT(mmay_file_read_all(
        mmay_system_allocator(), "cp19-default.c", strlen("cp19-default.c"),
        2 * 1024 * 1024, &default_source
    ) == MMAY_FILE_OK);
    MMAY_EXPECT(mmay_file_read_all(
        mmay_system_allocator(), "cp19-selected.c", strlen("cp19-selected.c"),
        2 * 1024 * 1024, &selected_source
    ) == MMAY_FILE_OK);
    MMAY_EXPECT(strstr(
        (const char *)default_source.data, "max_live_bytes = 67108864;"
    ) != NULL);
    MMAY_EXPECT(strstr(
        (const char *)selected_source.data, "max_live_bytes = 1048576;"
    ) != NULL);
    MMAY_EXPECT(default_source.length != selected_source.length
        || memcmp(default_source.data, selected_source.data,
            default_source.length) != 0);
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 3, cache_default, &default_path, &error
    ) == MMAY_EXIT_SUCCESS && error.length == 0);
    MMAY_EXPECT(mmay_cli_heap_run(
        session, 5, cache_selected, &selected_path, &error
    ) == MMAY_EXIT_SUCCESS && error.length == 0);
    MMAY_EXPECT(default_path.length != selected_path.length
        || memcmp(default_path.bytes, selected_path.bytes,
            default_path.length) != 0);
    mmay_owned_bytes_dispose(&selected_source);
    mmay_owned_bytes_dispose(&default_source);
    MMAY_EXPECT(remove("cp19-selected.c") == 0);
    MMAY_EXPECT(remove("cp19-default.c") == 0);
    MMAY_EXPECT(remove("cp19-resource-artifact") == 0);
}

void mmay_test_cli_heap(void) {
    MarimaySession session;
    mmay_session_init(&session, mmay_system_allocator());
    mmay_cli_heap_invalid_values(&session);
    mmay_cli_heap_help_and_success(&session);
    mmay_cli_heap_budget_diagnostics(&session);
    mmay_cli_heap_native_and_embedding(&session);
}
