#include "mmay/base/stream.h"

#include <stdbool.h>

#include "mmay/base/vector.h"

static MarimayReadStatus mmay_file_read(
    void *context,
    unsigned char *destination,
    size_t capacity,
    size_t *read_count
) {
    FILE *file = context;
    size_t count = 0;
    if (file == NULL || destination == NULL || capacity == 0 || read_count == NULL) {
        return MMAY_READ_ERROR;
    }
    count = fread(destination, 1, capacity, file);
    *read_count = count;
    if (count != 0) {
        return MMAY_READ_OK;
    }
    if (feof(file) != 0) {
        return MMAY_READ_END;
    }
    return MMAY_READ_ERROR;
}

MarimayReadStream mmay_file_read_stream(FILE *file) {
    MarimayReadStream stream;
    stream.context = file;
    stream.read = mmay_file_read;
    return stream;
}

FILE *mmay_read_stream_file(MarimayReadStream stream) {
    return stream.read == mmay_file_read ? stream.context : NULL;
}

MarimayReadAllStatus mmay_read_all(
    MarimayReadStream stream,
    MarimayAllocator allocator,
    size_t max_bytes,
    MarimayOwnedBytes *output
) {
    MarimayVector bytes;
    unsigned char chunk[4096];
    MarimayReadAllStatus result = MMAY_READ_ALL_OK;
    bool initialized = false;
    if (output == NULL || stream.read == NULL) {
        return MMAY_READ_ALL_INVALID_ARGUMENT;
    }
    output->allocator = allocator;
    output->data = NULL;
    output->length = 0;
    initialized = mmay_vector_init(&bytes, allocator, sizeof(unsigned char));
    if (!initialized) {
        return MMAY_READ_ALL_INVALID_ARGUMENT;
    }
    for (;;) {
        size_t count = 0;
        MarimayReadStatus status = stream.read(
            stream.context,
            chunk,
            sizeof(chunk),
            &count
        );
        if (status == MMAY_READ_END) {
            if (count != 0) {
                result = MMAY_READ_ALL_IO_ERROR;
            }
            break;
        }
        if (status != MMAY_READ_OK || count == 0 || count > sizeof(chunk)) {
            result = MMAY_READ_ALL_IO_ERROR;
            break;
        }
        if (bytes.length > max_bytes || count > max_bytes - bytes.length) {
            result = MMAY_READ_ALL_LIMIT_EXCEEDED;
            break;
        }
        if (!mmay_vector_append(&bytes, chunk, count)) {
            result = MMAY_READ_ALL_ALLOCATION_FAILED;
            break;
        }
    }
    if (result == MMAY_READ_ALL_OK
        && !mmay_owned_bytes_copy(output, allocator, bytes.data, bytes.length)) {
        result = MMAY_READ_ALL_ALLOCATION_FAILED;
    }
    mmay_vector_dispose(&bytes);
    if (result != MMAY_READ_ALL_OK) {
        mmay_owned_bytes_dispose(output);
    }
    return result;
}
