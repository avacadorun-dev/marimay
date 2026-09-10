#ifndef MMAY_JIT_MEMORY_H
#define MMAY_JIT_MEMORY_H
#include "mmay/base/allocator.h"
#include "mmay/base/code_memory.h"
typedef struct MarimayJitCodeBuild {
    MarimayAllocator allocator;
    MarimayCodeMemory memory;
    void *staging;
    size_t staging_bytes;
    size_t capacity;
    MarimayCodeMemoryStatus status;
    MarimayCodeMemoryFault *fault;
} MarimayJitCodeBuild;
void *mmay_sljit_allocate(size_t bytes, void *allocator);
void mmay_sljit_deallocate(void *allocation, void *allocator);
void *mmay_sljit_code_allocate(size_t bytes, void *context);
intptr_t mmay_sljit_code_offset(void *allocation);
void mmay_sljit_code_deallocate(void *allocation, void *context);
MarimayCodeMemoryStatus mmay_jit_code_publish(MarimayJitCodeBuild *build, void *code, size_t bytes);
void mmay_jit_code_dispose(MarimayJitCodeBuild *build);
#endif
