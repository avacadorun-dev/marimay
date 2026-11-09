#ifndef MMAY_LSP_ANALYSIS_OVERRIDES_H
#define MMAY_LSP_ANALYSIS_OVERRIDES_H

#include "workspace.h"

/* Borrowed request-local source replacements. No editor or disk mutation. */
typedef struct LspSourceOverride {
    const char *path;
    size_t path_length;
    const void *text;
    size_t text_length;
} LspSourceOverride;

LspStatus lsp_workspace_analyze_overrides(const LspWorkspace *workspace,
    const char *uri, size_t uri_length, const LspSourceOverride *overrides,
    size_t count, bool whole_module, LspAnalysis *analysis);

/* Workspace sibling: applies validated replacements before checking. */
LspStatus lsp_apply_source_overrides(MarimayCliCheckTarget *target,
    const LspSourceOverride *overrides, size_t count);

#endif
