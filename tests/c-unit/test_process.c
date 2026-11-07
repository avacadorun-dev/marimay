#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/platform/process.h"

void mmay_test_process(void) {
    static const unsigned char exact_input[] = {
        'a', 0, 0xff, '\r', '\n'
    };
    const char *executable = mmay_test_toolchain_executable();
    const char *version_arguments[4];
    const char *unknown_arguments[4];
    const char *space_arguments[5];
    const char *invalid_arguments[2];
    const char *missing_arguments[2] = {"mmay-definitely-missing-i0", NULL};
    const char *fixture_arguments[2] = {
        MMAY_TEST_CP32_PROCESS_FIXTURE, NULL
    };
    MarimayProcessResult result;
    MarimayProcessCaptureResult capture;
    MarimayOwnedBytes executable_path = {0};
    MarimayProcessStatus status;
    size_t failure_index = 0;
    bool reached_success = false;

    MMAY_EXPECT(executable != NULL);
    if (executable == NULL) {
        return;
    }
    MMAY_EXPECT(mmay_process_executable_path(
        mmay_system_allocator(), 32768, &executable_path
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(executable_path.data != NULL && executable_path.length != 0);
    mmay_owned_bytes_dispose(&executable_path);
    MMAY_EXPECT(mmay_process_executable_path(
        mmay_system_allocator(), 1, &executable_path
    ) == MMAY_PROCESS_OUTPUT_LIMIT);
    for (failure_index = 0; failure_index < 4; failure_index += 1) {
        MarimayFailAllocator state;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_process_executable_path(
            mmay_fail_allocator(&state), 32768, &executable_path
        );
        if (status == MMAY_PROCESS_OK) {
            reached_success = true;
            mmay_owned_bytes_dispose(&executable_path);
        } else {
            MMAY_EXPECT(status == MMAY_PROCESS_ALLOCATION_FAILED);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
    reached_success = false;
    version_arguments[0] = executable;
    version_arguments[1] = "version";
    version_arguments[2] = "--json";
    version_arguments[3] = NULL;
    unknown_arguments[0] = executable;
    unknown_arguments[1] = "buid";
    unknown_arguments[2] = NULL;
    unknown_arguments[3] = NULL;
    space_arguments[0] = executable;
    space_arguments[1] = "help";
    space_arguments[2] = "not a command";
    space_arguments[3] = "--diagnostic=json";
    space_arguments[4] = NULL;
    invalid_arguments[0] = executable;
    invalid_arguments[1] = "not-null-terminated";

    status = mmay_process_run(
        mmay_system_allocator(),
        version_arguments,
        3,
        16384,
        &result
    );
    MMAY_EXPECT(status == MMAY_PROCESS_OK);
    MMAY_EXPECT(result.exit_code == 0);
    MMAY_EXPECT(!result.signaled);
    MMAY_EXPECT(strstr((const char *)result.output.data, "\"schema\":\"marimay.version/2\"")
        != NULL);
    mmay_process_result_dispose(&result);

    memset(&capture, 0, sizeof(capture));
    status = mmay_process_run_capture(
        mmay_system_allocator(), unknown_arguments, 2, 16384, 16384,
        &capture
    );
    MMAY_EXPECT(status == MMAY_PROCESS_OK);
    MMAY_EXPECT(capture.exit_code == 2 && !capture.signaled);
    MMAY_EXPECT(capture.standard_output.length == 0);
    MMAY_EXPECT(strstr(
        (const char *)capture.standard_error.data, "error[U0001]"
    ) != NULL);
    mmay_process_capture_result_dispose(&capture);

    memset(&capture, 0, sizeof(capture));
    status = mmay_process_run_capture_input(
        mmay_system_allocator(), fixture_arguments, 1,
        exact_input, sizeof(exact_input), 16384, 16384, &capture
    );
    MMAY_EXPECT(status == MMAY_PROCESS_OK);
    MMAY_EXPECT(capture.exit_code == 0 && !capture.signaled);
    MMAY_EXPECT(capture.standard_output.length == sizeof(exact_input));
    MMAY_EXPECT(capture.standard_output.length != sizeof(exact_input)
        || memcmp(
            capture.standard_output.data, exact_input, sizeof(exact_input)
        ) == 0);
    MMAY_EXPECT(capture.standard_error.length == sizeof("fixture-error\n") - 1);
    MMAY_EXPECT(capture.standard_error.length != sizeof("fixture-error\n") - 1
        || memcmp(
            capture.standard_error.data, "fixture-error\n",
            sizeof("fixture-error\n") - 1
        ) == 0);
    mmay_process_capture_result_dispose(&capture);

    MMAY_EXPECT(mmay_process_run_capture_input(
        mmay_system_allocator(), fixture_arguments, 1,
        NULL, 1, 16384, 16384, &capture
    ) == MMAY_PROCESS_INVALID_ARGUMENT);

    {
        FILE *input_file = mmay_test_temporary_file();
        FILE *output_file = mmay_test_temporary_file();
        FILE *error_file = mmay_test_temporary_file();
        unsigned char actual_output[sizeof(exact_input)] = {0};
        char actual_error[sizeof("fixture-error\n")] = {0};
        MMAY_EXPECT(input_file != NULL && output_file != NULL
            && error_file != NULL);
        if (input_file != NULL && output_file != NULL && error_file != NULL) {
            MMAY_EXPECT(fwrite(
                exact_input, 1, sizeof(exact_input), input_file
            ) == sizeof(exact_input));
            MMAY_EXPECT(fseek(input_file, 0, SEEK_SET) == 0);
            status = mmay_process_run_stdio(
                mmay_system_allocator(), fixture_arguments, 1,
                input_file, output_file, error_file, &result
            );
            MMAY_EXPECT(status == MMAY_PROCESS_OK);
            MMAY_EXPECT(result.exit_code == 0 && !result.signaled);
            MMAY_EXPECT(fflush(output_file) == 0 && fflush(error_file) == 0);
            MMAY_EXPECT(fseek(output_file, 0, SEEK_SET) == 0);
            MMAY_EXPECT(fseek(error_file, 0, SEEK_SET) == 0);
            MMAY_EXPECT(fread(
                actual_output, 1, sizeof(actual_output), output_file
            ) == sizeof(actual_output));
            MMAY_EXPECT(memcmp(
                actual_output, exact_input, sizeof(exact_input)
            ) == 0);
            MMAY_EXPECT(fread(
                actual_error, 1, sizeof(actual_error) - 1, error_file
            ) == sizeof(actual_error) - 1);
            MMAY_EXPECT(strcmp(actual_error, "fixture-error\n") == 0);
            mmay_process_result_dispose(&result);
        }
        if (input_file != NULL) fclose(input_file);
        if (output_file != NULL) fclose(output_file);
        if (error_file != NULL) fclose(error_file);
    }

    MMAY_EXPECT(mmay_process_run_stdio(
        mmay_system_allocator(), fixture_arguments, 1,
        NULL, stdout, stderr, &result
    ) == MMAY_PROCESS_INVALID_ARGUMENT);

    {
        FILE *input_file = mmay_test_temporary_file();
        FILE *output_file = mmay_test_temporary_file();
        FILE *error_file = mmay_test_temporary_file();
        reached_success = false;
        MMAY_EXPECT(input_file != NULL && output_file != NULL
            && error_file != NULL);
        if (input_file != NULL && output_file != NULL && error_file != NULL) {
            for (failure_index = 0; failure_index < 16; failure_index += 1) {
                MarimayFailAllocator state;
                mmay_fail_allocator_init(
                    &state, mmay_system_allocator(), failure_index
                );
                status = mmay_process_run_stdio(
                    mmay_fail_allocator(&state), fixture_arguments, 1,
                    input_file, output_file, error_file, &result
                );
                if (status == MMAY_PROCESS_OK) {
                    reached_success = true;
                    mmay_process_result_dispose(&result);
                } else {
                    MMAY_EXPECT(status == MMAY_PROCESS_ALLOCATION_FAILED);
                }
                MMAY_EXPECT(state.live_allocations == 0);
                MMAY_EXPECT(state.live_bytes == 0);
                if (reached_success) break;
            }
        }
        MMAY_EXPECT(reached_success);
        if (input_file != NULL) fclose(input_file);
        if (output_file != NULL) fclose(output_file);
        if (error_file != NULL) fclose(error_file);
    }

    status = mmay_process_run(
        mmay_system_allocator(),
        unknown_arguments,
        2,
        16384,
        &result
    );
    MMAY_EXPECT(status == MMAY_PROCESS_OK);
    MMAY_EXPECT(result.exit_code == 2);
    MMAY_EXPECT(strstr((const char *)result.output.data, "error[U0001]") != NULL);
    mmay_process_result_dispose(&result);

    status = mmay_process_run(
        mmay_system_allocator(),
        space_arguments,
        4,
        16384,
        &result
    );
    MMAY_EXPECT(status == MMAY_PROCESS_OK);
    MMAY_EXPECT(result.exit_code == 2);
    MMAY_EXPECT(strstr((const char *)result.output.data, "not a command") != NULL);
    mmay_process_result_dispose(&result);

    status = mmay_process_run(
        mmay_system_allocator(),
        version_arguments,
        3,
        8,
        &result
    );
    MMAY_EXPECT(status == MMAY_PROCESS_OUTPUT_LIMIT);
    MMAY_EXPECT(result.output.data == NULL);

    status = mmay_process_run(
        mmay_system_allocator(),
        missing_arguments,
        1,
        1024,
        &result
    );
    MMAY_EXPECT(status == MMAY_PROCESS_SPAWN_FAILED);
    MMAY_EXPECT(result.output.data == NULL);

    status = mmay_process_run(
        mmay_system_allocator(),
        invalid_arguments,
        1,
        1024,
        &result
    );
    MMAY_EXPECT(status == MMAY_PROCESS_INVALID_ARGUMENT);

    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        status = mmay_process_run(allocator, version_arguments, 3, 16384, &result);
        if (status == MMAY_PROCESS_OK) {
            reached_success = true;
            MMAY_EXPECT(result.exit_code == 0);
            mmay_process_result_dispose(&result);
        } else {
            MMAY_EXPECT(status == MMAY_PROCESS_ALLOCATION_FAILED);
            MMAY_EXPECT(result.output.data == NULL);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        allocator = mmay_fail_allocator(&state);
        memset(&capture, 0, sizeof(capture));
        status = mmay_process_run_capture(
            allocator, version_arguments, 3, 16384, 16384, &capture
        );
        if (status == MMAY_PROCESS_OK) {
            reached_success = true;
            MMAY_EXPECT(capture.exit_code == 0);
            mmay_process_capture_result_dispose(&capture);
        } else {
            MMAY_EXPECT(status == MMAY_PROCESS_ALLOCATION_FAILED);
            MMAY_EXPECT(capture.standard_output.data == NULL);
            MMAY_EXPECT(capture.standard_error.data == NULL);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}
