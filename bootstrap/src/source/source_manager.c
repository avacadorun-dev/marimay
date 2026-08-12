#include "ruda/source/source_manager.h"

#include <string.h>

#include "ruda/base/owned_bytes.h"
#include "ruda/base/hash.h"
#include "ruda/base/utf8.h"

typedef struct RudaSourceFile {
    RudaAllocator allocator;
    RudaFileId id;
    RudaOwnedBytes path;
    RudaVector snapshots;
    RudaSourceSnapshot *current;
} RudaSourceFile;

struct RudaSourceSnapshot {
    RudaAllocator allocator;
    RudaSourceFile *file;
    RudaSnapshotId id;
    RudaOwnedBytes source;
    RudaVector line_starts;
    size_t content_start;
    unsigned char sha256[32];
    char identity[72];
    bool valid_utf8;
    size_t invalid_utf8_offset;
};

static RudaSourceFile *ruda_source_find_file(const RudaSourceManager *manager, RudaFileId id) {
    size_t index = 0;
    for (index = 0; index < manager->files.length; index += 1) {
        RudaSourceFile *const *slot = ruda_vector_at_const(&manager->files, index);
        if ((*slot)->id == id) {
            return *slot;
        }
    }
    return NULL;
}

static bool ruda_source_path_exists(
    const RudaSourceManager *manager,
    const char *path,
    size_t path_length
) {
    size_t index = 0;
    for (index = 0; index < manager->files.length; index += 1) {
        RudaSourceFile *const *slot = ruda_vector_at_const(&manager->files, index);
        RudaSourceFile *file = *slot;
        if (file->path.length == path_length
            && (path_length == 0 || memcmp(file->path.data, path, path_length) == 0)) {
            return true;
        }
    }
    return false;
}

static void ruda_source_snapshot_destroy(RudaSourceSnapshot *snapshot) {
    RudaAllocator allocator;
    if (snapshot == NULL) {
        return;
    }
    allocator = snapshot->allocator;
    ruda_vector_dispose(&snapshot->line_starts);
    ruda_owned_bytes_dispose(&snapshot->source);
    ruda_deallocate(&allocator, snapshot, sizeof(*snapshot));
}

static void ruda_source_file_destroy(RudaSourceFile *file) {
    size_t index = 0;
    RudaAllocator allocator;
    if (file == NULL) {
        return;
    }
    allocator = file->allocator;
    for (index = 0; index < file->snapshots.length; index += 1) {
        RudaSourceSnapshot *const *slot = ruda_vector_at_const(&file->snapshots, index);
        ruda_source_snapshot_destroy(*slot);
    }
    ruda_vector_dispose(&file->snapshots);
    ruda_owned_bytes_dispose(&file->path);
    ruda_deallocate(&allocator, file, sizeof(*file));
}

static bool ruda_source_build_lines(RudaSourceSnapshot *snapshot) {
    size_t offset = snapshot->content_start;
    size_t start = snapshot->content_start;
    if (!ruda_vector_push(&snapshot->line_starts, &start)) {
        return false;
    }
    while (offset < snapshot->source.length) {
        unsigned char byte = snapshot->source.data[offset];
        if (byte == '\r') {
            if (offset + 1 < snapshot->source.length
                && snapshot->source.data[offset + 1] == '\n') {
                offset += 2;
            } else {
                offset += 1;
            }
            start = offset;
            if (!ruda_vector_push(&snapshot->line_starts, &start)) {
                return false;
            }
        } else if (byte == '\n') {
            offset += 1;
            start = offset;
            if (!ruda_vector_push(&snapshot->line_starts, &start)) {
                return false;
            }
        } else {
            offset += 1;
        }
    }
    return true;
}

static RudaSourceSnapshot *ruda_source_snapshot_create(
    RudaSourceFile *file,
    RudaSnapshotId id,
    const void *source_bytes,
    size_t source_length
) {
    RudaSourceSnapshot *snapshot = NULL;
    snapshot = ruda_allocate(&file->allocator, sizeof(*snapshot));
    if (snapshot == NULL) {
        return NULL;
    }
    snapshot->allocator = file->allocator;
    snapshot->file = file;
    snapshot->id = id;
    snapshot->source.data = NULL;
    snapshot->source.length = 0;
    snapshot->content_start = 0;
    snapshot->valid_utf8 = false;
    snapshot->invalid_utf8_offset = 0;
    if (!ruda_vector_init(&snapshot->line_starts, file->allocator, sizeof(size_t))) {
        ruda_deallocate(&file->allocator, snapshot, sizeof(*snapshot));
        return NULL;
    }
    if (!ruda_owned_bytes_copy(
            &snapshot->source,
            file->allocator,
            source_bytes,
            source_length
        )) {
        ruda_vector_dispose(&snapshot->line_starts);
        ruda_deallocate(&file->allocator, snapshot, sizeof(*snapshot));
        return NULL;
    }
    if (!ruda_sha256(snapshot->source.data, snapshot->source.length, snapshot->sha256)) {
        ruda_source_snapshot_destroy(snapshot);
        return NULL;
    }
    memcpy(snapshot->identity, "sha256:", 7);
    ruda_sha256_hex(snapshot->sha256, snapshot->identity + 7);
    if (source_length >= 3 && snapshot->source.data[0] == 0xefU
        && snapshot->source.data[1] == 0xbbU && snapshot->source.data[2] == 0xbfU) {
        snapshot->content_start = 3;
    }
    snapshot->valid_utf8 = ruda_utf8_validate(
        (const char *)snapshot->source.data,
        snapshot->source.length,
        &snapshot->invalid_utf8_offset
    );
    if (!ruda_source_build_lines(snapshot)) {
        ruda_source_snapshot_destroy(snapshot);
        return NULL;
    }
    return snapshot;
}

bool ruda_source_manager_init(RudaSourceManager *manager, RudaAllocator allocator) {
    if (manager == NULL || !ruda_vector_init(&manager->files, allocator, sizeof(RudaSourceFile *))) {
        return false;
    }
    manager->allocator = allocator;
    manager->next_file_id = 1;
    manager->next_snapshot_id = 1;
    return true;
}

void ruda_source_manager_dispose(RudaSourceManager *manager) {
    size_t index = 0;
    if (manager == NULL) {
        return;
    }
    for (index = 0; index < manager->files.length; index += 1) {
        RudaSourceFile *const *slot = ruda_vector_at_const(&manager->files, index);
        ruda_source_file_destroy(*slot);
    }
    ruda_vector_dispose(&manager->files);
    manager->next_file_id = 0;
    manager->next_snapshot_id = 0;
}

RudaSourceStatus ruda_source_manager_add(
    RudaSourceManager *manager,
    const char *path,
    size_t path_length,
    const void *source_bytes,
    size_t source_length,
    const RudaSourceSnapshot **snapshot
) {
    RudaSourceFile *file = NULL;
    RudaSourceSnapshot *created = NULL;
    if (snapshot != NULL) {
        *snapshot = NULL;
    }
    if (manager == NULL || (path == NULL && path_length != 0)
        || (source_bytes == NULL && source_length != 0)) {
        return RUDA_SOURCE_INVALID_ARGUMENT;
    }
    if (manager->next_file_id == 0 || manager->next_snapshot_id == 0) {
        return RUDA_SOURCE_ID_EXHAUSTED;
    }
    if (ruda_source_path_exists(manager, path, path_length)) {
        return RUDA_SOURCE_DUPLICATE_PATH;
    }
    file = ruda_allocate(&manager->allocator, sizeof(*file));
    if (file == NULL) {
        return RUDA_SOURCE_ALLOCATION_FAILED;
    }
    file->allocator = manager->allocator;
    file->id = manager->next_file_id;
    file->path.data = NULL;
    file->path.length = 0;
    file->current = NULL;
    if (!ruda_vector_init(&file->snapshots, manager->allocator, sizeof(RudaSourceSnapshot *))) {
        ruda_deallocate(&manager->allocator, file, sizeof(*file));
        return RUDA_SOURCE_ALLOCATION_FAILED;
    }
    if (!ruda_owned_bytes_copy(&file->path, manager->allocator, path, path_length)) {
        ruda_source_file_destroy(file);
        return RUDA_SOURCE_ALLOCATION_FAILED;
    }
    created = ruda_source_snapshot_create(
        file,
        manager->next_snapshot_id,
        source_bytes,
        source_length
    );
    if (created == NULL || !ruda_vector_push(&file->snapshots, &created)) {
        ruda_source_snapshot_destroy(created);
        ruda_source_file_destroy(file);
        return RUDA_SOURCE_ALLOCATION_FAILED;
    }
    file->current = created;
    if (!ruda_vector_push(&manager->files, &file)) {
        ruda_source_file_destroy(file);
        return RUDA_SOURCE_ALLOCATION_FAILED;
    }
    manager->next_file_id = manager->next_file_id == UINT32_MAX
        ? 0
        : manager->next_file_id + 1;
    manager->next_snapshot_id = manager->next_snapshot_id == UINT64_MAX
        ? 0
        : manager->next_snapshot_id + 1;
    if (snapshot != NULL) {
        *snapshot = created;
    }
    return RUDA_SOURCE_OK;
}

RudaSourceStatus ruda_source_manager_overlay(
    RudaSourceManager *manager,
    RudaFileId file_id,
    const void *source_bytes,
    size_t source_length,
    const RudaSourceSnapshot **snapshot
) {
    RudaSourceFile *file = NULL;
    RudaSourceSnapshot *created = NULL;
    if (snapshot != NULL) {
        *snapshot = NULL;
    }
    if (manager == NULL || (source_bytes == NULL && source_length != 0)) {
        return RUDA_SOURCE_INVALID_ARGUMENT;
    }
    if (manager->next_snapshot_id == 0) {
        return RUDA_SOURCE_ID_EXHAUSTED;
    }
    file = ruda_source_find_file(manager, file_id);
    if (file == NULL) {
        return RUDA_SOURCE_UNKNOWN_FILE;
    }
    created = ruda_source_snapshot_create(
        file,
        manager->next_snapshot_id,
        source_bytes,
        source_length
    );
    if (created == NULL || !ruda_vector_push(&file->snapshots, &created)) {
        ruda_source_snapshot_destroy(created);
        return RUDA_SOURCE_ALLOCATION_FAILED;
    }
    file->current = created;
    manager->next_snapshot_id = manager->next_snapshot_id == UINT64_MAX
        ? 0
        : manager->next_snapshot_id + 1;
    if (snapshot != NULL) {
        *snapshot = created;
    }
    return RUDA_SOURCE_OK;
}

const RudaSourceSnapshot *ruda_source_manager_current(
    const RudaSourceManager *manager,
    RudaFileId file_id
) {
    RudaSourceFile *file = NULL;
    if (manager == NULL) {
        return NULL;
    }
    file = ruda_source_find_file(manager, file_id);
    return file == NULL ? NULL : file->current;
}

RudaFileId ruda_source_file_id(const RudaSourceSnapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->file->id;
}

RudaSnapshotId ruda_source_snapshot_id(const RudaSourceSnapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->id;
}

const char *ruda_source_path(const RudaSourceSnapshot *snapshot) {
    return snapshot == NULL ? NULL : (const char *)snapshot->file->path.data;
}

size_t ruda_source_path_length(const RudaSourceSnapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->file->path.length;
}

const unsigned char *ruda_source_bytes(const RudaSourceSnapshot *snapshot) {
    return snapshot == NULL ? NULL : snapshot->source.data;
}

size_t ruda_source_length(const RudaSourceSnapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->source.length;
}

const unsigned char *ruda_source_sha256(const RudaSourceSnapshot *snapshot) {
    return snapshot == NULL ? NULL : snapshot->sha256;
}

const char *ruda_source_snapshot_identity(const RudaSourceSnapshot *snapshot) {
    return snapshot == NULL ? NULL : snapshot->identity;
}

bool ruda_source_has_bom(const RudaSourceSnapshot *snapshot) {
    return snapshot != NULL && snapshot->content_start == 3;
}

bool ruda_source_is_valid_utf8(const RudaSourceSnapshot *snapshot) {
    return snapshot != NULL && snapshot->valid_utf8;
}

size_t ruda_source_invalid_utf8_offset(const RudaSourceSnapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->invalid_utf8_offset;
}

size_t ruda_source_line_count(const RudaSourceSnapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->line_starts.length;
}

bool ruda_source_position(
    const RudaSourceSnapshot *snapshot,
    size_t byte_offset,
    RudaSourcePosition *position
) {
    size_t lower = 0;
    size_t upper = 0;
    size_t line_index = 0;
    size_t cursor = 0;
    size_t column = 1;
    if (snapshot == NULL || position == NULL || byte_offset > snapshot->source.length) {
        return false;
    }
    if (byte_offset <= snapshot->content_start) {
        position->line = 1;
        position->column = 1;
        return true;
    }
    upper = snapshot->line_starts.length;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2;
        const size_t *start = ruda_vector_at_const(&snapshot->line_starts, middle);
        if (*start <= byte_offset) {
            lower = middle + 1;
        } else {
            upper = middle;
        }
    }
    line_index = lower == 0 ? 0 : lower - 1;
    cursor = *(const size_t *)ruda_vector_at_const(&snapshot->line_starts, line_index);
    while (cursor < byte_offset && cursor < snapshot->source.length) {
        unsigned char byte = snapshot->source.data[cursor];
        RudaUtf8Scalar scalar;
        if (byte == '\r' || byte == '\n') {
            break;
        }
        if (ruda_utf8_decode(
                snapshot->source.data + cursor,
                snapshot->source.length - cursor,
                &scalar
            )) {
            if (cursor + scalar.width > byte_offset) {
                break;
            }
            cursor += scalar.width;
        } else {
            cursor += 1;
        }
        column += 1;
    }
    position->line = line_index + 1;
    position->column = column;
    return true;
}

bool ruda_source_line_bounds(
    const RudaSourceSnapshot *snapshot,
    size_t line,
    size_t *start,
    size_t *end
) {
    const size_t *line_start = NULL;
    size_t line_end = 0;
    if (snapshot == NULL || start == NULL || end == NULL || line == 0
        || line > snapshot->line_starts.length) {
        return false;
    }
    line_start = ruda_vector_at_const(&snapshot->line_starts, line - 1);
    if (line_start == NULL) {
        return false;
    }
    if (line < snapshot->line_starts.length) {
        const size_t *next = ruda_vector_at_const(&snapshot->line_starts, line);
        if (next == NULL) {
            return false;
        }
        line_end = *next;
        if (line_end != 0 && snapshot->source.data[line_end - 1] == '\n') {
            line_end -= 1;
        }
        if (line_end != 0 && snapshot->source.data[line_end - 1] == '\r') {
            line_end -= 1;
        }
    } else {
        line_end = snapshot->source.length;
    }
    if (line_end < *line_start) {
        return false;
    }
    *start = *line_start;
    *end = line_end;
    return true;
}
