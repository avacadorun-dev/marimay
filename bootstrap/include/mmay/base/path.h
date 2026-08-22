#ifndef MMAY_BASE_PATH_H
#define MMAY_BASE_PATH_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"

typedef enum MarimayPathFlavor {
    MMAY_PATH_POSIX,
    MMAY_PATH_WINDOWS
} MarimayPathFlavor;

typedef enum MarimayPathStatus {
    MMAY_PATH_OK,
    MMAY_PATH_INVALID_ARGUMENT,
    MMAY_PATH_INVALID_UTF8,
    MMAY_PATH_INVALID_CHARACTER,
    MMAY_PATH_UNSUPPORTED_PREFIX,
    MMAY_PATH_ESCAPES_ROOT,
    MMAY_PATH_ALLOCATION_FAILED
} MarimayPathStatus;

typedef struct MarimayPath {
    MarimayOwnedBytes text;
    MarimayPathFlavor flavor;
    bool absolute;
} MarimayPath;

/*
 * Performs lexical normalization only: separators, dot segments, and bounded
 * parent resolution. It never queries the filesystem. Output uses '/'.
 */
MarimayPathStatus mmay_path_normalize(
    MarimayPath *output,
    MarimayAllocator allocator,
    MarimayPathFlavor flavor,
    const char *input,
    size_t input_length
);

void mmay_path_dispose(MarimayPath *path);

#endif
