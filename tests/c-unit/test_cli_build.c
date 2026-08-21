#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/cli.h"
#include "ruda/native/build.h"
#include "ruda/platform/file.h"
#include "ruda/platform/process.h"
#include "ruda/session.h"

#if !defined(_WIN32)
#include "test_native_c_emitter_support.h"
#endif

static int ruda_cli_build_in_process(
    RudaSession *session,
    int argument_count,
    const char *const arguments[],
    RudaTestBuffer *output,
    RudaTestBuffer *error
) {
    ruda_test_buffer_init(output);
    ruda_test_buffer_init(error);
    return ruda_cli_run(
        session, argument_count, arguments, (RudaReadStream){0},
        ruda_test_buffer_writer(output), ruda_test_buffer_writer(error)
    );
}

static void ruda_cli_build_expect_program(
    RudaSession *session,
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
    RudaTestBuffer output;
    RudaTestBuffer error;
    RudaProcessResult process;
    (void)remove(artifact);
    arguments[argument_count++] = "ruda";
    arguments[argument_count++] = "build";
    arguments[argument_count++] = source;
    if (release) arguments[argument_count++] = "--release";
    arguments[argument_count++] = "--output";
    arguments[argument_count++] = artifact;
    arguments[argument_count] = NULL;
    RUDA_EXPECT(ruda_cli_build_in_process(
        session, (int)argument_count, arguments, &output, &error
    ) == RUDA_EXIT_SUCCESS);
    RUDA_EXPECT(error.length == 0);
    RUDA_EXPECT(output.length == strlen(artifact) + 1);
    RUDA_EXPECT(memcmp(output.bytes, artifact, strlen(artifact)) == 0);
    if (strchr(artifact, '/') == NULL && strchr(artifact, '\\') == NULL) {
        int length = snprintf(
            executable_path, sizeof(executable_path), "./%s", artifact
        );
        RUDA_EXPECT(length > 0 && (size_t)length < sizeof(executable_path));
        run_arguments[0] = executable_path;
    } else {
        run_arguments[0] = artifact;
    }
    run_arguments[1] = NULL;
    memset(&process, 0, sizeof(process));
    RUDA_EXPECT(ruda_process_run(
        ruda_system_allocator(), run_arguments, 1, 65536, &process
    ) == RUDA_PROCESS_OK);
    RUDA_EXPECT(!process.signaled && process.exit_code == expected_exit);
    RUDA_EXPECT(process.output.length == expected_length);
    RUDA_EXPECT(expected_length == 0 || (process.output.data != NULL
        && process.output.length == expected_length
        && memcmp(process.output.data, expected, expected_length) == 0));
    ruda_process_result_dispose(&process);
    RUDA_EXPECT(remove(artifact) == 0);
}

static void ruda_cli_build_test_artifacts(RudaSession *session) {
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
    ruda_cli_build_expect_program(
        session, "run/cp9/void.ruda", "cp10 void artifact", false,
        (const unsigned char *)"", 0, 0
    );
    ruda_cli_build_expect_program(
        session, "run/cp9/stdout-bytes.ruda", "cp10-stdout-artifact", true,
        stdout_bytes, sizeof(stdout_bytes), 0
    );
    ruda_cli_build_expect_program(
        session, "run/cp9/acceptance.ruda", "cp10-acceptance-artifact", false,
        (const unsigned char *)"cp9 acceptance ok\n",
        sizeof("cp9 acceptance ok\n") - 1, 0
    );
    ruda_cli_build_expect_program(
        session, "examples/matrix.ruda", "cp10-matrix-debug", false,
        (const unsigned char *)"415.0\n", sizeof("415.0\n") - 1, 0
    );
    ruda_cli_build_expect_program(
        session, "examples/matrix.ruda", "cp10-matrix-release", true,
        (const unsigned char *)"415.0\n", sizeof("415.0\n") - 1, 0
    );
    ruda_cli_build_expect_program(
        session, "run/cp9/expected-error.ruda", "cp10-error-artifact", false,
        expected_error, sizeof(expected_error) - 1, 1
    );
    ruda_cli_build_expect_program(
        session, "run/cp9/expected-error-bytes.ruda",
        "cp10-counted-error-artifact", false,
        counted_error, sizeof(counted_error), 1
    );
    ruda_cli_build_expect_program(
        session, "run/cp9/panic-explicit.ruda", "cp10-explicit-artifact", false,
        explicit_panic, sizeof(explicit_panic) - 1, 1
    );
    ruda_cli_build_expect_program(
        session, "run/cp9/panic-bounds.ruda", "cp10-bounds-artifact", false,
        bounds_panic, sizeof(bounds_panic) - 1, 1
    );
    ruda_cli_build_expect_program(
        session, "run/cp9/panic-arithmetic.ruda", "cp10-arithmetic-artifact", false,
        arithmetic_panic, sizeof(arithmetic_panic) - 1, 1
    );
    ruda_cli_build_expect_program(
        session, "run/cp9/panic-division.ruda", "cp10-division-artifact", false,
        division_panic, sizeof(division_panic) - 1, 1
    );
    ruda_cli_build_expect_program(
        session, "run/cp9/panic-shift.ruda", "cp10-shift-artifact", false,
        shift_panic, sizeof(shift_panic) - 1, 1
    );
}

static void ruda_cli_build_test_cache_and_emit(RudaSession *session) {
    const char *cached[] = {"ruda", "build", "run/cp9/void.ruda"};
    const char *cached_release[] = {
        "ruda", "build", "run/cp9/void.ruda", "--release"
    };
    const char *explicit_native_quiet[] = {
        "ruda", "build", "run/cp9/void.ruda",
        "--engine", "native", "--quiet"
    };
    const char *emitted[] = {
        "ruda", "build", "run/cp9/stdout-bytes.ruda",
        "--emit", "c", "--output", "cp10-emitted.c"
    };
    const char *bad_emit[] = {
        "ruda", "build", "run/cp9/void.ruda", "--emit", "mir"
    };
    const char *bad_engine[] = {
        "ruda", "build", "run/cp9/void.ruda", "--engine", "vm"
    };
    RudaTestBuffer first;
    RudaTestBuffer second;
    RudaTestBuffer release;
    RudaTestBuffer error;
    RudaOwnedBytes source = {0};
    (void)remove("cp10-emitted.c");
    RUDA_EXPECT(ruda_cli_build_in_process(
        session, 3, cached, &first, &error
    ) == RUDA_EXIT_SUCCESS && error.length == 0);
    RUDA_EXPECT(ruda_cli_build_in_process(
        session, 3, cached, &second, &error
    ) == RUDA_EXIT_SUCCESS && error.length == 0);
    RUDA_EXPECT(first.length == second.length
        && memcmp(first.bytes, second.bytes, first.length) == 0);
    RUDA_EXPECT(ruda_cli_build_in_process(
        session, 4, cached_release, &release, &error
    ) == RUDA_EXIT_SUCCESS && error.length == 0);
    RUDA_EXPECT(release.length != first.length
        || memcmp(release.bytes, first.bytes, first.length) != 0);
    RUDA_EXPECT(ruda_cli_build_in_process(
        session, 6, explicit_native_quiet, &second, &error
    ) == RUDA_EXIT_SUCCESS && second.length == 0 && error.length == 0);
    RUDA_EXPECT(ruda_cli_build_in_process(
        session, 7, emitted, &first, &error
    ) == RUDA_EXIT_SUCCESS && error.length == 0);
    RUDA_EXPECT(ruda_file_read_all(
        ruda_system_allocator(), "cp10-emitted.c",
        strlen("cp10-emitted.c"), 1024 * 1024, &source
    ) == RUDA_FILE_OK);
    RUDA_EXPECT(source.length != 0 && strstr(
        (const char *)source.data, "int main(void)"
    ) != NULL);
#if !defined(_WIN32)
    ruda_test_native_compile_and_run_configured(
        &source,
        "error[I0002]: runtime host allocation failed\n",
        70,
        "-DRUDA_G_FAIL_ALLOCATION_AT=0"
    );
#endif
    ruda_owned_bytes_dispose(&source);
    RUDA_EXPECT(remove("cp10-emitted.c") == 0);
    RUDA_EXPECT(ruda_cli_build_in_process(
        session, 5, bad_emit, &first, &error
    ) == RUDA_EXIT_USAGE && strstr(error.bytes, "error[U0005]") != NULL);
    RUDA_EXPECT(ruda_cli_build_in_process(
        session, 5, bad_engine, &first, &error
    ) == RUDA_EXIT_USAGE && strstr(error.bytes, "error[U0005]") != NULL);
}

static void ruda_cli_build_test_atomic_failures(RudaSession *session) {
    static const unsigned char original[] = "prior artifact";
    const char *bad_source[] = {
        "ruda", "build", "check-bad.ruda",
        "--output", "cp10-stale-artifact"
    };
    const char *bad_output[] = {
        "ruda", "build", "run/cp9/void.ruda",
        "--output", "cp10-missing-directory/artifact"
    };
    const char *bad_output_json[] = {
        "ruda", "build", "run/cp9/void.ruda",
        "--output", "cp10-missing-directory/artifact",
        "--diagnostic", "json"
    };
    RudaTestBuffer output;
    RudaTestBuffer error;
    RudaOwnedBytes bytes = {0};
    FILE *file = NULL;
    (void)remove("cp10-stale-artifact");
    file = fopen("cp10-stale-artifact", "wb");
    RUDA_EXPECT(file != NULL);
    if (file != NULL) {
        RUDA_EXPECT(fwrite(
            original, 1, sizeof(original) - 1, file
        ) == sizeof(original) - 1);
        RUDA_EXPECT(fclose(file) == 0);
    }
    RUDA_EXPECT(ruda_cli_build_in_process(
        session, 5, bad_source, &output, &error
    ) == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(ruda_file_read_all(
        ruda_system_allocator(), "cp10-stale-artifact",
        strlen("cp10-stale-artifact"), 1024, &bytes
    ) == RUDA_FILE_OK);
    RUDA_EXPECT(bytes.length == sizeof(original) - 1
        && memcmp(bytes.data, original, sizeof(original) - 1) == 0);
    ruda_owned_bytes_dispose(&bytes);
    RUDA_EXPECT(remove("cp10-stale-artifact") == 0);
    RUDA_EXPECT(ruda_cli_build_in_process(
        session, 5, bad_output, &output, &error
    ) == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(strstr(error.bytes, "error[B0005]") != NULL);
    RUDA_EXPECT(ruda_cli_build_in_process(
        session, 7, bad_output_json, &output, &error
    ) == RUDA_EXIT_SOURCE_FAILURE);
    RUDA_EXPECT(strstr(error.bytes, "\"code\":\"B0005\"") != NULL);
}

static void ruda_cli_build_test_allocations(void) {
    const char *arguments[] = {
        "ruda", "build", "run/cp9/void.ruda",
        "--emit", "c", "--output", "cp10-allocation.c"
    };
    size_t failure_index = 0;
    bool reached_success = false;
    (void)remove("cp10-allocation.c");
    for (failure_index = 0; failure_index < 8192; failure_index += 1) {
        RudaFailAllocator state;
        RudaSession session;
        RudaTestBuffer output;
        RudaTestBuffer error;
        int status;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        ruda_session_init(&session, ruda_fail_allocator(&state));
        status = ruda_cli_build_in_process(
            &session, 7, arguments, &output, &error
        );
        if (status == RUDA_EXIT_SUCCESS) reached_success = true;
        else RUDA_EXPECT(status == RUDA_EXIT_INTERNAL);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
    RUDA_EXPECT(remove("cp10-allocation.c") == 0);
}

static void ruda_cli_build_test_missing_toolchain(void) {
    const RudaMirFunction *dummy = (const RudaMirFunction *)(uintptr_t)1;
    RudaNativeBuildRequest request;
    RudaNativeBuildResult result;
    memset(&request, 0, sizeof(request));
    memset(&result, 0, sizeof(result));
    request.functions = &dummy;
    request.function_count = 1;
    request.root_directory = ".";
    request.root_directory_length = 1;
    request.compiler_path = "ruda-cp10-definitely-missing-compiler";
    RUDA_EXPECT(ruda_native_build(
        ruda_system_allocator(), &request, &result
    ) == RUDA_NATIVE_BUILD_TOOLCHAIN_MISSING);
    RUDA_EXPECT(result.artifact_path.data == NULL);
    ruda_native_build_result_dispose(&result);
}

void ruda_test_cli_build(void) {
    RudaSession session;
    ruda_session_init(&session, ruda_system_allocator());
    ruda_cli_build_test_artifacts(&session);
    ruda_cli_build_test_cache_and_emit(&session);
    ruda_cli_build_test_atomic_failures(&session);
    ruda_cli_build_test_allocations();
    ruda_cli_build_test_missing_toolchain();
}
