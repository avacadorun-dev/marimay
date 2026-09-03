#include "cli_native_build.h"

#include <string.h>

#include "mmay/generated/diagnostic_codes.h"

static int mmay_cli_native_build_status_error(
    const MarimayCliOptions *options,
    MarimayNativeBuildStatus status,
    const MarimayOwnedBytes *tool_output,
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
    exit_code = mmay_cli_plain_error(
        standard_error, options->diagnostic_style,
        code, message, exit_code
    );
    if (status == MMAY_NATIVE_BUILD_COMPILE_FAILED
        && tool_output != NULL && tool_output->length != 0
        && options->diagnostic_style != MMAY_DIAGNOSTIC_JSON) {
        if (!mmay_writer_write_cstr(standard_error, "compiler output:\n")
            || !mmay_writer_write(
                standard_error, (const char *)tool_output->data,
                tool_output->length
            ) || (tool_output->data[tool_output->length - 1] != '\n'
                && !mmay_writer_write_cstr(standard_error, "\n"))) {
            return MMAY_EXIT_INTERNAL;
        }
    }
    return exit_code;
}

int mmay_cli_native_build_checked(
    MarimaySession *session,
    const MarimayCliOptions *options,
    const MarimayCliCheckTarget *target,
    const MarimayCompilerResult *compiler,
    const MarimayCheckedEntry *entry,
    bool announce_artifact,
    MarimayWriter standard_output,
    MarimayWriter standard_error,
    MarimayNativeBuildResult *result
) {
    const MarimayMirFunction **functions = NULL;
    size_t function_count = 0;
    size_t function_bytes = 0;
    size_t index = 0;
    MarimayNativeBuildRequest request;
    MarimayNativeBuildStatus status;
    MarimayCliFsCapabilities fs_capabilities;
    MarimayCliEnvCapabilities env_capabilities;
    int exit_code = MMAY_EXIT_INTERNAL;
    if (result == NULL) return MMAY_EXIT_INTERNAL;
    memset(result, 0, sizeof(*result));
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
    memset(&fs_capabilities, 0, sizeof(fs_capabilities));
    memset(&env_capabilities, 0, sizeof(env_capabilities));
    if (!mmay_cli_fs_capabilities(options, &fs_capabilities)
        || !mmay_cli_env_capabilities(options, &env_capabilities)) {
        goto cleanup;
    }
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
    request.read_roots = fs_capabilities.options.read_roots;
    request.read_root_count = fs_capabilities.options.read_root_count;
    request.write_roots = fs_capabilities.options.write_roots;
    request.write_root_count = fs_capabilities.options.write_root_count;
    request.env_grants = env_capabilities.options.grants;
    request.env_grant_count = env_capabilities.options.grant_count;
    request.takes_arguments = entry->takes_arguments;
    request.returns_status = entry->returns_status;
    request.returns_result = entry->returns_result;
    request.release = options->build_release;
    request.emit_c = options->build_emit != NULL;
    status = mmay_native_build(session->allocator, &request, result);
    if (status != MMAY_NATIVE_BUILD_OK) {
        exit_code = mmay_cli_native_build_status_error(
            options, status, &result->tool_output, standard_error
        );
    } else if (!announce_artifact || options->quiet
        || (mmay_writer_write(
                standard_output, (const char *)result->artifact_path.data,
                result->artifact_path.length
            ) && mmay_writer_write_cstr(standard_output, "\n"))) {
        exit_code = MMAY_EXIT_SUCCESS;
    }
cleanup:
    mmay_deallocate(&session->allocator, functions, function_bytes);
    return exit_code;
}
