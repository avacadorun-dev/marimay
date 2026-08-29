#ifndef MMAY_RUNTIME_PROCESS_CONTEXT_H
#define MMAY_RUNTIME_PROCESS_CONTEXT_H

#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/base/path.h"

#define MMAY_PROCESS_CONTEXT_MAX_PATH_BYTES 4096U

typedef enum MarimayProcessContextSourceStatus {
    MMAY_PROCESS_CONTEXT_SOURCE_OK,
    MMAY_PROCESS_CONTEXT_SOURCE_UNAVAILABLE,
    MMAY_PROCESS_CONTEXT_SOURCE_ALLOCATION_FAILED,
    MMAY_PROCESS_CONTEXT_SOURCE_LIMIT_EXCEEDED,
    MMAY_PROCESS_CONTEXT_SOURCE_INVALID_ENCODING
} MarimayProcessContextSourceStatus;

typedef MarimayProcessContextSourceStatus (*MarimayProcessContextReadFn)(
    void *context,
    MarimayAllocator allocator,
    size_t max_path_bytes,
    MarimayOwnedBytes *path
);

typedef struct MarimayProcessContextOptions {
    MarimayPathFlavor flavor;
    void *current_directory_context;
    MarimayProcessContextReadFn current_directory;
    void *executable_path_context;
    MarimayProcessContextReadFn executable_path;
} MarimayProcessContextOptions;

typedef enum MarimayProcessContextStatus {
    MMAY_PROCESS_CONTEXT_OK,
    MMAY_PROCESS_CONTEXT_INVALID_ARGUMENT,
    MMAY_PROCESS_CONTEXT_CURRENT_DIRECTORY_UNAVAILABLE,
    MMAY_PROCESS_CONTEXT_EXECUTABLE_PATH_UNAVAILABLE,
    MMAY_PROCESS_CONTEXT_INVALID_UTF8,
    MMAY_PROCESS_CONTEXT_INVALID_PATH,
    MMAY_PROCESS_CONTEXT_ALLOCATION_FAILED
} MarimayProcessContextStatus;

MarimayProcessContextOptions mmay_process_context_default_options(void);
MarimayProcessContextStatus mmay_process_context_current_directory(
    const MarimayProcessContextOptions *options,
    MarimayAllocator allocator,
    MarimayOwnedBytes *path,
    size_t *invalid_utf8_offset
);
MarimayProcessContextStatus mmay_process_context_executable_path(
    const MarimayProcessContextOptions *options,
    MarimayAllocator allocator,
    MarimayOwnedBytes *path,
    size_t *invalid_utf8_offset
);

#endif
