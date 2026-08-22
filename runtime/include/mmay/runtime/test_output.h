#ifndef MMAY_RUNTIME_TEST_OUTPUT_H
#define MMAY_RUNTIME_TEST_OUTPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/base/vector.h"
#include "mmay/runtime/io.h"

typedef enum MarimayTestOutputStatus {
    MMAY_TEST_OUTPUT_OK,
    MMAY_TEST_OUTPUT_MISMATCH,
    MMAY_TEST_OUTPUT_CAPABILITY_DENIED,
    MMAY_TEST_OUTPUT_WRITE_FAILED,
    MMAY_TEST_OUTPUT_ALLOCATION_FAILED,
    MMAY_TEST_OUTPUT_LIMIT_REACHED,
    MMAY_TEST_OUTPUT_INVALID_ARGUMENT
} MarimayTestOutputStatus;

/*
 * Invocation-owned stack used only by compiler-known test expectations.
 * Frames and failure bytes are allocator-owned and released by dispose.
 */
typedef struct MarimayTestOutput {
    MarimayAllocator allocator;
    MarimayVector frames;
    MarimayOwnedBytes failure;
    size_t captured_bytes;
    bool initialized;
} MarimayTestOutput;

/* Lazily initializes state and copies expected bytes into a nested frame. */
MarimayTestOutputStatus mmay_test_output_begin(
    MarimayTestOutput *output,
    MarimayAllocator allocator,
    const unsigned char *expected,
    size_t expected_length
);

/* Captures the complete line in the innermost frame or delegates to stdout. */
MarimayTestOutputStatus mmay_test_output_publish(
    MarimayTestOutput *output,
    const MarimayIoStdout *stdout_sink,
    const unsigned char *line,
    size_t line_length
);

/* Pops one frame and retains a bounded deterministic mismatch description. */
MarimayTestOutputStatus mmay_test_output_end(MarimayTestOutput *output);

size_t mmay_test_output_depth(const MarimayTestOutput *output);
const MarimayOwnedBytes *mmay_test_output_failure(const MarimayTestOutput *output);
void mmay_test_output_dispose(MarimayTestOutput *output);

#endif
