#ifndef RUDA_RUNTIME_TEST_OUTPUT_H
#define RUDA_RUNTIME_TEST_OUTPUT_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/vector.h"
#include "ruda/runtime/io.h"

typedef enum RudaTestOutputStatus {
    RUDA_TEST_OUTPUT_OK,
    RUDA_TEST_OUTPUT_MISMATCH,
    RUDA_TEST_OUTPUT_CAPABILITY_DENIED,
    RUDA_TEST_OUTPUT_WRITE_FAILED,
    RUDA_TEST_OUTPUT_ALLOCATION_FAILED,
    RUDA_TEST_OUTPUT_LIMIT_REACHED,
    RUDA_TEST_OUTPUT_INVALID_ARGUMENT
} RudaTestOutputStatus;

/*
 * Invocation-owned stack used only by compiler-known test expectations.
 * Frames and failure bytes are allocator-owned and released by dispose.
 */
typedef struct RudaTestOutput {
    RudaAllocator allocator;
    RudaVector frames;
    RudaOwnedBytes failure;
    size_t captured_bytes;
    bool initialized;
} RudaTestOutput;

/* Lazily initializes state and copies expected bytes into a nested frame. */
RudaTestOutputStatus ruda_test_output_begin(
    RudaTestOutput *output,
    RudaAllocator allocator,
    const unsigned char *expected,
    size_t expected_length
);

/* Captures the complete line in the innermost frame or delegates to stdout. */
RudaTestOutputStatus ruda_test_output_publish(
    RudaTestOutput *output,
    const RudaIoStdout *stdout_sink,
    const unsigned char *line,
    size_t line_length
);

/* Pops one frame and retains a bounded deterministic mismatch description. */
RudaTestOutputStatus ruda_test_output_end(RudaTestOutput *output);

size_t ruda_test_output_depth(const RudaTestOutput *output);
const RudaOwnedBytes *ruda_test_output_failure(const RudaTestOutput *output);
void ruda_test_output_dispose(RudaTestOutput *output);

#endif
