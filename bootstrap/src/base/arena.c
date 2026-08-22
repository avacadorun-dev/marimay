#include "mmay/base/arena.h"

#include <stdbool.h>
#include <stddef.h>

struct MarimayArenaBlock {
    MarimayArenaBlock *next;
    size_t allocation_size;
    size_t capacity;
    size_t used;
    max_align_t alignment_anchor;
    unsigned char bytes[];
};

static bool mmay_is_power_of_two(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

static MarimayArenaBlock *mmay_arena_add_block(
    MarimayArena *arena,
    size_t size,
    size_t alignment
) {
    size_t required = 0;
    size_t capacity = 0;
    size_t allocation_size = 0;
    MarimayArenaBlock *block = NULL;
    if (!mmay_size_add(size, alignment - 1, &required)) {
        return NULL;
    }
    capacity = arena->default_block_size > required
        ? arena->default_block_size
        : required;
    if (!mmay_size_add(offsetof(MarimayArenaBlock, bytes), capacity, &allocation_size)) {
        return NULL;
    }
    block = mmay_allocate(&arena->allocator, allocation_size);
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

void mmay_arena_init(MarimayArena *arena, MarimayAllocator allocator, size_t default_block_size) {
    if (arena != NULL) {
        arena->allocator = allocator;
        arena->blocks = NULL;
        arena->default_block_size = default_block_size == 0 ? 4096 : default_block_size;
    }
}

void *mmay_arena_allocate(MarimayArena *arena, size_t size, size_t alignment) {
    MarimayArenaBlock *block = NULL;
    size_t aligned = 0;
    size_t end = 0;
    if (arena == NULL || size == 0 || !mmay_is_power_of_two(alignment)
        || alignment > _Alignof(max_align_t)) {
        return NULL;
    }
    block = arena->blocks;
    if (block != NULL) {
        if (!mmay_size_add(block->used, alignment - 1, &aligned)) {
            return NULL;
        }
        aligned &= ~(alignment - 1);
        if (!mmay_size_add(aligned, size, &end)) {
            return NULL;
        }
    }
    if (block == NULL || end > block->capacity) {
        block = mmay_arena_add_block(arena, size, alignment);
        if (block == NULL) {
            return NULL;
        }
        aligned = 0;
        end = size;
    }
    block->used = end;
    return block->bytes + aligned;
}

void mmay_arena_reset(MarimayArena *arena) {
    MarimayArenaBlock *block = NULL;
    if (arena == NULL) {
        return;
    }
    block = arena->blocks;
    while (block != NULL) {
        MarimayArenaBlock *next = block->next;
        mmay_deallocate(&arena->allocator, block, block->allocation_size);
        block = next;
    }
    arena->blocks = NULL;
}

void mmay_arena_dispose(MarimayArena *arena) {
    mmay_arena_reset(arena);
}
