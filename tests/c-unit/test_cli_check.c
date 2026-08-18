#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "test_support.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define ruda_check_test_chdir _chdir
#else
#include <unistd.h>
#define ruda_check_test_chdir chdir
#endif

#include "ruda/base/allocator.h"
#include "ruda/cli.h"
#include "ruda/platform/directory.h"
#include "ruda/session.h"

static int ruda_check_test_run(
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

static bool ruda_check_test_read(
    const char *path,
    unsigned char *bytes,
    size_t capacity,
    size_t *length
) {
    FILE *file = fopen(path, "rb");
    size_t count = 0;
    bool ok = file != NULL;
    if (ok) {
        count = fread(bytes, 1, capacity, file);
        ok = ferror(file) == 0 && feof(file) != 0;
        ok = fclose(file) == 0 && ok;
    }
    if (ok) {
        *length = count;
    }
    return ok;
}

static size_t ruda_check_test_newlines(const char *text) {
    size_t count = 0;
    while (*text != '\0') {
        if (*text == '\n') {
            count += 1;
        }
        text += 1;
    }
    return count;
}

void ruda_test_cli_check(void) {
    RudaSession session;
    RudaTestBuffer output;
    RudaTestBuffer error;
    RudaOwnedBytes original_directory = {0};
    const char *help[] = {"ruda", "check", "--help"};
    const char *clean[] = {"ruda", "check", "check-tree"};
    const char *package[] = {"ruda", "check", "check-tree/math"};
    const char *standalone[] = {
        "ruda", "check", "check-tree/math/arithmetic.ruda"
    };
    const char *all_targets[] = {
        "ruda", "check", "check-tree", "--all-targets"
    };
    const char *bad_human[] = {"ruda", "check", "check-bad.ruda"};
    const char *bad_json[] = {
        "ruda", "check", "check-bad.ruda", "--diagnostic", "json"
    };
    const char *invalid_manifest[] = {
        "ruda", "check", "check-invalid-manifest"
    };
    const char *invalid_identity[] = {
        "ruda", "check", "check-invalid-identity"
    };
    const char *invalid_identity_nul[] = {
        "ruda", "check", "check-invalid-identity-nul"
    };
    const char *missing_json[] = {
        "ruda", "check", "missing-check-target", "--diagnostic", "json"
    };
    const char *incomplete[] = {
        "ruda", "check", "check-incomplete.ruda"
    };
    const char *explicit_test[] = {
        "ruda", "check", "check-tree/ignored_test.ruda"
    };
    const char *no_target[] = {"ruda", "check"};
    unsigned char before[4096];
    unsigned char after[4096];
    size_t before_length = 0;
    size_t after_length = 0;
    int result = 0;
    ruda_session_init(&session, ruda_system_allocator());

    result = ruda_check_test_run(&session, 3, help, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT_SNAPSHOT("check-help.txt", output.bytes);
    RUDA_EXPECT(error.length == 0);

    RUDA_EXPECT(ruda_check_test_read(
        "check-tree/main.ruda", before, sizeof(before), &before_length
    ));
    result = ruda_check_test_run(&session, 3, clean, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(output.length == 0 && error.length == 0);
    result = ruda_check_test_run(&session, 3, clean, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(output.length == 0 && error.length == 0);
    RUDA_EXPECT(ruda_check_test_read(
        "check-tree/main.ruda", after, sizeof(after), &after_length
    ));
    RUDA_EXPECT(before_length == after_length);
    RUDA_EXPECT(before_length != after_length
        || memcmp(before, after, before_length) == 0);

    result = ruda_check_test_run(&session, 3, package, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(output.length == 0 && error.length == 0);
    result = ruda_check_test_run(&session, 3, standalone, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(output.length == 0 && error.length == 0);

    RUDA_EXPECT(ruda_directory_current(
        ruda_system_allocator(), 32768, &original_directory
    ) == RUDA_FILE_OK);
    if (original_directory.data != NULL
        && ruda_check_test_chdir("check-tree/math") == 0) {
        result = ruda_check_test_run(
            &session, 2, no_target, &output, &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SUCCESS);
        RUDA_EXPECT(output.length == 0 && error.length == 0);
        RUDA_EXPECT(ruda_check_test_chdir(
            (const char *)original_directory.data
        ) == 0);
    } else {
        RUDA_EXPECT(false);
    }
    ruda_owned_bytes_dispose(&original_directory);

    result = ruda_check_test_run(
        &session, 4, all_targets, &output, &error
    );
    RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "error[R0001]") != NULL);
    RUDA_EXPECT(strstr(error.bytes, "ignored_test.ruda") != NULL
        || strstr(error.bytes, "integration.ruda") != NULL);

    result = ruda_check_test_run(&session, 3, bad_human, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "error[R0001]") != NULL);
    RUDA_EXPECT(strstr(error.bytes, "check-bad.ruda:2:") != NULL);

    result = ruda_check_test_run(&session, 5, bad_json, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "\"schema\":\"ruda.diagnostic/1\"")
        != NULL);
    RUDA_EXPECT(strstr(error.bytes, "\"code\":\"R0001\"") != NULL);
    RUDA_EXPECT(strstr(error.bytes, "\"primary\":{") != NULL);
    RUDA_EXPECT(ruda_check_test_newlines(error.bytes) == 1);

    result = ruda_check_test_run(
        &session, 3, invalid_manifest, &output, &error
    );
    RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "error[K0002]") != NULL);
    RUDA_EXPECT(strstr(error.bytes, "ruda.mod:1:") != NULL);

    result = ruda_check_test_run(
        &session, 3, invalid_identity, &output, &error
    );
    RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "error[K0005]") != NULL);
    RUDA_EXPECT(strstr(error.bytes, "ruda.mod:1:") != NULL);

    result = ruda_check_test_run(
        &session, 3, invalid_identity_nul, &output, &error
    );
    RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "error[K0005]") != NULL);
    RUDA_EXPECT(strstr(error.bytes, "ruda.mod:2:") != NULL);

    result = ruda_check_test_run(
        &session, 5, missing_json, &output, &error
    );
    RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT_SNAPSHOT("check-missing.jsonl", error.bytes);

    result = ruda_check_test_run(&session, 3, incomplete, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_INTERNAL);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "error[I0001]") != NULL);

    result = ruda_check_test_run(&session, 3, explicit_test, &output, &error);
    RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(output.length == 0);
    RUDA_EXPECT(strstr(error.bytes, "error[R0001]") != NULL);

    {
        char long_target[32769];
        const char *limited[] = {"ruda", "check", long_target};
        memset(long_target, 'a', sizeof(long_target) - 1);
        long_target[sizeof(long_target) - 1] = '\0';
        result = ruda_check_test_run(
            &session, 3, limited, &output, &error
        );
        RUDA_EXPECT(result == RUDA_EXIT_SOURCE_FAILURE);
        RUDA_EXPECT(output.length == 0);
        RUDA_EXPECT(strstr(error.bytes, "error[K0004]") != NULL);
        RUDA_EXPECT(strstr(error.bytes, "defensive") != NULL);
    }

    {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 2048; failure_index += 1) {
            RudaFailAllocator state;
            RudaSession failing_session;
            ruda_fail_allocator_init(
                &state, ruda_system_allocator(), failure_index
            );
            ruda_session_init(&failing_session, ruda_fail_allocator(&state));
            result = ruda_check_test_run(
                &failing_session, 3, clean, &output, &error
            );
            if (result == RUDA_EXIT_SUCCESS) {
                reached_success = true;
                RUDA_EXPECT(output.length == 0 && error.length == 0);
            } else {
                RUDA_EXPECT(result == RUDA_EXIT_INTERNAL);
            }
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        RUDA_EXPECT(reached_success);
    }

    ruda_test_buffer_init(&output);
    ruda_test_buffer_init(&error);
    error.failure_index = 0;
    result = ruda_cli_run(
        &session, 3, bad_human, (RudaReadStream){0},
        ruda_test_buffer_writer(&output),
        ruda_test_buffer_writer(&error)
    );
    RUDA_EXPECT(result == RUDA_EXIT_INTERNAL);
    RUDA_EXPECT(output.length == 0);
}
