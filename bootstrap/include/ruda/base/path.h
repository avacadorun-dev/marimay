#ifndef RUDA_BASE_PATH_H
#define RUDA_BASE_PATH_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"

typedef enum RudaPathFlavor {
    RUDA_PATH_POSIX,
    RUDA_PATH_WINDOWS
} RudaPathFlavor;

typedef enum RudaPathStatus {
    RUDA_PATH_OK,
    RUDA_PATH_INVALID_ARGUMENT,
    RUDA_PATH_INVALID_UTF8,
    RUDA_PATH_INVALID_CHARACTER,
    RUDA_PATH_UNSUPPORTED_PREFIX,
    RUDA_PATH_ESCAPES_ROOT,
    RUDA_PATH_ALLOCATION_FAILED
} RudaPathStatus;

typedef struct RudaPath {
    RudaOwnedBytes text;
    RudaPathFlavor flavor;
    bool absolute;
} RudaPath;

/*
 * Performs lexical normalization only: separators, dot segments, and bounded
 * parent resolution. It never queries the filesystem. Output uses '/'.
 */
RudaPathStatus ruda_path_normalize(
    RudaPath *output,
    RudaAllocator allocator,
    RudaPathFlavor flavor,
    const char *input,
    size_t input_length
);

void ruda_path_dispose(RudaPath *path);

#endif
