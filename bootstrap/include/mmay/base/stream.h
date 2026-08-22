#ifndef MMAY_BASE_STREAM_H
#define MMAY_BASE_STREAM_H

#include <stddef.h>
#include <stdio.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"

typedef enum MarimayReadStatus {
    MMAY_READ_OK,
    MMAY_READ_END,
    MMAY_READ_ERROR
} MarimayReadStatus;

typedef MarimayReadStatus (*MarimayReadFn)(
    void *context,
    unsigned char *destination,
    size_t capacity,
    size_t *read_count
);

typedef struct MarimayReadStream {
    void *context;
    MarimayReadFn read;
} MarimayReadStream;

typedef enum MarimayReadAllStatus {
    MMAY_READ_ALL_OK,
    MMAY_READ_ALL_INVALID_ARGUMENT,
    MMAY_READ_ALL_IO_ERROR,
    MMAY_READ_ALL_LIMIT_EXCEEDED,
    MMAY_READ_ALL_ALLOCATION_FAILED
} MarimayReadAllStatus;

/* The FILE must remain open while the returned stream is used. */
MarimayReadStream mmay_file_read_stream(FILE *file);

/*
 * Reads to an owned exact byte buffer, rejecting data larger than max_bytes.
 * Output is initialized empty on every failure.
 */
MarimayReadAllStatus mmay_read_all(
    MarimayReadStream stream,
    MarimayAllocator allocator,
    size_t max_bytes,
    MarimayOwnedBytes *output
);

#endif
