#ifndef RUDA_BASE_STREAM_H
#define RUDA_BASE_STREAM_H

#include <stddef.h>
#include <stdio.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"

typedef enum RudaReadStatus {
    RUDA_READ_OK,
    RUDA_READ_END,
    RUDA_READ_ERROR
} RudaReadStatus;

typedef RudaReadStatus (*RudaReadFn)(
    void *context,
    unsigned char *destination,
    size_t capacity,
    size_t *read_count
);

typedef struct RudaReadStream {
    void *context;
    RudaReadFn read;
} RudaReadStream;

typedef enum RudaReadAllStatus {
    RUDA_READ_ALL_OK,
    RUDA_READ_ALL_INVALID_ARGUMENT,
    RUDA_READ_ALL_IO_ERROR,
    RUDA_READ_ALL_LIMIT_EXCEEDED,
    RUDA_READ_ALL_ALLOCATION_FAILED
} RudaReadAllStatus;

/* The FILE must remain open while the returned stream is used. */
RudaReadStream ruda_file_read_stream(FILE *file);

/*
 * Reads to an owned exact byte buffer, rejecting data larger than max_bytes.
 * Output is initialized empty on every failure.
 */
RudaReadAllStatus ruda_read_all(
    RudaReadStream stream,
    RudaAllocator allocator,
    size_t max_bytes,
    RudaOwnedBytes *output
);

#endif
