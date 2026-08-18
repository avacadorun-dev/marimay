#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/platform/process.h"

enum {
    RUDA_FINAL_PREVIEW_OUTPUT_LIMIT = 256 * 1024
};

typedef struct RudaFinalPreviewPanicCase {
    const char *source;
    const char *code;
} RudaFinalPreviewPanicCase;

static bool ruda_final_preview_bytes_equal(
    const RudaOwnedBytes *left,
    const RudaOwnedBytes *right
) {
    return left->length == right->length
        && (left->length == 0
            || memcmp(left->data, right->data, left->length) == 0);
}

static bool ruda_final_preview_bytes_are(
    const RudaOwnedBytes *bytes,
    const char *expected
) {
    size_t length = strlen(expected);
    return bytes->length == length
        && (length == 0 || memcmp(bytes->data, expected, length) == 0);
}

static bool ruda_final_preview_capture(
    const char *const arguments[],
    size_t argument_count,
    RudaProcessCaptureResult *result
) {
    memset(result, 0, sizeof(*result));
    return ruda_process_run_capture(
        ruda_system_allocator(), arguments, argument_count,
        RUDA_FINAL_PREVIEW_OUTPUT_LIMIT,
        RUDA_FINAL_PREVIEW_OUTPUT_LIMIT,
        result
    ) == RUDA_PROCESS_OK;
}

static void ruda_final_preview_expect_success(
    const char *const arguments[],
    size_t argument_count,
    const char *expected_output
) {
    RudaProcessCaptureResult result;
    bool captured = ruda_final_preview_capture(
        arguments, argument_count, &result
    );
    RUDA_EXPECT(captured);
    if (!captured) {
        return;
    }
    RUDA_EXPECT(!result.signaled);
    RUDA_EXPECT(result.exit_code == 0);
    RUDA_EXPECT(ruda_final_preview_bytes_are(
        &result.standard_output, expected_output
    ));
    RUDA_EXPECT(result.standard_error.length == 0);
    ruda_process_capture_result_dispose(&result);
}

static bool ruda_final_preview_build(
    const char *executable,
    const char *source,
    const char *artifact,
    bool release
) {
    const char *arguments[7];
    RudaProcessCaptureResult result;
    size_t argument_count = 0;
    bool captured;
    (void)remove(artifact);
    arguments[argument_count++] = executable;
    arguments[argument_count++] = "build";
    arguments[argument_count++] = source;
    if (release) {
        arguments[argument_count++] = "--release";
    }
    arguments[argument_count++] = "--output";
    arguments[argument_count++] = artifact;
    arguments[argument_count] = NULL;
    captured = ruda_final_preview_capture(
        arguments, argument_count, &result
    );
    RUDA_EXPECT(captured);
    if (!captured) {
        return false;
    }
    RUDA_EXPECT(!result.signaled);
    RUDA_EXPECT(result.exit_code == 0);
    RUDA_EXPECT(result.standard_error.length == 0);
    RUDA_EXPECT(result.standard_output.length == strlen(artifact) + 1);
    RUDA_EXPECT(result.standard_output.length == 0
        || (memcmp(
            result.standard_output.data, artifact, strlen(artifact)
        ) == 0
            && result.standard_output.data[strlen(artifact)] == '\n'));
    ruda_process_capture_result_dispose(&result);
    return true;
}

static bool ruda_final_preview_run_artifact(
    const char *artifact,
    RudaProcessCaptureResult *result
) {
    const char *arguments[2];
    char executable_path[512];
    int length;
    if (strchr(artifact, '/') == NULL && strchr(artifact, '\\') == NULL) {
        length = snprintf(
            executable_path, sizeof(executable_path), "./%s", artifact
        );
        RUDA_EXPECT(length > 0
            && (size_t)length < sizeof(executable_path));
        if (length <= 0 || (size_t)length >= sizeof(executable_path)) {
            return false;
        }
        arguments[0] = executable_path;
    } else {
        arguments[0] = artifact;
    }
    arguments[1] = NULL;
    return ruda_final_preview_capture(arguments, 1, result);
}

static void ruda_final_preview_expect_observation_equal(
    const RudaProcessCaptureResult *vm,
    const RudaProcessCaptureResult *native
) {
    RUDA_EXPECT(vm->exit_code == native->exit_code);
    RUDA_EXPECT(vm->signaled == native->signaled);
    RUDA_EXPECT(vm->signal_code == native->signal_code);
    RUDA_EXPECT(ruda_final_preview_bytes_equal(
        &vm->standard_output, &native->standard_output
    ));
    RUDA_EXPECT(ruda_final_preview_bytes_equal(
        &vm->standard_error, &native->standard_error
    ));
}

static void ruda_final_preview_test_cli_matrix(const char *executable) {
    const char *format[] = {
        executable, "fmt", "checkpoint12", "--check", NULL
    };
    const char *check[] = {
        executable, "check", "checkpoint12", NULL
    };
    const char *run[] = {
        executable, "run", "checkpoint12", NULL
    };
    const char *test[] = {
        executable, "test", "checkpoint12", NULL
    };
    const char *test_both[] = {
        executable, "test", "checkpoint12", "--engine", "both", NULL
    };
    RudaProcessCaptureResult result;
    bool captured;
    ruda_final_preview_expect_success(format, 4, "");
    ruda_final_preview_expect_success(check, 3, "");
    ruda_final_preview_expect_success(run, 3, "checkpoint12 ok\n");

    captured = ruda_final_preview_capture(test, 3, &result);
    RUDA_EXPECT(captured);
    if (captured) {
        RUDA_EXPECT(!result.signaled && result.exit_code == 0);
        RUDA_EXPECT(result.standard_error.length == 0);
        RUDA_EXPECT(strstr(
            (const char *)result.standard_output.data,
            "test result: ok. 5 passed; 0 failed"
        ) != NULL);
        ruda_process_capture_result_dispose(&result);
    }

    captured = ruda_final_preview_capture(test_both, 5, &result);
    RUDA_EXPECT(captured);
    if (captured) {
        RUDA_EXPECT(!result.signaled && result.exit_code == 0);
        RUDA_EXPECT(result.standard_error.length == 0);
        RUDA_EXPECT(strstr(
            (const char *)result.standard_output.data,
            "test result: ok. 5 passed; 0 failed"
        ) != NULL);
        ruda_process_capture_result_dispose(&result);
    }
}

static void ruda_final_preview_test_native_profiles(
    const char *executable,
    const char *source,
    const char *artifact_prefix,
    const char *expected_output
) {
    const char *run_arguments[] = {
        executable, "run", source, NULL
    };
    const char *suffixes[] = {"-debug", "-release"};
    RudaProcessCaptureResult vm;
    size_t index;
    bool captured = ruda_final_preview_capture(run_arguments, 3, &vm);
    RUDA_EXPECT(captured);
    if (!captured) {
        return;
    }
    RUDA_EXPECT(!vm.signaled && vm.exit_code == 0);
    RUDA_EXPECT(ruda_final_preview_bytes_are(
        &vm.standard_output, expected_output
    ));
    RUDA_EXPECT(vm.standard_error.length == 0);
    for (index = 0; index < 2; index += 1) {
        char artifact[128];
        RudaProcessCaptureResult native;
        int length = snprintf(
            artifact, sizeof(artifact), "%s%s", artifact_prefix,
            suffixes[index]
        );
        RUDA_EXPECT(length > 0 && (size_t)length < sizeof(artifact));
        if (length <= 0 || (size_t)length >= sizeof(artifact)) {
            continue;
        }
        if (ruda_final_preview_build(
                executable, source, artifact, index == 1
            )) {
            captured = ruda_final_preview_run_artifact(artifact, &native);
            RUDA_EXPECT(captured);
            if (captured) {
                ruda_final_preview_expect_observation_equal(&vm, &native);
                ruda_process_capture_result_dispose(&native);
            }
            RUDA_EXPECT(remove(artifact) == 0);
        }
    }
    ruda_process_capture_result_dispose(&vm);
}

static void ruda_final_preview_test_panics(const char *executable) {
    static const RudaFinalPreviewPanicCase cases[] = {
        {"run/cp9/panic-explicit.ruda", "G4000"},
        {"run/cp9/panic-arithmetic.ruda", "G4001"},
        {"run/cp9/panic-division.ruda", "G4001"},
        {"run/cp9/panic-shift.ruda", "G4001"},
        {"run/cp9/panic-bounds.ruda", "G4002"}
    };
    size_t index;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        const char *run_arguments[] = {
            executable, "run", cases[index].source, NULL
        };
        const char artifact[] = "cp12-panic-native";
        RudaProcessCaptureResult vm;
        RudaProcessCaptureResult native;
        bool captured = ruda_final_preview_capture(run_arguments, 3, &vm);
        RUDA_EXPECT(captured);
        if (!captured) {
            continue;
        }
        RUDA_EXPECT(!vm.signaled && vm.exit_code == 1);
        RUDA_EXPECT(vm.standard_output.length == 0);
        RUDA_EXPECT(strstr(
            (const char *)vm.standard_error.data, cases[index].code
        ) != NULL);
        if (ruda_final_preview_build(
                executable, cases[index].source, artifact, false
            )) {
            captured = ruda_final_preview_run_artifact(artifact, &native);
            RUDA_EXPECT(captured);
            if (captured) {
                ruda_final_preview_expect_observation_equal(&vm, &native);
                ruda_process_capture_result_dispose(&native);
            }
            RUDA_EXPECT(remove(artifact) == 0);
        }
        ruda_process_capture_result_dispose(&vm);
    }
}

void ruda_test_final_preview(void) {
    const char *executable = ruda_test_toolchain_executable();
    RUDA_EXPECT(executable != NULL);
    if (executable == NULL) {
        return;
    }
    ruda_final_preview_test_cli_matrix(executable);
    ruda_final_preview_test_native_profiles(
        executable, "checkpoint12", "cp12-preview",
        "checkpoint12 ok\n"
    );
    ruda_final_preview_test_native_profiles(
        executable, "examples/matrix.ruda", "cp12-matrix",
        "415.0\n"
    );
    ruda_final_preview_test_panics(executable);
}
