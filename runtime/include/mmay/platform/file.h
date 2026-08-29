#ifndef MMAY_PLATFORM_FILE_H
#define MMAY_PLATFORM_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"

typedef enum MarimayFileStatus {
    MMAY_FILE_OK,
    MMAY_FILE_INVALID_ARGUMENT,
    MMAY_FILE_INVALID_UTF8,
    MMAY_FILE_ALLOCATION_FAILED,
    MMAY_FILE_LIMIT_EXCEEDED,
    MMAY_FILE_ALREADY_EXISTS,
    MMAY_FILE_IO_FAILED
} MarimayFileStatus;

typedef uintptr_t MarimayFileHandle;

typedef struct MarimayFileMetadata {
    bool regular; bool directory; bool symlink;
    uint64_t size;
} MarimayFileMetadata;

typedef MarimayFileStatus (*MarimayFileCreateExclusiveFn)(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayFileHandle *handle
);
typedef MarimayFileStatus (*MarimayFileWriteFn)(
    void *context,
    MarimayFileHandle handle,
    const unsigned char *bytes,
    size_t length,
    size_t *written
);
typedef MarimayFileStatus (*MarimayFileHandleFn)(void *context, MarimayFileHandle handle);
typedef MarimayFileStatus (*MarimayFileReplaceFn)(
    void *context,
    MarimayAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
);
typedef void (*MarimayFileRemoveFn)(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length
);

typedef struct MarimayFileOps {
    void *context;
    MarimayFileCreateExclusiveFn create_exclusive;
    MarimayFileWriteFn write;
    MarimayFileHandleFn sync;
    MarimayFileHandleFn close;
    MarimayFileReplaceFn replace;
    MarimayFileRemoveFn remove;
} MarimayFileOps;

typedef MarimayFileStatus (*MarimayFileOpenReadFn)(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayFileHandle *handle
);
typedef MarimayFileStatus (*MarimayFileReadFn)(
    void *context,
    MarimayFileHandle handle,
    unsigned char *bytes,
    size_t capacity,
    size_t *read_count,
    bool *end
);

typedef struct MarimayFileReadOps {
    void *context;
    MarimayFileOpenReadFn open;
    MarimayFileReadFn read;
    MarimayFileHandleFn close;
} MarimayFileReadOps;

/* Returns the host implementation. The returned value owns no state. */
MarimayFileOps mmay_platform_file_ops(void);
MarimayFileReadOps mmay_platform_file_read_ops(void);

/* Inspects one path without following its final symbolic-link component. */
MarimayFileStatus mmay_file_metadata(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayFileMetadata *metadata
);

/* Reads one UTF-8 host path into an exact owned buffer under a byte limit. */
MarimayFileStatus mmay_file_read_all_with_ops(
    MarimayFileReadOps ops,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    size_t max_bytes,
    MarimayOwnedBytes *output
);

MarimayFileStatus mmay_file_read_all(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    size_t max_bytes,
    MarimayOwnedBytes *output
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
MarimayFileStatus mmay_file_replace_atomic_with_ops(
    MarimayFileOps ops,
    MarimayAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length,
    const unsigned char *bytes,
    size_t length
);

MarimayFileStatus mmay_file_replace_atomic(
    MarimayAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length,
    const unsigned char *bytes,
    size_t length
);

/* Atomically publishes a compiler-created executable sibling. */
MarimayFileStatus mmay_file_replace_executable(
    MarimayAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
);

#endif
