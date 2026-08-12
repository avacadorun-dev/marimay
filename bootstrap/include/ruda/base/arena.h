#ifndef RUDA_BASE_ARENA_H
#define RUDA_BASE_ARENA_H

#include <stddef.h>

#include "ruda/base/allocator.h"

typedef struct RudaArenaBlock RudaArenaBlock;

typedef struct RudaArena {
    RudaAllocator allocator;
    RudaArenaBlock *blocks;
    size_t default_block_size;
} RudaArena;

/* Arena owns every returned pointer until reset/dispose. */
void ruda_arena_init(RudaArena *arena, RudaAllocator allocator, size_t default_block_size);

/* alignment must be a nonzero power of two no greater than max_align_t. */
void *ruda_arena_allocate(RudaArena *arena, size_t size, size_t alignment);

/* Frees all blocks and invalidates all pointers returned by the arena. */
void ruda_arena_reset(RudaArena *arena);
void ruda_arena_dispose(RudaArena *arena);

#define RUDA_ARENA_NEW(arena, Type) \
    ((Type *)ruda_arena_allocate((arena), sizeof(Type), _Alignof(Type)))

#endif
