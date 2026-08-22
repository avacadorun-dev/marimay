#include "mmay/base/allocator.h"

#include <stdlib.h>

static void *mmay_system_allocate(void *context, size_t size) {
    (void)context;
    return malloc(size);
}

static void *mmay_system_resize(
    void *context,
    void *allocation,
    size_t old_size,
    size_t new_size
) {
    (void)context;
    (void)old_size;
    return realloc(allocation, new_size);
}

static void mmay_system_deallocate(void *context, void *allocation, size_t size) {
    (void)context;
    (void)size;
    free(allocation);
}

MarimayAllocator mmay_system_allocator(void) {
    MarimayAllocator allocator;
    allocator.context = NULL;
    allocator.allocate = mmay_system_allocate;
    allocator.resize = mmay_system_resize;
    allocator.deallocate = mmay_system_deallocate;
    return allocator;
}

void *mmay_allocate(const MarimayAllocator *allocator, size_t size) {
    if (allocator == NULL || allocator->allocate == NULL || size == 0) {
        return NULL;
    }
    return allocator->allocate(allocator->context, size);
}

void *mmay_resize(
    const MarimayAllocator *allocator,
    void *allocation,
    size_t old_size,
    size_t new_size
) {
    if (allocation == NULL) {
        return mmay_allocate(allocator, new_size);
    }
    if (allocator == NULL) {
        return NULL;
    }
    if (new_size == 0) {
        mmay_deallocate(allocator, allocation, old_size);
        return NULL;
    }
    if (allocator->resize == NULL) {
        return NULL;
    }
    return allocator->resize(allocator->context, allocation, old_size, new_size);
}

void mmay_deallocate(const MarimayAllocator *allocator, void *allocation, size_t size) {
    if (allocator == NULL || allocator->deallocate == NULL || allocation == NULL) {
        return;
    }
    allocator->deallocate(allocator->context, allocation, size);
}

bool mmay_size_add(size_t left, size_t right, size_t *result) {
    if (result == NULL || right > SIZE_MAX - left) {
        return false;
    }
    *result = left + right;
    return true;
}

bool mmay_size_multiply(size_t left, size_t right, size_t *result) {
    if (result == NULL || (left != 0 && right > SIZE_MAX / left)) {
        return false;
    }
    *result = left * right;
    return true;
}

void *mmay_allocate_array(
    const MarimayAllocator *allocator,
    size_t count,
    size_t element_size
) {
    size_t size = 0;
    if (!mmay_size_multiply(count, element_size, &size)) {
        return NULL;
    }
    return mmay_allocate(allocator, size);
}

static bool mmay_fail_allocator_should_fail(MarimayFailAllocator *state) {
    bool should_fail = state->failure_index != SIZE_MAX
        && state->attempts == state->failure_index;
    if (state->attempts != SIZE_MAX) {
        state->attempts += 1;
    }
    return should_fail;
}

static void *mmay_fail_allocate(void *context, size_t size) {
    MarimayFailAllocator *state = context;
    void *allocation = NULL;
    if (mmay_fail_allocator_should_fail(state)) {
        return NULL;
    }
    allocation = mmay_allocate(&state->backing, size);
    if (allocation != NULL) {
        state->live_allocations += 1;
        state->live_bytes += size;
    }
    return allocation;
}

static void *mmay_fail_resize(
    void *context,
    void *allocation,
    size_t old_size,
    size_t new_size
) {
    MarimayFailAllocator *state = context;
    void *resized = NULL;
    if (mmay_fail_allocator_should_fail(state)) {
        return NULL;
    }
    resized = mmay_resize(&state->backing, allocation, old_size, new_size);
    if (resized != NULL) {
        if (new_size >= old_size) {
            state->live_bytes += new_size - old_size;
        } else {
            state->live_bytes -= old_size - new_size;
        }
    }
    return resized;
}

static void mmay_fail_deallocate(void *context, void *allocation, size_t size) {
    MarimayFailAllocator *state = context;
    mmay_deallocate(&state->backing, allocation, size);
    if (allocation != NULL) {
        state->live_allocations -= 1;
        state->live_bytes -= size;
    }
}

void mmay_fail_allocator_init(
    MarimayFailAllocator *state,
    MarimayAllocator backing,
    size_t failure_index
) {
    state->backing = backing;
    state->failure_index = failure_index;
    state->attempts = 0;
    state->live_allocations = 0;
    state->live_bytes = 0;
}

MarimayAllocator mmay_fail_allocator(MarimayFailAllocator *state) {
    MarimayAllocator allocator;
    allocator.context = state;
    allocator.allocate = mmay_fail_allocate;
    allocator.resize = mmay_fail_resize;
    allocator.deallocate = mmay_fail_deallocate;
    return allocator;
}
