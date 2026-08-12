#ifndef RUDA_BASE_ALLOCATOR_H
#define RUDA_BASE_ALLOCATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void *(*RudaAllocateFn)(void *context, size_t size);
typedef void *(*RudaResizeFn)(
    void *context,
    void *allocation,
    size_t old_size,
    size_t new_size
);
typedef void (*RudaDeallocateFn)(void *context, void *allocation, size_t size);

typedef struct RudaAllocator {
    void *context;
    RudaAllocateFn allocate;
    RudaResizeFn resize;
    RudaDeallocateFn deallocate;
} RudaAllocator;

/* Returns a process-heap allocator. The returned value owns no state. */
RudaAllocator ruda_system_allocator(void);

/*
 * Returns a new uninitialized allocation owned by the caller, or NULL on
 * failure. A zero-byte request returns NULL and is not an allocation attempt.
 */
void *ruda_allocate(const RudaAllocator *allocator, size_t size);

/*
 * Resizes caller-owned memory. On failure, the old allocation remains owned
 * by the caller. A zero new_size releases the allocation and returns NULL.
 */
void *ruda_resize(
    const RudaAllocator *allocator,
    void *allocation,
    size_t old_size,
    size_t new_size
);

/* Releases caller-owned memory. NULL is accepted and ignored. */
void ruda_deallocate(const RudaAllocator *allocator, void *allocation, size_t size);

/* Checked size arithmetic used before every allocation calculation. */
bool ruda_size_add(size_t left, size_t right, size_t *result);
bool ruda_size_multiply(size_t left, size_t right, size_t *result);

/* Allocates count * element_size bytes after checked multiplication. */
void *ruda_allocate_array(
    const RudaAllocator *allocator,
    size_t count,
    size_t element_size
);

typedef struct RudaFailAllocator {
    RudaAllocator backing;
    size_t failure_index;
    size_t attempts;
    size_t live_allocations;
    size_t live_bytes;
} RudaFailAllocator;

/*
 * Initializes deterministic failure injection. failure_index is zero-based;
 * SIZE_MAX disables failure. State must outlive the returned allocator.
 */
void ruda_fail_allocator_init(
    RudaFailAllocator *state,
    RudaAllocator backing,
    size_t failure_index
);
RudaAllocator ruda_fail_allocator(RudaFailAllocator *state);

#endif
