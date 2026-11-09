#ifndef MMAY_LSP_WORKSPACE_H
#define MMAY_LSP_WORKSPACE_H

#include <stdint.h>

#include "../cli/cli_check_targets.h"

typedef enum LspStatus {
    LSP_OK,
    LSP_INVALID,
    LSP_LIMIT,
    LSP_ALLOCATION_FAILED,
    LSP_INPUT_ERROR,
    LSP_INTERNAL_ERROR,
    LSP_STALE
} LspStatus;

typedef struct LspDocument {
    MarimayOwnedBytes uri;
    MarimayOwnedBytes path;
    MarimayOwnedBytes text;
    int64_t version;
} LspDocument;

typedef struct LspAnalysis {
    MarimayCliCheckTarget target;
    MarimayCompilerResult result;
    MarimaySourceManager intrinsic_sources;
    MarimayCompilerStatus compiler_status;
    MarimayCliCheckTargetStatus target_status;
    bool intrinsic_sources_initialized;
} LspAnalysis;

enum { LSP_ANALYSIS_CACHE_CAPACITY = 4 };

typedef struct LspCachedAnalysis {
    LspAnalysis analysis;
    size_t source_document;
    LspStatus status;
    bool occupied;
} LspCachedAnalysis;

typedef struct LspWorkspace {
    MarimayAllocator allocator;
    MarimayVector documents;
    size_t total_source_bytes;
    LspCachedAnalysis analyses[LSP_ANALYSIS_CACHE_CAPACITY];
    size_t next_analysis;
    bool initialized;
} LspWorkspace;

/* Zero-initialize owners. Allocator state must outlive workspace and analyses. */
bool lsp_workspace_init(LspWorkspace *workspace, MarimayAllocator allocator);
void lsp_workspace_dispose(LspWorkspace *workspace);
LspStatus lsp_workspace_open(LspWorkspace *workspace,
    const char *uri, size_t uri_length, int64_t version,
    const void *text, size_t text_length);
LspStatus lsp_workspace_change(LspWorkspace *workspace,
    const char *uri, size_t uri_length, int64_t version,
    const void *text, size_t text_length);
LspStatus lsp_workspace_close(LspWorkspace *workspace,
    const char *uri, size_t uri_length);
/* Borrowed until the next workspace mutation. */
const LspDocument *lsp_workspace_document(const LspWorkspace *workspace,
    const char *uri, size_t uri_length);

/* Fresh disk graph plus open overlays. Dispose output even on failure. */
LspStatus lsp_workspace_analyze(const LspWorkspace *workspace,
    const char *uri, size_t uri_length, LspAnalysis *analysis);
void lsp_analysis_dispose(LspAnalysis *analysis);
const MarimaySourceSnapshot *lsp_analysis_snapshot(const LspAnalysis *analysis,
    const char *path, size_t path_length);

#endif
