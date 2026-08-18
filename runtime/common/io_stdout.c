#include "ruda/runtime/io.h"

RudaIoStdoutStatus ruda_io_stdout_publish_line(
    const RudaIoStdout *stdout_sink,
    const unsigned char *line_bytes,
    size_t line_length
) {
    if (stdout_sink == NULL) {
        return RUDA_IO_STDOUT_INVALID_ARGUMENT;
    }
    if (!stdout_sink->capability_granted) {
        return RUDA_IO_STDOUT_CAPABILITY_DENIED;
    }
    if (line_bytes == NULL || line_length == 0
        || line_bytes[line_length - 1] != '\n'
        || stdout_sink->publish == NULL) {
        return RUDA_IO_STDOUT_INVALID_ARGUMENT;
    }
    return stdout_sink->publish(
        stdout_sink->context, line_bytes, line_length
    ) ? RUDA_IO_STDOUT_OK : RUDA_IO_STDOUT_WRITE_FAILED;
}
