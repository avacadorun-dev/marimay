#ifndef MMAY_BASE_VECTOR_H
#define MMAY_BASE_VECTOR_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"

typedef struct MarimayVector {
    MarimayAllocator allocator;
    void *data;
    size_t length;
    size_t capacity;
    size_t element_size;
} MarimayVector;

/* Initializes an empty owner. element_size must be nonzero. */
bool mmay_vector_init(MarimayVector *vector, MarimayAllocator allocator, size_t element_size);

/* Releases the vector's allocation; elements themselves have no destructor. */
void mmay_vector_dispose(MarimayVector *vector);

/* Growth is failure-atomic: existing data and capacity remain on failure. */
bool mmay_vector_reserve(MarimayVector *vector, size_t minimum_capacity);

/* Element input must not alias vector storage. Bytes are copied into the owner. */
bool mmay_vector_push(MarimayVector *vector, const void *element);
bool mmay_vector_append(MarimayVector *vector, const void *elements, size_t count);

/* Extending zero-initializes new elements; shrinking does not release capacity. */
bool mmay_vector_resize_zeroed(MarimayVector *vector, size_t new_length);

void *mmay_vector_at(MarimayVector *vector, size_t index);
const void *mmay_vector_at_const(const MarimayVector *vector, size_t index);

#endif
