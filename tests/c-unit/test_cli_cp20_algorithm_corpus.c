#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/cli.h"
#include "ruda/platform/process.h"
#include "ruda/session.h"

static int ruda_cli_cp20_run(
    RudaSession *session,
    int argument_count,
    const char *const arguments[],
    RudaTestBuffer *output,
    RudaTestBuffer *error
) {
    ruda_test_buffer_init(output);
    ruda_test_buffer_init(error);
    return ruda_cli_run(
        session,
        argument_count,
        arguments,
        (RudaReadStream){0},
        ruda_test_buffer_writer(output),
        ruda_test_buffer_writer(error)
    );
}

static void ruda_cli_cp20_expect_artifact(
    const char *artifact,
    const char *expected
) {
    char path[512];
    const char *arguments[2];
    RudaProcessCaptureResult process;
    int length = snprintf(path, sizeof(path), "./%s", artifact);
    RUDA_EXPECT(length > 0 && (size_t)length < sizeof(path));
    if (length <= 0 || (size_t)length >= sizeof(path)) return;
    arguments[0] = path;
    arguments[1] = NULL;
    memset(&process, 0, sizeof(process));
    RUDA_EXPECT(ruda_process_run_capture(
        ruda_system_allocator(), arguments, 1, 65536, 65536, &process
    ) == RUDA_PROCESS_OK);
    RUDA_EXPECT(!process.signaled);
    RUDA_EXPECT(process.exit_code == 0);
    RUDA_EXPECT(process.standard_output.length == strlen(expected));
    RUDA_EXPECT(process.standard_error.length == 0);
    if (process.standard_output.data != NULL) {
        RUDA_EXPECT(memcmp(
            process.standard_output.data, expected, strlen(expected)
        ) == 0);
    }
    ruda_process_capture_result_dispose(&process);
}

void ruda_test_cli_cp20_algorithm_corpus(void) {
    static const char corpus[] = "cp20-algorithm-corpus";
    static const char debug_artifact[] = "cp20-algorithm-corpus-debug";
    static const char release_artifact[] = "cp20-algorithm-corpus-release";
    static const char expected[] = "cp20 algorithm corpus ok\n";
    const char *format[] = {"ruda", "fmt", corpus, "--check"};
    const char *check[] = {"ruda", "check", corpus};
    const char *run[] = {"ruda", "run", corpus};
    const char *test[] = {
        "ruda", "test", corpus, "--engine", "both"
    };
    const char *build_debug[] = {
        "ruda", "build", corpus, "--output", debug_artifact
    };
    const char *build_release[] = {
        "ruda", "build", corpus, "--release",
        "--output", release_artifact
    };
    RudaSession session;
    RudaTestBuffer output;
    RudaTestBuffer error;
    (void)remove(debug_artifact);
    (void)remove(release_artifact);
    ruda_session_init(&session, ruda_system_allocator());

    RUDA_EXPECT(ruda_cli_cp20_run(
        &session, 4, format, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(output.length == 0 && error.length == 0);
    RUDA_EXPECT(ruda_cli_cp20_run(
        &session, 3, check, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(output.length == 0 && error.length == 0);
    RUDA_EXPECT(ruda_cli_cp20_run(
        &session, 3, run, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(output.length == strlen(expected));
    RUDA_EXPECT(strcmp(output.bytes, expected) == 0);
    RUDA_EXPECT(error.length == 0);
    RUDA_EXPECT(ruda_cli_cp20_run(
        &session, 5, test, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strstr(output.bytes, "2 passed; 0 failed") != NULL);
    RUDA_EXPECT(strstr(
        output.bytes, "allReusableAlgorithmsWork ... ok"
    ) != NULL);
    RUDA_EXPECT(strstr(
        output.bytes, "invalidGraphInputsAreRejected ... ok"
    ) != NULL);
    RUDA_EXPECT(error.length == 0);

    RUDA_EXPECT(ruda_cli_cp20_run(
        &session, 5, build_debug, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(error.length == 0);
    ruda_cli_cp20_expect_artifact(debug_artifact, expected);
    RUDA_EXPECT(ruda_cli_cp20_run(
        &session, 6, build_release, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(error.length == 0);
    ruda_cli_cp20_expect_artifact(release_artifact, expected);

    RUDA_EXPECT(remove(debug_artifact) == 0);
    RUDA_EXPECT(remove(release_artifact) == 0);
}
