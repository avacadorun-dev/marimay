#include "test_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/bytecode/bytecode.h"
#include "mmay/bytecode/vm.h"
#include "mmay/bytecode/vm_arguments.h"
#include "mmay/cli.h"
#include "mmay/platform/process.h"
#include "mmay/runtime/program_arguments.h"
#include "mmay/session.h"

static int mmay_cp31_cli(
    MarimaySession *session,
    int count,
    const char *const arguments[],
    MarimayTestBuffer *output,
    MarimayTestBuffer *error
) {
    mmay_test_buffer_init(output);
    mmay_test_buffer_init(error);
    return mmay_cli_run(
        session, count, arguments, (MarimayReadStream){0},
        mmay_test_buffer_writer(output), mmay_test_buffer_writer(error)
    );
}

static void mmay_cp31_expect_bytes(
    const MarimayTestBuffer *buffer,
    const unsigned char *expected,
    size_t expected_length
) {
    MMAY_EXPECT(buffer->length == expected_length);
    MMAY_EXPECT(expected_length == 0
        || memcmp(buffer->bytes, expected, expected_length) == 0);
}

static void mmay_cp31_run_contract(MarimaySession *session) {
    static const unsigned char expected[] =
        "alpha\ntwo words\n\n\xd0\x9c\xd0\xb8\xd1\x80\n--\n-dash\n"
        "quote\"value\nC:\\trail\\\ncleanup\n";
    const char *arguments[] = {
        "mmay", "run", "run/cp31/arguments.my", "--", "alpha",
        "two words", "", "\xd0\x9c\xd0\xb8\xd1\x80", "--", "-dash",
        "quote\"value", "C:\\trail\\"
    };
    const char *maximum_status[] = {
        "mmay", "run", "run/cp31/arguments.my", "--", "alpha"
    };
    const char *overlapping_status[] = {
        "mmay", "run", "run/cp31/arguments.my", "--", "status", "70"
    };
    const char *zero[] = {
        "mmay", "run", "run/cp31/arguments.my"
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    MMAY_EXPECT(mmay_cp31_cli(
        session, 12, arguments, &output, &error
    ) == 23);
    mmay_cp31_expect_bytes(&output, expected, sizeof(expected) - 1);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(mmay_cp31_cli(
        session, 3, zero, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strcmp(output.bytes, "zero\ncleanup\n") == 0);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(mmay_cp31_cli(
        session, 5, maximum_status, &output, &error
    ) == 255);
    MMAY_EXPECT(strcmp(output.bytes, "alpha\ncleanup\n") == 0);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(mmay_cp31_cli(
        session, 6, overlapping_status, &output, &error
    ) == MMAY_EXIT_INTERNAL);
    MMAY_EXPECT(strcmp(output.bytes, "status\n70\ncleanup\n") == 0);
    MMAY_EXPECT(error.length == 0);
}

static void mmay_cp31_status_result_panic_contract(
    MarimaySession *session
) {
    const char *invalid[] = {
        "mmay", "run", "run/cp31/invalid-status.my"
    };
    const char *invalid_json[] = {
        "mmay", "run", "run/cp31/invalid-status.my",
        "--diagnostic", "json"
    };
    const char *ok[] = {
        "mmay", "run", "run/cp31/result-status.my", "--", "alpha"
    };
    const char *error_result[] = {
        "mmay", "run", "run/cp31/result-status.my"
    };
    const char *invalid_result[] = {
        "mmay", "run", "run/cp31/result-status.my", "--", "a", "b"
    };
    const char *panic[] = {
        "mmay", "run", "run/cp31/panic.my", "--", "boom"
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    MMAY_EXPECT(mmay_cp31_cli(
        session, 3, invalid, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(strcmp(output.bytes, "cleanup\n") == 0);
    MMAY_EXPECT(strcmp(
        error.bytes,
        "error[G4007]: program exit status is outside 0..255\n"
    ) == 0);
    MMAY_EXPECT(mmay_cp31_cli(
        session, 5, invalid_json, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(strcmp(output.bytes, "cleanup\n") == 0);
    MMAY_EXPECT(strstr(error.bytes, "\"code\":\"G4007\"") != NULL);
    MMAY_EXPECT(mmay_cp31_cli(
        session, 5, ok, &output, &error
    ) == 7);
    MMAY_EXPECT(strcmp(output.bytes, "alpha\ncleanup\n") == 0);
    MMAY_EXPECT(error.length == 0);
    MMAY_EXPECT(mmay_cp31_cli(
        session, 3, error_result, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(strcmp(output.bytes, "cleanup\n") == 0);
    MMAY_EXPECT(strcmp(error.bytes, "error[G4003]: missing argument\n") == 0);
    MMAY_EXPECT(mmay_cp31_cli(
        session, 6, invalid_result, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(strcmp(output.bytes, "cleanup\n") == 0);
    MMAY_EXPECT(strcmp(
        error.bytes,
        "error[G4007]: program exit status is outside 0..255\n"
    ) == 0);
    MMAY_EXPECT(mmay_cp31_cli(
        session, 5, panic, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(strcmp(output.bytes, "cleanup\n") == 0);
    MMAY_EXPECT(strcmp(error.bytes, "panic[G4000]: boom\n") == 0);
}

static void mmay_cp31_invalid_encoding(MarimaySession *session) {
    char invalid_utf8[] = "\xff";
    const char *arguments[] = {
        "mmay", "run", "run/cp31/arguments.my", "--", invalid_utf8
    };
    const char *json[] = {
        "mmay", "run", "run/cp31/arguments.my", "--diagnostic", "json",
        "--", invalid_utf8
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    MMAY_EXPECT(mmay_cp31_cli(
        session, 5, arguments, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strcmp(
        error.bytes, "error[G4006]: program argument is not valid UTF-8\n"
    ) == 0);
    MMAY_EXPECT(mmay_cp31_cli(
        session, 7, json, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strstr(error.bytes, "\"code\":\"G4006\"") != NULL);
}

static void mmay_cp31_argument_limit_diagnostics(void) {
    static char oversized[MMAY_PROGRAM_ARGUMENT_MAX_SINGLE_BYTES + 2];
    static const char *count_arguments[
        MMAY_PROGRAM_ARGUMENT_MAX_COUNT + 5
    ];
    const char *byte_arguments[] = {
        "mmay", "run", "run/cp31/arguments.my", "--", oversized
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    MarimayFailAllocator state;
    MarimaySession session;
    size_t index = 0;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    mmay_session_init(&session, mmay_fail_allocator(&state));
    memset(oversized, 'a', sizeof(oversized) - 1);
    oversized[sizeof(oversized) - 1] = 0;
    MMAY_EXPECT(mmay_cp31_cli(
        &session, 5, byte_arguments, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strcmp(
        error.bytes,
        "error[G4006]: program arguments exceed the 1048576-byte total or 262144-byte per-argument limit\n"
    ) == 0);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);

    count_arguments[0] = "mmay";
    count_arguments[1] = "run";
    count_arguments[2] = "run/cp31/arguments.my";
    count_arguments[3] = "--";
    for (index = 4; index < sizeof(count_arguments)
            / sizeof(count_arguments[0]); index += 1) {
        count_arguments[index] = "";
    }
    MMAY_EXPECT(mmay_cp31_cli(
        &session,
        (int)(sizeof(count_arguments) / sizeof(count_arguments[0])),
        count_arguments, &output, &error
    ) == MMAY_EXIT_SOURCE_FAILURE);
    MMAY_EXPECT(output.length == 0);
    MMAY_EXPECT(strcmp(
        error.bytes,
        "error[G4006]: program argument count exceeds 4096\n"
    ) == 0);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static void mmay_cp31_vm_argument_boundary(void) {
    static const MarimayBytecodeType array_parameter[] = {
        MMAY_BYTECODE_TYPE_ARRAY
    };
    static const MarimayBytecodeType scalar_parameter[] = {
        MMAY_BYTECODE_TYPE_I64
    };
    static const unsigned char valid_bytes[] = "alpha";
    static const unsigned char nul_bytes[] = {'a', 0, 'b'};
    MarimayProgramArgument valid_value = {
        valid_bytes, sizeof(valid_bytes) - 1
    };
    MarimayProgramArgument invalid_value = {
        nul_bytes, sizeof(nul_bytes)
    };
    MarimayBytecodeFunction entry;
    MarimayBytecodeFunction wrong_entry;
    MarimayBytecodeExecution execution;
    MarimayBytecodeValue result;
    uint32_t layout = MMAY_BYTECODE_NO_LAYOUT;
    memset(&entry, 0, sizeof(entry));
    memset(&wrong_entry, 0, sizeof(wrong_entry));
    memset(&execution, 0, sizeof(execution));
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_bytecode_function_init(
        &entry, mmay_system_allocator(), MMAY_BYTECODE_TYPE_VOID, 1,
        array_parameter, 1, mmay_bytecode_function_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_add_array_type(
        &entry, MMAY_BYTECODE_TYPE_STRING, MMAY_BYTECODE_NO_LAYOUT, &layout
    ));
    MMAY_EXPECT(mmay_bytecode_set_register_layout(&entry, 0, layout));
    MMAY_EXPECT(mmay_bytecode_function_init(
        &wrong_entry, mmay_system_allocator(), MMAY_BYTECODE_TYPE_VOID, 1,
        scalar_parameter, 1, mmay_bytecode_function_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_execution_program_arguments(
        &execution, &entry, (MarimayProgramArgumentList){&valid_value, 1},
        &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_ARRAY);
    MMAY_EXPECT(mmay_bytecode_execution_program_arguments(
        &execution, &entry, (MarimayProgramArgumentList){&invalid_value, 1},
        &result
    ) == MMAY_BYTECODE_RUN_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_bytecode_execution_program_arguments(
        &execution, &wrong_entry,
        (MarimayProgramArgumentList){&valid_value, 1}, &result
    ) == MMAY_BYTECODE_RUN_INVALID_BYTECODE);
    MMAY_EXPECT(mmay_bytecode_execution_program_arguments(
        NULL, &entry, (MarimayProgramArgumentList){&valid_value, 1}, &result
    ) == MMAY_BYTECODE_RUN_INVALID_ARGUMENT);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_function_dispose(&wrong_entry);
    mmay_bytecode_function_dispose(&entry);
}

static void mmay_cp31_test_injection(MarimaySession *session) {
    const char *arguments[] = {
        "mmay", "test", "checkpoint31-args", "--engine", "both", "--",
        "alpha", "two words", "", "\xd0\x9c\xd0\xb8\xd1\x80", "--"
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    MMAY_EXPECT(mmay_cp31_cli(
        session, 11, arguments, &output, &error
    ) == MMAY_EXIT_SUCCESS);
    MMAY_EXPECT(strstr(output.bytes, "injectedArguments ... ok") != NULL);
    MMAY_EXPECT(strstr(
        output.bytes, "zeroParameterTestsRemainValid ... ok"
    ) != NULL);
    MMAY_EXPECT(strstr(output.bytes, "2 passed; 0 failed") != NULL);
    MMAY_EXPECT(error.length == 0);
}

static bool mmay_cp31_build(
    MarimaySession *session,
    const char *source,
    const char *artifact
) {
    const char *arguments[] = {
        "mmay", "build", source, "--release", "--output", artifact
    };
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    (void)remove(artifact);
    if (mmay_cp31_cli(
            session, 6, arguments, &output, &error
        ) != MMAY_EXIT_SUCCESS) {
        MMAY_EXPECT(false);
        return false;
    }
    MMAY_EXPECT(error.length == 0);
    return true;
}

static void mmay_cp31_native_artifact(MarimaySession *session) {
    static const unsigned char expected_output[] =
        "alpha\ntwo words\n\n\xd0\x9c\xd0\xb8\xd1\x80\n--\n-dash\n"
        "quote\"value\nC:\\trail\\\ncleanup\n";
    const char *artifact = "cp31-arguments-artifact";
    const char *result_artifact = "cp31-result-artifact";
    const char *invalid_artifact = "cp31-invalid-artifact";
    const char *panic_artifact = "cp31-panic-artifact";
    const char *arguments[] = {
        "./cp31-arguments-artifact", "alpha", "two words", "",
        "\xd0\x9c\xd0\xb8\xd1\x80", "--", "-dash", "quote\"value",
        "C:\\trail\\", NULL
    };
    const char *maximum_status_arguments[] = {
        "./cp31-arguments-artifact", "alpha", NULL
    };
    const char *overlapping_status_arguments[] = {
        "./cp31-arguments-artifact", "status", "70", NULL
    };
    const char *result_arguments[] = {
        "./cp31-result-artifact", "alpha", NULL
    };
    const char *result_error_arguments[] = {
        "./cp31-result-artifact", NULL
    };
    const char *result_invalid_arguments[] = {
        "./cp31-result-artifact", "a", "b", NULL
    };
    const char *invalid_arguments[] = {
        "./cp31-invalid-artifact", NULL
    };
    const char *panic_arguments[] = {
        "./cp31-panic-artifact", "boom", NULL
    };
    MarimayProcessCaptureResult capture;
    memset(&capture, 0, sizeof(capture));
    if (!mmay_cp31_build(
            session, "run/cp31/arguments.my", artifact
        )) return;
    MMAY_EXPECT(mmay_process_run_capture(
        mmay_system_allocator(), arguments, 9, 65536, 65536, &capture
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!capture.signaled && capture.exit_code == 23);
    MMAY_EXPECT(capture.standard_output.length == sizeof(expected_output) - 1
        && memcmp(
            capture.standard_output.data, expected_output,
            sizeof(expected_output) - 1
        ) == 0);
    MMAY_EXPECT(capture.standard_error.length == 0);
    mmay_process_capture_result_dispose(&capture);
    memset(&capture, 0, sizeof(capture));
    MMAY_EXPECT(mmay_process_run_capture(
        mmay_system_allocator(), maximum_status_arguments, 2,
        65536, 65536, &capture
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!capture.signaled && capture.exit_code == 255);
    MMAY_EXPECT(capture.standard_output.length
        == sizeof("alpha\ncleanup\n") - 1
        && memcmp(
            capture.standard_output.data, "alpha\ncleanup\n",
            sizeof("alpha\ncleanup\n") - 1
        ) == 0);
    MMAY_EXPECT(capture.standard_error.length == 0);
    mmay_process_capture_result_dispose(&capture);
    memset(&capture, 0, sizeof(capture));
    MMAY_EXPECT(mmay_process_run_capture(
        mmay_system_allocator(), overlapping_status_arguments, 3,
        65536, 65536, &capture
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!capture.signaled && capture.exit_code == 70);
    MMAY_EXPECT(capture.standard_output.length
        == sizeof("status\n70\ncleanup\n") - 1
        && memcmp(
            capture.standard_output.data, "status\n70\ncleanup\n",
            sizeof("status\n70\ncleanup\n") - 1
        ) == 0);
    MMAY_EXPECT(capture.standard_error.length == 0);
    mmay_process_capture_result_dispose(&capture);
    MMAY_EXPECT(remove(artifact) == 0);

    memset(&capture, 0, sizeof(capture));
    if (!mmay_cp31_build(
            session, "run/cp31/result-status.my", result_artifact
        )) return;
    MMAY_EXPECT(mmay_process_run_capture(
        mmay_system_allocator(), result_arguments, 2,
        65536, 65536, &capture
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!capture.signaled && capture.exit_code == 7);
    MMAY_EXPECT(capture.standard_output.length
        == sizeof("alpha\ncleanup\n") - 1
        && memcmp(
            capture.standard_output.data, "alpha\ncleanup\n",
            sizeof("alpha\ncleanup\n") - 1
        ) == 0);
    MMAY_EXPECT(capture.standard_error.length == 0);
    mmay_process_capture_result_dispose(&capture);
    memset(&capture, 0, sizeof(capture));
    MMAY_EXPECT(mmay_process_run_capture(
        mmay_system_allocator(), result_error_arguments, 1,
        65536, 65536, &capture
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!capture.signaled && capture.exit_code == 1);
    MMAY_EXPECT(capture.standard_output.length == sizeof("cleanup\n") - 1
        && memcmp(
            capture.standard_output.data, "cleanup\n",
            sizeof("cleanup\n") - 1
        ) == 0);
    MMAY_EXPECT(capture.standard_error.length
        == sizeof("error[G4003]: missing argument\n") - 1
        && memcmp(
            capture.standard_error.data,
            "error[G4003]: missing argument\n",
            sizeof("error[G4003]: missing argument\n") - 1
        ) == 0);
    mmay_process_capture_result_dispose(&capture);
    memset(&capture, 0, sizeof(capture));
    MMAY_EXPECT(mmay_process_run_capture(
        mmay_system_allocator(), result_invalid_arguments, 3,
        65536, 65536, &capture
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!capture.signaled && capture.exit_code == 1);
    MMAY_EXPECT(capture.standard_output.length == sizeof("cleanup\n") - 1
        && memcmp(
            capture.standard_output.data, "cleanup\n",
            sizeof("cleanup\n") - 1
        ) == 0);
    MMAY_EXPECT(capture.standard_error.length
        == sizeof(
            "error[G4007]: program exit status is outside 0..255\n"
        ) - 1 && memcmp(
            capture.standard_error.data,
            "error[G4007]: program exit status is outside 0..255\n",
            sizeof(
                "error[G4007]: program exit status is outside 0..255\n"
            ) - 1
        ) == 0);
    mmay_process_capture_result_dispose(&capture);
    MMAY_EXPECT(remove(result_artifact) == 0);

    memset(&capture, 0, sizeof(capture));
    if (!mmay_cp31_build(
            session, "run/cp31/invalid-status.my", invalid_artifact
        )) return;
    MMAY_EXPECT(mmay_process_run_capture(
        mmay_system_allocator(), invalid_arguments, 1,
        65536, 65536, &capture
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!capture.signaled && capture.exit_code == 1);
    MMAY_EXPECT(capture.standard_output.length == sizeof("cleanup\n") - 1
        && memcmp(
            capture.standard_output.data, "cleanup\n",
            sizeof("cleanup\n") - 1
        ) == 0);
    MMAY_EXPECT(capture.standard_error.length
        == sizeof(
            "error[G4007]: program exit status is outside 0..255\n"
        ) - 1 && memcmp(
            capture.standard_error.data,
            "error[G4007]: program exit status is outside 0..255\n",
            sizeof(
                "error[G4007]: program exit status is outside 0..255\n"
            ) - 1
        ) == 0);
    mmay_process_capture_result_dispose(&capture);
    MMAY_EXPECT(remove(invalid_artifact) == 0);

    memset(&capture, 0, sizeof(capture));
    if (!mmay_cp31_build(
            session, "run/cp31/panic.my", panic_artifact
        )) return;
    MMAY_EXPECT(mmay_process_run_capture(
        mmay_system_allocator(), panic_arguments, 2,
        65536, 65536, &capture
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!capture.signaled && capture.exit_code == 1);
    MMAY_EXPECT(capture.standard_output.length == sizeof("cleanup\n") - 1
        && memcmp(
            capture.standard_output.data, "cleanup\n",
            sizeof("cleanup\n") - 1
        ) == 0);
    MMAY_EXPECT(capture.standard_error.length
        == sizeof("panic[G4000]: boom\n") - 1
        && memcmp(
            capture.standard_error.data, "panic[G4000]: boom\n",
            sizeof("panic[G4000]: boom\n") - 1
        ) == 0);
    mmay_process_capture_result_dispose(&capture);
    MMAY_EXPECT(remove(panic_artifact) == 0);
}

static bool mmay_cp31_allocation_case(
    const char *const *arguments,
    size_t failure_index
) {
    MarimayFailAllocator state;
    MarimaySession session;
    MarimayTestBuffer output;
    MarimayTestBuffer error;
    int status;
    bool argument_failure = false;
    const char *trace = getenv("MMAY_TEST_TRACE_ALLOCATIONS");
    if (trace != NULL && strcmp(trace, "1") == 0) {
        /* Emit before entering the compiler: a sanitizer can abort before the
           normal assertion reports which deterministic failure was injected. */
        (void)fprintf(stderr, "CP31 allocation failure index: %zu\n", failure_index);
        (void)fflush(stderr);
    }
    mmay_fail_allocator_init(
        &state, mmay_system_allocator(), failure_index
    );
    mmay_session_init(&session, mmay_fail_allocator(&state));
    status = mmay_cp31_cli(&session, 5, arguments, &output, &error);
    if (status != 255) {
        MMAY_EXPECT(status == MMAY_EXIT_INTERNAL);
        argument_failure = strcmp(
            error.bytes,
            "error[I0002]: runtime host allocation failed\n"
        ) == 0;
    }
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    return argument_failure;
}

void mmay_test_cp31_program_argument_allocation_shard(
    size_t shard_index,
    size_t shard_count
) {
    const char *arguments[] = {
        "mmay", "run", "run/cp31/arguments.my", "--", "alpha"
    };
    const size_t failure_prefix_limit = 4096;
    size_t allocation_attempts = 0;
    size_t failure_index = 0;
    size_t failure_prefix = 0;
    MMAY_EXPECT(shard_count != 0 && shard_index < shard_count);
    if (shard_count == 0 || shard_index >= shard_count) return;
    {
        MarimayFailAllocator state;
        MarimaySession session;
        MarimayTestBuffer output;
        MarimayTestBuffer error;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), SIZE_MAX
        );
        mmay_session_init(&session, mmay_fail_allocator(&state));
        MMAY_EXPECT(mmay_cp31_cli(
            &session, 5, arguments, &output, &error
        ) == 255);
        allocation_attempts = state.attempts;
        MMAY_EXPECT(allocation_attempts != SIZE_MAX);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    /* Owner APIs carry exhaustive sweeps; this full-CLI test covers a bounded
       orchestration prefix plus the late program-argument allocation phase. */
    failure_prefix = allocation_attempts < failure_prefix_limit
        ? allocation_attempts : failure_prefix_limit;
    for (failure_index = shard_index; failure_index < failure_prefix;
        failure_index += shard_count) {
        (void)mmay_cp31_allocation_case(arguments, failure_index);
    }
    MMAY_EXPECT(allocation_attempts >= 32);
    if (allocation_attempts >= 32 && shard_index == 0) {
        MMAY_EXPECT(mmay_cp31_allocation_case(
            arguments, allocation_attempts - 32
        ));
    }
}

void mmay_test_cp31_program_arguments(void) {
    MarimaySession session;
    mmay_session_init(&session, mmay_system_allocator());
    mmay_cp31_run_contract(&session);
    mmay_cp31_status_result_panic_contract(&session);
    mmay_cp31_invalid_encoding(&session);
    mmay_cp31_argument_limit_diagnostics();
    mmay_cp31_vm_argument_boundary();
    mmay_cp31_test_injection(&session);
    mmay_cp31_native_artifact(&session);
}
