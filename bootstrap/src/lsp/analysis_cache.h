#ifndef MMAY_LSP_ANALYSIS_CACHE_H
#define MMAY_LSP_ANALYSIS_CACHE_H

#include "workspace.h"

/* Borrowed until a workspace mutation or a subsequent cache miss. On analysis
 * failure the nonnull output still owns the target's diagnostic information. */
LspStatus lsp_workspace_analysis(LspWorkspace *workspace,
    const char *uri, size_t uri_length, const LspAnalysis **analysis);

/* Saved files and file-watcher events also invalidate disk discovery. */
void lsp_workspace_invalidate(LspWorkspace *workspace);

#endif
