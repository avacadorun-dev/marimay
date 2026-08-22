#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/cli.h"
#include "ruda/platform/file.h"
#include "ruda/platform/process.h"
#include "ruda/session.h"

static int ruda_cli_heap_run(
    RudaSession *session,
    int count,
    const char *const arguments[],
    RudaTestBuffer *output,
    RudaTestBuffer *error
) {
    ruda_test_buffer_init(output);
    ruda_test_buffer_init(error);
    return ruda_cli_run(
        session, count, arguments, (RudaReadStream){0},
        ruda_test_buffer_writer(output), ruda_test_buffer_writer(error)
    );
}

static void ruda_cli_heap_invalid_values(RudaSession *session) {
    static const char *const values[] = {
        "0", "-1", "+1", " 1", "1 ", "one",
        "9223372036854775808", "18446744073709551616"
    };
    const char *arguments[] = {
        "ruda", "run", "cp19-resource-limits", "--max-heap-bytes", NULL
    };
    const char *empty[] = {
        "ruda", "run", "cp19-resource-limits", "--max-heap-bytes="
    };
    const char *missing[] = {
        "ruda", "run", "cp19-resource-limits", "--max-heap-bytes"
    };
    const char *repeated[] = {
        "ruda", "run", "cp19-resource-limits",
        "--max-heap-bytes=1", "--max-heap-bytes", "2"
    };
    const char *json[] = {
        "ruda", "run", "cp19-resource-limits", "--diagnostic", "json",
        "--max-heap-bytes", "-1"
    };
    RudaTestBuffer output;
    RudaTestBuffer error;
    size_t index = 0;
    for (index = 0; index < sizeof(values) / sizeof(values[0]); index += 1) {
        arguments[4] = values[index];
        RUDA_EXPECT(ruda_cli_heap_run(
            session, 5, arguments, &output, &error
        ) == RUDA_EXIT_USAGE);
        RUDA_EXPECT(output.length == 0);
        RUDA_EXPECT(strcmp(
            error.bytes,
            "error[U0005]: --max-heap-bytes expects a decimal byte count from 1 to 9223372036854775807\n"
        ) == 0);
    }
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 4, empty, &output, &error
    ) == RUDA_EXIT_USAGE);
    RUDA_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 4, missing, &output, &error
    ) == RUDA_EXIT_USAGE);
    RUDA_EXPECT(strcmp(
        error.bytes,
        "error[U0004]: option '--max-heap-bytes' requires a value\n"
    ) == 0);
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 6, repeated, &output, &error
    ) == RUDA_EXIT_USAGE);
    RUDA_EXPECT(strcmp(
        error.bytes,
        "error[U0005]: --max-heap-bytes may be specified only once\n"
    ) == 0);
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 7, json, &output, &error
    ) == RUDA_EXIT_USAGE);
    RUDA_EXPECT(strstr(error.bytes, "\"code\":\"U0005\"") != NULL);
    RUDA_EXPECT(strstr(
        error.bytes, "\"kind\":\"command-line\",\"argument\":6"
    ) != NULL);
}

static void ruda_cli_heap_help_and_success(RudaSession *session) {
    const char *help[] = {"ruda", "help", "run"};
    const char *run[] = {
        "ruda", "run", "cp19-resource-limits",
        "--max-heap-bytes", "1048576"
    };
    const char *test[] = {
        "ruda", "test", "cp19-resource-limits", "--engine", "both",
        "--max-heap-bytes=600"
    };
    const char *maximum[] = {
        "ruda", "run", "cp19-resource-limits",
        "--max-heap-bytes=9223372036854775807"
    };
    RudaTestBuffer output;
    RudaTestBuffer error;
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 3, help, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strstr(output.bytes, "--max-heap-bytes bytes") != NULL);
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 5, run, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strcmp(output.bytes, "cp19 resource limits ok\n") == 0);
    RUDA_EXPECT(error.length == 0);
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 6, test, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strstr(output.bytes, "2 passed; 0 failed") != NULL);
    RUDA_EXPECT(error.length == 0);
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 4, maximum, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strcmp(output.bytes, "cp19 resource limits ok\n") == 0);
    RUDA_EXPECT(error.length == 0);
}

static void ruda_cli_heap_budget_diagnostics(RudaSession *session) {
    static const char expected[] =
        "error[I0002]: managed heap budget exceeded: live=0 bytes, requested=320 bytes, limit=1 bytes\n";
    static const char expected_json[] =
        "{\"schema\":\"ruda.diagnostic/1\",\"code\":\"I0002\",\"severity\":\"error\","
        "\"message\":\"managed heap budget exceeded: live=0 bytes, requested=320 bytes, limit=1 bytes\","
        "\"primary\":null,\"labels\":[],\"notes\":[],\"fixes\":[],"
        "\"context\":{\"kind\":\"runtime-resource\",\"resource\":\"managed-heap\","
        "\"classification\":\"budget-exceeded\",\"liveBytes\":0,\"requestedBytes\":320,\"limitBytes\":1}}\n";
    const char *run[] = {
        "ruda", "run", "cp19-resource-limits", "--max-heap-bytes", "1"
    };
    const char *json[] = {
        "ruda", "run", "cp19-resource-limits", "--max-heap-bytes=1",
        "--diagnostic", "json"
    };
    const char *test[] = {
        "ruda", "test", "cp19-resource-limits", "--engine", "both",
        "--max-heap-bytes", "1"
    };
    RudaTestBuffer output;
    RudaTestBuffer error;
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 5, run, &output, &error
    ) == RUDA_EXIT_INTERNAL);
    RUDA_EXPECT(output.length == 0 && strcmp(error.bytes, expected) == 0);
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 6, json, &output, &error
    ) == RUDA_EXIT_INTERNAL);
    RUDA_EXPECT(output.length == 0 && strcmp(error.bytes, expected_json) == 0);
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 7, test, &output, &error
    ) == RUDA_EXIT_INTERNAL);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT(strcmp(
        error.bytes,
        "error[I0002]: managed heap budget exceeded: live=0 bytes, requested=320 bytes, limit=1 bytes\n"
    ) == 0);
}

static void ruda_cli_heap_native_and_embedding(RudaSession *session) {
    static const char expected[] =
        "error[I0002]: managed heap budget exceeded: live=0 bytes, requested=320 bytes, limit=1 bytes\n";
    const char *build[] = {
        "ruda", "build", "cp19-resource-limits", "--max-heap-bytes", "1",
        "--output", "cp19-resource-artifact"
    };
    const char *emit_default[] = {
        "ruda", "build", "run/cp9/void.ruda", "--emit", "c",
        "--output", "cp19-default.c"
    };
    const char *emit_selected[] = {
        "ruda", "build", "run/cp9/void.ruda", "--emit", "c",
        "--max-heap-bytes=1048576", "--output", "cp19-selected.c"
    };
    const char *native_arguments[] = {"./cp19-resource-artifact", NULL};
    const char *cache_default[] = {
        "ruda", "build", "run/cp9/void.ruda"
    };
    const char *cache_selected[] = {
        "ruda", "build", "run/cp9/void.ruda",
        "--max-heap-bytes", "1048576"
    };
    RudaTestBuffer output;
    RudaTestBuffer default_path;
    RudaTestBuffer selected_path;
    RudaTestBuffer error;
    RudaOwnedBytes default_source = {0};
    RudaOwnedBytes selected_source = {0};
    RudaProcessCaptureResult capture;
    (void)remove("cp19-resource-artifact");
    (void)remove("cp19-default.c");
    (void)remove("cp19-selected.c");
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 7, build, &output, &error
    ) == RUDA_EXIT_SUCCESS && error.length == 0);
    memset(&capture, 0, sizeof(capture));
    RUDA_EXPECT(ruda_process_run_capture(
        ruda_system_allocator(), native_arguments, 1, 4096, 4096, &capture
    ) == RUDA_PROCESS_OK);
    RUDA_EXPECT(!capture.signaled && capture.exit_code == RUDA_EXIT_INTERNAL);
    RUDA_EXPECT(capture.standard_output.length == 0);
    RUDA_EXPECT(capture.standard_error.length == sizeof(expected) - 1
        && memcmp(capture.standard_error.data, expected, sizeof(expected) - 1)
            == 0);
    ruda_process_capture_result_dispose(&capture);
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 7, emit_default, &output, &error
    ) == RUDA_EXIT_SUCCESS && error.length == 0);
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 8, emit_selected, &output, &error
    ) == RUDA_EXIT_SUCCESS && error.length == 0);
    RUDA_EXPECT(ruda_file_read_all(
        ruda_system_allocator(), "cp19-default.c", strlen("cp19-default.c"),
        2 * 1024 * 1024, &default_source
    ) == RUDA_FILE_OK);
    RUDA_EXPECT(ruda_file_read_all(
        ruda_system_allocator(), "cp19-selected.c", strlen("cp19-selected.c"),
        2 * 1024 * 1024, &selected_source
    ) == RUDA_FILE_OK);
    RUDA_EXPECT(strstr(
        (const char *)default_source.data, "max_live_bytes = 67108864;"
    ) != NULL);
    RUDA_EXPECT(strstr(
        (const char *)selected_source.data, "max_live_bytes = 1048576;"
    ) != NULL);
    RUDA_EXPECT(default_source.length != selected_source.length
        || memcmp(default_source.data, selected_source.data,
            default_source.length) != 0);
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 3, cache_default, &default_path, &error
    ) == RUDA_EXIT_SUCCESS && error.length == 0);
    RUDA_EXPECT(ruda_cli_heap_run(
        session, 5, cache_selected, &selected_path, &error
    ) == RUDA_EXIT_SUCCESS && error.length == 0);
    RUDA_EXPECT(default_path.length != selected_path.length
        || memcmp(default_path.bytes, selected_path.bytes,
            default_path.length) != 0);
    ruda_owned_bytes_dispose(&selected_source);
    ruda_owned_bytes_dispose(&default_source);
    RUDA_EXPECT(remove("cp19-selected.c") == 0);
    RUDA_EXPECT(remove("cp19-default.c") == 0);
    RUDA_EXPECT(remove("cp19-resource-artifact") == 0);
}

void ruda_test_cli_heap(void) {
    RudaSession session;
    ruda_session_init(&session, ruda_system_allocator());
    ruda_cli_heap_invalid_values(&session);
    ruda_cli_heap_help_and_success(&session);
    ruda_cli_heap_budget_diagnostics(&session);
    ruda_cli_heap_native_and_embedding(&session);
}
