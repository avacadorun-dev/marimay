#include "mmay/native/build.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/hash.h"
#include "mmay/build_config.h"
#include "mmay/native/c_emitter.h"
#include "mmay/native/toolchain.h"
#include "mmay/platform/directory.h"
#include "mmay/platform/file.h"
#include "mmay/platform/process.h"

enum {
    MMAY_NATIVE_BUILD_MAX_PATH = 32768,
    MMAY_NATIVE_BUILD_MAX_SOURCE = 64 * 1024 * 1024,
    MMAY_NATIVE_BUILD_MAX_TOOL_OUTPUT = 64 * 1024
};

static MarimayNativeBuildStatus mmay_native_build_join(
    MarimayAllocator allocator,
    const char *left,
    size_t left_length,
    const char *right,
    MarimayOwnedBytes *output
) {
    size_t right_length = strlen(right);
    bool separator = left_length != 0 && left[left_length - 1] != '/'
        && left[left_length - 1] != '\\';
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    memset(output, 0, sizeof(*output));
    if (!mmay_size_add(left_length, separator ? 1 : 0, &length)
        || !mmay_size_add(length, right_length, &length)
        || length > MMAY_NATIVE_BUILD_MAX_PATH
        || !mmay_size_add(length, 1, &allocation_size)) {
        return MMAY_NATIVE_BUILD_LIMIT_REACHED;
    }
    bytes = mmay_allocate(&allocator, allocation_size);
    if (bytes == NULL) return MMAY_NATIVE_BUILD_ALLOCATION_FAILED;
    memcpy(bytes, left, left_length);
    if (separator) bytes[left_length] = '/';
    memcpy(bytes + left_length + (separator ? 1 : 0), right, right_length);
    bytes[length] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = length;
    return MMAY_NATIVE_BUILD_OK;
}

static MarimayNativeBuildStatus mmay_native_build_append(
    MarimayAllocator allocator,
    const char *path,
    const char *suffix,
    MarimayOwnedBytes *output
) {
    size_t path_length = strlen(path);
    size_t suffix_length = strlen(suffix);
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    memset(output, 0, sizeof(*output));
    if (!mmay_size_add(path_length, suffix_length, &length)
        || length > MMAY_NATIVE_BUILD_MAX_PATH
        || !mmay_size_add(length, 1, &allocation_size)) {
        return MMAY_NATIVE_BUILD_LIMIT_REACHED;
    }
    bytes = mmay_allocate(&allocator, allocation_size);
    if (bytes == NULL) return MMAY_NATIVE_BUILD_ALLOCATION_FAILED;
    memcpy(bytes, path, path_length);
    memcpy(bytes + path_length, suffix, suffix_length);
    bytes[length] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = length;
    return MMAY_NATIVE_BUILD_OK;
}

static MarimayNativeBuildStatus mmay_native_build_make_directory(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *path
) {
    MarimayFileStatus status = mmay_directory_create(
        allocator, (const char *)path->data, path->length
    );
    if (status == MMAY_FILE_ALLOCATION_FAILED) {
        return MMAY_NATIVE_BUILD_ALLOCATION_FAILED;
    }
    return status == MMAY_FILE_OK
        ? MMAY_NATIVE_BUILD_OK : MMAY_NATIVE_BUILD_IO_FAILED;
}

static MarimayNativeBuildStatus mmay_native_build_cache_directory(
    MarimayAllocator allocator,
    const MarimayNativeBuildRequest *request,
    const char hash[65],
    MarimayOwnedBytes *directory
) {
    static const char *const components[] = {
        ".mmay", "build", "native"
    };
    MarimayOwnedBytes current = {0};
    MarimayOwnedBytes next = {0};
    MarimayNativeBuildStatus status;
    size_t index = 0;
    if (!mmay_owned_bytes_copy(
            &current, allocator, request->root_directory,
            request->root_directory_length
        )) return MMAY_NATIVE_BUILD_ALLOCATION_FAILED;
    for (index = 0; index < sizeof(components) / sizeof(components[0]);
        index += 1) {
        status = mmay_native_build_join(
            allocator, (const char *)current.data, current.length,
            components[index], &next
        );
        if (status != MMAY_NATIVE_BUILD_OK) goto cleanup;
        status = mmay_native_build_make_directory(allocator, &next);
        if (status != MMAY_NATIVE_BUILD_OK) goto cleanup;
        mmay_owned_bytes_dispose(&current);
        current = next;
        memset(&next, 0, sizeof(next));
    }
    status = mmay_native_build_join(
        allocator, (const char *)current.data, current.length,
        request->release ? "release" : "debug", &next
    );
    if (status != MMAY_NATIVE_BUILD_OK) goto cleanup;
    status = mmay_native_build_make_directory(allocator, &next);
    if (status != MMAY_NATIVE_BUILD_OK) goto cleanup;
    mmay_owned_bytes_dispose(&current);
    current = next;
    memset(&next, 0, sizeof(next));
    status = mmay_native_build_join(
        allocator, (const char *)current.data, current.length, hash, &next
    );
    if (status != MMAY_NATIVE_BUILD_OK) goto cleanup;
    status = mmay_native_build_make_directory(allocator, &next);
    if (status == MMAY_NATIVE_BUILD_OK) {
        *directory = next;
        memset(&next, 0, sizeof(next));
    }
cleanup:
    mmay_owned_bytes_dispose(&next);
    mmay_owned_bytes_dispose(&current);
    return status;
}

static bool mmay_native_build_hash(
    const MarimayOwnedBytes *source,
    const MarimayNativeBuildRequest *request,
    const MarimayNativeRuntimeArtifact *runtime,
    char output[65]
) {
    static const char contract[] = "mmay-native-build-cache-v4";
    static const char emit_only[] = "emit-c-without-linked-runtime";
    MarimaySha256 hash;
    unsigned char digest[32];
    mmay_sha256_init(&hash);
    if (!mmay_sha256_update(&hash, contract, sizeof(contract) - 1)
        || !mmay_sha256_update(&hash, source->data, source->length)
        || !mmay_sha256_update(&hash, MMAY_COMPILER_VERSION,
            sizeof(MMAY_COMPILER_VERSION) - 1)
        || !mmay_sha256_update(&hash, MMAY_RUNTIME_VERSION,
            sizeof(MMAY_RUNTIME_VERSION) - 1)
        || !mmay_sha256_update(&hash, MMAY_LANGUAGE_VERSION,
            sizeof(MMAY_LANGUAGE_VERSION) - 1)
        || !mmay_sha256_update(&hash, MMAY_NATIVE_C_COMPILER_ID,
            sizeof(MMAY_NATIVE_C_COMPILER_ID) - 1)
        || !mmay_sha256_update(&hash, MMAY_NATIVE_C_COMPILER_VERSION,
            sizeof(MMAY_NATIVE_C_COMPILER_VERSION) - 1)
        || !mmay_sha256_update(&hash, MMAY_HOST_SYSTEM,
            sizeof(MMAY_HOST_SYSTEM) - 1)
        || !mmay_sha256_update(&hash, MMAY_HOST_PROCESSOR,
            sizeof(MMAY_HOST_PROCESSOR) - 1)
        || !mmay_sha256_update(&hash, MMAY_NATIVE_C_SANITIZER_FLAG,
            sizeof(MMAY_NATIVE_C_SANITIZER_FLAG) - 1)
        || !mmay_sha256_update(
            &hash,
            runtime != NULL ? runtime->content_hash : emit_only,
            runtime != NULL ? 64 : sizeof(emit_only) - 1
        )
        || !mmay_sha256_update(&hash,
            request->release ? "release" : "debug",
            request->release ? 7 : 5)) return false;
    mmay_sha256_final(&hash, digest);
    mmay_sha256_hex(digest, output);
    return true;
}

static MarimayNativeBuildStatus mmay_native_build_write_source(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *source,
    const MarimayOwnedBytes *source_path
) {
    MarimayOwnedBytes temporary = {0};
    MarimayFileStatus file_status;
    MarimayNativeBuildStatus status = mmay_native_build_append(
        allocator, (const char *)source_path->data, ".tmp", &temporary
    );
    if (status != MMAY_NATIVE_BUILD_OK) return status;
    mmay_platform_file_ops().remove(
        NULL, allocator, (const char *)temporary.data, temporary.length
    );
    file_status = mmay_file_replace_atomic(
        allocator, (const char *)temporary.data, temporary.length,
        (const char *)source_path->data, source_path->length,
        source->data, source->length
    );
    mmay_owned_bytes_dispose(&temporary);
    if (file_status == MMAY_FILE_ALLOCATION_FAILED) {
        return MMAY_NATIVE_BUILD_ALLOCATION_FAILED;
    }
    return file_status == MMAY_FILE_OK
        ? MMAY_NATIVE_BUILD_OK : MMAY_NATIVE_BUILD_IO_FAILED;
}

static MarimayNativeBuildStatus mmay_native_build_output_parent(
    MarimayAllocator allocator,
    const char *path
) {
    size_t length = path != NULL ? strlen(path) : 0;
    size_t separator = SIZE_MAX;
    size_t index = 0;
    const char *parent = ".";
    size_t parent_length = 1;
    MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
    MarimayFileStatus file_status;
    if (length == 0 || length > MMAY_NATIVE_BUILD_MAX_PATH) {
        return length > MMAY_NATIVE_BUILD_MAX_PATH
            ? MMAY_NATIVE_BUILD_LIMIT_REACHED
            : MMAY_NATIVE_BUILD_IO_FAILED;
    }
    for (index = 0; index < length; index += 1) {
        if (path[index] == '/' || path[index] == '\\') separator = index;
    }
    if (separator != SIZE_MAX) {
        parent = path;
        parent_length = separator == 0 ? 1 : separator;
        if (separator == 2 && path[1] == ':') parent_length = 3;
    }
    file_status = mmay_directory_kind(
        allocator, parent, parent_length, &kind
    );
    if (file_status == MMAY_FILE_ALLOCATION_FAILED) {
        return MMAY_NATIVE_BUILD_ALLOCATION_FAILED;
    }
    return file_status == MMAY_FILE_OK
        && kind == MMAY_DIRECTORY_ENTRY_DIRECTORY
        ? MMAY_NATIVE_BUILD_OK : MMAY_NATIVE_BUILD_IO_FAILED;
}

static MarimayNativeBuildStatus mmay_native_build_compile(
    MarimayAllocator allocator,
    const MarimayNativeBuildRequest *request,
    const MarimayNativeRuntimeArtifact *runtime,
    const MarimayOwnedBytes *source_path,
    const char *destination,
    MarimayOwnedBytes *tool_output
) {
    const char *compiler = request->compiler_path != NULL
        ? request->compiler_path : MMAY_NATIVE_C_COMPILER;
    MarimayOwnedBytes temporary = {0};
    char bootstrap_include[MMAY_NATIVE_BUILD_MAX_PATH + 4];
    char runtime_include[MMAY_NATIVE_BUILD_MAX_PATH + 4];
#if defined(_WIN32)
    char executable_argument[MMAY_NATIVE_BUILD_MAX_PATH + 5];
#endif
    const char *arguments[32];
    size_t argument_count = 0;
    int written = 0;
    MarimayProcessResult process;
    MarimayProcessStatus process_status;
    MarimayNativeBuildStatus status = mmay_native_build_append(
        allocator, destination, ".mmay-tmp", &temporary
    );
    if (status != MMAY_NATIVE_BUILD_OK) return status;
    status = mmay_native_build_output_parent(allocator, destination);
    if (status != MMAY_NATIVE_BUILD_OK) {
        mmay_owned_bytes_dispose(&temporary);
        return status;
    }
    mmay_platform_file_ops().remove(
        NULL, allocator, (const char *)temporary.data, temporary.length
    );
    memset(&process, 0, sizeof(process));
#if defined(_WIN32)
    written = snprintf(bootstrap_include, sizeof(bootstrap_include),
        "/I%s", (const char *)runtime->bootstrap_include.data);
    if (written < 0 || (size_t)written >= sizeof(bootstrap_include)) goto limit;
    written = snprintf(runtime_include, sizeof(runtime_include),
        "/I%s", (const char *)runtime->runtime_include.data);
    if (written < 0 || (size_t)written >= sizeof(runtime_include)) goto limit;
    arguments[argument_count++] = compiler;
    arguments[argument_count++] = "/nologo";
    arguments[argument_count++] = "/std:c17";
    arguments[argument_count++] = "/W4";
    arguments[argument_count++] = "/WX";
    arguments[argument_count++] = "/fp:strict";
    arguments[argument_count++] = "/MT";
    arguments[argument_count++] = request->release ? "/O2" : "/Od";
    if (MMAY_NATIVE_C_SANITIZER_FLAG[0] != 0) {
        arguments[argument_count++] = MMAY_NATIVE_C_SANITIZER_FLAG;
    }
    arguments[argument_count++] = bootstrap_include;
    arguments[argument_count++] = runtime_include;
    arguments[argument_count++] = (const char *)source_path->data;
    arguments[argument_count++] = (const char *)runtime->archive.data;
    written = snprintf(executable_argument, sizeof(executable_argument),
        "/Fe:%s", (const char *)temporary.data);
    if (written < 0 || (size_t)written >= sizeof(executable_argument)) goto limit;
    arguments[argument_count++] = executable_argument;
#else
    written = snprintf(bootstrap_include, sizeof(bootstrap_include),
        "-I%s", (const char *)runtime->bootstrap_include.data);
    if (written < 0 || (size_t)written >= sizeof(bootstrap_include)) goto limit;
    written = snprintf(runtime_include, sizeof(runtime_include),
        "-I%s", (const char *)runtime->runtime_include.data);
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
    if (MMAY_NATIVE_C_SANITIZER_FLAG[0] != 0) {
        arguments[argument_count++] = MMAY_NATIVE_C_SANITIZER_FLAG;
    }
    arguments[argument_count++] = bootstrap_include;
    arguments[argument_count++] = runtime_include;
    arguments[argument_count++] = (const char *)source_path->data;
    arguments[argument_count++] = (const char *)runtime->archive.data;
    arguments[argument_count++] = "-lm";
    arguments[argument_count++] = "-o";
    arguments[argument_count++] = (const char *)temporary.data;
#endif
    arguments[argument_count] = NULL;
    process_status = mmay_process_run(
        allocator, arguments, argument_count,
        MMAY_NATIVE_BUILD_MAX_TOOL_OUTPUT, &process
    );
    if (process_status != MMAY_PROCESS_OK) {
        status = process_status == MMAY_PROCESS_ALLOCATION_FAILED
            ? MMAY_NATIVE_BUILD_ALLOCATION_FAILED
            : process_status == MMAY_PROCESS_SPAWN_FAILED
                ? MMAY_NATIVE_BUILD_TOOLCHAIN_MISSING
                : MMAY_NATIVE_BUILD_COMPILE_FAILED;
        goto cleanup;
    }
    if (process.signaled || process.exit_code != 0) {
        *tool_output = process.output;
        memset(&process.output, 0, sizeof(process.output));
        status = MMAY_NATIVE_BUILD_COMPILE_FAILED;
        goto cleanup;
    }
    if (mmay_file_replace_executable(
            allocator, (const char *)temporary.data, temporary.length,
            destination, strlen(destination)
        ) != MMAY_FILE_OK) {
        status = MMAY_NATIVE_BUILD_IO_FAILED;
        goto cleanup;
    }
    status = MMAY_NATIVE_BUILD_OK;
    goto cleanup;
limit:
    status = MMAY_NATIVE_BUILD_LIMIT_REACHED;
cleanup:
    mmay_process_result_dispose(&process);
    if (status != MMAY_NATIVE_BUILD_OK) {
        mmay_platform_file_ops().remove(
            NULL, allocator, (const char *)temporary.data, temporary.length
        );
    }
    mmay_owned_bytes_dispose(&temporary);
    return status;
}

static MarimayNativeBuildStatus mmay_native_build_runtime_status(
    MarimayNativeRuntimeArtifactStatus status
) {
    if (status == MMAY_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED) {
        return MMAY_NATIVE_BUILD_ALLOCATION_FAILED;
    }
    if (status == MMAY_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED) {
        return MMAY_NATIVE_BUILD_LIMIT_REACHED;
    }
    if (status == MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING) {
        return MMAY_NATIVE_BUILD_RUNTIME_MISSING;
    }
    if (status == MMAY_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE) {
        return MMAY_NATIVE_BUILD_RUNTIME_INCOMPATIBLE;
    }
    if (status == MMAY_NATIVE_RUNTIME_ARTIFACT_IO_FAILED) {
        return MMAY_NATIVE_BUILD_IO_FAILED;
    }
    return MMAY_NATIVE_BUILD_INVALID_ARGUMENT;
}

MarimayNativeBuildStatus mmay_native_build(
    MarimayAllocator allocator,
    const MarimayNativeBuildRequest *request,
    MarimayNativeBuildResult *result
) {
    MarimayNativeCProgram program;
    MarimayNativeCEmitOptions emit_options;
    MarimayNativeCEmitStatus emit_status;
    MarimayNativeRuntimeArtifact runtime = {0};
    MarimayNativeRuntimeArtifactStatus runtime_status;
    MarimayOwnedBytes source = {0};
    MarimayOwnedBytes directory = {0};
    MarimayOwnedBytes source_path = {0};
    MarimayOwnedBytes cache_artifact = {0};
    MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
    char hash[65];
    MarimayNativeBuildStatus status = MMAY_NATIVE_BUILD_OK;
    if (result == NULL) return MMAY_NATIVE_BUILD_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    if (request == NULL || request->functions == NULL
        || request->function_count == 0 || request->root_directory == NULL
        || request->root_directory_length == 0) {
        return MMAY_NATIVE_BUILD_INVALID_ARGUMENT;
    }
    if (!request->emit_c) {
        runtime_status = mmay_native_runtime_artifact_select(
            allocator,
            request->release
                ? MMAY_NATIVE_RUNTIME_RELEASE
                : MMAY_NATIVE_RUNTIME_DEBUG,
            request->tool_executable_path,
            request->runtime_paths,
            &runtime
        );
        if (runtime_status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) {
            status = mmay_native_build_runtime_status(runtime_status);
            goto cleanup;
        }
        {
            MarimayNativeToolchainStatus toolchain_status =
                mmay_native_toolchain_probe(
                    allocator,
                    request->compiler_path != NULL
                        ? request->compiler_path : MMAY_NATIVE_C_COMPILER,
                    MMAY_NATIVE_C_COMPILER_VERSION,
                    &result->tool_output
                );
            if (toolchain_status != MMAY_NATIVE_TOOLCHAIN_OK) {
                status = toolchain_status
                        == MMAY_NATIVE_TOOLCHAIN_ALLOCATION_FAILED
                    ? MMAY_NATIVE_BUILD_ALLOCATION_FAILED
                    : MMAY_NATIVE_BUILD_TOOLCHAIN_MISSING;
                goto cleanup;
            }
        }
        mmay_owned_bytes_dispose(&result->tool_output);
    }
    memset(&program, 0, sizeof(program));
    program.functions = request->functions;
    program.function_count = request->function_count;
    program.entry_symbol = request->entry_symbol;
    emit_options = mmay_native_c_emit_default_options();
    emit_options.executable_entry = true;
    emit_options.executable_takes_arguments = request->takes_arguments;
    emit_options.executable_returns_status = request->returns_status;
    emit_options.executable_returns_result = request->returns_result;
    emit_options.error_message_symbol = request->error_message_symbol;
    emit_options.test_entries = request->test_entries;
    emit_options.test_entry_count = request->test_entry_count;
    emit_options.panic_locations = request->panic_locations;
    emit_options.panic_location_count = request->panic_location_count;
    emit_options.read_roots = request->read_roots;
    emit_options.read_root_count = request->read_root_count;
    emit_options.write_roots = request->write_roots;
    emit_options.write_root_count = request->write_root_count;
    emit_options.env_grants = request->env_grants;
    emit_options.env_grant_count = request->env_grant_count;
    if (request->max_heap_bytes != 0) {
        emit_options.max_heap_bytes = request->max_heap_bytes;
    }
    emit_status = mmay_native_c_emit_program(
        &program, allocator, emit_options, &source
    );
    if (emit_status != MMAY_NATIVE_C_EMIT_OK) {
        status = emit_status == MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED
            ? MMAY_NATIVE_BUILD_ALLOCATION_FAILED
            : emit_status == MMAY_NATIVE_C_EMIT_LIMIT_REACHED
                ? MMAY_NATIVE_BUILD_LIMIT_REACHED
                : emit_status == MMAY_NATIVE_C_EMIT_UNSUPPORTED_HOST
                    ? MMAY_NATIVE_BUILD_UNSUPPORTED_HOST
                    : MMAY_NATIVE_BUILD_EMIT_FAILED;
        goto cleanup;
    }
    if (!mmay_native_build_hash(
            &source, request, request->emit_c ? NULL : &runtime, hash
        )) {
        status = MMAY_NATIVE_BUILD_LIMIT_REACHED;
        goto cleanup;
    }
    status = mmay_native_build_cache_directory(
        allocator, request, hash, &directory
    );
    if (status != MMAY_NATIVE_BUILD_OK) goto cleanup;
    status = mmay_native_build_join(
        allocator, (const char *)directory.data, directory.length,
        "program.c", &source_path
    );
    if (status != MMAY_NATIVE_BUILD_OK) goto cleanup;
    status = mmay_native_build_write_source(allocator, &source, &source_path);
    if (status != MMAY_NATIVE_BUILD_OK) goto cleanup;
    if (request->emit_c) {
        if (request->output_path != NULL) {
            MarimayOwnedBytes requested = {0};
            if (!mmay_owned_bytes_copy(
                    &requested, allocator, request->output_path,
                    strlen(request->output_path)
                )) {
                status = MMAY_NATIVE_BUILD_ALLOCATION_FAILED;
                goto cleanup;
            }
            status = mmay_native_build_write_source(
                allocator, &source, &requested
            );
            if (status == MMAY_NATIVE_BUILD_OK) {
                result->artifact_path = requested;
                memset(&requested, 0, sizeof(requested));
            }
            mmay_owned_bytes_dispose(&requested);
        } else {
            result->artifact_path = source_path;
            memset(&source_path, 0, sizeof(source_path));
        }
        goto cleanup;
    }
    status = mmay_native_build_join(
        allocator, (const char *)directory.data, directory.length,
#if defined(_WIN32)
        "app.exe",
#else
        "app",
#endif
        &cache_artifact
    );
    if (status != MMAY_NATIVE_BUILD_OK) goto cleanup;
    if (mmay_directory_kind(
            allocator, (const char *)cache_artifact.data,
            cache_artifact.length, &kind
        ) != MMAY_FILE_OK) {
        status = MMAY_NATIVE_BUILD_IO_FAILED;
        goto cleanup;
    }
    if (kind == MMAY_DIRECTORY_ENTRY_REGULAR) {
        result->cache_hit = true;
    } else if (kind != MMAY_DIRECTORY_ENTRY_MISSING) {
        status = MMAY_NATIVE_BUILD_IO_FAILED;
        goto cleanup;
    } else {
        status = mmay_native_build_compile(
            allocator, request, &runtime, &source_path,
            (const char *)cache_artifact.data, &result->tool_output
        );
        if (status != MMAY_NATIVE_BUILD_OK) goto cleanup;
    }
    if (request->output_path != NULL) {
        status = mmay_native_build_compile(
            allocator, request, &runtime, &source_path, request->output_path,
            &result->tool_output
        );
        if (status != MMAY_NATIVE_BUILD_OK) goto cleanup;
        if (!mmay_owned_bytes_copy(
                &result->artifact_path, allocator, request->output_path,
                strlen(request->output_path)
            )) status = MMAY_NATIVE_BUILD_ALLOCATION_FAILED;
    } else {
        result->artifact_path = cache_artifact;
        memset(&cache_artifact, 0, sizeof(cache_artifact));
    }
    result->runtime_path = runtime.archive;
    memset(&runtime.archive, 0, sizeof(runtime.archive));
    memcpy(result->runtime_hash, runtime.content_hash, 65);
    result->runtime_installed = runtime.installed;
cleanup:
    mmay_native_runtime_artifact_dispose(&runtime);
    mmay_owned_bytes_dispose(&cache_artifact);
    mmay_owned_bytes_dispose(&source_path);
    mmay_owned_bytes_dispose(&directory);
    mmay_owned_bytes_dispose(&source);
    if (status != MMAY_NATIVE_BUILD_OK) {
        mmay_owned_bytes_dispose(&result->artifact_path);
        mmay_owned_bytes_dispose(&result->runtime_path);
        memset(result->runtime_hash, 0, sizeof(result->runtime_hash));
        result->runtime_installed = false;
    }
    return status;
}

void mmay_native_build_result_dispose(MarimayNativeBuildResult *result) {
    if (result != NULL) {
        mmay_owned_bytes_dispose(&result->artifact_path);
        mmay_owned_bytes_dispose(&result->runtime_path);
        mmay_owned_bytes_dispose(&result->tool_output);
        memset(result->runtime_hash, 0, sizeof(result->runtime_hash));
        result->cache_hit = false;
        result->runtime_installed = false;
    }
}
