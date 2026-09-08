#include "mmay/runtime/line_reader.h"
#include "mmay/base/utf8.h"

MarimayIoStatus mmay_read_utf8_line(
    const MarimayIoInput *input, MarimayAllocator allocator, size_t limit,
    MarimayOwnedBytes *line, bool *eof, size_t *invalid_offset
) {
    unsigned char *buffer = NULL;
    size_t length = 0, capacity = 0, retries = 0;
    bool terminated = false;
    MarimayIoStatus status = MMAY_IO_OK;
    if (input == NULL || line == NULL || eof == NULL || input->read == NULL
        || limit > MMAY_IO_MAX_LINE_BYTES) return MMAY_IO_INVALID_ARGUMENT;
    *eof = false;
    if (invalid_offset != NULL) *invalid_offset = 0;
    mmay_owned_bytes_dispose(line);
    if (!input->capability_granted) return MMAY_IO_CAPABILITY_DENIED;
    for (;;) {
        unsigned char byte = 0;
        size_t count = 0;
        MarimayIoTransferStatus transfer = input->read(input->context, &byte, 1, &count);
        if (transfer == MMAY_IO_TRANSFER_RETRY && count == 0
            && retries < MMAY_IO_MAX_CONSECUTIVE_RETRIES) {
            retries += 1;
            continue;
        }
        if (transfer == MMAY_IO_TRANSFER_END && count == 0) {
            if (length > limit) { status = MMAY_IO_LINE_TOO_LONG; goto cleanup; }
            *eof = length == 0;
            break;
        }
        if (transfer != MMAY_IO_TRANSFER_OK || count != 1) {
            status = MMAY_IO_READ_FAILED;
            goto cleanup;
        }
        retries = 0;
        if (byte == '\n') { terminated = true; break; }
        if (length > limit || (length == limit && byte != '\r')) {
            status = MMAY_IO_LINE_TOO_LONG;
            goto cleanup;
        }
        if (length == capacity) {
            size_t next = capacity == 0 ? 128 : capacity * 2;
            if (next > limit + 1) next = limit + 1;
            void *grown = mmay_resize(&allocator, buffer, capacity, next);
            if (grown == NULL) { status = MMAY_IO_ALLOCATION_FAILED; goto cleanup; }
            buffer = grown;
            capacity = next;
        }
        buffer[length++] = byte;
    }
    if (length != 0 && buffer[length - 1] == '\r' && terminated) length -= 1;
    if (!mmay_utf8_validate((const char *)buffer, length, invalid_offset)) {
        status = MMAY_IO_INVALID_UTF8;
        goto cleanup;
    }
    if (!mmay_owned_bytes_copy(line, allocator, buffer, length)) status = MMAY_IO_ALLOCATION_FAILED;
cleanup:
    mmay_deallocate(&allocator, buffer, capacity);
    return status;
}
