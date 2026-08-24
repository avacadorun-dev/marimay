#include "cli_compile_report.h"
#include "cli_run_report.h"

#include <string.h>

#include "mmay/check/entry.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/native/build.h"

static int mmay_cli_build_status_error(
    const MarimayCliOptions *options,
    MarimayNativeBuildStatus status,
    MarimayWriter standard_error
) {
    const char *code = MMAY_DIAGNOSTIC_NATIVE_EMIT_FAILED;
    const char *message = "native build failed inside the compiler";
    int exit_code = MMAY_EXIT_INTERNAL;
    if (status == MMAY_NATIVE_BUILD_TOOLCHAIN_MISSING) {
        code = MMAY_DIAGNOSTIC_NATIVE_TOOLCHAIN_MISSING;
#if defined(_WIN32)
        message = "supported host C toolchain is unavailable; install Visual Studio Build Tools with Desktop development with C++";
#elif defined(__APPLE__)
        message = "supported host C toolchain is unavailable; run 'xcode-select --install' and retry";
#else
        message = "supported host C toolchain is unavailable; install the platform Clang or GCC C17 development tools and retry";
#endif
        exit_code = MMAY_EXIT_SOURCE_FAILURE;
    } else if (status == MMAY_NATIVE_BUILD_RUNTIME_MISSING) {
        code = MMAY_DIAGNOSTIC_NATIVE_RUNTIME_INCOMPATIBLE;
        message = "the version-matched native runtime artifact is missing; reinstall the Marimay toolchain";
        exit_code = MMAY_EXIT_SOURCE_FAILURE;
    } else if (status == MMAY_NATIVE_BUILD_RUNTIME_INCOMPATIBLE) {
        code = MMAY_DIAGNOSTIC_NATIVE_RUNTIME_INCOMPATIBLE;
        message = "the selected native runtime has a stale version or wrong build profile; reinstall the Marimay toolchain";
        exit_code = MMAY_EXIT_SOURCE_FAILURE;
    } else if (status == MMAY_NATIVE_BUILD_IO_FAILED) {
        code = MMAY_DIAGNOSTIC_BUILD_OUTPUT_IO;
        message = "cannot atomically write the native build cache or requested output";
        exit_code = MMAY_EXIT_SOURCE_FAILURE;
    } else if (status == MMAY_NATIVE_BUILD_COMPILE_FAILED) {
        code = MMAY_DIAGNOSTIC_NATIVE_COMPILE_FAILED;
        message = "the supported host C compiler rejected compiler-generated C";
    } else if (status == MMAY_NATIVE_BUILD_UNSUPPORTED_HOST) {
        code = MMAY_DIAGNOSTIC_NATIVE_HOST_UNSUPPORTED;
        message = "the selected host cannot preserve required native semantics";
        exit_code = MMAY_EXIT_SOURCE_FAILURE;
    } else if (status == MMAY_NATIVE_BUILD_LIMIT_REACHED) {
        code = MMAY_DIAGNOSTIC_BUILD_OUTPUT_IO;
        message = "native build exhausted a defensive path or artifact limit";
        exit_code = MMAY_EXIT_SOURCE_FAILURE;
    } else if (status == MMAY_NATIVE_BUILD_EMIT_FAILED) {
        code = MMAY_DIAGNOSTIC_NATIVE_EMIT_FAILED;
        message = "canonical typed MIR did not lower to portable C17";
    }
    return mmay_cli_plain_error(
        standard_error, options->diagnostic_style,
        code, message, exit_code
    );
}

static int mmay_cli_build_execute(
    MarimaySession *session,
    const MarimayCliOptions *options,
    const MarimayCliCheckTarget *target,
    const MarimayCompilerResult *compiler,
    const MarimayCheckedEntry *entry,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    const MarimayMirFunction **functions = NULL;
    size_t function_count = 0;
    size_t function_bytes = 0;
    size_t index = 0;
    MarimayNativeBuildRequest request;
    MarimayNativeBuildResult result;
    MarimayNativeBuildStatus status;
    int exit_code = MMAY_EXIT_INTERNAL;
    for (index = 0; index < compiler->checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &compiler->checked, index
        );
        if (function != NULL && !function->is_external) function_count += 1;
    }
    if (!mmay_size_multiply(
            function_count, sizeof(*functions), &function_bytes
        )) return MMAY_EXIT_INTERNAL;
    functions = mmay_allocate(&session->allocator, function_bytes);
    if (functions == NULL) return MMAY_EXIT_INTERNAL;
    function_count = 0;
    for (index = 0; index < compiler->checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
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
    request.tool_executable_path = options->tool_executable_path;
    request.max_heap_bytes = options->max_heap_bytes;
    request.takes_arguments = entry->takes_arguments;
    request.returns_status = entry->returns_status;
    request.returns_result = entry->returns_result;
    request.release = options->build_release;
    request.emit_c = options->build_emit != NULL;
    status = mmay_native_build(session->allocator, &request, &result);
    if (status != MMAY_NATIVE_BUILD_OK) {
        exit_code = mmay_cli_build_status_error(
            options, status, standard_error
        );
    } else if (options->quiet
        || (mmay_writer_write(
                standard_output, (const char *)result.artifact_path.data,
                result.artifact_path.length
            ) && mmay_writer_write_cstr(standard_output, "\n"))) {
        exit_code = MMAY_EXIT_SUCCESS;
    }
    mmay_native_build_result_dispose(&result);
    mmay_deallocate(&session->allocator, functions, function_bytes);
    return exit_code;
}

int mmay_cli_build_program(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    MarimayCliCheckTarget target;
    MarimayCompilerResult compiler;
    MarimayCheckedEntry entry;
    MarimayCliCheckTargetStatus target_status;
    MarimayCompilerStatus compiler_status;
    MarimayCheckEntryStatus entry_status;
    int exit_code = MMAY_EXIT_INTERNAL;
    memset(&target, 0, sizeof(target));
    memset(&compiler, 0, sizeof(compiler));
    memset(&entry, 0, sizeof(entry));
    target_status = mmay_cli_check_target_build(session, options, &target);
    if (target_status != MMAY_CLI_CHECK_TARGET_OK) {
        exit_code = mmay_cli_compile_target_error(
            options, &target, target_status, standard_error
        );
        goto cleanup;
    }
    compiler_status = mmay_compiler_service_check_graph(
        &target.service, target.compiler_packages.data,
        target.compiler_packages.length,
        (const char *)target.selected_import_path.data,
        target.selected_import_path.length, &compiler
    );
    if (compiler_status != MMAY_COMPILER_OK) {
        exit_code = mmay_cli_compile_status_error(
            options, compiler_status, standard_error
        );
        goto cleanup;
    }
    exit_code = mmay_cli_compile_render_result(
        options, &compiler, standard_error
    );
    if (exit_code != MMAY_EXIT_SUCCESS) goto cleanup;
    if (!mmay_cli_optimize_checked_mir(&compiler)) {
        exit_code = mmay_cli_plain_error(
            standard_error, options->diagnostic_style,
            MMAY_DIAGNOSTIC_NATIVE_EMIT_FAILED,
            "canonical typed MIR range proof was invalid",
            MMAY_EXIT_INTERNAL
        );
        goto cleanup;
    }
    entry_status = mmay_check_select_entry(
        &compiler.checked, (const char *)target.selected_import_path.data,
        target.selected_import_path.length, &entry
    );
    if (entry_status != MMAY_CHECK_ENTRY_OK) {
        exit_code = mmay_cli_run_entry_error(
            options, entry_status, standard_error
        );
        goto cleanup;
    }
    exit_code = mmay_cli_build_execute(
        session, options, &target, &compiler, &entry,
        standard_output, standard_error
    );
cleanup:
    mmay_compiler_result_dispose(&compiler);
    mmay_cli_check_target_dispose(&target);
    return exit_code;
}
