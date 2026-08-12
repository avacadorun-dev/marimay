#include "ruda/base/owned_bytes.h"

#include <string.h>

bool ruda_owned_bytes_copy(
    RudaOwnedBytes *destination,
    RudaAllocator allocator,
    const void *bytes,
    size_t length
) {
    size_t allocation_size = 0;
    unsigned char *copy = NULL;
    if (destination == NULL || (bytes == NULL && length != 0)
        || !ruda_size_add(length, 1, &allocation_size)) {
        return false;
    }
    destination->allocator = allocator;
    destination->data = NULL;
    destination->length = 0;
    copy = ruda_allocate(&allocator, allocation_size);
    if (copy == NULL) {
        return false;
    }
    if (length != 0) {
        memcpy(copy, bytes, length);
    }
    copy[length] = 0;
    destination->data = copy;
    destination->length = length;
    return true;
}

void ruda_owned_bytes_dispose(RudaOwnedBytes *bytes) {
    size_t allocation_size = 0;
    if (bytes == NULL) {
        return;
    }
    if (bytes->data != NULL && ruda_size_add(bytes->length, 1, &allocation_size)) {
        ruda_deallocate(&bytes->allocator, bytes->data, allocation_size);
    }
    bytes->data = NULL;
    bytes->length = 0;
}
