#ifndef RUDA_PLATFORM_DIRECTORY_H
#define RUDA_PLATFORM_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/platform/file.h"

typedef enum RudaDirectoryEntryKind {
    RUDA_DIRECTORY_ENTRY_MISSING,
    RUDA_DIRECTORY_ENTRY_REGULAR,
    RUDA_DIRECTORY_ENTRY_DIRECTORY,
    RUDA_DIRECTORY_ENTRY_SYMLINK,
    RUDA_DIRECTORY_ENTRY_OTHER
} RudaDirectoryEntryKind;

typedef uintptr_t RudaDirectoryHandle;

typedef RudaFileStatus (*RudaDirectoryCurrentFn)(
    void *context,
    RudaAllocator allocator,
    size_t max_path_bytes,
    RudaOwnedBytes *path
);
typedef RudaFileStatus (*RudaDirectoryKindFn)(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryEntryKind *kind
);
typedef RudaFileStatus (*RudaDirectoryOpenFn)(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryHandle *handle
);
typedef RudaFileStatus (*RudaDirectoryNextFn)(
    void *context,
    RudaDirectoryHandle handle,
    RudaAllocator allocator,
    size_t max_name_bytes,
    RudaOwnedBytes *name,
    RudaDirectoryEntryKind *kind,
    bool *end
);
typedef RudaFileStatus (*RudaDirectoryCloseFn)(
    void *context,
    RudaDirectoryHandle handle
);

typedef struct RudaDirectoryOps {
    void *context;
    RudaDirectoryCurrentFn current;
    RudaDirectoryKindFn kind;
    RudaDirectoryOpenFn open;
    RudaDirectoryNextFn next;
    RudaDirectoryCloseFn close;
} RudaDirectoryOps;

typedef struct RudaDirectoryIterator {
    RudaDirectoryOps ops;
    RudaDirectoryHandle handle;
    bool open;
} RudaDirectoryIterator;

/* Returns the host implementation. The returned value owns no state. */
RudaDirectoryOps ruda_platform_directory_ops(void);

RudaFileStatus ruda_directory_current_with_ops(
    RudaDirectoryOps ops,
    RudaAllocator allocator,
    size_t max_path_bytes,
    RudaOwnedBytes *path
);
RudaFileStatus ruda_directory_current(
    RudaAllocator allocator,
    size_t max_path_bytes,
    RudaOwnedBytes *path
);

RudaFileStatus ruda_directory_kind_with_ops(
    RudaDirectoryOps ops,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryEntryKind *kind
);
RudaFileStatus ruda_directory_kind(
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryEntryKind *kind
);

RudaFileStatus ruda_directory_open_with_ops(
    RudaDirectoryOps ops,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryIterator *iterator
);
RudaFileStatus ruda_directory_open(
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaDirectoryIterator *iterator
);

/* `end` owns no name. A non-end result owns `name` until disposed. */
RudaFileStatus ruda_directory_next(
    RudaDirectoryIterator *iterator,
    RudaAllocator allocator,
    size_t max_name_bytes,
    RudaOwnedBytes *name,
    RudaDirectoryEntryKind *kind,
    bool *end
);
RudaFileStatus ruda_directory_close(RudaDirectoryIterator *iterator);

#endif
