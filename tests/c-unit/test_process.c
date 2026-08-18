#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/platform/process.h"

void ruda_test_process(void) {
    const char *executable = ruda_test_toolchain_executable();
    const char *version_arguments[4];
    const char *unknown_arguments[4];
    const char *space_arguments[5];
    const char *invalid_arguments[2];
    const char *missing_arguments[2] = {"ruda-definitely-missing-i0", NULL};
    RudaProcessResult result;
    RudaProcessCaptureResult capture;
    RudaProcessStatus status;
    size_t failure_index = 0;
    bool reached_success = false;

    RUDA_EXPECT(executable != NULL);
    if (executable == NULL) {
        return;
    }
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

    status = ruda_process_run(
        ruda_system_allocator(),
        version_arguments,
        3,
        16384,
        &result
    );
    RUDA_EXPECT(status == RUDA_PROCESS_OK);
    RUDA_EXPECT(result.exit_code == 0);
    RUDA_EXPECT(!result.signaled);
    RUDA_EXPECT(strstr((const char *)result.output.data, "\"schema\":\"ruda.version/1\"")
        != NULL);
    ruda_process_result_dispose(&result);

    memset(&capture, 0, sizeof(capture));
    status = ruda_process_run_capture(
        ruda_system_allocator(), unknown_arguments, 2, 16384, 16384,
        &capture
    );
    RUDA_EXPECT(status == RUDA_PROCESS_OK);
    RUDA_EXPECT(capture.exit_code == 2 && !capture.signaled);
    RUDA_EXPECT(capture.standard_output.length == 0);
    RUDA_EXPECT(strstr(
        (const char *)capture.standard_error.data, "error[U0001]"
    ) != NULL);
    ruda_process_capture_result_dispose(&capture);

    status = ruda_process_run(
        ruda_system_allocator(),
        unknown_arguments,
        2,
        16384,
        &result
    );
    RUDA_EXPECT(status == RUDA_PROCESS_OK);
    RUDA_EXPECT(result.exit_code == 2);
    RUDA_EXPECT(strstr((const char *)result.output.data, "error[U0001]") != NULL);
    ruda_process_result_dispose(&result);

    status = ruda_process_run(
        ruda_system_allocator(),
        space_arguments,
        4,
        16384,
        &result
    );
    RUDA_EXPECT(status == RUDA_PROCESS_OK);
    RUDA_EXPECT(result.exit_code == 2);
    RUDA_EXPECT(strstr((const char *)result.output.data, "not a command") != NULL);
    ruda_process_result_dispose(&result);

    status = ruda_process_run(
        ruda_system_allocator(),
        version_arguments,
        3,
        8,
        &result
    );
    RUDA_EXPECT(status == RUDA_PROCESS_OUTPUT_LIMIT);
    RUDA_EXPECT(result.output.data == NULL);

    status = ruda_process_run(
        ruda_system_allocator(),
        missing_arguments,
        1,
        1024,
        &result
    );
    RUDA_EXPECT(status == RUDA_PROCESS_SPAWN_FAILED);
    RUDA_EXPECT(result.output.data == NULL);

    status = ruda_process_run(
        ruda_system_allocator(),
        invalid_arguments,
        1,
        1024,
        &result
    );
    RUDA_EXPECT(status == RUDA_PROCESS_INVALID_ARGUMENT);

    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_process_run(allocator, version_arguments, 3, 16384, &result);
        if (status == RUDA_PROCESS_OK) {
            reached_success = true;
            RUDA_EXPECT(result.exit_code == 0);
            ruda_process_result_dispose(&result);
        } else {
            RUDA_EXPECT(status == RUDA_PROCESS_ALLOCATION_FAILED);
            RUDA_EXPECT(result.output.data == NULL);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        allocator = ruda_fail_allocator(&state);
        memset(&capture, 0, sizeof(capture));
        status = ruda_process_run_capture(
            allocator, version_arguments, 3, 16384, 16384, &capture
        );
        if (status == RUDA_PROCESS_OK) {
            reached_success = true;
            RUDA_EXPECT(capture.exit_code == 0);
            ruda_process_capture_result_dispose(&capture);
        } else {
            RUDA_EXPECT(status == RUDA_PROCESS_ALLOCATION_FAILED);
            RUDA_EXPECT(capture.standard_output.data == NULL);
            RUDA_EXPECT(capture.standard_error.data == NULL);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
}
