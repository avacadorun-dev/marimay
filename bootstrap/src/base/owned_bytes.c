#include "mmay/base/owned_bytes.h"

#include <string.h>

bool mmay_owned_bytes_copy(
    MarimayOwnedBytes *destination,
    MarimayAllocator allocator,
    const void *bytes,
    size_t length
) {
    size_t allocation_size = 0;
    unsigned char *copy = NULL;
    if (destination == NULL || (bytes == NULL && length != 0)
        || !mmay_size_add(length, 1, &allocation_size)) {
        return false;
    }
    destination->allocator = allocator;
    destination->data = NULL;
    destination->length = 0;
    copy = mmay_allocate(&allocator, allocation_size);
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

void mmay_owned_bytes_dispose(MarimayOwnedBytes *bytes) {
    size_t allocation_size = 0;
    if (bytes == NULL) {
        return;
    }
    if (bytes->data != NULL && mmay_size_add(bytes->length, 1, &allocation_size)) {
        mmay_deallocate(&bytes->allocator, bytes->data, allocation_size);
    }
    bytes->data = NULL;
    bytes->length = 0;
}
