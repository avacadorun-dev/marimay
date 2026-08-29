#ifndef MMAY_NATIVE_BUILD_H
#define MMAY_NATIVE_BUILD_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/mir/mir.h"
#include "mmay/native/runtime_artifact.h"
#include "mmay/runtime/fs.h"
#include "mmay/runtime/env.h"

typedef struct MarimayNativeBuildRequest {
    const MarimayMirFunction *const *functions;
    size_t function_count;
    MarimaySymbolId entry_symbol;
    MarimaySymbolId error_message_symbol;
    const char *root_directory;
    size_t root_directory_length;
    const char *output_path;
    const char *compiler_path;
    const char *tool_executable_path;
    const MarimayNativeRuntimePaths *runtime_paths;
    /* Zero selects the documented 64 MiB default. */
    size_t max_heap_bytes;
    const MarimayFsPathView *read_roots;
    size_t read_root_count;
    const MarimayFsPathView *write_roots;
    size_t write_root_count;
    const MarimayEnvNameView *env_grants;
    size_t env_grant_count;
    bool takes_arguments;
    bool returns_status;
    bool returns_result;
    bool release;
    bool emit_c;
} MarimayNativeBuildRequest;

typedef struct MarimayNativeBuildResult {
    MarimayOwnedBytes artifact_path;
    MarimayOwnedBytes runtime_path;
    MarimayOwnedBytes tool_output;
    char runtime_hash[65];
    bool cache_hit;
    bool runtime_installed;
} MarimayNativeBuildResult;

typedef enum MarimayNativeBuildStatus {
    MMAY_NATIVE_BUILD_OK,
    MMAY_NATIVE_BUILD_INVALID_ARGUMENT,
    MMAY_NATIVE_BUILD_ALLOCATION_FAILED,
    MMAY_NATIVE_BUILD_LIMIT_REACHED,
    MMAY_NATIVE_BUILD_TOOLCHAIN_MISSING,
    MMAY_NATIVE_BUILD_RUNTIME_MISSING,
    MMAY_NATIVE_BUILD_RUNTIME_INCOMPATIBLE,
    MMAY_NATIVE_BUILD_EMIT_FAILED,
    MMAY_NATIVE_BUILD_IO_FAILED,
    MMAY_NATIVE_BUILD_COMPILE_FAILED,
    MMAY_NATIVE_BUILD_UNSUPPORTED_HOST
} MarimayNativeBuildStatus;

MarimayNativeBuildStatus mmay_native_build(
    MarimayAllocator allocator,
    const MarimayNativeBuildRequest *request,
    MarimayNativeBuildResult *result
);
void mmay_native_build_result_dispose(MarimayNativeBuildResult *result);

#endif
