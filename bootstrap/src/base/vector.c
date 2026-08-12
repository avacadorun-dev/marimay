#include "ruda/base/vector.h"

#include <string.h>

bool ruda_vector_init(RudaVector *vector, RudaAllocator allocator, size_t element_size) {
    if (vector == NULL || element_size == 0 || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL) {
        return false;
    }
    vector->allocator = allocator;
    vector->data = NULL;
    vector->length = 0;
    vector->capacity = 0;
    vector->element_size = element_size;
    return true;
}

void ruda_vector_dispose(RudaVector *vector) {
    size_t allocation_size = 0;
    if (vector == NULL) {
        return;
    }
    if (vector->data != NULL
        && ruda_size_multiply(vector->capacity, vector->element_size, &allocation_size)) {
        ruda_deallocate(&vector->allocator, vector->data, allocation_size);
    }
    vector->data = NULL;
    vector->length = 0;
    vector->capacity = 0;
}

bool ruda_vector_reserve(RudaVector *vector, size_t minimum_capacity) {
    size_t new_capacity = 0;
    size_t old_size = 0;
    size_t new_size = 0;
    void *resized = NULL;
    if (vector == NULL || vector->element_size == 0) {
        return false;
    }
    if (minimum_capacity <= vector->capacity) {
        return true;
    }
    new_capacity = vector->capacity == 0 ? 8 : vector->capacity;
    while (new_capacity < minimum_capacity) {
        size_t growth = new_capacity / 2 + 1;
        if (!ruda_size_add(new_capacity, growth, &new_capacity)) {
            new_capacity = minimum_capacity;
            break;
        }
    }
    if (!ruda_size_multiply(vector->capacity, vector->element_size, &old_size)
        || !ruda_size_multiply(new_capacity, vector->element_size, &new_size)) {
        return false;
    }
    resized = ruda_resize(&vector->allocator, vector->data, old_size, new_size);
    if (resized == NULL) {
        return false;
    }
    vector->data = resized;
    vector->capacity = new_capacity;
    return true;
}

bool ruda_vector_push(RudaVector *vector, const void *element) {
    size_t new_length = 0;
    unsigned char *destination = NULL;
    if (vector == NULL || element == NULL
        || !ruda_size_add(vector->length, 1, &new_length)
        || !ruda_vector_reserve(vector, new_length)) {
        return false;
    }
    destination = (unsigned char *)vector->data + vector->length * vector->element_size;
    memcpy(destination, element, vector->element_size);
    vector->length = new_length;
    return true;
}

bool ruda_vector_append(RudaVector *vector, const void *elements, size_t count) {
    size_t new_length = 0;
    size_t copy_size = 0;
    unsigned char *destination = NULL;
    if (vector == NULL || (elements == NULL && count != 0)) {
        return false;
    }
    if (!ruda_size_add(vector->length, count, &new_length)
        || !ruda_size_multiply(count, vector->element_size, &copy_size)
        || !ruda_vector_reserve(vector, new_length)) {
        return false;
    }
    if (copy_size != 0) {
        if (elements == NULL) {
            return false;
        }
        destination = (unsigned char *)vector->data
            + vector->length * vector->element_size;
        memcpy(destination, elements, copy_size);
    }
    vector->length = new_length;
    return true;
}

bool ruda_vector_resize_zeroed(RudaVector *vector, size_t new_length) {
    size_t added = 0;
    size_t clear_size = 0;
    unsigned char *start = NULL;
    if (vector == NULL || !ruda_vector_reserve(vector, new_length)) {
        return false;
    }
    if (new_length > vector->length) {
        added = new_length - vector->length;
        if (!ruda_size_multiply(added, vector->element_size, &clear_size)) {
            return false;
        }
        start = (unsigned char *)vector->data + vector->length * vector->element_size;
        memset(start, 0, clear_size);
    }
    vector->length = new_length;
    return true;
}

void *ruda_vector_at(RudaVector *vector, size_t index) {
    if (vector == NULL || index >= vector->length) {
        return NULL;
    }
    return (unsigned char *)vector->data + index * vector->element_size;
}

const void *ruda_vector_at_const(const RudaVector *vector, size_t index) {
    if (vector == NULL || index >= vector->length) {
        return NULL;
    }
    return (const unsigned char *)vector->data + index * vector->element_size;
}
