#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/platform/process.h"

enum {
    MMAY_FINAL_PREVIEW_OUTPUT_LIMIT = 256 * 1024
};

typedef struct MarimayFinalPreviewPanicCase {
    const char *source;
    const char *code;
} MarimayFinalPreviewPanicCase;

static bool mmay_final_preview_bytes_equal(
    const MarimayOwnedBytes *left,
    const MarimayOwnedBytes *right
) {
    return left->length == right->length
        && (left->length == 0
            || memcmp(left->data, right->data, left->length) == 0);
}

static bool mmay_final_preview_bytes_are(
    const MarimayOwnedBytes *bytes,
    const char *expected
) {
    size_t length = strlen(expected);
    return bytes->length == length
        && (length == 0 || memcmp(bytes->data, expected, length) == 0);
}

static bool mmay_final_preview_capture(
    const char *const arguments[],
    size_t argument_count,
    MarimayProcessCaptureResult *result
) {
    memset(result, 0, sizeof(*result));
    return mmay_process_run_capture(
        mmay_system_allocator(), arguments, argument_count,
        MMAY_FINAL_PREVIEW_OUTPUT_LIMIT,
        MMAY_FINAL_PREVIEW_OUTPUT_LIMIT,
        result
    ) == MMAY_PROCESS_OK;
}

static void mmay_final_preview_expect_success(
    const char *const arguments[],
    size_t argument_count,
    const char *expected_output
) {
    MarimayProcessCaptureResult result;
    bool captured = mmay_final_preview_capture(
        arguments, argument_count, &result
    );
    MMAY_EXPECT(captured);
    if (!captured) {
        return;
    }
    MMAY_EXPECT(!result.signaled);
    MMAY_EXPECT(result.exit_code == 0);
    MMAY_EXPECT(mmay_final_preview_bytes_are(
        &result.standard_output, expected_output
    ));
    MMAY_EXPECT(result.standard_error.length == 0);
    mmay_process_capture_result_dispose(&result);
}

static bool mmay_final_preview_build(
    const char *executable,
    const char *source,
    const char *artifact,
    bool release
) {
    const char *arguments[7];
    MarimayProcessCaptureResult result;
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
    captured = mmay_final_preview_capture(
        arguments, argument_count, &result
    );
    MMAY_EXPECT(captured);
    if (!captured) {
        return false;
    }
    MMAY_EXPECT(!result.signaled);
    MMAY_EXPECT(result.exit_code == 0);
    MMAY_EXPECT(result.standard_error.length == 0);
    MMAY_EXPECT(result.standard_output.length == strlen(artifact) + 1);
    MMAY_EXPECT(result.standard_output.length == 0
        || (memcmp(
            result.standard_output.data, artifact, strlen(artifact)
        ) == 0
            && result.standard_output.data[strlen(artifact)] == '\n'));
    mmay_process_capture_result_dispose(&result);
    return true;
}

static bool mmay_final_preview_run_artifact(
    const char *artifact,
    MarimayProcessCaptureResult *result
) {
    const char *arguments[2];
    char executable_path[512];
    int length;
    if (strchr(artifact, '/') == NULL && strchr(artifact, '\\') == NULL) {
        length = snprintf(
            executable_path, sizeof(executable_path), "./%s", artifact
        );
        MMAY_EXPECT(length > 0
            && (size_t)length < sizeof(executable_path));
        if (length <= 0 || (size_t)length >= sizeof(executable_path)) {
            return false;
        }
        arguments[0] = executable_path;
    } else {
        arguments[0] = artifact;
    }
    arguments[1] = NULL;
    return mmay_final_preview_capture(arguments, 1, result);
}

static void mmay_final_preview_expect_observation_equal(
    const MarimayProcessCaptureResult *vm,
    const MarimayProcessCaptureResult *native
) {
    MMAY_EXPECT(vm->exit_code == native->exit_code);
    MMAY_EXPECT(vm->signaled == native->signaled);
    MMAY_EXPECT(vm->signal_code == native->signal_code);
    MMAY_EXPECT(mmay_final_preview_bytes_equal(
        &vm->standard_output, &native->standard_output
    ));
    MMAY_EXPECT(mmay_final_preview_bytes_equal(
        &vm->standard_error, &native->standard_error
    ));
}

static void mmay_final_preview_test_cli_matrix(const char *executable) {
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
    MarimayProcessCaptureResult result;
    bool captured;
    mmay_final_preview_expect_success(format, 4, "");
    mmay_final_preview_expect_success(check, 3, "");
    mmay_final_preview_expect_success(run, 3, "checkpoint12 ok\n");

    captured = mmay_final_preview_capture(test, 3, &result);
    MMAY_EXPECT(captured);
    if (captured) {
        MMAY_EXPECT(!result.signaled && result.exit_code == 0);
        MMAY_EXPECT(result.standard_error.length == 0);
        MMAY_EXPECT(strstr(
            (const char *)result.standard_output.data,
            "test result: ok. 5 passed; 0 failed"
        ) != NULL);
        mmay_process_capture_result_dispose(&result);
    }

    captured = mmay_final_preview_capture(test_both, 5, &result);
    MMAY_EXPECT(captured);
    if (captured) {
        MMAY_EXPECT(!result.signaled && result.exit_code == 0);
        MMAY_EXPECT(result.standard_error.length == 0);
        MMAY_EXPECT(strstr(
            (const char *)result.standard_output.data,
            "test result: ok. 5 passed; 0 failed"
        ) != NULL);
        mmay_process_capture_result_dispose(&result);
    }
}

static void mmay_final_preview_test_native_profiles(
    const char *executable,
    const char *source,
    const char *artifact_prefix,
    const char *expected_output
) {
    const char *run_arguments[] = {
        executable, "run", source, NULL
    };
    const char *suffixes[] = {"-debug", "-release"};
    MarimayProcessCaptureResult vm;
    size_t index;
    bool captured = mmay_final_preview_capture(run_arguments, 3, &vm);
    MMAY_EXPECT(captured);
    if (!captured) {
        return;
    }
    MMAY_EXPECT(!vm.signaled && vm.exit_code == 0);
    MMAY_EXPECT(mmay_final_preview_bytes_are(
        &vm.standard_output, expected_output
    ));
    MMAY_EXPECT(vm.standard_error.length == 0);
    for (index = 0; index < 2; index += 1) {
        char artifact[128];
        MarimayProcessCaptureResult native;
        int length = snprintf(
            artifact, sizeof(artifact), "%s%s", artifact_prefix,
            suffixes[index]
        );
        MMAY_EXPECT(length > 0 && (size_t)length < sizeof(artifact));
        if (length <= 0 || (size_t)length >= sizeof(artifact)) {
            continue;
        }
        if (mmay_final_preview_build(
                executable, source, artifact, index == 1
            )) {
            captured = mmay_final_preview_run_artifact(artifact, &native);
            MMAY_EXPECT(captured);
            if (captured) {
                mmay_final_preview_expect_observation_equal(&vm, &native);
                mmay_process_capture_result_dispose(&native);
            }
            MMAY_EXPECT(remove(artifact) == 0);
        }
    }
    mmay_process_capture_result_dispose(&vm);
}

static void mmay_final_preview_test_panics(const char *executable) {
    static const MarimayFinalPreviewPanicCase cases[] = {
        {"run/cp9/panic-explicit.my", "G4000"},
        {"run/cp9/panic-arithmetic.my", "G4001"},
        {"run/cp9/panic-division.my", "G4001"},
        {"run/cp9/panic-shift.my", "G4001"},
        {"run/cp9/panic-bounds.my", "G4002"}
    };
    size_t index;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        const char *run_arguments[] = {
            executable, "run", cases[index].source, NULL
        };
        const char artifact[] = "cp12-panic-native";
        MarimayProcessCaptureResult vm;
        MarimayProcessCaptureResult native;
        bool captured = mmay_final_preview_capture(run_arguments, 3, &vm);
        MMAY_EXPECT(captured);
        if (!captured) {
            continue;
        }
        MMAY_EXPECT(!vm.signaled && vm.exit_code == 1);
        MMAY_EXPECT(vm.standard_output.length == 0);
        MMAY_EXPECT(strstr(
            (const char *)vm.standard_error.data, cases[index].code
        ) != NULL);
        if (mmay_final_preview_build(
                executable, cases[index].source, artifact, false
            )) {
            captured = mmay_final_preview_run_artifact(artifact, &native);
            MMAY_EXPECT(captured);
            if (captured) {
                mmay_final_preview_expect_observation_equal(&vm, &native);
                mmay_process_capture_result_dispose(&native);
            }
            MMAY_EXPECT(remove(artifact) == 0);
        }
        mmay_process_capture_result_dispose(&vm);
    }
}

void mmay_test_final_preview(void) {
    const char *executable = mmay_test_toolchain_executable();
    MMAY_EXPECT(executable != NULL);
    if (executable == NULL) {
        return;
    }
    mmay_final_preview_test_cli_matrix(executable);
    mmay_final_preview_test_native_profiles(
        executable, "checkpoint12", "cp12-preview",
        "checkpoint12 ok\n"
    );
    mmay_final_preview_test_native_profiles(
        executable, "examples/matrix.my", "cp12-matrix",
        "415.0\n"
    );
    mmay_final_preview_test_panics(executable);
}
