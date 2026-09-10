#ifndef MMAY_BASE_CODE_MEMORY_H
#define MMAY_BASE_CODE_MEMORY_H

#include <stdbool.h>
#include <stddef.h>

typedef enum MarimayCodeMemoryStatus {
    MMAY_CODE_MEMORY_OK,
    MMAY_CODE_MEMORY_UNAVAILABLE,
    MMAY_CODE_MEMORY_ALLOCATION_FAILED,
    MMAY_CODE_MEMORY_INVALID
} MarimayCodeMemoryStatus;

typedef struct MarimayCodeMemory {
    void *address;
    size_t bytes;
    bool published;
} MarimayCodeMemory;

/* Central executable-page allocator. Publication is one-way and never exposes
 * a writable pointer to its caller. No executable page enters a managed heap. */
bool mmay_code_memory_supported(void);
MarimayCodeMemoryStatus mmay_code_memory_publish(MarimayCodeMemory *memory,
    const void *bytes, size_t length, size_t limit);
void mmay_code_memory_dispose(MarimayCodeMemory *memory);

#endif
