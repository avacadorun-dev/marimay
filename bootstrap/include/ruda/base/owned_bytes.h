#ifndef RUDA_BASE_OWNED_BYTES_H
#define RUDA_BASE_OWNED_BYTES_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"

typedef struct RudaOwnedBytes {
    RudaAllocator allocator;
    unsigned char *data;
    size_t length;
} RudaOwnedBytes;

/*
 * Copies exactly length bytes and appends an uncounted zero sentinel. The
 * destination owns the result and must be disposed. Empty input still has a
 * non-NULL sentinel allocation on success.
 */
bool ruda_owned_bytes_copy(
    RudaOwnedBytes *destination,
    RudaAllocator allocator,
    const void *bytes,
    size_t length
);

void ruda_owned_bytes_dispose(RudaOwnedBytes *bytes);

#endif
