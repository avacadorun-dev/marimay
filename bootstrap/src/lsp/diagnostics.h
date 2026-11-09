#ifndef MMAY_LSP_DIAGNOSTICS_H
#define MMAY_LSP_DIAGNOSTICS_H

#include "workspace.h"
#include "mmay/base/writer.h"

/* Publishes authoritative diagnostics for each open document, including clears. */
bool lsp_publish_diagnostics(LspWorkspace *workspace, MarimayWriter transport);
bool lsp_clear_diagnostics(MarimayAllocator allocator, const void *uri,
    size_t length, MarimayWriter transport);
/* Writes array contents only. Related locations use the analysis snapshots. */
bool lsp_write_document_diagnostics(MarimayWriter writer,
    MarimayAllocator allocator, const LspDocument *document,
    const LspAnalysis *analysis, LspStatus status);

#endif
