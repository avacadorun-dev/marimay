#ifndef RUDA_BASE_VECTOR_H
#define RUDA_BASE_VECTOR_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"

typedef struct RudaVector {
    RudaAllocator allocator;
    void *data;
    size_t length;
    size_t capacity;
    size_t element_size;
} RudaVector;

/* Initializes an empty owner. element_size must be nonzero. */
bool ruda_vector_init(RudaVector *vector, RudaAllocator allocator, size_t element_size);

/* Releases the vector's allocation; elements themselves have no destructor. */
void ruda_vector_dispose(RudaVector *vector);

/* Growth is failure-atomic: existing data and capacity remain on failure. */
bool ruda_vector_reserve(RudaVector *vector, size_t minimum_capacity);

/* Element input must not alias vector storage. Bytes are copied into the owner. */
bool ruda_vector_push(RudaVector *vector, const void *element);
bool ruda_vector_append(RudaVector *vector, const void *elements, size_t count);

/* Extending zero-initializes new elements; shrinking does not release capacity. */
bool ruda_vector_resize_zeroed(RudaVector *vector, size_t new_length);

void *ruda_vector_at(RudaVector *vector, size_t index);
const void *ruda_vector_at_const(const RudaVector *vector, size_t index);

#endif
