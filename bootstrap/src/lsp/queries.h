#ifndef MMAY_LSP_QUERIES_H
#define MMAY_LSP_QUERIES_H

#include "workspace.h"
#include "json.h"

typedef enum LspQueryStatus {
    LSP_QUERY_OK, LSP_QUERY_INVALID, LSP_QUERY_FAILED, LSP_QUERY_UNKNOWN,
    LSP_QUERY_REJECTED
} LspQueryStatus;
/* Writes only a JSON result. Caller stages it before publishing the response. */
LspQueryStatus lsp_query(LspWorkspace *workspace, const MarimayLspJson *json,
    size_t method, size_t params, MarimayWriter writer);

#endif
