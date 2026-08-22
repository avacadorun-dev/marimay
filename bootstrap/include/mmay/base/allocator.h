#ifndef MMAY_BASE_ALLOCATOR_H
#define MMAY_BASE_ALLOCATOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void *(*MarimayAllocateFn)(void *context, size_t size);
typedef void *(*MarimayResizeFn)(
    void *context,
    void *allocation,
    size_t old_size,
    size_t new_size
);
typedef void (*MarimayDeallocateFn)(void *context, void *allocation, size_t size);

typedef struct MarimayAllocator {
    void *context;
    MarimayAllocateFn allocate;
    MarimayResizeFn resize;
    MarimayDeallocateFn deallocate;
} MarimayAllocator;

/* Returns a process-heap allocator. The returned value owns no state. */
MarimayAllocator mmay_system_allocator(void);

/*
 * Returns a new uninitialized allocation owned by the caller, or NULL on
 * failure. A zero-byte request returns NULL and is not an allocation attempt.
 */
void *mmay_allocate(const MarimayAllocator *allocator, size_t size);

/*
 * Resizes caller-owned memory. On failure, the old allocation remains owned
 * by the caller. A zero new_size releases the allocation and returns NULL.
 */
void *mmay_resize(
    const MarimayAllocator *allocator,
    void *allocation,
    size_t old_size,
    size_t new_size
);

/* Releases caller-owned memory. NULL is accepted and ignored. */
void mmay_deallocate(const MarimayAllocator *allocator, void *allocation, size_t size);

/* Checked size arithmetic used before every allocation calculation. */
bool mmay_size_add(size_t left, size_t right, size_t *result);
bool mmay_size_multiply(size_t left, size_t right, size_t *result);

/* Allocates count * element_size bytes after checked multiplication. */
void *mmay_allocate_array(
    const MarimayAllocator *allocator,
    size_t count,
    size_t element_size
);

typedef struct MarimayFailAllocator {
    MarimayAllocator backing;
    size_t failure_index;
    size_t attempts;
    size_t live_allocations;
    size_t live_bytes;
} MarimayFailAllocator;

/*
 * Initializes deterministic failure injection. failure_index is zero-based;
 * SIZE_MAX disables failure. State must outlive the returned allocator.
 */
void mmay_fail_allocator_init(
    MarimayFailAllocator *state,
    MarimayAllocator backing,
    size_t failure_index
);
MarimayAllocator mmay_fail_allocator(MarimayFailAllocator *state);

#endif
