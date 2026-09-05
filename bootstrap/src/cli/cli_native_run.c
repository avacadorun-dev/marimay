#include "cli_native_run.h"

#include <stdint.h>
#include <string.h>

#include "mmay/generated/diagnostic_codes.h"
#include "mmay/platform/process.h"

enum {
    MMAY_CLI_NATIVE_RUN_INPUT_LIMIT = 16 * 1024 * 1024,
    MMAY_CLI_NATIVE_RUN_OUTPUT_LIMIT = 16 * 1024 * 1024
};

bool mmay_cli_native_arguments_init(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *artifact_path,
    const MarimayCliOptions *options,
    MarimayCliNativeArguments *arguments
) {
    size_t pointer_count = 0;
    size_t index = 0;
    if (arguments == NULL) return false;
    memset(arguments, 0, sizeof(*arguments));
    if (allocator.allocate == NULL || artifact_path == NULL
        || artifact_path->data == NULL || options == NULL
        || (options->program_argument_count != 0
            && options->program_arguments == NULL)
        || !mmay_size_add(
            options->program_argument_count, 1, &arguments->count
        ) || !mmay_size_add(arguments->count, 1, &pointer_count)
        || !mmay_size_multiply(
            pointer_count, sizeof(*arguments->items),
            &arguments->allocated_bytes
        )) {
        memset(arguments, 0, sizeof(*arguments));
        return false;
    }
    arguments->allocator = allocator;
    arguments->items = mmay_allocate(
        &arguments->allocator, arguments->allocated_bytes
    );
    if (arguments->items == NULL) {
        memset(arguments, 0, sizeof(*arguments));
        return false;
    }
    arguments->items[0] = (const char *)artifact_path->data;
    for (index = 0; index < options->program_argument_count; index += 1) {
        arguments->items[index + 1] = options->program_arguments[index];
    }
    arguments->items[arguments->count] = NULL;
    return true;
}

void mmay_cli_native_arguments_dispose(
    MarimayCliNativeArguments *arguments
) {
    if (arguments == NULL) return;
    mmay_deallocate(
        &arguments->allocator, arguments->items, arguments->allocated_bytes
    );
    memset(arguments, 0, sizeof(*arguments));
}

static int mmay_cli_native_process_error(
    const MarimayCliOptions *options,
    MarimayWriter standard_error
) {
    return mmay_cli_plain_error(
        standard_error, options->diagnostic_style,
        MMAY_DIAGNOSTIC_RUNTIME_FAILURE,
        "native program process or bounded stream failed",
        MMAY_EXIT_INTERNAL
    );
}

int mmay_cli_native_run_checked(
    MarimaySession *session,
    const MarimayCliOptions *options,
    const MarimayCliCheckTarget *target,
    const MarimayCompilerResult *compiler,
    const MarimayCheckedEntry *entry,
    MarimayReadStream standard_input,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    MarimayNativeBuildResult build;
    MarimayCliNativeArguments arguments;
    MarimayProcessResult process;
    MarimayProcessCaptureResult capture;
    MarimayOwnedBytes input;
    FILE *input_file = mmay_read_stream_file(standard_input);
    FILE *output_file = mmay_writer_file(standard_output);
    FILE *error_file = mmay_writer_file(standard_error);
    MarimayProcessStatus process_status;
    int exit_code = MMAY_EXIT_INTERNAL;
    memset(&build, 0, sizeof(build));
    memset(&arguments, 0, sizeof(arguments));
    memset(&process, 0, sizeof(process));
    memset(&capture, 0, sizeof(capture));
    memset(&input, 0, sizeof(input));
    exit_code = mmay_cli_native_build_checked(
        session, options, target, compiler, entry,
        false, standard_output, standard_error, &build
    );
    if (exit_code != MMAY_EXIT_SUCCESS) goto cleanup;
    if (!mmay_cli_native_arguments_init(
            session->allocator, &build.artifact_path, options, &arguments
        )) {
        exit_code = mmay_cli_native_process_error(options, standard_error);
        goto cleanup;
    }
    if (input_file != NULL && output_file != NULL && error_file != NULL) {
        process_status = mmay_process_run_stdio(
            session->allocator, arguments.items, arguments.count,
            input_file, output_file, error_file, &process
        );
        if (process_status != MMAY_PROCESS_OK) {
            exit_code = mmay_cli_native_process_error(options, standard_error);
            goto cleanup;
        }
        exit_code = process.exit_code <= INT32_MAX
            ? (int)process.exit_code : MMAY_EXIT_INTERNAL;
    } else {
        if (standard_input.read != NULL) {
            MarimayReadAllStatus read_status = mmay_read_all(
                standard_input, session->allocator,
                MMAY_CLI_NATIVE_RUN_INPUT_LIMIT, &input
            );
            if (read_status != MMAY_READ_ALL_OK) {
                exit_code = mmay_cli_native_process_error(
                    options, standard_error
                );
                goto cleanup;
            }
        }
        process_status = mmay_process_run_capture_input(
            session->allocator, arguments.items, arguments.count,
            input.data, input.length,
            MMAY_CLI_NATIVE_RUN_OUTPUT_LIMIT,
            MMAY_CLI_NATIVE_RUN_OUTPUT_LIMIT, &capture
        );
        if (process_status != MMAY_PROCESS_OK
            || !mmay_writer_write(
                standard_output, (const char *)capture.standard_output.data,
                capture.standard_output.length
            ) || !mmay_writer_write(
                standard_error, (const char *)capture.standard_error.data,
                capture.standard_error.length
            )) {
            exit_code = mmay_cli_native_process_error(options, standard_error);
            goto cleanup;
        }
        exit_code = capture.exit_code <= INT32_MAX
            ? (int)capture.exit_code : MMAY_EXIT_INTERNAL;
    }
cleanup:
    mmay_cli_native_arguments_dispose(&arguments);
    mmay_owned_bytes_dispose(&input);
    mmay_process_capture_result_dispose(&capture);
    mmay_process_result_dispose(&process);
    mmay_native_build_result_dispose(&build);
    return exit_code;
}
