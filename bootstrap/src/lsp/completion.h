#ifndef MMAY_LSP_COMPLETION_H
#define MMAY_LSP_COMPLETION_H

#include "queries.h"

/* Compiler-backed candidates, including a request-local incomplete-token probe. */
LspQueryStatus lsp_completion(LspWorkspace *workspace,
    const MarimayLspJson *json, size_t params, MarimayWriter writer);

#endif
