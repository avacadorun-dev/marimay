#ifndef MMAY_RUNTIME_LINE_READER_H
#define MMAY_RUNTIME_LINE_READER_H
#include "mmay/runtime/io.h"

/* Shared bounded UTF-8/LF/CRLF parser. Invalid offsets are line-relative.
 * An error publishes no line, but does not rewind the source cursor. */
MarimayIoStatus mmay_read_utf8_line(
    const MarimayIoInput *input, MarimayAllocator allocator, size_t limit,
    MarimayOwnedBytes *line, bool *eof, size_t *invalid_offset
);
#endif
