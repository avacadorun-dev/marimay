#ifndef RUDA_PLATFORM_FILE_H
#define RUDA_PLATFORM_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"

typedef enum RudaFileStatus {
    RUDA_FILE_OK,
    RUDA_FILE_INVALID_ARGUMENT,
    RUDA_FILE_INVALID_UTF8,
    RUDA_FILE_ALLOCATION_FAILED,
    RUDA_FILE_LIMIT_EXCEEDED,
    RUDA_FILE_ALREADY_EXISTS,
    RUDA_FILE_IO_FAILED
} RudaFileStatus;

typedef uintptr_t RudaFileHandle;

typedef RudaFileStatus (*RudaFileCreateExclusiveFn)(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaFileHandle *handle
);
typedef RudaFileStatus (*RudaFileWriteFn)(
    void *context,
    RudaFileHandle handle,
    const unsigned char *bytes,
    size_t length,
    size_t *written
);
typedef RudaFileStatus (*RudaFileHandleFn)(void *context, RudaFileHandle handle);
typedef RudaFileStatus (*RudaFileReplaceFn)(
    void *context,
    RudaAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
);
typedef void (*RudaFileRemoveFn)(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length
);

typedef struct RudaFileOps {
    void *context;
    RudaFileCreateExclusiveFn create_exclusive;
    RudaFileWriteFn write;
    RudaFileHandleFn sync;
    RudaFileHandleFn close;
    RudaFileReplaceFn replace;
    RudaFileRemoveFn remove;
} RudaFileOps;

typedef RudaFileStatus (*RudaFileOpenReadFn)(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaFileHandle *handle
);
typedef RudaFileStatus (*RudaFileReadFn)(
    void *context,
    RudaFileHandle handle,
    unsigned char *bytes,
    size_t capacity,
    size_t *read_count,
    bool *end
);

typedef struct RudaFileReadOps {
    void *context;
    RudaFileOpenReadFn open;
    RudaFileReadFn read;
    RudaFileHandleFn close;
} RudaFileReadOps;

/* Returns the host implementation. The returned value owns no state. */
RudaFileOps ruda_platform_file_ops(void);
RudaFileReadOps ruda_platform_file_read_ops(void);

/* Reads one UTF-8 host path into an exact owned buffer under a byte limit. */
RudaFileStatus ruda_file_read_all_with_ops(
    RudaFileReadOps ops,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    size_t max_bytes,
    RudaOwnedBytes *output
);

RudaFileStatus ruda_file_read_all(
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    size_t max_bytes,
    RudaOwnedBytes *output
);

/*
 * Writes bytes to a caller-selected, non-existent sibling temporary path,
 * syncs and closes it, then atomically replaces destination_path. The caller
 * retains all inputs. The temporary file is removed after any failure.
 *
 * Both paths must be valid UTF-8 without embedded zero bytes, differ from one
 * another, and have the same lexical parent directory. Output visibility is
 * failure-atomic: destination_path is unchanged until replace succeeds.
 */
RudaFileStatus ruda_file_replace_atomic_with_ops(
    RudaFileOps ops,
    RudaAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length,
    const unsigned char *bytes,
    size_t length
);

RudaFileStatus ruda_file_replace_atomic(
    RudaAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length,
    const unsigned char *bytes,
    size_t length
);

/* Atomically publishes a compiler-created executable sibling. */
RudaFileStatus ruda_file_replace_executable(
    RudaAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
);

#endif
