#ifndef RUDA_PLATFORM_PROCESS_H
#define RUDA_PLATFORM_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"

typedef enum RudaProcessStatus {
    RUDA_PROCESS_OK,
    RUDA_PROCESS_INVALID_ARGUMENT,
    RUDA_PROCESS_ALLOCATION_FAILED,
    RUDA_PROCESS_SPAWN_FAILED,
    RUDA_PROCESS_IO_FAILED,
    RUDA_PROCESS_OUTPUT_LIMIT,
    RUDA_PROCESS_WAIT_FAILED
} RudaProcessStatus;

typedef struct RudaProcessResult {
    RudaOwnedBytes output;
    uint32_t exit_code;
    bool signaled;
    uint32_t signal_code;
} RudaProcessResult;

/*
 * Runs a null-terminated argv vector directly without a command shell.
 * stdout/stderr are combined in program write order and bounded by max_output.
 */
RudaProcessStatus ruda_process_run(
    RudaAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    size_t max_output,
    RudaProcessResult *result
);

void ruda_process_result_dispose(RudaProcessResult *result);

#endif
