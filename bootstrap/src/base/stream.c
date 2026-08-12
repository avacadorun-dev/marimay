#include "ruda/base/stream.h"

#include <stdbool.h>

#include "ruda/base/vector.h"

static RudaReadStatus ruda_file_read(
    void *context,
    unsigned char *destination,
    size_t capacity,
    size_t *read_count
) {
    FILE *file = context;
    size_t count = 0;
    if (file == NULL || destination == NULL || capacity == 0 || read_count == NULL) {
        return RUDA_READ_ERROR;
    }
    count = fread(destination, 1, capacity, file);
    *read_count = count;
    if (count != 0) {
        return RUDA_READ_OK;
    }
    if (feof(file) != 0) {
        return RUDA_READ_END;
    }
    return RUDA_READ_ERROR;
}

RudaReadStream ruda_file_read_stream(FILE *file) {
    RudaReadStream stream;
    stream.context = file;
    stream.read = ruda_file_read;
    return stream;
}

RudaReadAllStatus ruda_read_all(
    RudaReadStream stream,
    RudaAllocator allocator,
    size_t max_bytes,
    RudaOwnedBytes *output
) {
    RudaVector bytes;
    unsigned char chunk[4096];
    RudaReadAllStatus result = RUDA_READ_ALL_OK;
    bool initialized = false;
    if (output == NULL || stream.read == NULL) {
        return RUDA_READ_ALL_INVALID_ARGUMENT;
    }
    output->allocator = allocator;
    output->data = NULL;
    output->length = 0;
    initialized = ruda_vector_init(&bytes, allocator, sizeof(unsigned char));
    if (!initialized) {
        return RUDA_READ_ALL_INVALID_ARGUMENT;
    }
    for (;;) {
        size_t count = 0;
        RudaReadStatus status = stream.read(
            stream.context,
            chunk,
            sizeof(chunk),
            &count
        );
        if (status == RUDA_READ_END) {
            if (count != 0) {
                result = RUDA_READ_ALL_IO_ERROR;
            }
            break;
        }
        if (status != RUDA_READ_OK || count == 0 || count > sizeof(chunk)) {
            result = RUDA_READ_ALL_IO_ERROR;
            break;
        }
        if (bytes.length > max_bytes || count > max_bytes - bytes.length) {
            result = RUDA_READ_ALL_LIMIT_EXCEEDED;
            break;
        }
        if (!ruda_vector_append(&bytes, chunk, count)) {
            result = RUDA_READ_ALL_ALLOCATION_FAILED;
            break;
        }
    }
    if (result == RUDA_READ_ALL_OK
        && !ruda_owned_bytes_copy(output, allocator, bytes.data, bytes.length)) {
        result = RUDA_READ_ALL_ALLOCATION_FAILED;
    }
    ruda_vector_dispose(&bytes);
    if (result != RUDA_READ_ALL_OK) {
        ruda_owned_bytes_dispose(output);
    }
    return result;
}
