#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/cli.h"
#include "mmay/platform/process.h"
#include "mmay/session.h"

static int mmay_cli_cp18_run(
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

static void mmay_cli_cp18_expect_artifact(
    const char *artifact,
    const char *expected,
    uint32_t exit_code
) {
    char path[512];
    const char *arguments[2];
    MarimayProcessResult process;
    int length = snprintf(path, sizeof(path), "./%s", artifact);
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(path));
    if (length <= 0 || (size_t)length >= sizeof(path)) return;
    arguments[0] = path;
    arguments[1] = NULL;
    memset(&process, 0, sizeof(process));
    MMAY_EXPECT(mmay_process_run(
        mmay_system_allocator(), arguments, 1, 65536, &process
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!process.signaled);
    MMAY_EXPECT(process.exit_code == exit_code);
    MMAY_EXPECT(process.output.length == strlen(expected));
    MMAY_EXPECT(process.output.data != NULL);
    if (process.output.data != NULL) {
        MMAY_EXPECT(memcmp(
            process.output.data, expected, strlen(expected)
        ) == 0);
    }
    mmay_process_result_dispose(&process);
}

void mmay_test_cli_cp18_array_spelling(void) {
    static const char corpus[] = "cp18-array-spelling";
    static const char bounds_corpus[] = "cp18-array-spelling-bounds";
    static const char artifact[] = "cp18-array-spelling-artifact";
    static const char bounds_artifact[] =
        "cp18-array-spelling-bounds-artifact";
    static const char bounds_panic[] =
        "panic[G4002]: array index 2 is outside length 1\n";
    const char *format[] = {"mmay", "fmt", corpus, "--check"};
    const char *check[] = {"mmay", "check", corpus};
    const char *run[] = {"mmay", "run", corpus};
    const char *test[] = {
        "mmay", "test", corpus, "--engine", "both"
    };
    const char *build[] = {
        "mmay", "build", corpus, "--output", artifact
    };
    const char *bounds_run[] = {"mmay", "run", bounds_corpus};
    const char *bounds_build[] = {
        "mmay", "build", bounds_corpus, "--output", bounds_artifact
    };
    MarimaySession session;
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    (void)remove(artifact);
    (void)remove(bounds_artifact);
    mmay_session_init(&session, mmay_system_allocator());

    MMAY_EXPECT(mmay_cli_cp18_run(
        &session, 4, format, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == 0 && error.length == 0);
    MMAY_EXPECT(mmay_cli_cp18_run(
        &session, 3, check, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == 0 && error.length == 0);
    MMAY_EXPECT(mmay_cli_cp18_run(
        &session, 3, run, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strcmp(output.bytes, "cp18 array spelling ok\n") == 0);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(mmay_cli_cp18_run(
        &session, 5, test, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strstr(output.bytes, "2 passed; 0 failed") != NULL);
    MMAY_EXPECT(strstr(
        output.bytes, "nestedArrayIdentityAndMutationWork ... ok"
    ) != NULL);
    MMAY_EXPECT(strstr(
        output.bytes, "contextualEmptyArrayWorks ... ok"
    ) != NULL);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(mmay_cli_cp18_run(
        &session, 5, build, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(error.length == 0);
    mmay_cli_cp18_expect_artifact(
        artifact, "cp18 array spelling ok\n", 0
    );

    MMAY_EXPECT(mmay_cli_cp18_run(
        &session, 3, bounds_run, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strcmp(error.bytes, bounds_panic) == 0);
    MMAY_EXPECT(mmay_cli_cp18_run(
        &session, 5, bounds_build, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(error.length == 0);
    mmay_cli_cp18_expect_artifact(bounds_artifact, bounds_panic, 1);

    MMAY_EXPECT(remove(artifact) == 0);
    MMAY_EXPECT(remove(bounds_artifact) == 0);
}
