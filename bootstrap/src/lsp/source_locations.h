#ifndef MMAY_LSP_SOURCE_LOCATIONS_H
#define MMAY_LSP_SOURCE_LOCATIONS_H

#include "workspace.h"

/* Exact compiler snapshots become editor-openable, content-addressed source
 * views under the target's tool-owned .mmay directory. Views are never inputs. */
bool lsp_source_openable(const MarimaySourceSnapshot *snapshot);
bool lsp_source_is_view(const char *path, size_t path_length);
const MarimaySourceSnapshot *lsp_source_snapshot(const LspAnalysis *analysis,
    const char *path, size_t path_length);
/* Output owns its URI on success; dispose it even after failure. */
LspStatus lsp_source_uri(const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, MarimayOwnedBytes *uri);
bool lsp_source_write_location(MarimayWriter writer, const LspAnalysis *analysis,
    const MarimaySourceSnapshot *snapshot, size_t start, size_t end);

#endif
