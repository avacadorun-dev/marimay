#include "source_locations.h"

#include <string.h>

#include "output.h"
#include "positions.h"
#include "mmay/platform/file.h"

static const char lsp_source_cache[] = "/.mmay/lsp/stdlib/";

static bool lsp_source_absolute(const char *path, size_t length) {
    return path != NULL && length != 0 && (path[0] == '/'
        || (length >= 3 && path[1] == ':' && (path[2] == '/' || path[2] == '\\')));
}

static bool lsp_source_standard(const char *path, size_t length) {
    size_t start = 0, index = 0;
    if (path == NULL || length <= 4 || memcmp(path, "std/", 4) != 0) return false;
    for (index = 0; index <= length; index += 1) {
        if (index != length && path[index] != '/') {
            unsigned char byte = (unsigned char)path[index];
            if (!((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
                    || (byte >= '0' && byte <= '9') || byte == '_' || byte == '.'
                    || byte == '-')) return false;
            continue;
        }
        if (index == start || (index - start == 1 && path[start] == '.')
            || (index - start == 2 && path[start] == '.' && path[start + 1] == '.'))
            return false;
        start = index + 1;
    }
    return length >= 3 && memcmp(path + length - 3, ".my", 3) == 0;
}

bool lsp_source_openable(const MarimaySourceSnapshot *snapshot) {
    return snapshot != NULL && (lsp_source_absolute(mmay_source_path(snapshot),
        mmay_source_path_length(snapshot)) || lsp_source_standard(
        mmay_source_path(snapshot), mmay_source_path_length(snapshot)));
}

bool lsp_source_is_view(const char *path, size_t path_length) {
    size_t index = 0;
    if (!lsp_source_absolute(path, path_length)) return false;
    for (index = 0; index + sizeof(lsp_source_cache) - 1 < path_length; index += 1)
        if (memcmp(path + index, lsp_source_cache, sizeof(lsp_source_cache) - 1) == 0)
            return true;
    return false;
}

static LspStatus lsp_source_status(MarimayFileStatus status) {
    if (status == MMAY_FILE_OK) return LSP_OK;
    if (status == MMAY_FILE_ALLOCATION_FAILED) return LSP_ALLOCATION_FAILED;
    if (status == MMAY_FILE_LIMIT_EXCEEDED) return LSP_LIMIT;
    return LSP_INPUT_ERROR;
}

static LspStatus lsp_source_path(const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, char *path, size_t capacity,
    size_t *path_length, size_t *root_length) {
    static const char hex[] = "0123456789abcdef";
    const MarimayOwnedBytes *root = &analysis->target.root_directory;
    const char *source = mmay_source_path(snapshot);
    const unsigned char *hash = mmay_source_sha256(snapshot);
    size_t source_length = mmay_source_path_length(snapshot);
    size_t length = root->length, index = 0;
    if (!lsp_source_absolute((const char *)root->data, length)
        || !lsp_source_standard(source, source_length) || hash == NULL) return LSP_INVALID;
    while (length != 0 && (root->data[length - 1] == '/' || root->data[length - 1] == '\\'))
        length -= 1;
    if (length >= capacity || sizeof(lsp_source_cache) + 64 + source_length >= capacity - length)
        return LSP_LIMIT;
    memcpy(path, root->data, length);
    *root_length = length;
    memcpy(path + length, lsp_source_cache, sizeof(lsp_source_cache) - 1);
    length += sizeof(lsp_source_cache) - 1;
    for (index = 0; index < 32; index += 1) {
        path[length++] = hex[hash[index] >> 4];
        path[length++] = hex[hash[index] & 15];
    }
    path[length++] = '/';
    memcpy(path + length, source, source_length);
    length += source_length;
    path[length] = 0;
    *path_length = length;
    return LSP_OK;
}

const MarimaySourceSnapshot *lsp_source_snapshot(const LspAnalysis *analysis,
    const char *path, size_t path_length) {
    char expected[MMAY_CLI_CHECK_MAX_PATH_BYTES];
    size_t index = 0, length = 0, root_length = 0;
    const MarimaySourceSnapshot *snapshot = lsp_analysis_snapshot(analysis, path, path_length);
    if (snapshot != NULL || analysis == NULL || path == NULL) return snapshot;
    bool source_view = lsp_source_is_view(path, path_length);
    if (analysis->intrinsic_sources_initialized) {
        /* Source-manager IDs are assigned densely from one and never removed.
         * Intrinsic interface views retain their registry owner's snapshots. */
        for (index = 0; index < analysis->intrinsic_sources.files.length
            && index < UINT32_MAX; index += 1) {
            snapshot = mmay_source_manager_current(&analysis->intrinsic_sources,
                (MarimayFileId)(index + 1));
            if (snapshot == NULL) continue;
            if (mmay_source_path_length(snapshot) == path_length
                && memcmp(mmay_source_path(snapshot), path, path_length) == 0)
                return snapshot;
            if (source_view && lsp_source_path(analysis, snapshot, expected,
                    sizeof(expected), &length, &root_length) == LSP_OK
                && length == path_length && memcmp(expected, path, length) == 0)
                return snapshot;
        }
    }
    if (!source_view) return NULL;
    for (index = 0; index < analysis->result.units.length; index += 1) {
        const MarimayCompilerUnit *unit = mmay_compiler_result_unit(&analysis->result, index);
        if (unit != NULL && lsp_source_path(analysis, unit->snapshot, expected,
                sizeof(expected), &length, &root_length) == LSP_OK
            && length == path_length && memcmp(expected, path, length) == 0)
            return unit->snapshot;
    }
    return NULL;
}

static LspStatus lsp_source_directories(const LspAnalysis *analysis,
    char *path, size_t path_length, size_t root_length) {
    size_t index = 0;
    for (index = root_length + 1; index < path_length; index += 1) {
        MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
        MarimayFileStatus status;
        if (path[index] != '/') continue;
        status = mmay_directory_kind(analysis->target.allocator, path, index, &kind);
        if (status != MMAY_FILE_OK) return lsp_source_status(status);
        if (kind == MMAY_DIRECTORY_ENTRY_MISSING) {
            status = mmay_directory_create(analysis->target.allocator, path, index);
            if (status != MMAY_FILE_OK) return lsp_source_status(status);
        } else if (kind != MMAY_DIRECTORY_ENTRY_DIRECTORY) return LSP_INPUT_ERROR;
    }
    return LSP_OK;
}

static LspStatus lsp_source_materialize(const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, char *path, size_t capacity,
    size_t path_length, size_t root_length) {
    static const char suffix[] = ".tmp-";
    char temporary[MMAY_CLI_CHECK_MAX_PATH_BYTES];
    MarimayOwnedBytes existing = {0};
    MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
    MarimayFileStatus file_status;
    LspStatus status = lsp_source_directories(analysis, path, path_length, root_length);
    size_t attempt = 0;
    if (status != LSP_OK) return status;
    if (mmay_source_length(snapshot) > MMAY_CLI_CHECK_MAX_SOURCE_BYTES) return LSP_LIMIT;
    file_status = mmay_directory_kind(analysis->target.allocator, path, path_length, &kind);
    if (file_status != MMAY_FILE_OK) return lsp_source_status(file_status);
    if (kind == MMAY_DIRECTORY_ENTRY_REGULAR) {
        file_status = mmay_file_read_all(analysis->target.allocator, path, path_length,
            MMAY_CLI_CHECK_MAX_SOURCE_BYTES, &existing);
        if (file_status != MMAY_FILE_OK) {
            mmay_owned_bytes_dispose(&existing);
            return lsp_source_status(file_status);
        }
        bool matches = existing.length == mmay_source_length(snapshot)
            && (existing.length == 0
                || memcmp(existing.data, mmay_source_bytes(snapshot), existing.length) == 0);
        mmay_owned_bytes_dispose(&existing);
        if (matches) return LSP_OK;
    } else if (kind != MMAY_DIRECTORY_ENTRY_MISSING) return LSP_INPUT_ERROR;
    if (path_length + sizeof(suffix) + 1 >= capacity) return LSP_LIMIT;
    memcpy(temporary, path, path_length);
    memcpy(temporary + path_length, suffix, sizeof(suffix) - 1);
    for (attempt = 0; attempt < 16; attempt += 1) {
        temporary[path_length + sizeof(suffix) - 1] = "0123456789abcdef"[attempt];
        temporary[path_length + sizeof(suffix)] = 0;
        file_status = mmay_file_replace_atomic(analysis->target.allocator,
            temporary, path_length + sizeof(suffix), path, path_length,
            mmay_source_bytes(snapshot), mmay_source_length(snapshot));
        if (file_status != MMAY_FILE_ALREADY_EXISTS) return lsp_source_status(file_status);
    }
    return LSP_INPUT_ERROR;
}

LspStatus lsp_source_uri(const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, MarimayOwnedBytes *uri) {
    char path[MMAY_CLI_CHECK_MAX_PATH_BYTES];
    size_t length = 0, root_length = 0;
    LspStatus status;
    if (analysis == NULL || snapshot == NULL || uri == NULL) return LSP_INVALID;
    if (lsp_source_absolute(mmay_source_path(snapshot), mmay_source_path_length(snapshot)))
        return lsp_path_to_uri(analysis->target.allocator, mmay_source_path(snapshot),
            mmay_source_path_length(snapshot), uri);
    status = lsp_source_path(analysis, snapshot, path, sizeof(path), &length, &root_length);
    if (status != LSP_OK) return status;
    status = lsp_source_materialize(analysis, snapshot, path, sizeof(path), length, root_length);
    return status == LSP_OK
        ? lsp_path_to_uri(analysis->target.allocator, path, length, uri) : status;
}

bool lsp_source_write_location(MarimayWriter writer, const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, size_t start, size_t end) {
    MarimayOwnedBytes uri = {0};
    bool ok = lsp_source_uri(analysis, snapshot, &uri) == LSP_OK;
    if (ok) ok = mmay_writer_write_cstr(writer, "{\"uri\":")
        && mmay_lsp_json_write_string(writer, uri.data, uri.length)
        && mmay_writer_write_cstr(writer, ",\"range\":")
        && lsp_write_range(writer, snapshot, start, end)
        && mmay_writer_write_cstr(writer, "}");
    mmay_owned_bytes_dispose(&uri);
    return ok;
}
