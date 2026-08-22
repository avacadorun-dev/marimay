#ifndef MMAY_BASE_OWNED_BYTES_H
#define MMAY_BASE_OWNED_BYTES_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"

typedef struct MarimayOwnedBytes {
    MarimayAllocator allocator;
    unsigned char *data;
    size_t length;
} MarimayOwnedBytes;

/*
 * Copies exactly length bytes and appends an uncounted zero sentinel. The
 * destination owns the result and must be disposed. Empty input still has a
 * non-NULL sentinel allocation on success.
 */
bool mmay_owned_bytes_copy(
    MarimayOwnedBytes *destination,
    MarimayAllocator allocator,
    const void *bytes,
    size_t length
);

void mmay_owned_bytes_dispose(MarimayOwnedBytes *bytes);

#endif
