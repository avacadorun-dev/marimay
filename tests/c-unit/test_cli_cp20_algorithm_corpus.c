#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/cli.h"
#include "mmay/platform/process.h"
#include "mmay/session.h"

static int mmay_cli_cp20_run(
    MarimaySession *session,
    int argument_count,
    const char *const arguments[],
    MarimayTestBuffer *output,
    MarimayTestBuffer *error
) {
    mmay_test_buffer_init(output);
    mmay_test_buffer_init(error);
    return mmay_cli_run(
        session,
        argument_count,
        arguments,
        (MarimayReadStream){0},
        mmay_test_buffer_writer(output),
        mmay_test_buffer_writer(error)
    );
}

static void mmay_cli_cp20_expect_artifact(
    const char *artifact,
    const char *expected
) {
    char path[512];
    const char *arguments[2];
    MarimayProcessCaptureResult process;
    int length = snprintf(path, sizeof(path), "./%s", artifact);
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(path));
    if (length <= 0 || (size_t)length >= sizeof(path)) return;
    arguments[0] = path;
    arguments[1] = NULL;
    memset(&process, 0, sizeof(process));
    MMAY_EXPECT(mmay_process_run_capture(
        mmay_system_allocator(), arguments, 1, 65536, 65536, &process
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!process.signaled);
    MMAY_EXPECT(process.exit_code == 0);
    MMAY_EXPECT(process.standard_output.length == strlen(expected));
    MMAY_EXPECT(process.standard_error.length == 0);
    if (process.standard_output.data != NULL) {
        MMAY_EXPECT(memcmp(
            process.standard_output.data, expected, strlen(expected)
        ) == 0);
    }
    mmay_process_capture_result_dispose(&process);
}

void mmay_test_cli_cp20_algorithm_corpus(void) {
    static const char corpus[] = "cp20-algorithm-corpus";
    static const char debug_artifact[] = "cp20-algorithm-corpus-debug";
    static const char release_artifact[] = "cp20-algorithm-corpus-release";
    static const char expected[] = "cp20 algorithm corpus ok\n";
    static const char expected_test[] =
        "test test/cp20-algorithms.allReusableAlgorithmsWork ... ok\n"
        "test test/cp20-algorithms.invalidGraphInputsAreRejected ... ok\n"
        "test result: ok. 2 passed; 0 failed\n";
    const char *format[] = {"mmay", "fmt", corpus, "--check"};
    const char *check[] = {"mmay", "check", corpus};
    const char *run[] = {"mmay", "run", corpus};
    const char *test[] = {
        "mmay", "test", corpus, "--engine", "both"
    };
    const char *build_debug[] = {
        "mmay", "build", corpus, "--output", debug_artifact
    };
    const char *build_release[] = {
        "mmay", "build", corpus, "--release",
        "--output", release_artifact
    };
    MarimaySession session;
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    (void)remove(debug_artifact);
    (void)remove(release_artifact);
    mmay_session_init(&session, mmay_system_allocator());

    MMAY_EXPECT(mmay_cli_cp20_run(
        &session, 4, format, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == 0 && error.length == 0);
    MMAY_EXPECT(mmay_cli_cp20_run(
        &session, 3, check, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == 0 && error.length == 0);
    MMAY_EXPECT(mmay_cli_cp20_run(
        &session, 3, run, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == strlen(expected));
    MMAY_EXPECT(strcmp(output.bytes, expected) == 0);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(mmay_cli_cp20_run(
        &session, 5, test, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == strlen(expected_test));
    MMAY_EXPECT(strcmp(output.bytes, expected_test) == 0);
    MMAY_EXPECT(error.length == 0);

    MMAY_EXPECT(mmay_cli_cp20_run(
        &session, 5, build_debug, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(error.length == 0);
    mmay_cli_cp20_expect_artifact(debug_artifact, expected);
    MMAY_EXPECT(mmay_cli_cp20_run(
        &session, 6, build_release, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(error.length == 0);
    mmay_cli_cp20_expect_artifact(release_artifact, expected);

    MMAY_EXPECT(remove(debug_artifact) == 0);
    MMAY_EXPECT(remove(release_artifact) == 0);
}
