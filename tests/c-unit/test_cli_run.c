#include "test_support.h"

#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/cli.h"
#include "ruda/platform/process.h"
#include "ruda/session.h"

static int ruda_cli_run_test_in_process(
    RudaSession *session,
    int argument_count,
    const char *const arguments[],
    RudaTestBuffer *output,
    RudaTestBuffer *error
) {
    ruda_test_buffer_init(output);
    ruda_test_buffer_init(error);
    return ruda_cli_run(
        session, argument_count, arguments, (RudaReadStream){0},
        ruda_test_buffer_writer(output), ruda_test_buffer_writer(error)
    );
}

static void ruda_cli_run_test_successes(RudaSession *session) {
    static const unsigned char stdout_bytes[] = {
        '\n', 'a', '\0', 'b', '\n', 'a', '\n', 'b', '\n'
    };
    const char *acceptance[] = {
        "ruda", "run", "run/cp9/acceptance.ruda", "--", "ignored"
    };
    const char *matrix[] = {"ruda", "run", "examples/matrix.ruda"};
    const char *bytes[] = {"ruda", "run", "run/cp9/stdout-bytes.ruda"};
    RudaTestBuffer output;
    RudaTestBuffer error;
    int status = ruda_cli_run_test_in_process(
        session, 5, acceptance, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(output.length == sizeof("cp9 acceptance ok\n") - 1);
    RUDA_EXPECT(memcmp(
        output.bytes, "cp9 acceptance ok\n", output.length
    ) == 0);
    RUDA_EXPECT(error.length == 0);

    status = ruda_cli_run_test_in_process(
        session, 3, matrix, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(output.length == sizeof("415.0\n") - 1);
    RUDA_EXPECT(memcmp(output.bytes, "415.0\n", output.length) == 0);
    RUDA_EXPECT(strstr(output.bytes, "ok:f64:") == NULL);
    RUDA_EXPECT(error.length == 0);

    status = ruda_cli_run_test_in_process(
        session, 3, bytes, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(output.length == sizeof(stdout_bytes));
    RUDA_EXPECT(memcmp(output.bytes, stdout_bytes, sizeof(stdout_bytes)) == 0);
    RUDA_EXPECT(error.length == 0);
}

static void ruda_cli_run_test_failures(RudaSession *session) {
    const char *expected[] = {
        "ruda", "run", "run/cp9/expected-error.ruda"
    };
    const char *expected_json[] = {
        "ruda", "run", "run/cp9/expected-error.ruda",
        "--diagnostic", "json"
    };
    const char *expected_bytes[] = {
        "ruda", "run", "run/cp9/expected-error-bytes.ruda"
    };
    const char *expected_bytes_json[] = {
        "ruda", "run", "run/cp9/expected-error-bytes.ruda",
        "--diagnostic", "json"
    };
    const char *invalid[] = {
        "ruda", "run", "run/cp9/invalid-entry.ruda"
    };
    const char *missing[] = {
        "ruda", "run", "run/cp9/missing-entry.ruda"
    };
    const char *explicit_panic[] = {
        "ruda", "run", "run/cp9/panic-explicit.ruda"
    };
    const char *explicit_panic_json[] = {
        "ruda", "run", "run/cp9/panic-explicit.ruda",
        "--diagnostic", "json"
    };
    const char *bounds[] = {
        "ruda", "run", "run/cp9/panic-bounds.ruda"
    };
    const char *arithmetic[] = {
        "ruda", "run", "run/cp9/panic-arithmetic.ruda"
    };
    const char *division[] = {
        "ruda", "run", "run/cp9/panic-division.ruda"
    };
    const char *shift[] = {
        "ruda", "run", "run/cp9/panic-shift.ruda"
    };
    const char *native[] = {
        "ruda", "run", "--engine", "native", "run/cp9/void.ruda"
    };
    RudaTestBuffer output;
    RudaTestBuffer error;
    int status = ruda_cli_run_test_in_process(
        session, 3, expected, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE && output.length == 0);
    RUDA_EXPECT(strcmp(error.bytes, "error[G4003]: application failed\n") == 0);

    status = ruda_cli_run_test_in_process(
        session, 5, expected_json, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE && output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "\"code\":\"G4003\"") != NULL);

    status = ruda_cli_run_test_in_process(
        session, 3, expected_bytes, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE && output.length == 0);
    {
        static const unsigned char expected_error[] = {
            'e', 'r', 'r', 'o', 'r', '[', 'G', '4', '0', '0', '3', ']', ':',
            ' ', 'a', '\0', 'b', '\n'
        };
        RUDA_EXPECT(error.length == sizeof(expected_error));
        RUDA_EXPECT(memcmp(
            error.bytes, expected_error, sizeof(expected_error)
        ) == 0);
    }
    status = ruda_cli_run_test_in_process(
        session, 5, expected_bytes_json, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE && output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "\"message\":\"a\\u0000b\"") != NULL);

    status = ruda_cli_run_test_in_process(
        session, 3, invalid, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE && output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "error[T0017]") != NULL);
    status = ruda_cli_run_test_in_process(
        session, 3, missing, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE && output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "error[T0017]") != NULL);

    status = ruda_cli_run_test_in_process(
        session, 3, explicit_panic, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE && output.length == 0);
    RUDA_EXPECT(strcmp(error.bytes, "panic[G4000]: explicit failure\n") == 0);
    status = ruda_cli_run_test_in_process(
        session, 5, explicit_panic_json, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE && output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "\"code\":\"G4000\"") != NULL);
    RUDA_EXPECT(strstr(error.bytes, "\"message\":\"explicit failure\"")
        != NULL);
    status = ruda_cli_run_test_in_process(
        session, 3, bounds, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE && output.length == 0);
    RUDA_EXPECT(strcmp(
        error.bytes, "panic[G4002]: array index 2 is outside length 1\n"
    ) == 0);
    status = ruda_cli_run_test_in_process(
        session, 3, arithmetic, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE && output.length == 0);
    RUDA_EXPECT(strcmp(error.bytes, "panic[G4001]: integer overflow\n") == 0);
    status = ruda_cli_run_test_in_process(
        session, 3, division, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE && output.length == 0);
    RUDA_EXPECT(strcmp(
        error.bytes, "panic[G4001]: integer division by zero\n"
    ) == 0);
    status = ruda_cli_run_test_in_process(
        session, 3, shift, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE && output.length == 0);
    RUDA_EXPECT(strcmp(
        error.bytes, "panic[G4001]: integer shift count is outside width\n"
    ) == 0);

    status = ruda_cli_run_test_in_process(
        session, 5, native, &output, &error
    );
    RUDA_EXPECT(status == RUDA_EXIT_USAGE && output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);
}

static void ruda_cli_run_test_writer_failures(RudaSession *session) {
    const char *bytes[] = {"ruda", "run", "run/cp9/stdout-bytes.ruda"};
    const char *expected[] = {
        "ruda", "run", "run/cp9/expected-error.ruda"
    };
    RudaTestBuffer output;
    RudaTestBuffer error;
    int status;
    ruda_test_buffer_init(&output);
    ruda_test_buffer_init(&error);
    output.failure_index = 0;
    status = ruda_cli_run(
        session, 3, bytes, (RudaReadStream){0},
        ruda_test_buffer_writer(&output), ruda_test_buffer_writer(&error)
    );
    RUDA_EXPECT(status == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT(strcmp(error.bytes, "error[G4003]: stdout write failed\n") == 0);

    ruda_test_buffer_init(&output);
    ruda_test_buffer_init(&error);
    error.failure_index = 0;
    status = ruda_cli_run(
        session, 3, expected, (RudaReadStream){0},
        ruda_test_buffer_writer(&output), ruda_test_buffer_writer(&error)
    );
    RUDA_EXPECT(status == RUDA_EXIT_INTERNAL);
    RUDA_EXPECT(output.length == 0 && error.length == 0);
}

static void ruda_cli_run_test_allocations(void) {
    const char *arguments[] = {"ruda", "run", "run/cp19-managed.ruda"};
    size_t failure_index = 0;
    bool reached_success = false;
    bool saw_managed_host_failure = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaSession session;
        RudaTestBuffer output;
        RudaTestBuffer error;
        int status;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        ruda_session_init(&session, ruda_fail_allocator(&state));
        status = ruda_cli_run_test_in_process(
            &session, 3, arguments, &output, &error
        );
        if (status == RUDA_EXIT_SUCCESS) {
            reached_success = true;
            RUDA_EXPECT(output.length == 0 && error.length == 0);
        } else {
            RUDA_EXPECT(status == RUDA_EXIT_INTERNAL);
            if (strcmp(
                    error.bytes,
                    "error[I0002]: runtime host allocation failed\n"
                ) == 0) {
                saw_managed_host_failure = true;
            }
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
    RUDA_EXPECT(saw_managed_host_failure);
}

static void ruda_cli_run_test_process(void) {
    const char *arguments[4];
    RudaProcessResult result;
    const char *executable = ruda_test_toolchain_executable();
    arguments[0] = executable;
    arguments[1] = "run";
    arguments[2] = "examples/matrix.ruda";
    arguments[3] = NULL;
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_process_run(
        ruda_system_allocator(), arguments, 3, 1024, &result
    ) == RUDA_PROCESS_OK);
    RUDA_EXPECT(result.exit_code == 0 && !result.signaled);
    RUDA_EXPECT(result.output.length == sizeof("415.0\n") - 1);
    RUDA_EXPECT(memcmp(
        result.output.data, "415.0\n", result.output.length
    ) == 0);
    ruda_process_result_dispose(&result);
}

void ruda_test_cli_run(void) {
    RudaSession session;
    ruda_session_init(&session, ruda_system_allocator());
    ruda_cli_run_test_successes(&session);
    ruda_cli_run_test_failures(&session);
    ruda_cli_run_test_writer_failures(&session);
    ruda_cli_run_test_allocations();
    ruda_cli_run_test_process();
}
