#ifndef MMAY_LSP_POSITIONS_H
#define MMAY_LSP_POSITIONS_H

#include "workspace.h"

/* LSP lines and UTF-16 code units are zero-based. Split scalars are invalid. */
bool lsp_position_to_offset(const unsigned char *bytes, size_t length,
    size_t line, size_t character, size_t *offset);
bool lsp_offset_to_position(const unsigned char *bytes, size_t length,
    size_t offset, size_t *line, size_t *character);

/* Local absolute file URIs only; outputs own a zero-terminated byte copy. */
LspStatus lsp_uri_to_path(MarimayAllocator allocator,
    const char *uri, size_t length, MarimayOwnedBytes *path);
LspStatus lsp_path_to_uri(MarimayAllocator allocator,
    const char *path, size_t length, MarimayOwnedBytes *uri);

#endif
