#ifndef MMAY_PLATFORM_DIRECTORY_H
#define MMAY_PLATFORM_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/platform/file.h"

typedef enum MarimayDirectoryEntryKind {
    MMAY_DIRECTORY_ENTRY_MISSING,
    MMAY_DIRECTORY_ENTRY_REGULAR,
    MMAY_DIRECTORY_ENTRY_DIRECTORY,
    MMAY_DIRECTORY_ENTRY_SYMLINK,
    MMAY_DIRECTORY_ENTRY_OTHER
} MarimayDirectoryEntryKind;

typedef uintptr_t MarimayDirectoryHandle;

typedef MarimayFileStatus (*MarimayDirectoryCurrentFn)(
    void *context,
    MarimayAllocator allocator,
    size_t max_path_bytes,
    MarimayOwnedBytes *path
);
typedef MarimayFileStatus (*MarimayDirectoryKindFn)(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryEntryKind *kind
);
typedef MarimayFileStatus (*MarimayDirectoryOpenFn)(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryHandle *handle
);
typedef MarimayFileStatus (*MarimayDirectoryNextFn)(
    void *context,
    MarimayDirectoryHandle handle,
    MarimayAllocator allocator,
    size_t max_name_bytes,
    MarimayOwnedBytes *name,
    MarimayDirectoryEntryKind *kind,
    bool *end
);
typedef MarimayFileStatus (*MarimayDirectoryCloseFn)(
    void *context,
    MarimayDirectoryHandle handle
);

typedef struct MarimayDirectoryOps {
    void *context;
    MarimayDirectoryCurrentFn current;
    MarimayDirectoryKindFn kind;
    MarimayDirectoryOpenFn open;
    MarimayDirectoryNextFn next;
    MarimayDirectoryCloseFn close;
} MarimayDirectoryOps;

typedef struct MarimayDirectoryIterator {
    MarimayDirectoryOps ops;
    MarimayDirectoryHandle handle;
    bool open;
} MarimayDirectoryIterator;

/* Returns the host implementation. The returned value owns no state. */
MarimayDirectoryOps mmay_platform_directory_ops(void);

MarimayFileStatus mmay_directory_current_with_ops(
    MarimayDirectoryOps ops,
    MarimayAllocator allocator,
    size_t max_path_bytes,
    MarimayOwnedBytes *path
);
MarimayFileStatus mmay_directory_current(
    MarimayAllocator allocator,
    size_t max_path_bytes,
    MarimayOwnedBytes *path
);

MarimayFileStatus mmay_directory_kind_with_ops(
    MarimayDirectoryOps ops,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryEntryKind *kind
);
MarimayFileStatus mmay_directory_kind(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryEntryKind *kind
);

/* Creates one directory, succeeding when that exact directory exists. */
MarimayFileStatus mmay_directory_create(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length
);

/* Creates one previously missing directory without accepting a collision. */
MarimayFileStatus mmay_directory_create_exclusive(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length
);

/* Publishes a complete sibling directory into a missing destination. */
MarimayFileStatus mmay_directory_publish(
    MarimayAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
);

/* Removes one empty directory. */
MarimayFileStatus mmay_directory_remove_empty(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length
);

MarimayFileStatus mmay_directory_open_with_ops(
    MarimayDirectoryOps ops,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryIterator *iterator
);
MarimayFileStatus mmay_directory_open(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayDirectoryIterator *iterator
);

/* `end` owns no name. A non-end result owns `name` until disposed. */
MarimayFileStatus mmay_directory_next(
    MarimayDirectoryIterator *iterator,
    MarimayAllocator allocator,
    size_t max_name_bytes,
    MarimayOwnedBytes *name,
    MarimayDirectoryEntryKind *kind,
    bool *end
);
MarimayFileStatus mmay_directory_close(MarimayDirectoryIterator *iterator);

#endif
