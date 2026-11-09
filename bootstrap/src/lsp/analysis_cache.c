#include "analysis_cache.h"
#include "source_locations.h"

#include <string.h>
#include "mmay/platform/file.h"

void lsp_workspace_invalidate(LspWorkspace *workspace) {
    size_t index;
    if (workspace == NULL) return;
    for (index = 0; index < LSP_ANALYSIS_CACHE_CAPACITY; index += 1) {
        lsp_analysis_dispose(&workspace->analyses[index].analysis);
        workspace->analyses[index].occupied = false;
    }
    workspace->next_analysis = 0;
}

static bool lsp_cache_overlaid(const LspWorkspace *workspace,
    const MarimaySourceSnapshot *snapshot) {
    size_t index;
    for (index = 0; index < workspace->documents.length; index += 1) {
        const LspDocument *document = mmay_vector_at_const(&workspace->documents, index);
        if (document->path.length == mmay_source_path_length(snapshot)
            && memcmp(document->path.data, mmay_source_path(snapshot), document->path.length) == 0)
            return true;
    }
    return false;
}

static bool lsp_cache_file_current(MarimayAllocator allocator,
    const MarimaySourceSnapshot *snapshot) {
    MarimayOwnedBytes bytes = {0};
    const char *path = mmay_source_path(snapshot);
    size_t length = mmay_source_path_length(snapshot);
    bool ok;
    /* Embedded standard packages are immutable within this server process. */
    if (length == 0 || (path[0] != '/' && !(length >= 3 && path[1] == ':')))
        return true;
    ok = mmay_file_read_all(allocator, path, length,
            MMAY_CLI_CHECK_MAX_SOURCE_BYTES, &bytes) == MMAY_FILE_OK
        && bytes.length == mmay_source_length(snapshot)
        && (bytes.length == 0 || memcmp(bytes.data, mmay_source_bytes(snapshot), bytes.length) == 0);
    mmay_owned_bytes_dispose(&bytes);
    return ok;
}

static bool lsp_cache_current(const LspWorkspace *workspace, const LspAnalysis *analysis) {
    size_t index;
    for (index = 0; index < analysis->target.service.documents.length; index += 1) {
        const MarimayCompilerDocument *document = mmay_vector_at_const(
            &analysis->target.service.documents, index);
        if (!lsp_cache_overlaid(workspace, document->snapshot)
            && !lsp_cache_file_current(workspace->allocator, document->snapshot)) return false;
    }
    if (analysis->target.problem_source != NULL
        && !lsp_cache_file_current(workspace->allocator, analysis->target.problem_source)) return false;
    if (analysis->target.manifest.snapshot != NULL
        && analysis->target.manifest.snapshot != analysis->target.problem_source
        && !lsp_cache_file_current(workspace->allocator, analysis->target.manifest.snapshot)) return false;
    return true;
}

static LspStatus lsp_cache_view_status(const LspAnalysis *analysis,
    const LspDocument *document) {
    const MarimaySourceSnapshot *snapshot = lsp_source_snapshot(analysis,
        (const char *)document->path.data, document->path.length);
    return snapshot != NULL && document->text.length == mmay_source_length(snapshot)
        && (document->text.length == 0 || memcmp(document->text.data,
            mmay_source_bytes(snapshot), document->text.length) == 0)
        ? LSP_OK : LSP_INVALID;
}

LspStatus lsp_workspace_analysis(LspWorkspace *workspace,
    const char *uri, size_t uri_length, const LspAnalysis **analysis) {
    const LspDocument *selected;
    size_t index, selected_index;
    LspCachedAnalysis *entry;
    bool source_view;
    if (analysis == NULL) return LSP_INVALID;
    *analysis = NULL;
    if (workspace == NULL || !workspace->initialized) return LSP_INVALID;
    selected = lsp_workspace_document(workspace, uri, uri_length);
    if (selected == NULL) return LSP_INVALID;
    source_view = lsp_source_is_view((const char *)selected->path.data, selected->path.length);
    selected_index = (size_t)(selected - (const LspDocument *)workspace->documents.data);
    for (index = 0; index < LSP_ANALYSIS_CACHE_CAPACITY; index += 1) {
        entry = &workspace->analyses[index];
        if (!entry->occupied) continue;
        if (entry->source_document != selected_index
            && !(entry->status == LSP_OK && (entry->analysis.target.module_wide || source_view)
                && lsp_source_snapshot(&entry->analysis, (const char *)selected->path.data,
                    selected->path.length) != NULL)) continue;
        if (!lsp_cache_current(workspace, &entry->analysis)) {
            lsp_workspace_invalidate(workspace);
            break;
        }
        *analysis = &entry->analysis;
        return source_view ? lsp_cache_view_status(*analysis, selected) : entry->status;
    }
    if (source_view) {
        /* Derived standard source views retain their real package identity. */
        for (index = 0; index < workspace->documents.length; index += 1) {
            const LspDocument *document = mmay_vector_at_const(&workspace->documents, index);
            const LspAnalysis *candidate = NULL;
            if (lsp_source_is_view((const char *)document->path.data, document->path.length)) continue;
            if (lsp_workspace_analysis(workspace, (const char *)document->uri.data,
                    document->uri.length, &candidate) == LSP_OK
                && lsp_source_snapshot(candidate, (const char *)selected->path.data,
                    selected->path.length) != NULL) {
                *analysis = candidate;
                return lsp_cache_view_status(candidate, selected);
            }
        }
        return LSP_INPUT_ERROR;
    }
    entry = &workspace->analyses[workspace->next_analysis];
    workspace->next_analysis = (workspace->next_analysis + 1) % LSP_ANALYSIS_CACHE_CAPACITY;
    lsp_analysis_dispose(&entry->analysis);
    entry->occupied = true;
    entry->source_document = selected_index;
    entry->status = lsp_workspace_analyze(workspace, uri, uri_length, &entry->analysis);
    /* Resource failures are retryable; never pin an OOM result in the cache. */
    entry->occupied = entry->status == LSP_OK;
    *analysis = &entry->analysis;
    return entry->status;
}
