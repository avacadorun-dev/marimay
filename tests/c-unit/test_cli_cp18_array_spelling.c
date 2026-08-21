#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/cli.h"
#include "ruda/platform/process.h"
#include "ruda/session.h"

static int ruda_cli_cp18_run(
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

static void ruda_cli_cp18_expect_artifact(
    const char *artifact,
    const char *expected,
    uint32_t exit_code
) {
    char path[512];
    const char *arguments[2];
    RudaProcessResult process;
    int length = snprintf(path, sizeof(path), "./%s", artifact);
    RUDA_EXPECT(length > 0 && (size_t)length < sizeof(path));
    if (length <= 0 || (size_t)length >= sizeof(path)) return;
    arguments[0] = path;
    arguments[1] = NULL;
    memset(&process, 0, sizeof(process));
    RUDA_EXPECT(ruda_process_run(
        ruda_system_allocator(), arguments, 1, 65536, &process
    ) == RUDA_PROCESS_OK);
    RUDA_EXPECT(!process.signaled);
    RUDA_EXPECT(process.exit_code == exit_code);
    RUDA_EXPECT(process.output.length == strlen(expected));
    RUDA_EXPECT(process.output.data != NULL);
    if (process.output.data != NULL) {
        RUDA_EXPECT(memcmp(
            process.output.data, expected, strlen(expected)
        ) == 0);
    }
    ruda_process_result_dispose(&process);
}

void ruda_test_cli_cp18_array_spelling(void) {
    static const char corpus[] = "cp18-array-spelling";
    static const char bounds_corpus[] = "cp18-array-spelling-bounds";
    static const char artifact[] = "cp18-array-spelling-artifact";
    static const char bounds_artifact[] =
        "cp18-array-spelling-bounds-artifact";
    static const char bounds_panic[] =
        "panic[G4002]: array index 2 is outside length 1\n";
    const char *format[] = {"ruda", "fmt", corpus, "--check"};
    const char *check[] = {"ruda", "check", corpus};
    const char *run[] = {"ruda", "run", corpus};
    const char *test[] = {
        "ruda", "test", corpus, "--engine", "both"
    };
    const char *build[] = {
        "ruda", "build", corpus, "--output", artifact
    };
    const char *bounds_run[] = {"ruda", "run", bounds_corpus};
    const char *bounds_build[] = {
        "ruda", "build", bounds_corpus, "--output", bounds_artifact
    };
    RudaSession session;
    RudaTestBuffer output;
    RudaTestBuffer error;
    (void)remove(artifact);
    (void)remove(bounds_artifact);
    ruda_session_init(&session, ruda_system_allocator());

    RUDA_EXPECT(ruda_cli_cp18_run(
        &session, 4, format, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(output.length == 0 && error.length == 0);
    RUDA_EXPECT(ruda_cli_cp18_run(
        &session, 3, check, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(output.length == 0 && error.length == 0);
    RUDA_EXPECT(ruda_cli_cp18_run(
        &session, 3, run, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strcmp(output.bytes, "cp18 array spelling ok\n") == 0);
    RUDA_EXPECT(error.length == 0);
    RUDA_EXPECT(ruda_cli_cp18_run(
        &session, 5, test, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(strstr(output.bytes, "2 passed; 0 failed") != NULL);
    RUDA_EXPECT(strstr(
        output.bytes, "nestedArrayIdentityAndMutationWork ... ok"
    ) != NULL);
    RUDA_EXPECT(strstr(
        output.bytes, "contextualEmptyArrayWorks ... ok"
    ) != NULL);
    RUDA_EXPECT(error.length == 0);
    RUDA_EXPECT(ruda_cli_cp18_run(
        &session, 5, build, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(error.length == 0);
    ruda_cli_cp18_expect_artifact(
        artifact, "cp18 array spelling ok\n", 0
    );

    RUDA_EXPECT(ruda_cli_cp18_run(
        &session, 3, bounds_run, &output, &error
    ) == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT(strcmp(error.bytes, bounds_panic) == 0);
    RUDA_EXPECT(ruda_cli_cp18_run(
        &session, 5, bounds_build, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(error.length == 0);
    ruda_cli_cp18_expect_artifact(bounds_artifact, bounds_panic, 1);

    RUDA_EXPECT(remove(artifact) == 0);
    RUDA_EXPECT(remove(bounds_artifact) == 0);
}
