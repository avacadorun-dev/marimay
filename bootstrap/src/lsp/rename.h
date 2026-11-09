#ifndef MMAY_LSP_RENAME_H
#define MMAY_LSP_RENAME_H

#include "queries.h"

/* Stages edits only after a fresh whole-module check preserves every binding. */
LspQueryStatus lsp_rename(LspWorkspace *workspace, const MarimayLspJson *json,
    size_t params, bool prepare, MarimayWriter writer);

#endif
