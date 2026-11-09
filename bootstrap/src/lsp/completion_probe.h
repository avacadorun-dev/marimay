#ifndef MMAY_LSP_COMPLETION_PROBE_H
#define MMAY_LSP_COMPLETION_PROBE_H

#include "workspace.h"

/* Syntax-only request repair. Names remain borrowed from the original unit. */
typedef struct LspCompletionProbe {
    MarimayOwnedBytes text;
    MarimayToken receiver[64];
    size_t receiver_count;
    size_t prefix_start;
    size_t prefix_end;
    size_t prefix_length;
    size_t expression_start;
    size_t query_offset;
    bool member;
} LspCompletionProbe;

LspStatus lsp_completion_probe_build(const MarimayCompilerUnit *unit,
    size_t offset, MarimayAllocator allocator, LspCompletionProbe *probe);
/* Bounded fallback: trim the containing statement/block tail using the CST. */
LspStatus lsp_completion_probe_trim(const MarimayCompilerUnit *unit,
    size_t block_index, MarimayAllocator allocator, LspCompletionProbe *probe);
void lsp_completion_probe_dispose(LspCompletionProbe *probe);

#endif
