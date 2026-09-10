#include "jit_memory.h"
#include <stdalign.h>
#include <string.h>
typedef union MarimayJitAllocation {
    max_align_t alignment;
    struct { size_t bytes; intptr_t executable_offset; } owned;
} MarimayJitAllocation;
void *mmay_sljit_allocate(size_t bytes, void *context) {
    size_t total = 0;
    MarimayAllocator *allocator = context;
    if (allocator == NULL || bytes == 0 || !mmay_size_add(sizeof(MarimayJitAllocation), bytes, &total)) return NULL;
    MarimayJitAllocation *allocation = mmay_allocate(allocator, total);
    if (allocation == NULL) return NULL;
    allocation->owned.bytes = total; allocation->owned.executable_offset = 0;
    return allocation + 1;
}
void mmay_sljit_deallocate(void *pointer, void *context) {
    if (pointer == NULL) return;
    MarimayJitAllocation *allocation = (MarimayJitAllocation *)pointer - 1;
    mmay_deallocate(context, allocation, allocation->owned.bytes);
}
void *mmay_sljit_code_allocate(size_t bytes, void *context) {
    MarimayJitCodeBuild *build = context;
    if (build == NULL || build->staging != NULL || build->memory.address != NULL) return NULL;
    build->status = mmay_code_memory_reserve(&build->memory, bytes, build->capacity, build->fault);
    if (build->status != MMAY_CODE_MEMORY_OK) return NULL;
    build->staging = mmay_sljit_allocate(bytes, &build->allocator);
    if (build->staging == NULL) {
        build->status = MMAY_CODE_MEMORY_ALLOCATION_FAILED;
        mmay_code_memory_dispose(&build->memory);
        return NULL;
    }
    build->staging_bytes = bytes;
    MarimayJitAllocation *allocation = (MarimayJitAllocation *)build->staging - 1;
    /* SLJIT's dual-address contract takes the modular target-minus-source
     * displacement. Do not subtract pointers to unrelated C allocations. */
    uintptr_t delta = (uintptr_t)build->memory.address - (uintptr_t)build->staging;
    _Static_assert(sizeof(delta) == sizeof(allocation->owned.executable_offset), "code displacement representation");
    memcpy(&allocation->owned.executable_offset, &delta, sizeof(delta));
    return build->staging;
}
intptr_t mmay_sljit_code_offset(void *pointer) {
    if (pointer == NULL) return 0;
    return ((MarimayJitAllocation *)pointer - 1)->owned.executable_offset;
}
void mmay_sljit_code_deallocate(void *pointer, void *context) {
    MarimayJitCodeBuild *build = context;
    if (build != NULL && (pointer == build->staging || pointer == build->memory.address)) mmay_jit_code_dispose(build);
}
MarimayCodeMemoryStatus mmay_jit_code_publish(MarimayJitCodeBuild *build, void *code, size_t bytes) {
    if (build == NULL || code == NULL || code != build->memory.address || build->staging == NULL
        || bytes == 0 || bytes > build->staging_bytes) return MMAY_CODE_MEMORY_INVALID;
    build->status = mmay_code_memory_publish(&build->memory, build->staging, bytes);
    mmay_sljit_deallocate(build->staging, &build->allocator); build->staging = NULL;
    build->staging_bytes = 0;
    return build->status;
}
void mmay_jit_code_dispose(MarimayJitCodeBuild *build) {
    if (build == NULL) return;
    mmay_sljit_deallocate(build->staging, &build->allocator); build->staging = NULL;
    build->staging_bytes = 0;
    mmay_code_memory_dispose(&build->memory);
}
