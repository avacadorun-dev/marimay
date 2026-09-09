#include "test_support.h"

#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/cli.h"
#include "mmay/platform/process.h"
#include "mmay/session.h"
#include "../../bootstrap/src/cli/cli_bytecode_report.h"
#include "../../bootstrap/src/cli/cli_run_report.h"

static int mmay_cli_run_test_in_process(
    MarimaySession *session,
    int argument_count,
    const char *const arguments[],
    MarimayTestBuffer *output,
    MarimayTestBuffer *error
) {
    mmay_test_buffer_init(output);
    mmay_test_buffer_init(error);
    return mmay_cli_run(
        session, argument_count, arguments, (MarimayReadStream){0},
        mmay_test_buffer_writer(output), mmay_test_buffer_writer(error)
    );
}

static void mmay_cli_run_test_successes(MarimaySession *session) {
    static const unsigned char stdout_bytes[] = {
        '\n', 'a', '\0', 'b', '\n', 'a', '\n', 'b', '\n'
    };
    const char *acceptance[] = {
        "mmay", "run", "run/cp9/acceptance.my", "--", "ignored"
    };
    const char *matrix[] = {"mmay", "run", "examples/matrix.my"};
    const char *bytes[] = {"mmay", "run", "run/cp9/stdout-bytes.my"};
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    int status = mmay_cli_run_test_in_process(
        session, 5, acceptance, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == sizeof("cp9 acceptance ok\n") - 1);
    MMAY_EXPECT(memcmp(
        output.bytes, "cp9 acceptance ok\n", output.length
    ) == 0);
    MMAY_EXPECT(error.length == 0);

    status = mmay_cli_run_test_in_process(
        session, 3, matrix, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == sizeof("415.0\n") - 1);
    MMAY_EXPECT(memcmp(output.bytes, "415.0\n", output.length) == 0);
    MMAY_EXPECT(strstr(output.bytes, "ok:f64:") == NULL);
    MMAY_EXPECT(error.length == 0);

    status = mmay_cli_run_test_in_process(
        session, 3, bytes, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == sizeof(stdout_bytes));
    MMAY_EXPECT(memcmp(output.bytes, stdout_bytes, sizeof(stdout_bytes)) == 0);
    MMAY_EXPECT(error.length == 0);
}

static void mmay_cli_run_test_failures(MarimaySession *session) {
    const char *expected[] = {
        "mmay", "run", "run/cp9/expected-error.my"
    };
    const char *expected_json[] = {
        "mmay", "run", "run/cp9/expected-error.my",
        "--diagnostic", "json"
    };
    const char *expected_bytes[] = {
        "mmay", "run", "run/cp9/expected-error-bytes.my"
    };
    const char *expected_bytes_json[] = {
        "mmay", "run", "run/cp9/expected-error-bytes.my",
        "--diagnostic", "json"
    };
    const char *invalid[] = {
        "mmay", "run", "run/cp9/invalid-entry.my"
    };
    const char *missing[] = {
        "mmay", "run", "run/cp9/missing-entry.my"
    };
    const char *explicit_panic[] = {
        "mmay", "run", "run/cp9/panic-explicit.my"
    };
    const char *explicit_panic_json[] = {
        "mmay", "run", "run/cp9/panic-explicit.my",
        "--diagnostic", "json"
    };
    const char *bounds[] = {
        "mmay", "run", "run/cp9/panic-bounds.my"
    };
    const char *arithmetic[] = {
        "mmay", "run", "run/cp9/panic-arithmetic.my"
    };
    const char *division[] = {
        "mmay", "run", "run/cp9/panic-division.my"
    };
    const char *shift[] = {
        "mmay", "run", "run/cp9/panic-shift.my"
    };
    const char *invalid_engine[] = {
        "mmay", "run", "--engine", "both", "run/cp9/void.my"
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    int status = mmay_cli_run_test_in_process(
        session, 3, expected, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE && output.length == 0);
    MMAY_EXPECT(strcmp(error.bytes, "error[G4003]: application failed\n") == 0);

    status = mmay_cli_run_test_in_process(
        session, 5, expected_json, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE && output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "\"code\":\"G4003\"") != NULL);

    status = mmay_cli_run_test_in_process(
        session, 3, expected_bytes, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE && output.length == 0);
    {
        static const unsigned char expected_error[] = {
            'e', 'r', 'r', 'o', 'r', '[', 'G', '4', '0', '0', '3', ']', ':',
            ' ', 'a', '\0', 'b', '\n'
        };
        MMAY_EXPECT(error.length == sizeof(expected_error));
        MMAY_EXPECT(memcmp(
            error.bytes, expected_error, sizeof(expected_error)
        ) == 0);
    }
    status = mmay_cli_run_test_in_process(
        session, 5, expected_bytes_json, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE && output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "\"message\":\"a\\u0000b\"") != NULL);

    status = mmay_cli_run_test_in_process(
        session, 3, invalid, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE && output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[T0017]") != NULL);
    status = mmay_cli_run_test_in_process(
        session, 3, missing, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE && output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[T0017]") != NULL);

    status = mmay_cli_run_test_in_process(
        session, 3, explicit_panic, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE && output.length == 0);
    MMAY_EXPECT(strcmp(error.bytes, "panic[G4000]: explicit failure\n") == 0);
    status = mmay_cli_run_test_in_process(
        session, 5, explicit_panic_json, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE && output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "\"code\":\"G4000\"") != NULL);
    MMAY_EXPECT(strstr(error.bytes, "\"message\":\"explicit failure\"")
        != NULL);
    status = mmay_cli_run_test_in_process(
        session, 3, bounds, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE && output.length == 0);
    MMAY_EXPECT(strcmp(
        error.bytes, "panic[G4002]: array index 2 is outside length 1\n"
    ) == 0);
    status = mmay_cli_run_test_in_process(
        session, 3, arithmetic, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE && output.length == 0);
    MMAY_EXPECT(strcmp(error.bytes, "panic[G4001]: integer overflow\n") == 0);
    status = mmay_cli_run_test_in_process(
        session, 3, division, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE && output.length == 0);
    MMAY_EXPECT(strcmp(
        error.bytes, "panic[G4001]: integer division by zero\n"
    ) == 0);
    status = mmay_cli_run_test_in_process(
        session, 3, shift, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE && output.length == 0);
    MMAY_EXPECT(strcmp(
        error.bytes, "panic[G4001]: integer shift count is outside width\n"
    ) == 0);

    status = mmay_cli_run_test_in_process(
        session, 5, invalid_engine, &output, &error
    );
    MMAY_EXPECT(status == MMAY_EXIT_USAGE && output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[U0005]") != NULL);
}

static void mmay_cli_run_test_writer_failures(MarimaySession *session) {
    const char *bytes[] = {"mmay", "run", "run/cp9/stdout-bytes.my"};
    const char *expected[] = {
        "mmay", "run", "run/cp9/expected-error.my"
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    int status;
    mmay_test_buffer_init(&output);
    mmay_test_buffer_init(&error);
    output.failure_index = 0;
    status = mmay_cli_run(
        session, 3, bytes, (MarimayReadStream){0},
        mmay_test_buffer_writer(&output), mmay_test_buffer_writer(&error)
    );
    MMAY_EXPECT(status == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strcmp(
        error.bytes, "error[G4003]: standard stream write failed\n"
    ) == 0);

    mmay_test_buffer_init(&output);
    mmay_test_buffer_init(&error);
    error.failure_index = 0;
    status = mmay_cli_run(
        session, 3, expected, (MarimayReadStream){0},
        mmay_test_buffer_writer(&output), mmay_test_buffer_writer(&error)
    );
    MMAY_EXPECT(status == MMAY_EXIT_INTERNAL);
    MMAY_EXPECT(output.length == 0 && error.length == 0);
}

static void mmay_cli_run_test_allocations(void) {
    const char *arguments[] = {"mmay", "run", "run/cp19-managed.my"};
    size_t failure_index = 0;
    bool reached_success = false;
    bool saw_managed_host_failure = false;
    bool saw_checked_fallback = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimaySession session;
        MarimayTestBuffer output;
        MarimayTestBuffer error;
        int status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        mmay_session_init(&session, mmay_fail_allocator(&state));
        status = mmay_cli_run_test_in_process(
            &session, 3, arguments, &output, &error
        );
        if (status == MMAY_EXIT_SUCCESS) {
            /* Optional preparation may recover from the injected failure. */
            reached_success = state.attempts <= failure_index;
            saw_checked_fallback |= !reached_success;
            MMAY_EXPECT(output.length == 0 && error.length == 0);
        } else {
            MMAY_EXPECT(status == MMAY_EXIT_INTERNAL);
            if (strcmp(
                    error.bytes,
                    "error[I0002]: runtime host allocation failed\n"
                ) == 0) {
                saw_managed_host_failure = true;
            }
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
    MMAY_EXPECT(saw_managed_host_failure);
    MMAY_EXPECT(saw_checked_fallback);
}

static void mmay_cli_run_test_process(void) {
    const char *arguments[4];
    MarimayProcessResult result;
    const char *executable = mmay_test_toolchain_executable();
    arguments[0] = executable;
    arguments[1] = "run";
    arguments[2] = "examples/matrix.my";
    arguments[3] = NULL;
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_process_run(
        mmay_system_allocator(), arguments, 3, 1024, &result
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(result.exit_code == 0 && !result.signaled);
    MMAY_EXPECT(result.output.length == sizeof("415.0\n") - 1);
    MMAY_EXPECT(memcmp(
        result.output.data, "415.0\n", result.output.length
    ) == 0);
    mmay_process_result_dispose(&result);
}

void mmay_test_cli_run(void) {
    MarimaySession session;
    {
        MarimayCliOptions options;
        MarimayTestBuffer error;
        memset(&options, 0, sizeof(options));
        options.diagnostic_style = MMAY_DIAGNOSTIC_JSON;
        mmay_test_buffer_init(&error);
        MMAY_EXPECT(mmay_cli_run_pipeline_error(
            &options,
            mmay_cli_bytecode_codec_message(MMAY_BYTECODE_CODEC_UNSUPPORTED_VERSION),
            mmay_test_buffer_writer(&error)
        ) == 70);
        MMAY_EXPECT(strstr((const char *)error.bytes,
            "unsupported format version or feature flags") != NULL);
        MMAY_EXPECT(strstr((const char *)error.bytes, "B0001") != NULL);
    }
    mmay_session_init(&session, mmay_system_allocator());
    mmay_cli_run_test_successes(&session);
    mmay_cli_run_test_failures(&session);
    mmay_cli_run_test_writer_failures(&session);
    mmay_cli_run_test_allocations();
    mmay_cli_run_test_process();
}
