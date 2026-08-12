#include "ruda/base/arena.h"

#include <stdbool.h>
#include <stddef.h>

struct RudaArenaBlock {
    RudaArenaBlock *next;
    size_t allocation_size;
    size_t capacity;
    size_t used;
    max_align_t alignment_anchor;
    unsigned char bytes[];
};

static bool ruda_is_power_of_two(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

static RudaArenaBlock *ruda_arena_add_block(
    RudaArena *arena,
    size_t size,
    size_t alignment
) {
    size_t required = 0;
    size_t capacity = 0;
    size_t allocation_size = 0;
    RudaArenaBlock *block = NULL;
    if (!ruda_size_add(size, alignment - 1, &required)) {
        return NULL;
    }
    capacity = arena->default_block_size > required
        ? arena->default_block_size
        : required;
    if (!ruda_size_add(offsetof(RudaArenaBlock, bytes), capacity, &allocation_size)) {
        return NULL;
    }
    block = ruda_allocate(&arena->allocator, allocation_size);
    if (block == NULL) {
        return NULL;
    }
    block->next = arena->blocks;
    block->allocation_size = allocation_size;
    block->capacity = capacity;
    block->used = 0;
    arena->blocks = block;
    return block;
}

void ruda_arena_init(RudaArena *arena, RudaAllocator allocator, size_t default_block_size) {
    if (arena != NULL) {
        arena->allocator = allocator;
        arena->blocks = NULL;
        arena->default_block_size = default_block_size == 0 ? 4096 : default_block_size;
    }
}

void *ruda_arena_allocate(RudaArena *arena, size_t size, size_t alignment) {
    RudaArenaBlock *block = NULL;
    size_t aligned = 0;
    size_t end = 0;
    if (arena == NULL || size == 0 || !ruda_is_power_of_two(alignment)
        || alignment > _Alignof(max_align_t)) {
        return NULL;
    }
    block = arena->blocks;
    if (block != NULL) {
        if (!ruda_size_add(block->used, alignment - 1, &aligned)) {
            return NULL;
        }
        aligned &= ~(alignment - 1);
        if (!ruda_size_add(aligned, size, &end)) {
            return NULL;
        }
    }
    if (block == NULL || end > block->capacity) {
        block = ruda_arena_add_block(arena, size, alignment);
        if (block == NULL) {
            return NULL;
        }
        aligned = 0;
        end = size;
    }
    block->used = end;
    return block->bytes + aligned;
}

void ruda_arena_reset(RudaArena *arena) {
    RudaArenaBlock *block = NULL;
    if (arena == NULL) {
        return;
    }
    block = arena->blocks;
    while (block != NULL) {
        RudaArenaBlock *next = block->next;
        ruda_deallocate(&arena->allocator, block, block->allocation_size);
        block = next;
    }
    arena->blocks = NULL;
}

void ruda_arena_dispose(RudaArena *arena) {
    ruda_arena_reset(arena);
}
