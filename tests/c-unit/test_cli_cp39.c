#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "cli_native_run.h"
#include "mmay/base/allocator.h"
#include "mmay/build_config.h"
#include "mmay/cli.h"
#include "mmay/native/toolchain.h"
#include "mmay/platform/directory.h"
#include "mmay/platform/file.h"
#include "mmay/platform/process.h"
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

#if defined(_WIN32)
static void mmay_cp39_expect_msvc_identity(void) {
    const char *version = MMAY_NATIVE_C_COMPILER_VERSION;
    size_t base_length = 0;
    size_t separators = 0;
    char alternate[128];
    MarimayOwnedBytes output = {0};
    int written = 0;
    while (version[base_length] != '\0') {
        if (version[base_length] == '.' && ++separators == 3) break;
        base_length += 1;
    }
    MMAY_EXPECT(base_length > 0 && base_length < sizeof(alternate));
    if (base_length == 0 || base_length >= sizeof(alternate)) return;
    if (strcmp(version + base_length, ".0") == 0) {
        memcpy(alternate, version, base_length);
        alternate[base_length] = '\0';
        MMAY_EXPECT(mmay_native_toolchain_probe(
            mmay_system_allocator(), MMAY_NATIVE_C_COMPILER,
            alternate, &output
        ) == MMAY_NATIVE_TOOLCHAIN_OK);
        mmay_owned_bytes_dispose(&output);
    }
    written = snprintf(alternate, sizeof(alternate), "%.*s.%u",
        (int)base_length, version,
        strcmp(version + base_length, ".1") == 0 ? 2U : 1U);
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(alternate));
    if (written <= 0 || (size_t)written >= sizeof(alternate)) return;
    MMAY_EXPECT(mmay_native_toolchain_probe(
        mmay_system_allocator(), MMAY_NATIVE_C_COMPILER,
        alternate, &output
    ) == MMAY_NATIVE_TOOLCHAIN_UNAVAILABLE);
    MMAY_EXPECT(output.data == NULL);
    /* A shorter final field must not match the prefix of the reported build. */
    memcpy(alternate, version, base_length - 1);
    alternate[base_length - 1] = '\0';
    MMAY_EXPECT(mmay_native_toolchain_probe(
        mmay_system_allocator(), MMAY_NATIVE_C_COMPILER,
        alternate, &output
    ) == MMAY_NATIVE_TOOLCHAIN_UNAVAILABLE);
    MMAY_EXPECT(output.data == NULL);
}
#endif

static void mmay_cp39_expect_toolchain(void) {
    MarimayOwnedBytes output = {0};
    MarimayNativeToolchainStatus probe_status;
    size_t failure_index = 0;
    bool reached_success = false;
    probe_status = mmay_native_toolchain_probe(
        mmay_system_allocator(), MMAY_NATIVE_C_COMPILER,
        MMAY_NATIVE_C_COMPILER_VERSION, &output
    );
#if defined(_WIN32)
    if (probe_status != MMAY_NATIVE_TOOLCHAIN_OK) {
        const char *arguments[] = {MMAY_NATIVE_C_COMPILER, "/?", NULL};
        MarimayProcessResult query = {0};
        MarimayProcessStatus query_status = mmay_process_run(
            mmay_system_allocator(), arguments, 2, 65536, &query
        );
        size_t index = 0;
        size_t code_count = 0;
        DWORD input_flags = 0;
        HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        bool input_valid = input != NULL && input != INVALID_HANDLE_VALUE
            && GetHandleInformation(input, &input_flags) != 0;
        fprintf(stderr,
            "MSVC raw identity query: status=%d; exit=%u; signaled=%u; bytes=%zu; stdinValid=%u; stdinInheritable=%u; head=",
            (int)query_status, (unsigned int)query.exit_code,
            query.signaled ? 1U : 0U, query.output.length,
            input_valid ? 1U : 0U,
            input_valid && (input_flags & HANDLE_FLAG_INHERIT) != 0 ? 1U : 0U);
        for (index = 0; index < query.output.length && index < 16; index += 1) {
            fprintf(stderr, "%02x", (unsigned int)query.output.data[index]);
        }
        if (query.exit_code != 0) {
            fputs("; codes=", stderr);
            for (index = 0; index + 5 <= query.output.length && code_count < 8;
                index += 1) {
                const unsigned char *candidate = query.output.data + index;
                size_t digit = 0;
                if (candidate[0] != 'D' && candidate[0] != 'C') continue;
                for (digit = 1; digit < 5; digit += 1) {
                    if (candidate[digit] < '0' || candidate[digit] > '9') break;
                }
                if (digit == 5 && (index + 5 == query.output.length
                        || query.output.data[index + 5] < '0'
                        || query.output.data[index + 5] > '9')) {
                    if (code_count != 0) fputc(',', stderr);
                    (void)fwrite(candidate, 1, 5, stderr);
                    code_count += 1;
                }
            }
        }
        fputc('\n', stderr);
        mmay_process_result_dispose(&query);
    }
#endif
    MMAY_EXPECT(probe_status == MMAY_NATIVE_TOOLCHAIN_OK);
    MMAY_EXPECT(output.length != 0);
    mmay_owned_bytes_dispose(&output);
#if defined(_WIN32)
    mmay_cp39_expect_msvc_identity();
#endif
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

static void mmay_cp39_expect_native_arguments(void) {
    static const char artifact_text[] = "/installed path/bin/mmay-app";
    const char *program_arguments[] = {"alpha", "two words"};
    MarimayOwnedBytes artifact = {0};
    MarimayCliOptions options;
    MarimayCliNativeArguments arguments;
    MarimayFailAllocator state;
    memset(&options, 0, sizeof(options));
    memset(&arguments, 0, sizeof(arguments));
    artifact.data = (unsigned char *)artifact_text;
    artifact.length = sizeof(artifact_text) - 1;
    options.program_arguments = program_arguments;
    options.program_argument_count = 2;
    MMAY_EXPECT(mmay_cli_native_arguments_init(
        mmay_system_allocator(), &artifact, &options, &arguments
    ));
    MMAY_EXPECT(arguments.count == 3);
    MMAY_EXPECT(arguments.items != NULL);
    MMAY_EXPECT(arguments.items[0] == (const char *)artifact.data);
    MMAY_EXPECT(arguments.items[1] == program_arguments[0]);
    MMAY_EXPECT(arguments.items[2] == program_arguments[1]);
    MMAY_EXPECT(arguments.items[3] == NULL);
    mmay_cli_native_arguments_dispose(&arguments);

    mmay_fail_allocator_init(&state, mmay_system_allocator(), 0);
    MMAY_EXPECT(!mmay_cli_native_arguments_init(
        mmay_fail_allocator(&state), &artifact, &options, &arguments
    ));
    MMAY_EXPECT(arguments.items == NULL);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);

    options.program_argument_count = SIZE_MAX;
    MMAY_EXPECT(!mmay_cli_native_arguments_init(
        mmay_system_allocator(), &artifact, &options, &arguments
    ));
    MMAY_EXPECT(arguments.items == NULL);
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
    mmay_cp39_expect_native_arguments();
    mmay_cp39_expect_doctor();
    mmay_cp39_expect_init();
    mmay_cp39_expect_init_allocations();
}
