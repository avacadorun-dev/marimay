#include "cli_compile_report.h"
#include "cli_run_report.h"

#include <string.h>

#include "ruda/check/entry.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/native/build.h"

static int ruda_cli_build_status_error(
    const RudaCliOptions *options,
    RudaNativeBuildStatus status,
    RudaWriter standard_error
) {
    const char *code = RUDA_DIAGNOSTIC_NATIVE_EMIT_FAILED;
    const char *message = "native build failed inside the compiler";
    int exit_code = RUDA_EXIT_INTERNAL;
    if (status == RUDA_NATIVE_BUILD_TOOLCHAIN_MISSING) {
        code = RUDA_DIAGNOSTIC_NATIVE_TOOLCHAIN_MISSING;
#if defined(_WIN32)
        message = "supported host C toolchain is unavailable; install Visual Studio Build Tools with Desktop development with C++";
#elif defined(__APPLE__)
        message = "supported host C toolchain is unavailable; run 'xcode-select --install' and retry";
#else
        message = "supported host C toolchain is unavailable; install the platform Clang or GCC C17 development tools and retry";
#endif
        exit_code = RUDA_EXIT_SOURCE_FAILURE;
    } else if (status == RUDA_NATIVE_BUILD_IO_FAILED) {
        code = RUDA_DIAGNOSTIC_BUILD_OUTPUT_IO;
        message = "cannot atomically write the native build cache or requested output";
        exit_code = RUDA_EXIT_SOURCE_FAILURE;
    } else if (status == RUDA_NATIVE_BUILD_COMPILE_FAILED) {
        code = RUDA_DIAGNOSTIC_NATIVE_COMPILE_FAILED;
        message = "the supported host C compiler rejected compiler-generated C";
    } else if (status == RUDA_NATIVE_BUILD_UNSUPPORTED_HOST) {
        code = RUDA_DIAGNOSTIC_NATIVE_HOST_UNSUPPORTED;
        message = "the selected host cannot preserve required native semantics";
        exit_code = RUDA_EXIT_SOURCE_FAILURE;
    } else if (status == RUDA_NATIVE_BUILD_LIMIT_REACHED) {
        code = RUDA_DIAGNOSTIC_BUILD_OUTPUT_IO;
        message = "native build exhausted a defensive path or artifact limit";
        exit_code = RUDA_EXIT_SOURCE_FAILURE;
    } else if (status == RUDA_NATIVE_BUILD_EMIT_FAILED) {
        code = RUDA_DIAGNOSTIC_NATIVE_EMIT_FAILED;
        message = "canonical typed MIR did not lower to portable C17";
    }
    return ruda_cli_plain_error(
        standard_error, options->diagnostic_style,
        code, message, exit_code
    );
}

static int ruda_cli_build_execute(
    RudaSession *session,
    const RudaCliOptions *options,
    const RudaCliCheckTarget *target,
    const RudaCompilerResult *compiler,
    const RudaCheckedEntry *entry,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    const RudaMirFunction **functions = NULL;
    size_t function_count = 0;
    size_t function_bytes = 0;
    size_t index = 0;
    RudaNativeBuildRequest request;
    RudaNativeBuildResult result;
    RudaNativeBuildStatus status;
    int exit_code = RUDA_EXIT_INTERNAL;
    for (index = 0; index < compiler->checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &compiler->checked, index
        );
        if (function != NULL && !function->is_external) function_count += 1;
    }
    if (!ruda_size_multiply(
            function_count, sizeof(*functions), &function_bytes
        )) return RUDA_EXIT_INTERNAL;
    functions = ruda_allocate(&session->allocator, function_bytes);
    if (functions == NULL) return RUDA_EXIT_INTERNAL;
    function_count = 0;
    for (index = 0; index < compiler->checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &compiler->checked, index
        );
        if (function != NULL && !function->is_external) {
            functions[function_count++] = &function->mir;
        }
    }
    memset(&request, 0, sizeof(request));
    memset(&result, 0, sizeof(result));
    request.functions = functions;
    request.function_count = function_count;
    request.entry_symbol = entry->function->mir.symbol;
    if (entry->error_message != NULL) {
        request.error_message_symbol = entry->error_message->mir.symbol;
    }
    request.root_directory = (const char *)target->root_directory.data;
    request.root_directory_length = target->root_directory.length;
    request.output_path = options->build_output;
    request.returns_result = entry->returns_result;
    request.release = options->build_release;
    request.emit_c = options->build_emit != NULL;
    status = ruda_native_build(session->allocator, &request, &result);
    if (status != RUDA_NATIVE_BUILD_OK) {
        exit_code = ruda_cli_build_status_error(
            options, status, standard_error
        );
    } else if (options->quiet
        || (ruda_writer_write(
                standard_output, (const char *)result.artifact_path.data,
                result.artifact_path.length
            ) && ruda_writer_write_cstr(standard_output, "\n"))) {
        exit_code = RUDA_EXIT_SUCCESS;
    }
    ruda_native_build_result_dispose(&result);
    ruda_deallocate(&session->allocator, functions, function_bytes);
    return exit_code;
}

int ruda_cli_build_program(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    RudaCliCheckTarget target;
    RudaCompilerResult compiler;
    RudaCheckedEntry entry;
    RudaCliCheckTargetStatus target_status;
    RudaCompilerStatus compiler_status;
    RudaCheckEntryStatus entry_status;
    int exit_code = RUDA_EXIT_INTERNAL;
    memset(&target, 0, sizeof(target));
    memset(&compiler, 0, sizeof(compiler));
    memset(&entry, 0, sizeof(entry));
    target_status = ruda_cli_check_target_build(session, options, &target);
    if (target_status != RUDA_CLI_CHECK_TARGET_OK) {
        exit_code = ruda_cli_compile_target_error(
            options, &target, target_status, standard_error
        );
        goto cleanup;
    }
    compiler_status = ruda_compiler_service_check_graph(
        &target.service, target.compiler_packages.data,
        target.compiler_packages.length,
        (const char *)target.selected_import_path.data,
        target.selected_import_path.length, &compiler
    );
    if (compiler_status != RUDA_COMPILER_OK) {
        exit_code = ruda_cli_compile_status_error(
            options, compiler_status, standard_error
        );
        goto cleanup;
    }
    exit_code = ruda_cli_compile_render_result(
        options, &compiler, standard_error
    );
    if (exit_code != RUDA_EXIT_SUCCESS) goto cleanup;
    entry_status = ruda_check_select_entry(
        &compiler.checked, (const char *)target.selected_import_path.data,
        target.selected_import_path.length, &entry
    );
    if (entry_status != RUDA_CHECK_ENTRY_OK) {
        exit_code = ruda_cli_run_entry_error(
            options, entry_status, standard_error
        );
        goto cleanup;
    }
    exit_code = ruda_cli_build_execute(
        session, options, &target, &compiler, &entry,
        standard_output, standard_error
    );
cleanup:
    ruda_compiler_result_dispose(&compiler);
    ruda_cli_check_target_dispose(&target);
    return exit_code;
}
