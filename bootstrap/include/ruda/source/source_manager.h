#ifndef RUDA_SOURCE_SOURCE_MANAGER_H
#define RUDA_SOURCE_SOURCE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/vector.h"

typedef uint32_t RudaFileId;
typedef uint64_t RudaSnapshotId;

typedef enum RudaSourceStatus {
    RUDA_SOURCE_OK,
    RUDA_SOURCE_INVALID_ARGUMENT,
    RUDA_SOURCE_ALLOCATION_FAILED,
    RUDA_SOURCE_DUPLICATE_PATH,
    RUDA_SOURCE_UNKNOWN_FILE,
    RUDA_SOURCE_ID_EXHAUSTED
} RudaSourceStatus;

typedef struct RudaSourceSnapshot RudaSourceSnapshot;

typedef struct RudaSourceManager {
    RudaAllocator allocator;
    RudaVector files;
    RudaFileId next_file_id;
    RudaSnapshotId next_snapshot_id;
} RudaSourceManager;

typedef struct RudaSourcePosition {
    size_t line;
    size_t column;
} RudaSourcePosition;

/* Manager owns every file and immutable snapshot until dispose. */
bool ruda_source_manager_init(RudaSourceManager *manager, RudaAllocator allocator);
void ruda_source_manager_dispose(RudaSourceManager *manager);

/* path/source bytes are borrowed and copied before return. */
RudaSourceStatus ruda_source_manager_add(
    RudaSourceManager *manager,
    const char *path,
    size_t path_length,
    const void *source_bytes,
    size_t source_length,
    const RudaSourceSnapshot **snapshot
);

/* Creates a retained editor/stdin overlay for an existing stable file ID. */
RudaSourceStatus ruda_source_manager_overlay(
    RudaSourceManager *manager,
    RudaFileId file_id,
    const void *source_bytes,
    size_t source_length,
    const RudaSourceSnapshot **snapshot
);

const RudaSourceSnapshot *ruda_source_manager_current(
    const RudaSourceManager *manager,
    RudaFileId file_id
);

RudaFileId ruda_source_file_id(const RudaSourceSnapshot *snapshot);
RudaSnapshotId ruda_source_snapshot_id(const RudaSourceSnapshot *snapshot);
const char *ruda_source_path(const RudaSourceSnapshot *snapshot);
size_t ruda_source_path_length(const RudaSourceSnapshot *snapshot);
const unsigned char *ruda_source_bytes(const RudaSourceSnapshot *snapshot);
size_t ruda_source_length(const RudaSourceSnapshot *snapshot);
const unsigned char *ruda_source_sha256(const RudaSourceSnapshot *snapshot);
const char *ruda_source_snapshot_identity(const RudaSourceSnapshot *snapshot);
bool ruda_source_has_bom(const RudaSourceSnapshot *snapshot);
bool ruda_source_is_valid_utf8(const RudaSourceSnapshot *snapshot);
size_t ruda_source_invalid_utf8_offset(const RudaSourceSnapshot *snapshot);
size_t ruda_source_line_count(const RudaSourceSnapshot *snapshot);

/* Maps an exact byte offset in [0, length] under D022. */
bool ruda_source_position(
    const RudaSourceSnapshot *snapshot,
    size_t byte_offset,
    RudaSourcePosition *position
);

/* Returns the physical byte range of a 1-based logical line, excluding newline bytes. */
bool ruda_source_line_bounds(
    const RudaSourceSnapshot *snapshot,
    size_t line,
    size_t *start,
    size_t *end
);

#endif
