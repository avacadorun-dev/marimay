#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/cli.h"
#include "mmay/native/build.h"
#include "mmay/platform/directory.h"
#include "mmay/platform/file.h"
#include "mmay/platform/process.h"
#include "mmay/session.h"

#if !defined(_WIN32)
#include "test_native_c_emitter_support.h"
#endif

static int mmay_cli_build_in_process(
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

static void mmay_cli_build_expect_program(
    MarimaySession *session,
    const char *source,
    const char *artifact,
    bool release,
    const unsigned char *expected,
    size_t expected_length,
    uint32_t expected_exit
) {
    const char *arguments[7];
    const char *run_arguments[2];
    char executable_path[512];
    size_t argument_count = 0;
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    MarimayProcessResult process;
    (void)remove(artifact);
    arguments[argument_count++] = "mmay";
    arguments[argument_count++] = "build";
    arguments[argument_count++] = source;
    if (release) arguments[argument_count++] = "--release";
    arguments[argument_count++] = "--output";
    arguments[argument_count++] = artifact;
    arguments[argument_count] = NULL;
    MMAY_EXPECT(mmay_cli_build_in_process(
        session, (int)argument_count, arguments, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(output.length == strlen(artifact) + 1);
    MMAY_EXPECT(memcmp(output.bytes, artifact, strlen(artifact)) == 0);
    if (strchr(artifact, '/') == NULL && strchr(artifact, '\\') == NULL) {
        int length = snprintf(
            executable_path, sizeof(executable_path), "./%s", artifact
        );
        MMAY_EXPECT(length > 0 && (size_t)length < sizeof(executable_path));
        run_arguments[0] = executable_path;
    } else {
        run_arguments[0] = artifact;
    }
    run_arguments[1] = NULL;
    memset(&process, 0, sizeof(process));
    MMAY_EXPECT(mmay_process_run(
        mmay_system_allocator(), run_arguments, 1, 65536, &process
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!process.signaled && process.exit_code == expected_exit);
    MMAY_EXPECT(process.output.length == expected_length);
    MMAY_EXPECT(expected_length == 0 || (process.output.data != NULL
        && process.output.length == expected_length
        && memcmp(process.output.data, expected, expected_length) == 0));
    mmay_process_result_dispose(&process);
    MMAY_EXPECT(remove(artifact) == 0);
}

static void mmay_cli_build_test_artifacts(MarimaySession *session) {
    static const unsigned char stdout_bytes[] = {
        '\n', 'a', '\0', 'b', '\n', 'a', '\n', 'b', '\n'
    };
    static const unsigned char expected_error[] =
        "error[G4003]: application failed\n";
    static const unsigned char explicit_panic[] =
        "panic[G4000]: explicit failure\n";
    static const unsigned char bounds_panic[] =
        "panic[G4002]: array index 2 is outside length 1\n";
    static const unsigned char arithmetic_panic[] =
        "panic[G4001]: integer overflow\n";
    static const unsigned char division_panic[] =
        "panic[G4001]: integer division by zero\n";
    static const unsigned char shift_panic[] =
        "panic[G4001]: integer shift count is outside width\n";
    static const unsigned char counted_error[] = {
        'e', 'r', 'r', 'o', 'r', '[', 'G', '4', '0', '0', '3', ']', ':',
        ' ', 'a', '\0', 'b', '\n'
    };
    mmay_cli_build_expect_program(
        session, "run/cp9/void.my", "cp10 void artifact", false,
        (const unsigned char *)"", 0, 0
    );
    mmay_cli_build_expect_program(
        session, "run/cp9/stdout-bytes.my", "cp10-stdout-artifact", true,
        stdout_bytes, sizeof(stdout_bytes), 0
    );
    mmay_cli_build_expect_program(
        session, "run/cp9/acceptance.my", "cp10-acceptance-artifact", false,
        (const unsigned char *)"cp9 acceptance ok\n",
        sizeof("cp9 acceptance ok\n") - 1, 0
    );
    mmay_cli_build_expect_program(
        session, "examples/matrix.my", "cp10-matrix-debug", false,
        (const unsigned char *)"415.0\n", sizeof("415.0\n") - 1, 0
    );
    mmay_cli_build_expect_program(
        session, "examples/matrix.my", "cp10-matrix-release", true,
        (const unsigned char *)"415.0\n", sizeof("415.0\n") - 1, 0
    );
    mmay_cli_build_expect_program(
        session, "run/cp9/expected-error.my", "cp10-error-artifact", false,
        expected_error, sizeof(expected_error) - 1, 1
    );
    mmay_cli_build_expect_program(
        session, "run/cp9/expected-error-bytes.my",
        "cp10-counted-error-artifact", false,
        counted_error, sizeof(counted_error), 1
    );
    mmay_cli_build_expect_program(
        session, "run/cp9/panic-explicit.my", "cp10-explicit-artifact", false,
        explicit_panic, sizeof(explicit_panic) - 1, 1
    );
    mmay_cli_build_expect_program(
        session, "run/cp9/panic-bounds.my", "cp10-bounds-artifact", false,
        bounds_panic, sizeof(bounds_panic) - 1, 1
    );
    mmay_cli_build_expect_program(
        session, "run/cp9/panic-arithmetic.my", "cp10-arithmetic-artifact", false,
        arithmetic_panic, sizeof(arithmetic_panic) - 1, 1
    );
    mmay_cli_build_expect_program(
        session, "run/cp9/panic-division.my", "cp10-division-artifact", false,
        division_panic, sizeof(division_panic) - 1, 1
    );
    mmay_cli_build_expect_program(
        session, "run/cp9/panic-shift.my", "cp10-shift-artifact", false,
        shift_panic, sizeof(shift_panic) - 1, 1
    );
}

static void mmay_cli_build_test_cache_and_emit(MarimaySession *session) {
    const char *cached[] = {"mmay", "build", "run/cp9/void.my"};
    const char *cached_release[] = {
        "mmay", "build", "run/cp9/void.my", "--release"
    };
    const char *explicit_native_quiet[] = {
        "mmay", "build", "run/cp9/void.my",
        "--engine", "native", "--quiet"
    };
    const char *emitted[] = {
        "mmay", "build", "run/cp9/stdout-bytes.my",
        "--emit", "c", "--output", "cp10-emitted.c"
    };
    const char *bad_emit[] = {
        "mmay", "build", "run/cp9/void.my", "--emit", "mir"
    };
    const char *bad_engine[] = {
        "mmay", "build", "run/cp9/void.my", "--engine", "vm"
    };
    MarimayTestBuffer first;
    MarimayTestBuffer second;
    MarimayTestBuffer release;
    MarimayTestBuffer error;
    MarimayOwnedBytes source = {0};
    (void)remove("cp10-emitted.c");
    MMAY_EXPECT(mmay_cli_build_in_process(
        session, 3, cached, &first, &error
    ) == MMAY_EXIT_SUCCESS && error.length == 0);
    MMAY_EXPECT(mmay_cli_build_in_process(
        session, 3, cached, &second, &error
    ) == MMAY_EXIT_SUCCESS && error.length == 0);
    MMAY_EXPECT(first.length == second.length
        && memcmp(first.bytes, second.bytes, first.length) == 0);
    MMAY_EXPECT(mmay_cli_build_in_process(
        session, 4, cached_release, &release, &error
    ) == MMAY_EXIT_SUCCESS && error.length == 0);
    MMAY_EXPECT(release.length != first.length
        || memcmp(release.bytes, first.bytes, first.length) != 0);
    MMAY_EXPECT(mmay_cli_build_in_process(
        session, 6, explicit_native_quiet, &second, &error
    ) == MMAY_EXIT_SUCCESS && second.length == 0 && error.length == 0);
    MMAY_EXPECT(mmay_cli_build_in_process(
        session, 7, emitted, &first, &error
    ) == MMAY_EXIT_SUCCESS && error.length == 0);
    MMAY_EXPECT(mmay_file_read_all(
        mmay_system_allocator(), "cp10-emitted.c",
        strlen("cp10-emitted.c"), 1024 * 1024, &source
    ) == MMAY_FILE_OK);
    MMAY_EXPECT(source.length != 0 && strstr(
        (const char *)source.data, "int main(int mmay_g_argc"
    ) != NULL);
#if !defined(_WIN32)
    mmay_test_native_compile_and_run_configured(
        &source,
        "error[I0002]: runtime host allocation failed\n",
        70,
        "-DMMAY_G_FAIL_ALLOCATION_AT=0"
    );
#endif
    mmay_owned_bytes_dispose(&source);
    MMAY_EXPECT(remove("cp10-emitted.c") == 0);
    MMAY_EXPECT(mmay_cli_build_in_process(
        session, 5, bad_emit, &first, &error
    ) == MMAY_EXIT_USAGE && strstr(error.bytes, "error[U0005]") != NULL);
    MMAY_EXPECT(mmay_cli_build_in_process(
        session, 5, bad_engine, &first, &error
    ) == MMAY_EXIT_USAGE && strstr(error.bytes, "error[U0005]") != NULL);
}

static void mmay_cli_build_test_atomic_failures(MarimaySession *session) {
    static const unsigned char original[] = "prior artifact";
    const char *bad_source[] = {
        "mmay", "build", "check-bad.my",
        "--output", "cp10-stale-artifact"
    };
    const char *bad_output[] = {
        "mmay", "build", "run/cp9/void.my",
        "--output", "cp10-missing-directory/artifact"
    };
    const char *bad_output_json[] = {
        "mmay", "build", "run/cp9/void.my",
        "--output", "cp10-missing-directory/artifact",
        "--diagnostic", "json"
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    MarimayOwnedBytes bytes = {0};
    FILE *file = NULL;
    (void)remove("cp10-stale-artifact");
    file = mmay_test_open_file("cp10-stale-artifact", "wb");
    MMAY_EXPECT(file != NULL);
    if (file != NULL) {
        MMAY_EXPECT(fwrite(
            original, 1, sizeof(original) - 1, file
        ) == sizeof(original) - 1);
        MMAY_EXPECT(fclose(file) == 0);
    }
    MMAY_EXPECT(mmay_cli_build_in_process(
        session, 5, bad_source, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(mmay_file_read_all(
        mmay_system_allocator(), "cp10-stale-artifact",
        strlen("cp10-stale-artifact"), 1024, &bytes
    ) == MMAY_FILE_OK);
    MMAY_EXPECT(bytes.length == sizeof(original) - 1
        && memcmp(bytes.data, original, sizeof(original) - 1) == 0);
    mmay_owned_bytes_dispose(&bytes);
    MMAY_EXPECT(remove("cp10-stale-artifact") == 0);
    MMAY_EXPECT(mmay_cli_build_in_process(
        session, 5, bad_output, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(strstr(error.bytes, "error[B0005]") != NULL);
    MMAY_EXPECT(mmay_cli_build_in_process(
        session, 7, bad_output_json, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(strstr(error.bytes, "\"code\":\"B0005\"") != NULL);
}

static void mmay_cli_build_test_allocations(void) {
    const char *arguments[] = {
        "mmay", "build", "run/cp9/void.my",
        "--emit", "c", "--output", "cp10-allocation.c"
    };
    size_t failure_index = 0;
    bool reached_success = false;
    (void)remove("cp10-allocation.c");
    for (failure_index = 0; failure_index < 8192; failure_index += 1) {
        MarimayFailAllocator state;
        MarimaySession session;
        MarimayTestBuffer output;
        MarimayTestBuffer error;
        int status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        mmay_session_init(&session, mmay_fail_allocator(&state));
        status = mmay_cli_build_in_process(
            &session, 7, arguments, &output, &error
        );
        if (status == MMAY_EXIT_SUCCESS) reached_success = state.attempts <= failure_index;
        else MMAY_EXPECT(status == MMAY_EXIT_INTERNAL);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
    MMAY_EXPECT(remove("cp10-allocation.c") == 0);
}

static void mmay_cli_build_expect_silent_artifact(
    const MarimayNativeBuildResult *result, const char *expected_path
) {
    const char *arguments[] = {(const char *)result->artifact_path.data, NULL};
    MarimayProcessResult process = {0};
    MMAY_EXPECT(result->artifact_path.data != NULL && !result->cache_hit);
    if (result->artifact_path.data == NULL) return;
    MMAY_EXPECT(strcmp((const char *)result->artifact_path.data, expected_path) == 0);
    MMAY_EXPECT(mmay_process_run(
        mmay_system_allocator(), arguments, 1, 1024, &process
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!process.signaled && process.exit_code == 0
        && process.output.length == 0);
    mmay_process_result_dispose(&process);
}

static void mmay_cli_build_test_native_allocations(void) {
    static const char root[] = "cp10 native allocation";
    MarimayMirFunction function = {0};
    const MarimayMirFunction *functions[] = {&function};
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirTerminator terminator = {0};
    MarimayNativeBuildRequest request = {0};
    MarimayNativeBuildResult result = {0};
    MarimayFailAllocator control;
    MarimayNativeBuildStatus status;
    char cached_path[32769];
    char object_path[32769];
    const char *separator = NULL;
    size_t attempts = 0;
    size_t index = 0;
    int written = 0;
    bool initialized = mmay_mir_function_init(
        &function, mmay_system_allocator(), (MarimaySymbolId){{0x98}},
        NULL, 0, MMAY_TYPE_VOID, mmay_mir_function_default_options()
    );
    MMAY_EXPECT(initialized);
    if (!initialized) return;
    MMAY_EXPECT(mmay_mir_add_block(&function, &block));
    terminator.kind = MMAY_MIR_RETURN;
    terminator.value = MMAY_MIR_NO_REGISTER;
    terminator.target = MMAY_MIR_NO_BLOCK;
    terminator.otherwise = MMAY_MIR_NO_BLOCK;
    MMAY_EXPECT(mmay_mir_set_terminator(&function, block, terminator));
    MMAY_EXPECT(mmay_mir_verify(&function) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_directory_create(
        mmay_system_allocator(), root, sizeof(root) - 1
    ) == MMAY_FILE_OK);
    request.functions = functions;
    request.function_count = 1;
    request.entry_symbol = function.symbol;
    request.root_directory = root;
    request.root_directory_length = sizeof(root) - 1;
    request.tool_executable_path = mmay_test_toolchain_executable();
    status = mmay_native_build(mmay_system_allocator(), &request, &result);
    MMAY_EXPECT(status == MMAY_NATIVE_BUILD_OK);
    if (status != MMAY_NATIVE_BUILD_OK || result.artifact_path.data == NULL
        || result.artifact_path.length >= sizeof(cached_path)) goto cleanup;
    memcpy(cached_path, result.artifact_path.data, result.artifact_path.length + 1);
    separator = strrchr(cached_path, '/');
    if (strrchr(cached_path, '\\') != NULL
        && (separator == NULL || strrchr(cached_path, '\\') > separator)) {
        separator = strrchr(cached_path, '\\');
    }
    MMAY_EXPECT(separator != NULL);
    if (separator == NULL) goto cleanup;
    written = snprintf(
        object_path, sizeof(object_path), "%.*sprogram.c.obj",
        (int)(separator + 1 - cached_path), cached_path
    );
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(object_path));
    if (written <= 0 || (size_t)written >= sizeof(object_path)) goto cleanup;
    mmay_native_build_result_dispose(&result);
    MMAY_EXPECT(mmay_test_remove_file(cached_path) == 0);
    mmay_fail_allocator_init(&control, mmay_system_allocator(), SIZE_MAX);
    status = mmay_native_build(mmay_fail_allocator(&control), &request, &result);
    MMAY_EXPECT(status == MMAY_NATIVE_BUILD_OK);
    if (status == MMAY_NATIVE_BUILD_OK) {
        mmay_cli_build_expect_silent_artifact(&result, cached_path);
    }
    attempts = control.attempts;
    mmay_native_build_result_dispose(&result);
    MMAY_EXPECT(control.live_allocations == 0 && control.live_bytes == 0);
    MMAY_EXPECT(attempts != 0 && attempts < 4096);
    for (index = 0; index < attempts && index < 4096; index += 1) {
        MarimayFailAllocator state;
        (void)mmay_test_remove_file(cached_path);
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        status = mmay_native_build(mmay_fail_allocator(&state), &request, &result);
        if (status == MMAY_NATIVE_BUILD_OK) {
            /* Pipe chunking may leave the index unreached. A reached failure
             * can also be an optional void FileOps.remove conversion; either
             * case still requires the exact executable result. */
            mmay_cli_build_expect_silent_artifact(&result, cached_path);
        } else {
            MMAY_EXPECT(index < state.attempts);
            MMAY_EXPECT(status == MMAY_NATIVE_BUILD_ALLOCATION_FAILED
                || status == MMAY_NATIVE_BUILD_IO_FAILED
                || status == MMAY_NATIVE_BUILD_COMPILE_FAILED);
            MMAY_EXPECT(result.artifact_path.data == NULL);
        }
        if (state.attempts > attempts) attempts = state.attempts;
        mmay_native_build_result_dispose(&result);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        /* A failed allocation in optional removal may leave cache-local disk
         * state; test teardown retries it with the system allocator. */
        mmay_platform_file_ops().remove(
            NULL, mmay_system_allocator(), object_path, strlen(object_path)
        );
    }
    MMAY_EXPECT(attempts < 4096);
    (void)mmay_test_remove_file(cached_path);
cleanup:
    mmay_native_build_result_dispose(&result);
    mmay_mir_function_dispose(&function);
}

static void mmay_cli_build_test_missing_toolchain(void) {
    const MarimayMirFunction *dummy = (const MarimayMirFunction *)(uintptr_t)1;
    MarimayNativeBuildRequest request;
    MarimayNativeBuildResult result;
    memset(&request, 0, sizeof(request));
    memset(&result, 0, sizeof(result));
    request.functions = &dummy;
    request.function_count = 1;
    request.root_directory = ".";
    request.root_directory_length = 1;
    request.compiler_path = "mmay-cp10-definitely-missing-compiler";
    request.tool_executable_path = mmay_test_toolchain_executable();
    MMAY_EXPECT(mmay_native_build(
        mmay_system_allocator(), &request, &result
    ) == MMAY_NATIVE_BUILD_TOOLCHAIN_MISSING);
    MMAY_EXPECT(result.artifact_path.data == NULL);
    mmay_native_build_result_dispose(&result);
}

void mmay_test_cli_build(void) {
    MarimaySession session;
    mmay_session_init(&session, mmay_system_allocator());
    mmay_cli_build_test_artifacts(&session);
    mmay_cli_build_test_cache_and_emit(&session);
    mmay_cli_build_test_atomic_failures(&session);
    mmay_cli_build_test_allocations();
    mmay_cli_build_test_missing_toolchain();
    mmay_cli_build_test_native_allocations();
}
