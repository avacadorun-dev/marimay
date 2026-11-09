#ifndef MMAY_LSP_TRANSPORT_H
#define MMAY_LSP_TRANSPORT_H

#include "mmay/base/stream.h"
#include "mmay/base/writer.h"

typedef enum MarimayLspReadStatus {
    MMAY_LSP_READ_OK,
    MMAY_LSP_READ_END,
    MMAY_LSP_READ_INVALID,
    MMAY_LSP_READ_IO_ERROR,
    MMAY_LSP_READ_ALLOCATION_FAILED
} MarimayLspReadStatus;

/* Header <=8KiB, body <=16MiB. EOF is clean only between complete frames. */
MarimayLspReadStatus mmay_lsp_read_message(MarimayReadStream input,
    MarimayAllocator allocator, MarimayOwnedBytes *message);
bool mmay_lsp_write_message(MarimayWriter output, const void *bytes,
    size_t length);

#endif
