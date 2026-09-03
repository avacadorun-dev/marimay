#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/build_config.h"
#include "mmay/cli.h"
#include "mmay/native/toolchain.h"
#include "mmay/platform/directory.h"
#include "mmay/platform/file.h"
#include "mmay/session.h"

static int mmay_cp39_run(
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

static void mmay_cp39_remove_project(const char *directory) {
    static const char *const names[] = {
        "mmay.mod", "main.my", "main_test.my"
    };
    MarimayFileOps ops = mmay_platform_file_ops();
    size_t index = 0;
    for (index = 0; index < sizeof(names) / sizeof(names[0]); index += 1) {
        char path[512];
        int length = snprintf(path, sizeof(path), "%s/%s", directory, names[index]);
        if (length > 0 && (size_t)length < sizeof(path)) {
            (void)ops.remove(
                ops.context, mmay_system_allocator(), path, (size_t)length
            );
        }
    }
    (void)mmay_directory_remove_empty(
        mmay_system_allocator(), directory, strlen(directory)
    );
}

static void mmay_cp39_expect_init(void) {
    static const char destination[] = "cp39 init ü";
    static const char staging_collision[] = "cp39 init ü.mmay-init-tmp-0";
    const char *tool = mmay_test_toolchain_executable();
    const char *arguments[] = {tool, "init", destination};
    MarimaySession session;
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    MarimayOwnedBytes manifest = {0};
    MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
    int result;
    mmay_cp39_remove_project(destination);
    (void)mmay_directory_remove_empty(
        mmay_system_allocator(), staging_collision,
        sizeof(staging_collision) - 1
    );
    MMAY_EXPECT(mmay_directory_create_exclusive(
        mmay_system_allocator(), staging_collision,
        sizeof(staging_collision) - 1
    ) == MMAY_FILE_OK);
    mmay_session_init(&session, mmay_system_allocator());
    result = mmay_cp39_run(&session, 3, arguments, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strstr(output.bytes, "initialized ") != NULL);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(mmay_directory_kind(
        mmay_system_allocator(), destination, sizeof(destination) - 1, &kind
    ) == MMAY_FILE_OK && kind == MMAY_DIRECTORY_ENTRY_DIRECTORY);
    MMAY_EXPECT(mmay_directory_kind(
        mmay_system_allocator(), staging_collision,
        sizeof(staging_collision) - 1, &kind
    ) == MMAY_FILE_OK && kind == MMAY_DIRECTORY_ENTRY_DIRECTORY);
    MMAY_EXPECT(mmay_file_read_all(
        mmay_system_allocator(), "cp39 init ü/mmay.mod",
        strlen("cp39 init ü/mmay.mod"), 4096, &manifest
    ) == MMAY_FILE_OK);
    MMAY_EXPECT(manifest.data != NULL && strstr(
        (const char *)manifest.data, "module \"local/cp39-init---\""
    ) != NULL);
    mmay_owned_bytes_dispose(&manifest);

    result = mmay_cp39_run(&session, 3, arguments, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "error[K0006]") != NULL);
    mmay_cp39_remove_project(destination);
    MMAY_EXPECT(mmay_directory_remove_empty(
        mmay_system_allocator(), staging_collision,
        sizeof(staging_collision) - 1
    ) == MMAY_FILE_OK);
}

static void mmay_cp39_expect_init_allocations(void) {
    static const char destination[] = "cp39-init-allocation";
    const char *tool = mmay_test_toolchain_executable();
    const char *arguments[] = {tool, "init", destination};
    size_t failure_index = 0;
    bool reached_success = false;
    mmay_cp39_remove_project(destination);
    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        MarimayFailAllocator state;
        MarimaySession session;
        MarimayTestBuffer output;
        MarimayTestBuffer error;
        int result;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        mmay_session_init(&session, mmay_fail_allocator(&state));
        result = mmay_cp39_run(&session, 3, arguments, &output, &error);
        if (result == MMAY_EXIT_SUCCESS) {
            reached_success = true;
            mmay_cp39_remove_project(destination);
        } else {
            MMAY_EXPECT(result == MMAY_EXIT_INTERNAL);
            MMAY_EXPECT(strstr(error.bytes, "error[I0002]") != NULL);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_cp39_expect_toolchain(void) {
    MarimayOwnedBytes output = {0};
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_native_toolchain_probe(
        mmay_system_allocator(), MMAY_NATIVE_C_COMPILER,
        MMAY_NATIVE_C_COMPILER_VERSION, &output
    ) == MMAY_NATIVE_TOOLCHAIN_OK);
    MMAY_EXPECT(output.length != 0);
    mmay_owned_bytes_dispose(&output);
    MMAY_EXPECT(mmay_native_toolchain_probe(
        mmay_system_allocator(), MMAY_NATIVE_C_COMPILER,
        "999.999.999", &output
    ) == MMAY_NATIVE_TOOLCHAIN_UNAVAILABLE);
    MMAY_EXPECT(output.data == NULL);
    MMAY_EXPECT(mmay_native_toolchain_probe(
        mmay_system_allocator(), MMAY_NATIVE_C_COMPILER,
        "invalid", &output
    ) == MMAY_NATIVE_TOOLCHAIN_INVALID_ARGUMENT);
    for (failure_index = 0; failure_index < 16; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayNativeToolchainStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_native_toolchain_probe(
            mmay_fail_allocator(&state), MMAY_NATIVE_C_COMPILER,
            MMAY_NATIVE_C_COMPILER_VERSION, &output
        );
        if (status == MMAY_NATIVE_TOOLCHAIN_OK) {
            reached_success = true;
            mmay_owned_bytes_dispose(&output);
        } else {
            MMAY_EXPECT(status == MMAY_NATIVE_TOOLCHAIN_ALLOCATION_FAILED);
            MMAY_EXPECT(output.data == NULL);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_cp39_expect_doctor(void) {
    const char *tool = mmay_test_toolchain_executable();
    const char *arguments[] = {tool, "doctor", "--json"};
    MarimaySession session;
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    int result;
    mmay_session_init(&session, mmay_system_allocator());
    result = mmay_cp39_run(&session, 3, arguments, &output, &error);
    MMAY_EXPECT(result == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strstr(output.bytes, "\"schema\":\"marimay.doctor/1\"")
        != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"ok\":true") != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"debug\":{\"status\":\"ok\"")
        != NULL);
    MMAY_EXPECT(strstr(output.bytes, "\"release\":{\"status\":\"ok\"")
        != NULL);
    MMAY_EXPECT(error.length == 0);
}

void mmay_test_cli_cp39(void) {
    mmay_cp39_expect_toolchain();
    mmay_cp39_expect_doctor();
    mmay_cp39_expect_init();
    mmay_cp39_expect_init_allocations();
}
