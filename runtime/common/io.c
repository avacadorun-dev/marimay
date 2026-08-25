#include "mmay/runtime/io.h"

#include "mmay/base/utf8.h"

MarimayIoStatus mmay_io_write(
    const MarimayIoOutput *output,
    const unsigned char *bytes,
    size_t length
) {
    size_t offset = 0;
    size_t retries = 0;
    if (output == NULL || (bytes == NULL && length != 0) || output->write == NULL) {
        return MMAY_IO_INVALID_ARGUMENT;
    }
    if (!output->capability_granted) return MMAY_IO_CAPABILITY_DENIED;
    while (offset < length) {
        size_t count = 0;
        MarimayIoTransferStatus transfer = output->write(
            output->context, bytes + offset, length - offset, &count
        );
        if (transfer == MMAY_IO_TRANSFER_RETRY && count == 0
            && retries < MMAY_IO_MAX_CONSECUTIVE_RETRIES) {
            retries += 1;
            continue;
        }
        if (transfer != MMAY_IO_TRANSFER_OK || count == 0
            || count > length - offset) {
            return MMAY_IO_WRITE_FAILED;
        }
        offset += count;
        retries = 0;
    }
    return MMAY_IO_OK;
}

MarimayIoStatus mmay_io_flush(const MarimayIoOutput *output) {
    size_t retries = 0;
    MarimayIoTransferStatus transfer;
    if (output == NULL || output->flush == NULL) return MMAY_IO_INVALID_ARGUMENT;
    if (!output->capability_granted) return MMAY_IO_CAPABILITY_DENIED;
    do {
        transfer = output->flush(output->context);
        if (transfer != MMAY_IO_TRANSFER_RETRY) break;
        retries += 1;
    } while (retries <= MMAY_IO_MAX_CONSECUTIVE_RETRIES);
    return transfer == MMAY_IO_TRANSFER_OK
        ? MMAY_IO_OK : MMAY_IO_FLUSH_FAILED;
}

MarimayIoStatus mmay_io_read_line(
    const MarimayIoInput *input,
    MarimayAllocator allocator,
    size_t limit,
    MarimayOwnedBytes *line,
    bool *eof
) {
    unsigned char *buffer = NULL;
    size_t length = 0;
    size_t retries = 0;
    bool ended = false;
    if (input == NULL || line == NULL || eof == NULL || input->read == NULL
        || limit > MMAY_IO_MAX_LINE_BYTES) return MMAY_IO_INVALID_ARGUMENT;
    *eof = false;
    mmay_owned_bytes_dispose(line);
    if (!input->capability_granted) return MMAY_IO_CAPABILITY_DENIED;
    buffer = mmay_allocate(&allocator, limit + 1);
    if (buffer == NULL) return MMAY_IO_ALLOCATION_FAILED;
    while (!ended) {
        unsigned char byte = 0;
        size_t count = 0;
        MarimayIoTransferStatus transfer = input->read(
            input->context, &byte, 1, &count
        );
        if (transfer == MMAY_IO_TRANSFER_RETRY && count == 0
            && retries < MMAY_IO_MAX_CONSECUTIVE_RETRIES) {
            retries += 1;
            continue;
        }
        if (transfer == MMAY_IO_TRANSFER_END && count == 0) {
            if (length > limit) {
                mmay_deallocate(&allocator, buffer, limit + 1);
                return MMAY_IO_LINE_TOO_LONG;
            }
            if (length == 0) *eof = true;
            break;
        }
        if (transfer != MMAY_IO_TRANSFER_OK || count != 1) {
            mmay_deallocate(&allocator, buffer, limit + 1);
            return MMAY_IO_READ_FAILED;
        }
        retries = 0;
        if (byte == '\n') { ended = true; break; }
        if (length > limit || (length == limit && byte != '\r')) {
            mmay_deallocate(&allocator, buffer, limit + 1);
            return MMAY_IO_LINE_TOO_LONG;
        }
        buffer[length++] = byte;
    }
    if (length != 0 && buffer[length - 1] == '\r' && ended) length -= 1;
    if (!mmay_utf8_validate((const char *)buffer, length, NULL)) {
        mmay_deallocate(&allocator, buffer, limit + 1);
        return MMAY_IO_INVALID_UTF8;
    }
    if (!mmay_owned_bytes_copy(line, allocator, buffer, length)) {
        mmay_deallocate(&allocator, buffer, limit + 1);
        return MMAY_IO_ALLOCATION_FAILED;
    }
    mmay_deallocate(&allocator, buffer, limit + 1);
    return MMAY_IO_OK;
}
