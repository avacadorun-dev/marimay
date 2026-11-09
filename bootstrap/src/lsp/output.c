#include "output.h"
#include "positions.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static bool lsp_output_append(void *context, const char *bytes, size_t length) {
    LspOutput *output = context;
    const size_t limit = 16U * 1024U * 1024U;
    return output->bytes.length <= limit && length <= limit - output->bytes.length
        && mmay_vector_append(&output->bytes, bytes, length);
}

bool lsp_output_init(LspOutput *output, MarimayAllocator allocator) {
    memset(output, 0, sizeof(*output));
    return mmay_vector_init(&output->bytes, allocator, 1);
}

void lsp_output_dispose(LspOutput *output) {
    mmay_vector_dispose(&output->bytes);
    memset(output, 0, sizeof(*output));
}

MarimayWriter lsp_output_writer(LspOutput *output) {
    return (MarimayWriter){output, lsp_output_append, NULL};
}

bool lsp_write_integer(MarimayWriter writer, int64_t value) {
    char bytes[32];
    int count = snprintf(bytes, sizeof(bytes), "%" PRId64, value);
    return count > 0 && (size_t)count < sizeof(bytes)
        && mmay_writer_write(writer, bytes, (size_t)count);
}

bool lsp_write_size(MarimayWriter writer, size_t value) {
    char bytes[32];
    int count = snprintf(bytes, sizeof(bytes), "%zu", value);
    return count > 0 && (size_t)count < sizeof(bytes)
        && mmay_writer_write(writer, bytes, (size_t)count);
}

static bool lsp_write_position(MarimayWriter writer,
    const MarimaySourceSnapshot *snapshot, size_t offset) {
    size_t line = 0, character = 0;
    return lsp_offset_to_position(mmay_source_bytes(snapshot),
            mmay_source_length(snapshot), offset, &line, &character)
        && mmay_writer_write_cstr(writer, "{\"line\":")
        && lsp_write_size(writer, line)
        && mmay_writer_write_cstr(writer, ",\"character\":")
        && lsp_write_size(writer, character)
        && mmay_writer_write_cstr(writer, "}");
}

bool lsp_write_range(MarimayWriter writer,
    const MarimaySourceSnapshot *snapshot, size_t start, size_t end) {
    return snapshot != NULL && start <= end
        && mmay_writer_write_cstr(writer, "{\"start\":")
        && lsp_write_position(writer, snapshot, start)
        && mmay_writer_write_cstr(writer, ",\"end\":")
        && lsp_write_position(writer, snapshot, end)
        && mmay_writer_write_cstr(writer, "}");
}

bool lsp_write_location(MarimayWriter writer, MarimayAllocator allocator,
    const MarimaySourceSnapshot *snapshot, size_t start, size_t end) {
    MarimayOwnedBytes uri = {0};
    bool ok = snapshot != NULL && lsp_path_to_uri(allocator,
        mmay_source_path(snapshot), mmay_source_path_length(snapshot), &uri) == LSP_OK;
    if (ok) ok = mmay_writer_write_cstr(writer, "{\"uri\":")
        && mmay_lsp_json_write_string(writer, uri.data, uri.length)
        && mmay_writer_write_cstr(writer, ",\"range\":")
        && lsp_write_range(writer, snapshot, start, end)
        && mmay_writer_write_cstr(writer, "}");
    mmay_owned_bytes_dispose(&uri);
    return ok;
}
