#ifndef MMAY_BASE_ARENA_H
#define MMAY_BASE_ARENA_H

#include <stddef.h>

#include "mmay/base/allocator.h"

typedef struct MarimayArenaBlock MarimayArenaBlock;

typedef struct MarimayArena {
    MarimayAllocator allocator;
    MarimayArenaBlock *blocks;
    size_t default_block_size;
} MarimayArena;

/* Arena owns every returned pointer until reset/dispose. */
void mmay_arena_init(MarimayArena *arena, MarimayAllocator allocator, size_t default_block_size);

/* alignment must be a nonzero power of two no greater than max_align_t. */
void *mmay_arena_allocate(MarimayArena *arena, size_t size, size_t alignment);

/* Frees all blocks and invalidates all pointers returned by the arena. */
void mmay_arena_reset(MarimayArena *arena);
void mmay_arena_dispose(MarimayArena *arena);

#define MMAY_ARENA_NEW(arena, Type) \
    ((Type *)mmay_arena_allocate((arena), sizeof(Type), _Alignof(Type)))

#endif
