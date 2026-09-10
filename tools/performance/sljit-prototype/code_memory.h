#ifndef MMAY_BASE_CODE_MEMORY_H
#define MMAY_BASE_CODE_MEMORY_H
#include <stdbool.h>
#include <stddef.h>

typedef enum MarimayCodeMemoryStatus {
    MMAY_CODE_MEMORY_OK,
    MMAY_CODE_MEMORY_UNAVAILABLE,
    MMAY_CODE_MEMORY_ALLOCATION_FAILED,
    MMAY_CODE_MEMORY_INVALID,
    MMAY_CODE_MEMORY_LIMIT
} MarimayCodeMemoryStatus;

/* Optional deterministic platform fault ledger. It must outlive its maps. */
typedef struct MarimayCodeMemoryFault {
    size_t failure_index, attempts, live_mappings, live_bytes;
} MarimayCodeMemoryFault;
typedef struct MarimayCodeMemory {
    void *address;
    size_t bytes;
    bool published;
    MarimayCodeMemoryFault *fault;
} MarimayCodeMemory;

bool mmay_code_memory_supported(void);
MarimayCodeMemoryStatus mmay_code_memory_reserve(MarimayCodeMemory *memory,
    size_t bytes, size_t limit, MarimayCodeMemoryFault *fault);
MarimayCodeMemoryStatus mmay_code_memory_publish(MarimayCodeMemory *memory,
    const void *source, size_t bytes);
void mmay_code_memory_dispose(MarimayCodeMemory *memory);
#endif
