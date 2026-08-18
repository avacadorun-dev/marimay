#ifndef RUDA_RUNTIME_IO_H
#define RUDA_RUNTIME_IO_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Invocation-owned program stdout. The caller supplies one complete line:
 * exact UTF-8 payload bytes followed by one LF. The sink does not borrow or
 * retain the bytes after this call returns.
 */
typedef bool (*RudaIoStdoutPublishFn)(
    void *context,
    const unsigned char *line_bytes,
    size_t line_length
);

typedef struct RudaIoStdout {
    bool capability_granted;
    void *context;
    RudaIoStdoutPublishFn publish;
} RudaIoStdout;

typedef enum RudaIoStdoutStatus {
    RUDA_IO_STDOUT_OK,
    RUDA_IO_STDOUT_CAPABILITY_DENIED,
    RUDA_IO_STDOUT_WRITE_FAILED,
    RUDA_IO_STDOUT_INVALID_ARGUMENT
} RudaIoStdoutStatus;

/*
 * Calls the sink exactly once only for a granted, complete non-empty line.
 * The runtime neither allocates nor owns the sink or the supplied bytes.
 */
RudaIoStdoutStatus ruda_io_stdout_publish_line(
    const RudaIoStdout *stdout_sink,
    const unsigned char *line_bytes,
    size_t line_length
);

#endif
