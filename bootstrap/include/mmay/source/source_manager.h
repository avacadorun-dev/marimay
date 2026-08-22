#ifndef MMAY_SOURCE_SOURCE_MANAGER_H
#define MMAY_SOURCE_SOURCE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"
#include "mmay/base/vector.h"

typedef uint32_t MarimayFileId;
typedef uint64_t MarimaySnapshotId;

typedef enum MarimaySourceStatus {
    MMAY_SOURCE_OK,
    MMAY_SOURCE_INVALID_ARGUMENT,
    MMAY_SOURCE_ALLOCATION_FAILED,
    MMAY_SOURCE_DUPLICATE_PATH,
    MMAY_SOURCE_UNKNOWN_FILE,
    MMAY_SOURCE_ID_EXHAUSTED
} MarimaySourceStatus;

typedef struct MarimaySourceSnapshot MarimaySourceSnapshot;

typedef struct MarimaySourceManager {
    MarimayAllocator allocator;
    MarimayVector files;
    MarimayFileId next_file_id;
    MarimaySnapshotId next_snapshot_id;
} MarimaySourceManager;

typedef struct MarimaySourcePosition {
    size_t line;
    size_t column;
} MarimaySourcePosition;

/* Manager owns every file and immutable snapshot until dispose. */
bool mmay_source_manager_init(MarimaySourceManager *manager, MarimayAllocator allocator);
void mmay_source_manager_dispose(MarimaySourceManager *manager);

/* path/source bytes are borrowed and copied before return. */
MarimaySourceStatus mmay_source_manager_add(
    MarimaySourceManager *manager,
    const char *path,
    size_t path_length,
    const void *source_bytes,
    size_t source_length,
    const MarimaySourceSnapshot **snapshot
);

/* Creates a retained editor/stdin overlay for an existing stable file ID. */
MarimaySourceStatus mmay_source_manager_overlay(
    MarimaySourceManager *manager,
    MarimayFileId file_id,
    const void *source_bytes,
    size_t source_length,
    const MarimaySourceSnapshot **snapshot
);

const MarimaySourceSnapshot *mmay_source_manager_current(
    const MarimaySourceManager *manager,
    MarimayFileId file_id
);

MarimayFileId mmay_source_file_id(const MarimaySourceSnapshot *snapshot);
MarimaySnapshotId mmay_source_snapshot_id(const MarimaySourceSnapshot *snapshot);
const char *mmay_source_path(const MarimaySourceSnapshot *snapshot);
size_t mmay_source_path_length(const MarimaySourceSnapshot *snapshot);
const unsigned char *mmay_source_bytes(const MarimaySourceSnapshot *snapshot);
size_t mmay_source_length(const MarimaySourceSnapshot *snapshot);
const unsigned char *mmay_source_sha256(const MarimaySourceSnapshot *snapshot);
const char *mmay_source_snapshot_identity(const MarimaySourceSnapshot *snapshot);
bool mmay_source_has_bom(const MarimaySourceSnapshot *snapshot);
bool mmay_source_is_valid_utf8(const MarimaySourceSnapshot *snapshot);
size_t mmay_source_invalid_utf8_offset(const MarimaySourceSnapshot *snapshot);
size_t mmay_source_line_count(const MarimaySourceSnapshot *snapshot);

/* Maps an exact byte offset in [0, length] under D022. */
bool mmay_source_position(
    const MarimaySourceSnapshot *snapshot,
    size_t byte_offset,
    MarimaySourcePosition *position
);

/* Returns the physical byte range of a 1-based logical line, excluding newline bytes. */
bool mmay_source_line_bounds(
    const MarimaySourceSnapshot *snapshot,
    size_t line,
    size_t *start,
    size_t *end
);

#endif
