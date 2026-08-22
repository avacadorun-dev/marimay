#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "test_support.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define mmay_check_test_chdir _chdir
#else
#include <unistd.h>
#define mmay_check_test_chdir chdir
#endif

#include "mmay/base/allocator.h"
#include "mmay/cli.h"
#include "mmay/platform/directory.h"
#include "mmay/session.h"

static int mmay_check_test_run(
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

static bool mmay_check_test_read(
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

static size_t mmay_check_test_newlines(const char *text) {
    size_t count = 0;
    while (*text != '\0') {
        if (*text == '\n') {
            count += 1;
        }
        text += 1;
    }
    return count;
}

void mmay_test_cli_check(void) {
    MarimaySession session;
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    MarimayOwnedBytes original_directory = {0};
    const char *help[] = {"mmay", "check", "--help"};
    const char *clean[] = {"mmay", "check", "check-tree"};
    const char *package[] = {"mmay", "check", "check-tree/math"};
    const char *standalone[] = {
        "mmay", "check", "check-tree/math/arithmetic.my"
    };
    const char *all_targets[] = {
        "mmay", "check", "check-tree", "--all-targets"
    };
    const char *bad_human[] = {"mmay", "check", "check-bad.my"};
    const char *bad_json[] = {
        "mmay", "check", "check-bad.my", "--diagnostic", "json"
    };
    const char *invalid_manifest[] = {
        "mmay", "check", "check-invalid-manifest"
    };
    const char *invalid_identity[] = {
        "mmay", "check", "check-invalid-identity"
    };
    const char *invalid_identity_nul[] = {
        "mmay", "check", "check-invalid-identity-nul"
    };
    const char *legacy_manifest[] = {
        "mmay", "check", "check-legacy-manifest"
    };
    const char *missing_json[] = {
        "mmay", "check", "missing-check-target", "--diagnostic", "json"
    };
    const char *incomplete[] = {
        "mmay", "check", "check-incomplete.my"
    };
    const char *explicit_test[] = {
        "mmay", "check", "check-tree/ignored_test.my"
    };
    const char *no_target[] = {"mmay", "check"};
    unsigned char before[4096];
    unsigned char after[4096];
    size_t before_length = 0;
    size_t after_length = 0;
    int result = 0;
    mmay_session_init(&session, mmay_system_allocator());

    result = mmay_check_test_run(&session, 3, help, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT_SNAPSHOT("check-help.txt", output.bytes);
    MMAY_EXPECT(error.length == 0);

    MMAY_EXPECT(mmay_check_test_read(
        "check-tree/main.my", before, sizeof(before), &before_length
    ));
    result = mmay_check_test_run(&session, 3, clean, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == 0 && error.length == 0);
    result = mmay_check_test_run(&session, 3, clean, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == 0 && error.length == 0);
    MMAY_EXPECT(mmay_check_test_read(
        "check-tree/main.my", after, sizeof(after), &after_length
    ));
    MMAY_EXPECT(before_length == after_length);
    MMAY_EXPECT(before_length != after_length
        || memcmp(before, after, before_length) == 0);

    result = mmay_check_test_run(&session, 3, package, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == 0 && error.length == 0);
    result = mmay_check_test_run(&session, 3, standalone, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(output.length == 0 && error.length == 0);

    MMAY_EXPECT(mmay_directory_current(
        mmay_system_allocator(), 32768, &original_directory
    ) == MMAY_FILE_OK);
    if (original_directory.data != NULL
        && mmay_check_test_chdir("check-tree/math") == 0) {
        result = mmay_check_test_run(
            &session, 2, no_target, &output, &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
        MMAY_EXPECT(output.length == 0 && error.length == 0);
        MMAY_EXPECT(mmay_check_test_chdir(
            (const char *)original_directory.data
        ) == 0);
    } else {
        MMAY_EXPECT(false);
    }
    mmay_owned_bytes_dispose(&original_directory);

    result = mmay_check_test_run(
        &session, 4, all_targets, &output, &error
    );
    MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[R0001]") != NULL);
    MMAY_EXPECT(strstr(error.bytes, "ignored_test.my") != NULL
        || strstr(error.bytes, "integration.my") != NULL);

    result = mmay_check_test_run(&session, 3, bad_human, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[R0001]") != NULL);
    MMAY_EXPECT(strstr(error.bytes, "check-bad.my:2:") != NULL);

    result = mmay_check_test_run(&session, 5, bad_json, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "\"schema\":\"marimay.diagnostic/1\"")
        != NULL);
    MMAY_EXPECT(strstr(error.bytes, "\"code\":\"R0001\"") != NULL);
    MMAY_EXPECT(strstr(error.bytes, "\"primary\":{") != NULL);
    MMAY_EXPECT(mmay_check_test_newlines(error.bytes) == 1);

    result = mmay_check_test_run(
        &session, 3, invalid_manifest, &output, &error
    );
    MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[K0002]") != NULL);
    MMAY_EXPECT(strstr(error.bytes, "mmay.mod:1:") != NULL);

    result = mmay_check_test_run(
        &session, 3, invalid_identity, &output, &error
    );
    MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[K0005]") != NULL);
    MMAY_EXPECT(strstr(error.bytes, "mmay.mod:1:") != NULL);

    result = mmay_check_test_run(
        &session, 3, invalid_identity_nul, &output, &error
    );
    MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[K0005]") != NULL);
    MMAY_EXPECT(strstr(error.bytes, "mmay.mod:2:") != NULL);

    result = mmay_check_test_run(
        &session, 3, legacy_manifest, &output, &error
    );
    MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[K0004]") != NULL);

    result = mmay_check_test_run(
        &session, 5, missing_json, &output, &error
    );
    MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT_SNAPSHOT("check-missing.jsonl", error.bytes);

    result = mmay_check_test_run(&session, 3, incomplete, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_INTERNAL);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[I0001]") != NULL);

    result = mmay_check_test_run(&session, 3, explicit_test, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[R0001]") != NULL);

    {
        char long_target[32769];
        const char *limited[] = {"mmay", "check", long_target};
        memset(long_target, 'a', sizeof(long_target) - 1);
        long_target[sizeof(long_target) - 1] = '\0';
        result = mmay_check_test_run(
            &session, 3, limited, &output, &error
        );
        MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
        MMAY_EXPECT(output.length == 0);
        MMAY_EXPECT(strstr(error.bytes, "error[K0004]") != NULL);
        MMAY_EXPECT(strstr(error.bytes, "defensive") != NULL);
    }

    {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 2048; failure_index += 1) {
            MarimayFailAllocator state;
            MarimaySession failing_session;
            mmay_fail_allocator_init(
                &state, mmay_system_allocator(), failure_index
            );
            mmay_session_init(&failing_session, mmay_fail_allocator(&state));
            result = mmay_check_test_run(
                &failing_session, 3, clean, &output, &error
            );
            if (result == MMAY_EXIT_SUCCESS) {
                reached_success = true;
                MMAY_EXPECT(output.length == 0 && error.length == 0);
            } else {
                MMAY_EXPECT(result == MMAY_EXIT_INTERNAL);
            }
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            if (reached_success) {
                break;
            }
        }
        MMAY_EXPECT(reached_success);
    }

    mmay_test_buffer_init(&output);
    mmay_test_buffer_init(&error);
    error.failure_index = 0;
    result = mmay_cli_run(
        &session, 3, bad_human, (MarimayReadStream){0},
        mmay_test_buffer_writer(&output),
        mmay_test_buffer_writer(&error)
    );
    MMAY_EXPECT(result == MMAY_EXIT_INTERNAL);
    MMAY_EXPECT(output.length == 0);
}
