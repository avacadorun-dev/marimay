#ifndef MMAY_RUNTIME_IO_H
#define MMAY_RUNTIME_IO_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"

#define MMAY_IO_MAX_LINE_BYTES ((size_t)1048576)

typedef enum MarimayIoStatus {
    MMAY_IO_OK,
    MMAY_IO_CAPABILITY_DENIED,
    MMAY_IO_READ_FAILED,
    MMAY_IO_WRITE_FAILED,
    MMAY_IO_FLUSH_FAILED,
    MMAY_IO_INVALID_UTF8,
    MMAY_IO_LINE_TOO_LONG,
    MMAY_IO_ALLOCATION_FAILED,
    MMAY_IO_INVALID_ARGUMENT
} MarimayIoStatus;

/* A read callback returns bytes copied into destination; it may return fewer
 * bytes than requested. false is a deterministic read failure. */
typedef bool (*MarimayIoReadFn)(
    void *context, unsigned char *destination, size_t capacity, size_t *count
);
typedef bool (*MarimayIoWriteFn)(
    void *context, const unsigned char *bytes, size_t length, size_t *count
);
typedef bool (*MarimayIoFlushFn)(void *context);

typedef struct MarimayIoInput {
    bool capability_granted;
    void *context;
    MarimayIoReadFn read;
} MarimayIoInput;

typedef struct MarimayIoOutput {
    bool capability_granted;
    void *context;
    MarimayIoWriteFn write;
    MarimayIoFlushFn flush;
} MarimayIoOutput;

/* Reads one bounded UTF-8 line. eof is set only for EOF before any byte. */
MarimayIoStatus mmay_io_read_line(
    const MarimayIoInput *input,
    MarimayAllocator allocator,
    size_t limit,
    MarimayOwnedBytes *line,
    bool *eof
);

/* Completes the whole request through bounded short writes. */
MarimayIoStatus mmay_io_write(
    const MarimayIoOutput *output,
    const unsigned char *bytes,
    size_t length
);
MarimayIoStatus mmay_io_flush(const MarimayIoOutput *output);

/*
 * Invocation-owned program stdout. The caller supplies one complete line:
 * exact UTF-8 payload bytes followed by one LF. The sink does not borrow or
 * retain the bytes after this call returns.
 */
typedef bool (*MarimayIoStdoutPublishFn)(
    void *context,
    const unsigned char *line_bytes,
    size_t line_length
);

typedef struct MarimayIoStdout {
    bool capability_granted;
    void *context;
    MarimayIoStdoutPublishFn publish;
    bool stderr_capability_granted;
    void *stderr_context;
    MarimayIoStdoutPublishFn publish_stderr;
    MarimayIoFlushFn flush_stdout;
    MarimayIoFlushFn flush_stderr;
    bool stdin_capability_granted;
    void *stdin_context;
    MarimayIoReadFn read_stdin;
} MarimayIoStdout;

typedef enum MarimayIoStdoutStatus {
    MMAY_IO_STDOUT_OK,
    MMAY_IO_STDOUT_CAPABILITY_DENIED,
    MMAY_IO_STDOUT_WRITE_FAILED,
    MMAY_IO_STDOUT_INVALID_ARGUMENT
} MarimayIoStdoutStatus;

/*
 * Calls the sink exactly once only for a granted, complete non-empty line.
 * The runtime neither allocates nor owns the sink or the supplied bytes.
 */
MarimayIoStdoutStatus mmay_io_stdout_publish_line(
    const MarimayIoStdout *stdout_sink,
    const unsigned char *line_bytes,
    size_t line_length
);

#endif
