#ifndef MMAY_LSP_OUTPUT_H
#define MMAY_LSP_OUTPUT_H

#include "json.h"
#include "mmay/source/source_manager.h"

/* Invocation-owned JSON construction; never writes a partial frame. */
typedef struct LspOutput { MarimayVector bytes; } LspOutput;
bool lsp_output_init(LspOutput *output, MarimayAllocator allocator);
void lsp_output_dispose(LspOutput *output);
MarimayWriter lsp_output_writer(LspOutput *output);
bool lsp_write_integer(MarimayWriter writer, int64_t value);
bool lsp_write_size(MarimayWriter writer, size_t value);
bool lsp_write_range(MarimayWriter writer,
    const MarimaySourceSnapshot *snapshot, size_t start, size_t end);
bool lsp_write_location(MarimayWriter writer, MarimayAllocator allocator,
    const MarimaySourceSnapshot *snapshot, size_t start, size_t end);

#endif
