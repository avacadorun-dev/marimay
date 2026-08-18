#include "ruda/native/build.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/hash.h"
#include "ruda/build_config.h"
#include "ruda/native/c_emitter.h"
#include "ruda/platform/directory.h"
#include "ruda/platform/file.h"
#include "ruda/platform/process.h"

#ifndef RUDA_NATIVE_RUNTIME_ARCHIVE
#define RUDA_NATIVE_RUNTIME_ARCHIVE ""
#endif

enum {
    RUDA_NATIVE_BUILD_MAX_PATH = 32768,
    RUDA_NATIVE_BUILD_MAX_SOURCE = 64 * 1024 * 1024,
    RUDA_NATIVE_BUILD_MAX_TOOL_OUTPUT = 64 * 1024
};

static RudaNativeBuildStatus ruda_native_build_join(
    RudaAllocator allocator,
    const char *left,
    size_t left_length,
    const char *right,
    RudaOwnedBytes *output
) {
    size_t right_length = strlen(right);
    bool separator = left_length != 0 && left[left_length - 1] != '/'
        && left[left_length - 1] != '\\';
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    memset(output, 0, sizeof(*output));
    if (!ruda_size_add(left_length, separator ? 1 : 0, &length)
        || !ruda_size_add(length, right_length, &length)
        || length > RUDA_NATIVE_BUILD_MAX_PATH
        || !ruda_size_add(length, 1, &allocation_size)) {
        return RUDA_NATIVE_BUILD_LIMIT_REACHED;
    }
    bytes = ruda_allocate(&allocator, allocation_size);
    if (bytes == NULL) return RUDA_NATIVE_BUILD_ALLOCATION_FAILED;
    memcpy(bytes, left, left_length);
    if (separator) bytes[left_length] = '/';
    memcpy(bytes + left_length + (separator ? 1 : 0), right, right_length);
    bytes[length] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = length;
    return RUDA_NATIVE_BUILD_OK;
}

static RudaNativeBuildStatus ruda_native_build_append(
    RudaAllocator allocator,
    const char *path,
    const char *suffix,
    RudaOwnedBytes *output
) {
    size_t path_length = strlen(path);
    size_t suffix_length = strlen(suffix);
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    memset(output, 0, sizeof(*output));
    if (!ruda_size_add(path_length, suffix_length, &length)
        || length > RUDA_NATIVE_BUILD_MAX_PATH
        || !ruda_size_add(length, 1, &allocation_size)) {
        return RUDA_NATIVE_BUILD_LIMIT_REACHED;
    }
    bytes = ruda_allocate(&allocator, allocation_size);
    if (bytes == NULL) return RUDA_NATIVE_BUILD_ALLOCATION_FAILED;
    memcpy(bytes, path, path_length);
    memcpy(bytes + path_length, suffix, suffix_length);
    bytes[length] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = length;
    return RUDA_NATIVE_BUILD_OK;
}

static RudaNativeBuildStatus ruda_native_build_make_directory(
    RudaAllocator allocator,
    const RudaOwnedBytes *path
) {
    RudaFileStatus status = ruda_directory_create(
        allocator, (const char *)path->data, path->length
    );
    if (status == RUDA_FILE_ALLOCATION_FAILED) {
        return RUDA_NATIVE_BUILD_ALLOCATION_FAILED;
    }
    return status == RUDA_FILE_OK
        ? RUDA_NATIVE_BUILD_OK : RUDA_NATIVE_BUILD_IO_FAILED;
}

static RudaNativeBuildStatus ruda_native_build_cache_directory(
    RudaAllocator allocator,
    const RudaNativeBuildRequest *request,
    const char hash[65],
    RudaOwnedBytes *directory
) {
    static const char *const components[] = {
        ".ruda", "build", "native"
    };
    RudaOwnedBytes current = {0};
    RudaOwnedBytes next = {0};
    RudaNativeBuildStatus status;
    size_t index = 0;
    if (!ruda_owned_bytes_copy(
            &current, allocator, request->root_directory,
            request->root_directory_length
        )) return RUDA_NATIVE_BUILD_ALLOCATION_FAILED;
    for (index = 0; index < sizeof(components) / sizeof(components[0]);
        index += 1) {
        status = ruda_native_build_join(
            allocator, (const char *)current.data, current.length,
            components[index], &next
        );
        if (status != RUDA_NATIVE_BUILD_OK) goto cleanup;
        status = ruda_native_build_make_directory(allocator, &next);
        if (status != RUDA_NATIVE_BUILD_OK) goto cleanup;
        ruda_owned_bytes_dispose(&current);
        current = next;
        memset(&next, 0, sizeof(next));
    }
    status = ruda_native_build_join(
        allocator, (const char *)current.data, current.length,
        request->release ? "release" : "debug", &next
    );
    if (status != RUDA_NATIVE_BUILD_OK) goto cleanup;
    status = ruda_native_build_make_directory(allocator, &next);
    if (status != RUDA_NATIVE_BUILD_OK) goto cleanup;
    ruda_owned_bytes_dispose(&current);
    current = next;
    memset(&next, 0, sizeof(next));
    status = ruda_native_build_join(
        allocator, (const char *)current.data, current.length, hash, &next
    );
    if (status != RUDA_NATIVE_BUILD_OK) goto cleanup;
    status = ruda_native_build_make_directory(allocator, &next);
    if (status == RUDA_NATIVE_BUILD_OK) {
        *directory = next;
        memset(&next, 0, sizeof(next));
    }
cleanup:
    ruda_owned_bytes_dispose(&next);
    ruda_owned_bytes_dispose(&current);
    return status;
}

static bool ruda_native_build_hash(
    const RudaOwnedBytes *source,
    const RudaNativeBuildRequest *request,
    char output[65]
) {
    static const char contract[] = "ruda-native-build-cache-v1";
    RudaSha256 hash;
    unsigned char digest[32];
    ruda_sha256_init(&hash);
    if (!ruda_sha256_update(&hash, contract, sizeof(contract) - 1)
        || !ruda_sha256_update(&hash, source->data, source->length)
        || !ruda_sha256_update(&hash, RUDA_COMPILER_VERSION,
            sizeof(RUDA_COMPILER_VERSION) - 1)
        || !ruda_sha256_update(&hash, RUDA_RUNTIME_VERSION,
            sizeof(RUDA_RUNTIME_VERSION) - 1)
        || !ruda_sha256_update(&hash, RUDA_LANGUAGE_VERSION,
            sizeof(RUDA_LANGUAGE_VERSION) - 1)
        || !ruda_sha256_update(&hash, RUDA_NATIVE_C_COMPILER_ID,
            sizeof(RUDA_NATIVE_C_COMPILER_ID) - 1)
        || !ruda_sha256_update(&hash, RUDA_NATIVE_C_COMPILER_VERSION,
            sizeof(RUDA_NATIVE_C_COMPILER_VERSION) - 1)
        || !ruda_sha256_update(&hash, RUDA_HOST_SYSTEM,
            sizeof(RUDA_HOST_SYSTEM) - 1)
        || !ruda_sha256_update(&hash, RUDA_HOST_PROCESSOR,
            sizeof(RUDA_HOST_PROCESSOR) - 1)
        || !ruda_sha256_update(&hash, RUDA_NATIVE_C_SANITIZER_FLAG,
            sizeof(RUDA_NATIVE_C_SANITIZER_FLAG) - 1)
        || !ruda_sha256_update(&hash,
            request->release ? "release" : "debug",
            request->release ? 7 : 5)) return false;
    ruda_sha256_final(&hash, digest);
    ruda_sha256_hex(digest, output);
    return true;
}

static RudaNativeBuildStatus ruda_native_build_probe(
    RudaAllocator allocator,
    const char *compiler,
    RudaOwnedBytes *tool_output
) {
    const char *arguments[3];
    RudaProcessResult process;
    RudaProcessStatus status;
    memset(&process, 0, sizeof(process));
    arguments[0] = compiler;
#if defined(_WIN32)
    arguments[1] = "/Bv";
#else
    arguments[1] = "--version";
#endif
    arguments[2] = NULL;
    status = ruda_process_run(
        allocator, arguments, 2, RUDA_NATIVE_BUILD_MAX_TOOL_OUTPUT, &process
    );
    if (status != RUDA_PROCESS_OK) {
        ruda_process_result_dispose(&process);
        return status == RUDA_PROCESS_ALLOCATION_FAILED
            ? RUDA_NATIVE_BUILD_ALLOCATION_FAILED
            : RUDA_NATIVE_BUILD_TOOLCHAIN_MISSING;
    }
    if (process.signaled || process.exit_code != 0) {
        ruda_process_result_dispose(&process);
        return RUDA_NATIVE_BUILD_TOOLCHAIN_MISSING;
    }
    *tool_output = process.output;
    memset(&process.output, 0, sizeof(process.output));
    ruda_process_result_dispose(&process);
    return RUDA_NATIVE_BUILD_OK;
}

static RudaNativeBuildStatus ruda_native_build_write_source(
    RudaAllocator allocator,
    const RudaOwnedBytes *source,
    const RudaOwnedBytes *source_path
) {
    RudaOwnedBytes temporary = {0};
    RudaFileStatus file_status;
    RudaNativeBuildStatus status = ruda_native_build_append(
        allocator, (const char *)source_path->data, ".tmp", &temporary
    );
    if (status != RUDA_NATIVE_BUILD_OK) return status;
    ruda_platform_file_ops().remove(
        NULL, allocator, (const char *)temporary.data, temporary.length
    );
    file_status = ruda_file_replace_atomic(
        allocator, (const char *)temporary.data, temporary.length,
        (const char *)source_path->data, source_path->length,
        source->data, source->length
    );
    ruda_owned_bytes_dispose(&temporary);
    if (file_status == RUDA_FILE_ALLOCATION_FAILED) {
        return RUDA_NATIVE_BUILD_ALLOCATION_FAILED;
    }
    return file_status == RUDA_FILE_OK
        ? RUDA_NATIVE_BUILD_OK : RUDA_NATIVE_BUILD_IO_FAILED;
}

static RudaNativeBuildStatus ruda_native_build_output_parent(
    RudaAllocator allocator,
    const char *path
) {
    size_t length = path != NULL ? strlen(path) : 0;
    size_t separator = SIZE_MAX;
    size_t index = 0;
    const char *parent = ".";
    size_t parent_length = 1;
    RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
    RudaFileStatus file_status;
    if (length == 0 || length > RUDA_NATIVE_BUILD_MAX_PATH) {
        return length > RUDA_NATIVE_BUILD_MAX_PATH
            ? RUDA_NATIVE_BUILD_LIMIT_REACHED
            : RUDA_NATIVE_BUILD_IO_FAILED;
    }
    for (index = 0; index < length; index += 1) {
        if (path[index] == '/' || path[index] == '\\') separator = index;
    }
    if (separator != SIZE_MAX) {
        parent = path;
        parent_length = separator == 0 ? 1 : separator;
        if (separator == 2 && path[1] == ':') parent_length = 3;
    }
    file_status = ruda_directory_kind(
        allocator, parent, parent_length, &kind
    );
    if (file_status == RUDA_FILE_ALLOCATION_FAILED) {
        return RUDA_NATIVE_BUILD_ALLOCATION_FAILED;
    }
    return file_status == RUDA_FILE_OK
        && kind == RUDA_DIRECTORY_ENTRY_DIRECTORY
        ? RUDA_NATIVE_BUILD_OK : RUDA_NATIVE_BUILD_IO_FAILED;
}

static RudaNativeBuildStatus ruda_native_build_compile(
    RudaAllocator allocator,
    const RudaNativeBuildRequest *request,
    const RudaOwnedBytes *source_path,
    const char *destination,
    RudaOwnedBytes *tool_output
) {
    const char *compiler = request->compiler_path != NULL
        ? request->compiler_path : RUDA_NATIVE_C_COMPILER;
    RudaOwnedBytes temporary = {0};
    char bootstrap_include[RUDA_NATIVE_BUILD_MAX_PATH + 4];
    char runtime_include[RUDA_NATIVE_BUILD_MAX_PATH + 4];
#if defined(_WIN32)
    char executable_argument[RUDA_NATIVE_BUILD_MAX_PATH + 5];
#endif
    const char *arguments[32];
    size_t argument_count = 0;
    int written = 0;
    RudaProcessResult process;
    RudaProcessStatus process_status;
    RudaNativeBuildStatus status = ruda_native_build_append(
        allocator, destination, ".ruda-tmp", &temporary
    );
    if (status != RUDA_NATIVE_BUILD_OK) return status;
    status = ruda_native_build_output_parent(allocator, destination);
    if (status != RUDA_NATIVE_BUILD_OK) {
        ruda_owned_bytes_dispose(&temporary);
        return status;
    }
    ruda_platform_file_ops().remove(
        NULL, allocator, (const char *)temporary.data, temporary.length
    );
    memset(&process, 0, sizeof(process));
#if defined(_WIN32)
    written = snprintf(bootstrap_include, sizeof(bootstrap_include),
        "/I%s", RUDA_NATIVE_BOOTSTRAP_INCLUDE);
    if (written < 0 || (size_t)written >= sizeof(bootstrap_include)) goto limit;
    written = snprintf(runtime_include, sizeof(runtime_include),
        "/I%s", RUDA_NATIVE_RUNTIME_INCLUDE);
    if (written < 0 || (size_t)written >= sizeof(runtime_include)) goto limit;
    arguments[argument_count++] = compiler;
    arguments[argument_count++] = "/nologo";
    arguments[argument_count++] = "/std:c17";
    arguments[argument_count++] = "/W4";
    arguments[argument_count++] = "/WX";
    arguments[argument_count++] = "/fp:strict";
    arguments[argument_count++] = request->release ? "/O2" : "/Od";
    if (RUDA_NATIVE_C_SANITIZER_FLAG[0] != 0) {
        arguments[argument_count++] = RUDA_NATIVE_C_SANITIZER_FLAG;
    }
    arguments[argument_count++] = bootstrap_include;
    arguments[argument_count++] = runtime_include;
    arguments[argument_count++] = (const char *)source_path->data;
    arguments[argument_count++] = RUDA_NATIVE_RUNTIME_ARCHIVE;
    written = snprintf(executable_argument, sizeof(executable_argument),
        "/Fe:%s", (const char *)temporary.data);
    if (written < 0 || (size_t)written >= sizeof(executable_argument)) goto limit;
    arguments[argument_count++] = executable_argument;
#else
    written = snprintf(bootstrap_include, sizeof(bootstrap_include),
        "-I%s", RUDA_NATIVE_BOOTSTRAP_INCLUDE);
    if (written < 0 || (size_t)written >= sizeof(bootstrap_include)) goto limit;
    written = snprintf(runtime_include, sizeof(runtime_include),
        "-I%s", RUDA_NATIVE_RUNTIME_INCLUDE);
    if (written < 0 || (size_t)written >= sizeof(runtime_include)) goto limit;
    arguments[argument_count++] = compiler;
    arguments[argument_count++] = "-std=c17";
    arguments[argument_count++] = request->release ? "-O2" : "-O0";
    arguments[argument_count++] = "-Wall";
    arguments[argument_count++] = "-Wextra";
    arguments[argument_count++] = "-Wpedantic";
    arguments[argument_count++] = "-Wconversion";
    arguments[argument_count++] = "-Wshadow";
    arguments[argument_count++] = "-Werror";
    arguments[argument_count++] = "-fno-fast-math";
    arguments[argument_count++] = "-frounding-math";
    arguments[argument_count++] = "-ffp-contract=off";
    arguments[argument_count++] = "-fexcess-precision=standard";
    if (RUDA_NATIVE_C_SANITIZER_FLAG[0] != 0) {
        arguments[argument_count++] = RUDA_NATIVE_C_SANITIZER_FLAG;
    }
    arguments[argument_count++] = bootstrap_include;
    arguments[argument_count++] = runtime_include;
    arguments[argument_count++] = (const char *)source_path->data;
    arguments[argument_count++] = RUDA_NATIVE_RUNTIME_ARCHIVE;
    arguments[argument_count++] = "-lm";
    arguments[argument_count++] = "-o";
    arguments[argument_count++] = (const char *)temporary.data;
#endif
    arguments[argument_count] = NULL;
    process_status = ruda_process_run(
        allocator, arguments, argument_count,
        RUDA_NATIVE_BUILD_MAX_TOOL_OUTPUT, &process
    );
    if (process_status != RUDA_PROCESS_OK) {
        status = process_status == RUDA_PROCESS_ALLOCATION_FAILED
            ? RUDA_NATIVE_BUILD_ALLOCATION_FAILED
            : process_status == RUDA_PROCESS_SPAWN_FAILED
                ? RUDA_NATIVE_BUILD_TOOLCHAIN_MISSING
                : RUDA_NATIVE_BUILD_COMPILE_FAILED;
        goto cleanup;
    }
    if (process.signaled || process.exit_code != 0) {
        *tool_output = process.output;
        memset(&process.output, 0, sizeof(process.output));
        status = RUDA_NATIVE_BUILD_COMPILE_FAILED;
        goto cleanup;
    }
    if (ruda_file_replace_executable(
            allocator, (const char *)temporary.data, temporary.length,
            destination, strlen(destination)
        ) != RUDA_FILE_OK) {
        status = RUDA_NATIVE_BUILD_IO_FAILED;
        goto cleanup;
    }
    status = RUDA_NATIVE_BUILD_OK;
    goto cleanup;
limit:
    status = RUDA_NATIVE_BUILD_LIMIT_REACHED;
cleanup:
    ruda_process_result_dispose(&process);
    if (status != RUDA_NATIVE_BUILD_OK) {
        ruda_platform_file_ops().remove(
            NULL, allocator, (const char *)temporary.data, temporary.length
        );
    }
    ruda_owned_bytes_dispose(&temporary);
    return status;
}

RudaNativeBuildStatus ruda_native_build(
    RudaAllocator allocator,
    const RudaNativeBuildRequest *request,
    RudaNativeBuildResult *result
) {
    RudaNativeCProgram program;
    RudaNativeCEmitOptions emit_options;
    RudaNativeCEmitStatus emit_status;
    RudaOwnedBytes source = {0};
    RudaOwnedBytes directory = {0};
    RudaOwnedBytes source_path = {0};
    RudaOwnedBytes cache_artifact = {0};
    RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
    char hash[65];
    RudaNativeBuildStatus status = RUDA_NATIVE_BUILD_OK;
    if (result == NULL) return RUDA_NATIVE_BUILD_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    if (request == NULL || request->functions == NULL
        || request->function_count == 0 || request->root_directory == NULL
        || request->root_directory_length == 0) {
        return RUDA_NATIVE_BUILD_INVALID_ARGUMENT;
    }
    if (!request->emit_c) {
        status = ruda_native_build_probe(
            allocator,
            request->compiler_path != NULL
                ? request->compiler_path : RUDA_NATIVE_C_COMPILER,
            &result->tool_output
        );
        if (status != RUDA_NATIVE_BUILD_OK) goto cleanup;
        ruda_owned_bytes_dispose(&result->tool_output);
    }
    memset(&program, 0, sizeof(program));
    program.functions = request->functions;
    program.function_count = request->function_count;
    program.entry_symbol = request->entry_symbol;
    emit_options = ruda_native_c_emit_default_options();
    emit_options.executable_entry = true;
    emit_options.executable_returns_result = request->returns_result;
    emit_options.error_message_symbol = request->error_message_symbol;
    emit_status = ruda_native_c_emit_program(
        &program, allocator, emit_options, &source
    );
    if (emit_status != RUDA_NATIVE_C_EMIT_OK) {
        status = emit_status == RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED
            ? RUDA_NATIVE_BUILD_ALLOCATION_FAILED
            : emit_status == RUDA_NATIVE_C_EMIT_LIMIT_REACHED
                ? RUDA_NATIVE_BUILD_LIMIT_REACHED
                : emit_status == RUDA_NATIVE_C_EMIT_UNSUPPORTED_HOST
                    ? RUDA_NATIVE_BUILD_UNSUPPORTED_HOST
                    : RUDA_NATIVE_BUILD_EMIT_FAILED;
        goto cleanup;
    }
    if (!ruda_native_build_hash(&source, request, hash)) {
        status = RUDA_NATIVE_BUILD_LIMIT_REACHED;
        goto cleanup;
    }
    status = ruda_native_build_cache_directory(
        allocator, request, hash, &directory
    );
    if (status != RUDA_NATIVE_BUILD_OK) goto cleanup;
    status = ruda_native_build_join(
        allocator, (const char *)directory.data, directory.length,
        "program.c", &source_path
    );
    if (status != RUDA_NATIVE_BUILD_OK) goto cleanup;
    status = ruda_native_build_write_source(allocator, &source, &source_path);
    if (status != RUDA_NATIVE_BUILD_OK) goto cleanup;
    if (request->emit_c) {
        if (request->output_path != NULL) {
            RudaOwnedBytes requested = {0};
            if (!ruda_owned_bytes_copy(
                    &requested, allocator, request->output_path,
                    strlen(request->output_path)
                )) {
                status = RUDA_NATIVE_BUILD_ALLOCATION_FAILED;
                goto cleanup;
            }
            status = ruda_native_build_write_source(
                allocator, &source, &requested
            );
            if (status == RUDA_NATIVE_BUILD_OK) {
                result->artifact_path = requested;
                memset(&requested, 0, sizeof(requested));
            }
            ruda_owned_bytes_dispose(&requested);
        } else {
            result->artifact_path = source_path;
            memset(&source_path, 0, sizeof(source_path));
        }
        goto cleanup;
    }
    status = ruda_native_build_join(
        allocator, (const char *)directory.data, directory.length,
#if defined(_WIN32)
        "app.exe",
#else
        "app",
#endif
        &cache_artifact
    );
    if (status != RUDA_NATIVE_BUILD_OK) goto cleanup;
    if (ruda_directory_kind(
            allocator, (const char *)cache_artifact.data,
            cache_artifact.length, &kind
        ) != RUDA_FILE_OK) {
        status = RUDA_NATIVE_BUILD_IO_FAILED;
        goto cleanup;
    }
    if (kind == RUDA_DIRECTORY_ENTRY_REGULAR) {
        result->cache_hit = true;
    } else if (kind != RUDA_DIRECTORY_ENTRY_MISSING) {
        status = RUDA_NATIVE_BUILD_IO_FAILED;
        goto cleanup;
    } else {
        status = ruda_native_build_compile(
            allocator, request, &source_path,
            (const char *)cache_artifact.data, &result->tool_output
        );
        if (status != RUDA_NATIVE_BUILD_OK) goto cleanup;
    }
    if (request->output_path != NULL) {
        status = ruda_native_build_compile(
            allocator, request, &source_path, request->output_path,
            &result->tool_output
        );
        if (status != RUDA_NATIVE_BUILD_OK) goto cleanup;
        if (!ruda_owned_bytes_copy(
                &result->artifact_path, allocator, request->output_path,
                strlen(request->output_path)
            )) status = RUDA_NATIVE_BUILD_ALLOCATION_FAILED;
    } else {
        result->artifact_path = cache_artifact;
        memset(&cache_artifact, 0, sizeof(cache_artifact));
    }
cleanup:
    ruda_owned_bytes_dispose(&cache_artifact);
    ruda_owned_bytes_dispose(&source_path);
    ruda_owned_bytes_dispose(&directory);
    ruda_owned_bytes_dispose(&source);
    if (status != RUDA_NATIVE_BUILD_OK) {
        ruda_owned_bytes_dispose(&result->artifact_path);
    }
    return status;
}

void ruda_native_build_result_dispose(RudaNativeBuildResult *result) {
    if (result != NULL) {
        ruda_owned_bytes_dispose(&result->artifact_path);
        ruda_owned_bytes_dispose(&result->tool_output);
        result->cache_hit = false;
    }
}
