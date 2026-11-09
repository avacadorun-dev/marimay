#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "../../bootstrap/src/lsp/source_locations.h"
#include "../../bootstrap/src/lsp/positions.h"
#include "mmay/base/hash.h"

static const char source_text[] = "public fn answer(): int { return 42; }\n";

typedef struct SourceFixture {
    LspAnalysis analysis;
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *snapshot;
    char root[32768];
} SourceFixture;

static void source_remove_tree(const char *path) {
    MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
    MarimayAllocator allocator = mmay_system_allocator();
    MarimayDirectoryIterator iterator = {0};
    if (mmay_directory_kind(allocator, path, strlen(path), &kind) != MMAY_FILE_OK) return;
    if (kind == MMAY_DIRECTORY_ENTRY_DIRECTORY) {
        if (mmay_directory_open(allocator, path, strlen(path), &iterator) != MMAY_FILE_OK) return;
        for (;;) {
            MarimayOwnedBytes name = {0};
            char child[32768];
            bool end = false;
            MarimayFileStatus status = mmay_directory_next(&iterator, allocator, 4096,
                &name, &kind, &end);
            if (status == MMAY_FILE_OK && !end) {
                int length = snprintf(child, sizeof(child), "%s/%s", path, name.data);
                if (length > 0 && (size_t)length < sizeof(child)) source_remove_tree(child);
            }
            mmay_owned_bytes_dispose(&name);
            if (status != MMAY_FILE_OK || end) break;
        }
        (void)mmay_directory_close(&iterator);
        (void)mmay_directory_remove_empty(allocator, path, strlen(path));
    } else if (kind != MMAY_DIRECTORY_ENTRY_MISSING) {
        mmay_platform_file_ops().remove(NULL, allocator, path, strlen(path));
    }
}

static bool source_fixture_init(SourceFixture *fixture) {
    MarimayAllocator allocator = mmay_system_allocator();
    MarimayOwnedBytes current = {0};
    MarimayCompilerUnit unit = {0};
    bool ok = mmay_directory_current(allocator, sizeof(fixture->root), &current) == MMAY_FILE_OK;
    int length = ok ? snprintf(fixture->root, sizeof(fixture->root),
        "%s/lsp-source-locations-fixture", current.data) : -1;
    mmay_owned_bytes_dispose(&current);
    if (length <= 0 || (size_t)length >= sizeof(fixture->root)) return false;
    source_remove_tree(fixture->root);
    fixture->analysis.target.allocator = allocator;
    if (mmay_directory_create_exclusive(allocator, fixture->root, (size_t)length) != MMAY_FILE_OK
        || !mmay_owned_bytes_copy(&fixture->analysis.target.root_directory,
            allocator, fixture->root, (size_t)length)
        || !mmay_source_manager_init(&fixture->sources, allocator)
        || mmay_source_manager_add(&fixture->sources, "std/demo/api.my", 15,
            source_text, sizeof(source_text) - 1, &fixture->snapshot) != MMAY_SOURCE_OK
        || !mmay_vector_init(&fixture->analysis.result.units, allocator,
            sizeof(MarimayCompilerUnit))) return false;
    unit.snapshot = fixture->snapshot;
    fixture->analysis.result.initialized = true;
    return mmay_vector_push(&fixture->analysis.result.units, &unit);
}

static void source_fixture_dispose(SourceFixture *fixture) {
    source_remove_tree(fixture->root);
    if (fixture->analysis.intrinsic_sources_initialized)
        mmay_source_manager_dispose(&fixture->analysis.intrinsic_sources);
    mmay_vector_dispose(&fixture->analysis.result.units);
    mmay_owned_bytes_dispose(&fixture->analysis.target.root_directory);
    mmay_source_manager_dispose(&fixture->sources);
}

static bool source_write(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    if (file == NULL) return false;
    bool ok = fwrite(text, 1, strlen(text), file) == strlen(text);
    return fclose(file) == 0 && ok;
}

static bool source_read_exact(const char *path) {
    MarimayOwnedBytes bytes = {0};
    bool ok = mmay_file_read_all(mmay_system_allocator(), path, strlen(path),
        4096, &bytes) == MMAY_FILE_OK && bytes.length == sizeof(source_text) - 1
        && memcmp(bytes.data, source_text, sizeof(source_text) - 1) == 0;
    mmay_owned_bytes_dispose(&bytes);
    return ok;
}

static void source_locations_behavior(void) {
    SourceFixture fixture = {0};
    MarimayOwnedBytes uri = {0}, second = {0}, path = {0};
    const MarimaySourceSnapshot *other = NULL;
    MarimayAllocator allocator = mmay_system_allocator();
    MMAY_EXPECT(source_fixture_init(&fixture));
    if (fixture.snapshot == NULL) goto cleanup;
    MMAY_EXPECT(lsp_source_openable(fixture.snapshot));
    MMAY_EXPECT(lsp_source_uri(&fixture.analysis, fixture.snapshot, &uri) == LSP_OK);
    MMAY_EXPECT(lsp_uri_to_path(allocator, (const char *)uri.data, uri.length, &path) == LSP_OK);
    MMAY_EXPECT(path.data != NULL && source_read_exact((const char *)path.data));
    MMAY_EXPECT(lsp_source_is_view((const char *)path.data, path.length));
    MMAY_EXPECT(lsp_source_snapshot(&fixture.analysis,
        (const char *)path.data, path.length) == fixture.snapshot);
    MMAY_EXPECT(lsp_source_uri(&fixture.analysis, fixture.snapshot, &second) == LSP_OK);
    MMAY_EXPECT(second.length == uri.length && memcmp(uri.data, second.data, uri.length) == 0);
    mmay_owned_bytes_dispose(&second);
    MMAY_EXPECT(source_write((const char *)path.data, "changed derived view"));
    MMAY_EXPECT(lsp_source_uri(&fixture.analysis, fixture.snapshot, &second) == LSP_OK);
    MMAY_EXPECT(source_read_exact((const char *)path.data));
    mmay_owned_bytes_dispose(&second);
    MMAY_EXPECT(mmay_source_manager_overlay(&fixture.sources,
        mmay_source_file_id(fixture.snapshot), "// newer compiler source\n", 25, &other) == MMAY_SOURCE_OK);
    MMAY_EXPECT(lsp_source_uri(&fixture.analysis, other, &second) == LSP_OK);
    MMAY_EXPECT(uri.length == second.length && memcmp(uri.data, second.data, uri.length) != 0);
    MMAY_EXPECT(source_read_exact((const char *)path.data));
    mmay_owned_bytes_dispose(&second);
    MMAY_EXPECT(mmay_source_manager_add(&fixture.sources, "std/../outside.my", 17,
        "", 0, &other) == MMAY_SOURCE_OK);
    MMAY_EXPECT(!lsp_source_openable(other));
    MMAY_EXPECT(lsp_source_uri(&fixture.analysis, other, &second) == LSP_INVALID);
    MMAY_EXPECT(!lsp_source_is_view("std/demo/api.my", 15));
    MMAY_EXPECT(!lsp_source_is_view(fixture.root, strlen(fixture.root)));
    MMAY_EXPECT(lsp_source_snapshot(&fixture.analysis, fixture.root, strlen(fixture.root)) == NULL);
    MMAY_EXPECT(lsp_source_uri(NULL, fixture.snapshot, &second) == LSP_INVALID);
cleanup:
    mmay_owned_bytes_dispose(&uri);
    mmay_owned_bytes_dispose(&second);
    mmay_owned_bytes_dispose(&path);
    source_fixture_dispose(&fixture);
}

/* Existing matching and corrupted views have different allocation paths from
 * first publication. Recreate each disk state for every failure index. */
static LspStatus source_allocation_flow(size_t failure_index, unsigned mode,
    size_t *attempts) {
    SourceFixture fixture = {0};
    MarimayOwnedBytes uri = {0}, path = {0};
    MarimayFailAllocator state;
    LspStatus status = LSP_INPUT_ERROR;
    if (!source_fixture_init(&fixture)) goto cleanup;
    if (mode == 3) {
        char absolute[32768];
        int length = snprintf(absolute, sizeof(absolute), "%s/source.my", fixture.root);
        if (length <= 0 || (size_t)length >= sizeof(absolute)
            || mmay_source_manager_add(&fixture.sources, absolute, (size_t)length,
                source_text, sizeof(source_text) - 1, &fixture.snapshot) != MMAY_SOURCE_OK)
            goto cleanup;
    } else if (mode != 0) {
        if (lsp_source_uri(&fixture.analysis, fixture.snapshot, &uri) != LSP_OK
            || lsp_uri_to_path(mmay_system_allocator(), (const char *)uri.data,
                uri.length, &path) != LSP_OK) goto cleanup;
        if (mode == 2 && !source_write((const char *)path.data, "damaged cache")) goto cleanup;
        mmay_owned_bytes_dispose(&uri);
    }
    mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
    fixture.analysis.target.allocator = mmay_fail_allocator(&state);
    status = lsp_source_uri(&fixture.analysis, fixture.snapshot, &uri);
    mmay_owned_bytes_dispose(&uri);
    *attempts = state.attempts;
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
cleanup:
    mmay_owned_bytes_dispose(&uri);
    mmay_owned_bytes_dispose(&path);
    source_fixture_dispose(&fixture);
    return status;
}

static void source_locations_allocations(void) {
    unsigned mode = 0;
    for (mode = 0; mode < 4; mode += 1) {
        size_t attempts = 0, index = 0, ignored = 0;
        MMAY_EXPECT(source_allocation_flow(SIZE_MAX, mode, &attempts) == LSP_OK);
        MMAY_EXPECT(attempts != 0);
        for (index = 0; index <= attempts; index += 1) {
            LspStatus status = source_allocation_flow(index, mode, &ignored);
            MMAY_EXPECT(status == LSP_OK || status == LSP_ALLOCATION_FAILED);
        }
    }
}

static void source_locations_collisions(void) {
    SourceFixture fixture = {0};
    MarimayOwnedBytes uri = {0};
    char path[32768];
    MMAY_EXPECT(source_fixture_init(&fixture));
    int length = snprintf(path, sizeof(path), "%s/.mmay", fixture.root);
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(path));
    MMAY_EXPECT(source_write(path, "regular file blocking tool directory"));
    MMAY_EXPECT(lsp_source_uri(&fixture.analysis, fixture.snapshot, &uri) == LSP_INPUT_ERROR);
    MMAY_EXPECT(uri.data == NULL);
    source_fixture_dispose(&fixture);
    mmay_owned_bytes_dispose(&uri);
}

static void source_locations_intrinsic_view(void) {
    static const char source[] = "// Compiler intrinsic API\npublic fn encode<T>(value: T): bytes.Bytes;\n";
    static const char source_path[] = "std/json/intrinsics.my";
    SourceFixture fixture = {0};
    MarimayOwnedBytes uri = {0}, path = {0}, bytes = {0};
    const MarimaySourceSnapshot *snapshot = NULL;
    char hash[65];
    MMAY_EXPECT(source_fixture_init(&fixture));
    fixture.analysis.intrinsic_sources_initialized = mmay_source_manager_init(
        &fixture.analysis.intrinsic_sources, mmay_system_allocator());
    MMAY_EXPECT(fixture.analysis.intrinsic_sources_initialized);
    MMAY_EXPECT(mmay_source_manager_add(&fixture.analysis.intrinsic_sources,
        source_path, sizeof(source_path) - 1, source, sizeof(source) - 1,
        &snapshot) == MMAY_SOURCE_OK);
    if (snapshot == NULL) goto cleanup;
    MMAY_EXPECT(lsp_source_snapshot(&fixture.analysis,
        source_path, sizeof(source_path) - 1) == snapshot);
    MMAY_EXPECT(lsp_source_uri(&fixture.analysis, snapshot, &uri) == LSP_OK);
    MMAY_EXPECT(lsp_uri_to_path(mmay_system_allocator(), (const char *)uri.data,
        uri.length, &path) == LSP_OK);
    MMAY_EXPECT(path.data != NULL && lsp_source_is_view((const char *)path.data, path.length));
    MMAY_EXPECT(lsp_source_snapshot(&fixture.analysis,
        (const char *)path.data, path.length) == snapshot);
    mmay_sha256_hex(mmay_source_sha256(snapshot), hash);
    MMAY_EXPECT(strstr((const char *)path.data, hash) != NULL);
    MMAY_EXPECT(mmay_file_read_all(mmay_system_allocator(), (const char *)path.data,
        path.length, 4096, &bytes) == MMAY_FILE_OK);
    MMAY_EXPECT(bytes.length == sizeof(source) - 1
        && memcmp(bytes.data, source, sizeof(source) - 1) == 0);
cleanup:
    mmay_owned_bytes_dispose(&uri);
    mmay_owned_bytes_dispose(&path);
    mmay_owned_bytes_dispose(&bytes);
    source_fixture_dispose(&fixture);
}

void mmay_test_lsp_source_locations(void) {
    source_locations_behavior();
    source_locations_allocations();
    source_locations_collisions();
    source_locations_intrinsic_view();
}
