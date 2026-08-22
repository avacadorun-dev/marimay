#include "mmay/source/source_manager.h"

#include <string.h>

#include "mmay/base/owned_bytes.h"
#include "mmay/base/hash.h"
#include "mmay/base/utf8.h"

typedef struct MarimaySourceFile {
    MarimayAllocator allocator;
    MarimayFileId id;
    MarimayOwnedBytes path;
    MarimayVector snapshots;
    MarimaySourceSnapshot *current;
} MarimaySourceFile;

struct MarimaySourceSnapshot {
    MarimayAllocator allocator;
    MarimaySourceFile *file;
    MarimaySnapshotId id;
    MarimayOwnedBytes source;
    MarimayVector line_starts;
    size_t content_start;
    unsigned char sha256[32];
    char identity[72];
    bool valid_utf8;
    size_t invalid_utf8_offset;
};

static MarimaySourceFile *mmay_source_find_file(const MarimaySourceManager *manager, MarimayFileId id) {
    size_t index = 0;
    for (index = 0; index < manager->files.length; index += 1) {
        MarimaySourceFile *const *slot = mmay_vector_at_const(&manager->files, index);
        if ((*slot)->id == id) {
            return *slot;
        }
    }
    return NULL;
}

static bool mmay_source_path_exists(
    const MarimaySourceManager *manager,
    const char *path,
    size_t path_length
) {
    size_t index = 0;
    for (index = 0; index < manager->files.length; index += 1) {
        MarimaySourceFile *const *slot = mmay_vector_at_const(&manager->files, index);
        MarimaySourceFile *file = *slot;
        if (file->path.length == path_length
            && (path_length == 0 || memcmp(file->path.data, path, path_length) == 0)) {
            return true;
        }
    }
    return false;
}

static void mmay_source_snapshot_destroy(MarimaySourceSnapshot *snapshot) {
    MarimayAllocator allocator;
    if (snapshot == NULL) {
        return;
    }
    allocator = snapshot->allocator;
    mmay_vector_dispose(&snapshot->line_starts);
    mmay_owned_bytes_dispose(&snapshot->source);
    mmay_deallocate(&allocator, snapshot, sizeof(*snapshot));
}

static void mmay_source_file_destroy(MarimaySourceFile *file) {
    size_t index = 0;
    MarimayAllocator allocator;
    if (file == NULL) {
        return;
    }
    allocator = file->allocator;
    for (index = 0; index < file->snapshots.length; index += 1) {
        MarimaySourceSnapshot *const *slot = mmay_vector_at_const(&file->snapshots, index);
        mmay_source_snapshot_destroy(*slot);
    }
    mmay_vector_dispose(&file->snapshots);
    mmay_owned_bytes_dispose(&file->path);
    mmay_deallocate(&allocator, file, sizeof(*file));
}

static bool mmay_source_build_lines(MarimaySourceSnapshot *snapshot) {
    size_t offset = snapshot->content_start;
    size_t start = snapshot->content_start;
    if (!mmay_vector_push(&snapshot->line_starts, &start)) {
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
            if (!mmay_vector_push(&snapshot->line_starts, &start)) {
                return false;
            }
        } else if (byte == '\n') {
            offset += 1;
            start = offset;
            if (!mmay_vector_push(&snapshot->line_starts, &start)) {
                return false;
            }
        } else {
            offset += 1;
        }
    }
    return true;
}

static MarimaySourceSnapshot *mmay_source_snapshot_create(
    MarimaySourceFile *file,
    MarimaySnapshotId id,
    const void *source_bytes,
    size_t source_length
) {
    MarimaySourceSnapshot *snapshot = NULL;
    snapshot = mmay_allocate(&file->allocator, sizeof(*snapshot));
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
    if (!mmay_vector_init(&snapshot->line_starts, file->allocator, sizeof(size_t))) {
        mmay_deallocate(&file->allocator, snapshot, sizeof(*snapshot));
        return NULL;
    }
    if (!mmay_owned_bytes_copy(
            &snapshot->source,
            file->allocator,
            source_bytes,
            source_length
        )) {
        mmay_vector_dispose(&snapshot->line_starts);
        mmay_deallocate(&file->allocator, snapshot, sizeof(*snapshot));
        return NULL;
    }
    if (!mmay_sha256(snapshot->source.data, snapshot->source.length, snapshot->sha256)) {
        mmay_source_snapshot_destroy(snapshot);
        return NULL;
    }
    memcpy(snapshot->identity, "sha256:", 7);
    mmay_sha256_hex(snapshot->sha256, snapshot->identity + 7);
    if (source_length >= 3 && snapshot->source.data[0] == 0xefU
        && snapshot->source.data[1] == 0xbbU && snapshot->source.data[2] == 0xbfU) {
        snapshot->content_start = 3;
    }
    snapshot->valid_utf8 = mmay_utf8_validate(
        (const char *)snapshot->source.data,
        snapshot->source.length,
        &snapshot->invalid_utf8_offset
    );
    if (!mmay_source_build_lines(snapshot)) {
        mmay_source_snapshot_destroy(snapshot);
        return NULL;
    }
    return snapshot;
}

bool mmay_source_manager_init(MarimaySourceManager *manager, MarimayAllocator allocator) {
    if (manager == NULL || !mmay_vector_init(&manager->files, allocator, sizeof(MarimaySourceFile *))) {
        return false;
    }
    manager->allocator = allocator;
    manager->next_file_id = 1;
    manager->next_snapshot_id = 1;
    return true;
}

void mmay_source_manager_dispose(MarimaySourceManager *manager) {
    size_t index = 0;
    if (manager == NULL) {
        return;
    }
    for (index = 0; index < manager->files.length; index += 1) {
        MarimaySourceFile *const *slot = mmay_vector_at_const(&manager->files, index);
        mmay_source_file_destroy(*slot);
    }
    mmay_vector_dispose(&manager->files);
    manager->next_file_id = 0;
    manager->next_snapshot_id = 0;
}

MarimaySourceStatus mmay_source_manager_add(
    MarimaySourceManager *manager,
    const char *path,
    size_t path_length,
    const void *source_bytes,
    size_t source_length,
    const MarimaySourceSnapshot **snapshot
) {
    MarimaySourceFile *file = NULL;
    MarimaySourceSnapshot *created = NULL;
    if (snapshot != NULL) {
        *snapshot = NULL;
    }
    if (manager == NULL || (path == NULL && path_length != 0)
        || (source_bytes == NULL && source_length != 0)) {
        return MMAY_SOURCE_INVALID_ARGUMENT;
    }
    if (manager->next_file_id == 0 || manager->next_snapshot_id == 0) {
        return MMAY_SOURCE_ID_EXHAUSTED;
    }
    if (mmay_source_path_exists(manager, path, path_length)) {
        return MMAY_SOURCE_DUPLICATE_PATH;
    }
    file = mmay_allocate(&manager->allocator, sizeof(*file));
    if (file == NULL) {
        return MMAY_SOURCE_ALLOCATION_FAILED;
    }
    file->allocator = manager->allocator;
    file->id = manager->next_file_id;
    file->path.data = NULL;
    file->path.length = 0;
    file->current = NULL;
    if (!mmay_vector_init(&file->snapshots, manager->allocator, sizeof(MarimaySourceSnapshot *))) {
        mmay_deallocate(&manager->allocator, file, sizeof(*file));
        return MMAY_SOURCE_ALLOCATION_FAILED;
    }
    if (!mmay_owned_bytes_copy(&file->path, manager->allocator, path, path_length)) {
        mmay_source_file_destroy(file);
        return MMAY_SOURCE_ALLOCATION_FAILED;
    }
    created = mmay_source_snapshot_create(
        file,
        manager->next_snapshot_id,
        source_bytes,
        source_length
    );
    if (created == NULL || !mmay_vector_push(&file->snapshots, &created)) {
        mmay_source_snapshot_destroy(created);
        mmay_source_file_destroy(file);
        return MMAY_SOURCE_ALLOCATION_FAILED;
    }
    file->current = created;
    if (!mmay_vector_push(&manager->files, &file)) {
        mmay_source_file_destroy(file);
        return MMAY_SOURCE_ALLOCATION_FAILED;
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
    return MMAY_SOURCE_OK;
}

MarimaySourceStatus mmay_source_manager_overlay(
    MarimaySourceManager *manager,
    MarimayFileId file_id,
    const void *source_bytes,
    size_t source_length,
    const MarimaySourceSnapshot **snapshot
) {
    MarimaySourceFile *file = NULL;
    MarimaySourceSnapshot *created = NULL;
    if (snapshot != NULL) {
        *snapshot = NULL;
    }
    if (manager == NULL || (source_bytes == NULL && source_length != 0)) {
        return MMAY_SOURCE_INVALID_ARGUMENT;
    }
    if (manager->next_snapshot_id == 0) {
        return MMAY_SOURCE_ID_EXHAUSTED;
    }
    file = mmay_source_find_file(manager, file_id);
    if (file == NULL) {
        return MMAY_SOURCE_UNKNOWN_FILE;
    }
    created = mmay_source_snapshot_create(
        file,
        manager->next_snapshot_id,
        source_bytes,
        source_length
    );
    if (created == NULL || !mmay_vector_push(&file->snapshots, &created)) {
        mmay_source_snapshot_destroy(created);
        return MMAY_SOURCE_ALLOCATION_FAILED;
    }
    file->current = created;
    manager->next_snapshot_id = manager->next_snapshot_id == UINT64_MAX
        ? 0
        : manager->next_snapshot_id + 1;
    if (snapshot != NULL) {
        *snapshot = created;
    }
    return MMAY_SOURCE_OK;
}

const MarimaySourceSnapshot *mmay_source_manager_current(
    const MarimaySourceManager *manager,
    MarimayFileId file_id
) {
    MarimaySourceFile *file = NULL;
    if (manager == NULL) {
        return NULL;
    }
    file = mmay_source_find_file(manager, file_id);
    return file == NULL ? NULL : file->current;
}

MarimayFileId mmay_source_file_id(const MarimaySourceSnapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->file->id;
}

MarimaySnapshotId mmay_source_snapshot_id(const MarimaySourceSnapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->id;
}

const char *mmay_source_path(const MarimaySourceSnapshot *snapshot) {
    return snapshot == NULL ? NULL : (const char *)snapshot->file->path.data;
}

size_t mmay_source_path_length(const MarimaySourceSnapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->file->path.length;
}

const unsigned char *mmay_source_bytes(const MarimaySourceSnapshot *snapshot) {
    return snapshot == NULL ? NULL : snapshot->source.data;
}

size_t mmay_source_length(const MarimaySourceSnapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->source.length;
}

const unsigned char *mmay_source_sha256(const MarimaySourceSnapshot *snapshot) {
    return snapshot == NULL ? NULL : snapshot->sha256;
}

const char *mmay_source_snapshot_identity(const MarimaySourceSnapshot *snapshot) {
    return snapshot == NULL ? NULL : snapshot->identity;
}

bool mmay_source_has_bom(const MarimaySourceSnapshot *snapshot) {
    return snapshot != NULL && snapshot->content_start == 3;
}

bool mmay_source_is_valid_utf8(const MarimaySourceSnapshot *snapshot) {
    return snapshot != NULL && snapshot->valid_utf8;
}

size_t mmay_source_invalid_utf8_offset(const MarimaySourceSnapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->invalid_utf8_offset;
}

size_t mmay_source_line_count(const MarimaySourceSnapshot *snapshot) {
    return snapshot == NULL ? 0 : snapshot->line_starts.length;
}

bool mmay_source_position(
    const MarimaySourceSnapshot *snapshot,
    size_t byte_offset,
    MarimaySourcePosition *position
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
        const size_t *start = mmay_vector_at_const(&snapshot->line_starts, middle);
        if (*start <= byte_offset) {
            lower = middle + 1;
        } else {
            upper = middle;
        }
    }
    line_index = lower == 0 ? 0 : lower - 1;
    cursor = *(const size_t *)mmay_vector_at_const(&snapshot->line_starts, line_index);
    while (cursor < byte_offset && cursor < snapshot->source.length) {
        unsigned char byte = snapshot->source.data[cursor];
        MarimayUtf8Scalar scalar;
        if (byte == '\r' || byte == '\n') {
            break;
        }
        if (mmay_utf8_decode(
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

bool mmay_source_line_bounds(
    const MarimaySourceSnapshot *snapshot,
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
    line_start = mmay_vector_at_const(&snapshot->line_starts, line - 1);
    if (line_start == NULL) {
        return false;
    }
    if (line < snapshot->line_starts.length) {
        const size_t *next = mmay_vector_at_const(&snapshot->line_starts, line);
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
