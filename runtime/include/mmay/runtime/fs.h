#ifndef MMAY_RUNTIME_FS_H
#define MMAY_RUNTIME_FS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/path.h"
#include "mmay/base/vector.h"
#include "mmay/platform/directory.h"
#include "mmay/platform/file.h"

#define MMAY_FS_MAX_PATH_BYTES 4096U
#define MMAY_FS_MAX_FILE_BYTES 1048576U
#define MMAY_FS_MAX_DIRECTORY_ENTRIES 65536U
#define MMAY_FS_MAX_CAPABILITY_ROOTS 64U

typedef enum MarimayFsStatus {
    MMAY_FS_OK,
    MMAY_FS_INVALID_ARGUMENT,
    MMAY_FS_INVALID_LIMIT,
    MMAY_FS_LIMIT_EXCEEDED,
    MMAY_FS_CAPABILITY_DENIED,
    MMAY_FS_NOT_FOUND,
    MMAY_FS_NOT_FILE,
    MMAY_FS_NOT_DIRECTORY,
    MMAY_FS_SYMLINK,
    MMAY_FS_CLOSED,
    MMAY_FS_OPEN_FAILED,
    MMAY_FS_READ_FAILED,
    MMAY_FS_WRITE_FAILED,
    MMAY_FS_SYNC_FAILED,
    MMAY_FS_CLOSE_FAILED,
    MMAY_FS_REPLACE_FAILED,
    MMAY_FS_INVALID_UTF8,
    MMAY_FS_TOO_MANY_ENTRIES,
    MMAY_FS_DIRECTORY_FAILED,
    MMAY_FS_ALLOCATION_FAILED,
    MMAY_FS_INTERNAL_FAILED
} MarimayFsStatus;

typedef struct MarimayFsPathView {
    const char *data;
    size_t length;
} MarimayFsPathView;

typedef MarimayFileStatus (*MarimayFsMetadataFn)(
    void *context, MarimayAllocator allocator,
    const char *path, size_t path_length, MarimayFileMetadata *metadata
);

typedef struct MarimayFsOptions {
    MarimayPathFlavor flavor;
    MarimayFsPathView working_directory;
    const MarimayFsPathView *read_roots;
    size_t read_root_count;
    const MarimayFsPathView *write_roots;
    size_t write_root_count;
    size_t max_open_files;
    MarimayFileReadOps read_ops;
    MarimayFileOps write_ops;
    MarimayDirectoryOps directory_ops;
    void *metadata_context;
    MarimayFsMetadataFn metadata;
} MarimayFsOptions;

typedef struct MarimayFsContext {
    MarimayAllocator allocator;
    MarimayPathFlavor flavor;
    MarimayOwnedBytes working_directory;
    MarimayVector read_roots;
    MarimayVector write_roots;
    MarimayVector files;
    MarimayFileReadOps read_ops;
    MarimayFileOps write_ops;
    MarimayDirectoryOps directory_ops;
    void *metadata_context;
    MarimayFsMetadataFn metadata;
    size_t max_open_files;
    uint64_t temporary_counter;
    bool initialized;
} MarimayFsContext;

typedef struct MarimayFsMetadata {
    bool directory;
    uint64_t size;
} MarimayFsMetadata;

typedef struct MarimayFsEntry {
    MarimayOwnedBytes name;
    MarimayDirectoryEntryKind kind;
} MarimayFsEntry;

MarimayFsOptions mmay_fs_default_options(void);
MarimayFsStatus mmay_fs_init_status(
    MarimayFsContext *context, MarimayAllocator allocator,
    const MarimayFsOptions *options
);
bool mmay_fs_init(
    MarimayFsContext *context, MarimayAllocator allocator,
    const MarimayFsOptions *options
);
void mmay_fs_dispose(MarimayFsContext *context);

MarimayFsStatus mmay_fs_open_read(
    MarimayFsContext *context, const char *path, size_t path_length,
    int64_t *handle
);
MarimayFsStatus mmay_fs_read(
    MarimayFsContext *context, int64_t handle, int64_t limit,
    MarimayOwnedBytes *output
);
MarimayFsStatus mmay_fs_close(MarimayFsContext *context, int64_t handle);
void mmay_fs_close_best_effort(MarimayFsContext *context, int64_t handle);
MarimayFsStatus mmay_fs_read_all(
    MarimayFsContext *context, const char *path, size_t path_length,
    int64_t limit, MarimayOwnedBytes *output
);
MarimayFsStatus mmay_fs_write_atomic(
    MarimayFsContext *context, const char *path, size_t path_length,
    const unsigned char *bytes, size_t length
);
MarimayFsStatus mmay_fs_metadata(
    MarimayFsContext *context, const char *path, size_t path_length,
    MarimayFsMetadata *metadata
);
MarimayFsStatus mmay_fs_read_directory(
    MarimayFsContext *context, const char *path, size_t path_length,
    int64_t limit, MarimayVector *entries
);
void mmay_fs_entries_dispose(MarimayVector *entries);

#endif
