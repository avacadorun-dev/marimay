#ifndef RUDA_NATIVE_BUILD_H
#define RUDA_NATIVE_BUILD_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/mir/mir.h"

typedef struct RudaNativeBuildRequest {
    const RudaMirFunction *const *functions;
    size_t function_count;
    RudaSymbolId entry_symbol;
    RudaSymbolId error_message_symbol;
    const char *root_directory;
    size_t root_directory_length;
    const char *output_path;
    const char *compiler_path;
    bool returns_result;
    bool release;
    bool emit_c;
} RudaNativeBuildRequest;

typedef struct RudaNativeBuildResult {
    RudaOwnedBytes artifact_path;
    RudaOwnedBytes tool_output;
    bool cache_hit;
} RudaNativeBuildResult;

typedef enum RudaNativeBuildStatus {
    RUDA_NATIVE_BUILD_OK,
    RUDA_NATIVE_BUILD_INVALID_ARGUMENT,
    RUDA_NATIVE_BUILD_ALLOCATION_FAILED,
    RUDA_NATIVE_BUILD_LIMIT_REACHED,
    RUDA_NATIVE_BUILD_TOOLCHAIN_MISSING,
    RUDA_NATIVE_BUILD_EMIT_FAILED,
    RUDA_NATIVE_BUILD_IO_FAILED,
    RUDA_NATIVE_BUILD_COMPILE_FAILED,
    RUDA_NATIVE_BUILD_UNSUPPORTED_HOST
} RudaNativeBuildStatus;

RudaNativeBuildStatus ruda_native_build(
    RudaAllocator allocator,
    const RudaNativeBuildRequest *request,
    RudaNativeBuildResult *result
);
void ruda_native_build_result_dispose(RudaNativeBuildResult *result);

#endif
