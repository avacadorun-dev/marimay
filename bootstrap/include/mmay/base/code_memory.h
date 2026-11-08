#ifndef MMAY_BASE_CODE_MEMORY_H
#define MMAY_BASE_CODE_MEMORY_H
#include <stdbool.h>
#include <stddef.h>

/* Readable zero-filled entry padding accommodates host indirect-call probes.
 * It is part of the page-rounded allocation and executable-code budget. */
#define MMAY_CODE_MEMORY_ENTRY_PADDING 16U

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
    /* Callable payload start; the owned mapping begins ENTRY_PADDING earlier. */
    void *address;
    /* Full mapped byte count, including entry padding and page rounding. */
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
