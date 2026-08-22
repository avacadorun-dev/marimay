#ifndef MMAY_RUNTIME_IO_H
#define MMAY_RUNTIME_IO_H

#include <stdbool.h>
#include <stddef.h>

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
