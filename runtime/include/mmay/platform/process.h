#ifndef MMAY_PLATFORM_PROCESS_H
#define MMAY_PLATFORM_PROCESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"

typedef enum MarimayProcessStatus {
    MMAY_PROCESS_OK,
    MMAY_PROCESS_INVALID_ARGUMENT,
    MMAY_PROCESS_ALLOCATION_FAILED,
    MMAY_PROCESS_SPAWN_FAILED,
    MMAY_PROCESS_IO_FAILED,
    MMAY_PROCESS_OUTPUT_LIMIT,
    MMAY_PROCESS_WAIT_FAILED
} MarimayProcessStatus;

typedef struct MarimayProcessResult {
    MarimayOwnedBytes output;
    uint32_t exit_code;
    bool signaled;
    uint32_t signal_code;
} MarimayProcessResult;

typedef struct MarimayProcessCaptureResult {
    MarimayOwnedBytes standard_output;
    MarimayOwnedBytes standard_error;
    uint32_t exit_code;
    bool signaled;
    uint32_t signal_code;
} MarimayProcessCaptureResult;

/* Returns the current process executable path for relocatable tool lookup. */
MarimayProcessStatus mmay_process_executable_path(
    MarimayAllocator allocator,
    size_t max_path,
    MarimayOwnedBytes *path
);

/*
 * Runs a null-terminated argv vector directly without a command shell.
 * stdout/stderr are combined in program write order and bounded by max_output.
 */
MarimayProcessStatus mmay_process_run(
    MarimayAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    size_t max_output,
    MarimayProcessResult *result
);

/* Runs argv directly and captures stdout and stderr in separate bounded buffers. */
MarimayProcessStatus mmay_process_run_capture(
    MarimayAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    size_t max_output,
    size_t max_error,
    MarimayProcessCaptureResult *result
);

void mmay_process_result_dispose(MarimayProcessResult *result);
void mmay_process_capture_result_dispose(MarimayProcessCaptureResult *result);

#endif
