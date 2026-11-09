#include "test_support.h"

#include <stdio.h>
#include <string.h>
#include "../../bootstrap/src/lsp/analysis_cache.h"
#include "../../bootstrap/src/lsp/positions.h"

static const char cache_source[] = "fn answer(): int { return 42; }\n";

static bool cache_uri(MarimayOwnedBytes *uri) {
    MarimayOwnedBytes directory = {0};
    char path[32768];
    int length;
    bool ok = mmay_directory_current(mmay_system_allocator(), sizeof(path),
        &directory) == MMAY_FILE_OK;
    if (ok) {
        length = snprintf(path, sizeof(path), "%s/.mmay/lsp-cache-unit.my", directory.data);
        ok = length > 0 && (size_t)length < sizeof(path)
            && lsp_path_to_uri(mmay_system_allocator(), path, (size_t)length, uri) == LSP_OK;
    }
    mmay_owned_bytes_dispose(&directory);
    return ok;
}

static LspStatus cache_flow(MarimayAllocator allocator,
    const MarimayOwnedBytes *uri, bool assertions) {
    LspWorkspace workspace = {0};
    const LspAnalysis *analysis = NULL, *repeated = NULL;
    LspStatus status = LSP_ALLOCATION_FAILED;
    const char *uri_text = (const char *)uri->data;
    if (!lsp_workspace_init(&workspace, allocator)) goto cleanup;
    status = lsp_workspace_open(&workspace, uri_text, uri->length, 1,
        cache_source, sizeof(cache_source) - 1);
    if (status != LSP_OK) goto cleanup;
    status = lsp_workspace_analysis(&workspace, uri_text, uri->length, &analysis);
    if (status != LSP_OK) goto cleanup;
    if (assertions) MMAY_EXPECT(analysis != NULL && analysis->result.checked.error_count == 0);
    status = lsp_workspace_analysis(&workspace, uri_text, uri->length, &repeated);
    if (status != LSP_OK) goto cleanup;
    if (assertions) MMAY_EXPECT(analysis == repeated);
    if (assertions) {
        static const char view_uri[] = "file:///tmp/.mmay/lsp/stdlib/test/std/cli/cli.my";
        size_t next_analysis = workspace.next_analysis;
        MMAY_EXPECT(lsp_workspace_open(&workspace, view_uri, sizeof(view_uri) - 1,
            1, "// source view\n", sizeof("// source view\n") - 1) == LSP_OK);
        MMAY_EXPECT(workspace.analyses[0].occupied && workspace.next_analysis == next_analysis);
        MMAY_EXPECT(lsp_workspace_change(&workspace, view_uri, sizeof(view_uri) - 1,
            2, "// changed\n", sizeof("// changed\n") - 1) == LSP_OK);
        MMAY_EXPECT(workspace.analyses[0].occupied && workspace.next_analysis == next_analysis);
    }
    if (assertions) MMAY_EXPECT(lsp_workspace_change(&workspace, uri_text,
        uri->length, 1, "", 0) == LSP_STALE);
    status = lsp_workspace_change(&workspace, uri_text, uri->length, 2,
        "fn answer(): int { return false; }\n", sizeof("fn answer(): int { return false; }\n") - 1);
    if (status != LSP_OK) goto cleanup;
    if (assertions) {
        for (size_t index = 0; index < LSP_ANALYSIS_CACHE_CAPACITY; index += 1)
            MMAY_EXPECT(!workspace.analyses[index].occupied);
    }
    status = lsp_workspace_analysis(&workspace, uri_text, uri->length, &analysis);
    if (status != LSP_OK) goto cleanup;
    if (assertions) MMAY_EXPECT(analysis->result.checked.error_count != 0);
    status = lsp_workspace_close(&workspace, uri_text, uri->length);
cleanup:
    lsp_workspace_dispose(&workspace);
    return status;
}

static bool cache_closed_path(MarimayOwnedBytes *path) {
    MarimayOwnedBytes directory = {0};
    char buffer[32768];
    int length;
    bool ready = false;
    if (mmay_directory_current(mmay_system_allocator(), sizeof(buffer),
            &directory) != MMAY_FILE_OK) goto cleanup;
    length = snprintf(buffer, sizeof(buffer), "%s/.mmay", directory.data);
    if (length <= 0 || (size_t)length >= sizeof(buffer)
        || mmay_directory_create(mmay_system_allocator(), buffer, (size_t)length) != MMAY_FILE_OK)
        goto cleanup;
    length = snprintf(buffer, sizeof(buffer), "%s/.mmay/lsp-cache-closed-unit.my", directory.data);
    ready = length > 0 && (size_t)length < sizeof(buffer)
        && mmay_owned_bytes_copy(path, mmay_system_allocator(), buffer, (size_t)length);
cleanup:
    mmay_owned_bytes_dispose(&directory);
    return ready;
}

/* A minimal retained graph isolates the allocation-capable closed-file cache
 * validation path. Successful validation needs no lexer/checker work. Failure
 * invalidates this packet and runs the ordinary analysis and teardown path. */
static LspStatus cache_closed_flow(MarimayFailAllocator *state,
    const MarimayOwnedBytes *uri, const MarimayOwnedBytes *path, bool changed,
    bool assertions) {
    static const char stale[] = "fn answer(): int { return 43; }\n";
    LspWorkspace workspace = {0};
    const LspAnalysis *analysis = NULL;
    MarimayAllocator allocator = mmay_fail_allocator(state);
    MarimayFileId file = 0;
    LspStatus status = LSP_ALLOCATION_FAILED;
    size_t lookup_start = 0;
    if (!lsp_workspace_init(&workspace, allocator)) goto cleanup;
    status = lsp_workspace_open(&workspace, (const char *)uri->data, uri->length,
        1, cache_source, sizeof(cache_source) - 1);
    if (status != LSP_OK) goto cleanup;
    status = LSP_ALLOCATION_FAILED;
    LspCachedAnalysis *entry = &workspace.analyses[0];
    MarimayCliCheckTarget *target = &entry->analysis.target;
    target->allocator = allocator;
    if (mmay_compiler_service_init(&target->service, allocator,
            mmay_compiler_default_options()) != MMAY_COMPILER_OK) goto cleanup;
    target->service_initialized = true;
    if (mmay_compiler_service_add_document(&target->service, (const char *)path->data,
            path->length, 1, changed ? stale : cache_source, sizeof(cache_source) - 1,
            &file) != MMAY_COMPILER_OK) goto cleanup;
    entry->occupied = true;
    entry->status = LSP_OK;
    entry->source_document = 0;
    lookup_start = state->attempts;
    status = lsp_workspace_analysis(&workspace, (const char *)uri->data, uri->length, &analysis);
    if (status == LSP_OK) {
        bool lookup_failed = state->failure_index >= lookup_start
            && state->failure_index < state->attempts;
        MMAY_EXPECT(analysis != NULL);
        if (analysis != NULL && (changed || lookup_failed)) {
            /* A failed file read must not return the retained stale packet. */
            MMAY_EXPECT(analysis->result.initialized);
            MMAY_EXPECT(!mmay_compiler_result_has_errors(&analysis->result));
        }
        if (assertions && !changed) {
            MMAY_EXPECT(analysis == &workspace.analyses[0].analysis);
            MMAY_EXPECT(analysis != NULL && !analysis->result.initialized);
            MMAY_EXPECT(workspace.next_analysis == 0);
        }
    }
cleanup:
    lsp_workspace_dispose(&workspace);
    return status;
}

static void cache_closed_allocation_failures(const MarimayOwnedBytes *uri) {
    MarimayOwnedBytes path = {0};
    MarimayFailAllocator state;
    FILE *file;
    bool ready = cache_closed_path(&path);
    bool created = false;
    MMAY_EXPECT(ready);
    if (!ready) { mmay_owned_bytes_dispose(&path); return; }
    file = fopen((const char *)path.data, "wb");
    MMAY_EXPECT(file != NULL);
    if (file == NULL) goto cleanup;
    created = true;
    ready = fwrite(cache_source, 1, sizeof(cache_source) - 1, file) == sizeof(cache_source) - 1;
    ready = fclose(file) == 0 && ready;
    MMAY_EXPECT(ready);
    if (!ready) goto cleanup;
    for (unsigned mode = 0; mode < 2; mode += 1) {
        size_t attempts;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(cache_closed_flow(&state, uri, &path, mode != 0, true) == LSP_OK);
        attempts = state.attempts;
        MMAY_EXPECT(attempts > 0 && state.live_allocations == 0 && state.live_bytes == 0);
        for (size_t index = 0; index <= attempts; index += 1) {
            LspStatus status;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
            status = cache_closed_flow(&state, uri, &path, mode != 0, false);
            MMAY_EXPECT(status == LSP_OK || status == LSP_ALLOCATION_FAILED);
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        }
    }
cleanup:
    if (created) MMAY_EXPECT(remove((const char *)path.data) == 0);
    mmay_owned_bytes_dispose(&path);
}

void mmay_test_lsp_cache(void) {
    MarimayOwnedBytes uri = {0};
    MarimayFailAllocator state;
    size_t attempts;
    bool ready = cache_uri(&uri);
    MMAY_EXPECT(ready);
    if (!ready) return;
    MMAY_EXPECT(cache_flow(mmay_system_allocator(), &uri, true) == LSP_OK);
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(cache_flow(mmay_fail_allocator(&state), &uri, false) == LSP_OK);
    attempts = state.attempts;
    MMAY_EXPECT(attempts > 0 && state.live_allocations == 0 && state.live_bytes == 0);
    for (size_t index = 0; index <= attempts; index += 1) {
        LspStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        status = cache_flow(mmay_fail_allocator(&state), &uri, false);
        MMAY_EXPECT(status == LSP_OK || status == LSP_ALLOCATION_FAILED);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    cache_closed_allocation_failures(&uri);
    mmay_owned_bytes_dispose(&uri);
}
