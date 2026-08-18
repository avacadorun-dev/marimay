#include "test_support.h"

#include <string.h>

#include "ruda/runtime/io.h"

typedef struct RudaRuntimeIoSink {
    unsigned char bytes[64];
    size_t length;
    size_t calls;
    bool accept;
} RudaRuntimeIoSink;

static bool ruda_test_runtime_io_publish(
    void *context,
    const unsigned char *line_bytes,
    size_t line_length
) {
    RudaRuntimeIoSink *sink = context;
    if (sink == NULL || line_bytes == NULL || line_length > sizeof(sink->bytes)) {
        return false;
    }
    sink->calls += 1;
    if (!sink->accept) {
        return false;
    }
    memcpy(sink->bytes, line_bytes, line_length);
    sink->length = line_length;
    return true;
}

static RudaIoStdout ruda_test_runtime_io_stdout(
    RudaRuntimeIoSink *sink,
    bool granted
) {
    RudaIoStdout stdout_sink;
    stdout_sink.capability_granted = granted;
    stdout_sink.context = sink;
    stdout_sink.publish = ruda_test_runtime_io_publish;
    return stdout_sink;
}

static void ruda_test_runtime_io_exact_line_bytes(void) {
    static const unsigned char embedded_nul[] = { 'a', '\0', 'b', '\n' };
    static const unsigned char payload_newline[] = { 'a', '\n', 'b', '\n' };
    static const unsigned char empty_line[] = { '\n' };
    RudaRuntimeIoSink sink = {0};
    RudaIoStdout stdout_sink = ruda_test_runtime_io_stdout(&sink, true);

    sink.accept = true;
    RUDA_EXPECT(ruda_io_stdout_publish_line(
        &stdout_sink, embedded_nul, sizeof(embedded_nul)
    ) == RUDA_IO_STDOUT_OK);
    RUDA_EXPECT(sink.calls == 1);
    RUDA_EXPECT(sink.length == sizeof(embedded_nul));
    RUDA_EXPECT(memcmp(sink.bytes, embedded_nul, sizeof(embedded_nul)) == 0);

    RUDA_EXPECT(ruda_io_stdout_publish_line(
        &stdout_sink, payload_newline, sizeof(payload_newline)
    ) == RUDA_IO_STDOUT_OK);
    RUDA_EXPECT(sink.calls == 2);
    RUDA_EXPECT(sink.length == sizeof(payload_newline));
    RUDA_EXPECT(memcmp(sink.bytes, payload_newline, sizeof(payload_newline)) == 0);

    RUDA_EXPECT(ruda_io_stdout_publish_line(
        &stdout_sink, empty_line, sizeof(empty_line)
    ) == RUDA_IO_STDOUT_OK);
    RUDA_EXPECT(sink.calls == 3);
    RUDA_EXPECT(sink.length == sizeof(empty_line));
    RUDA_EXPECT(memcmp(sink.bytes, empty_line, sizeof(empty_line)) == 0);
}

static void ruda_test_runtime_io_denial_and_failure(void) {
    static const unsigned char line[] = { 'o', 'k', '\n' };
    RudaRuntimeIoSink sink = {0};
    RudaIoStdout denied = ruda_test_runtime_io_stdout(&sink, false);
    RudaIoStdout granted = ruda_test_runtime_io_stdout(&sink, true);

    sink.accept = true;
    RUDA_EXPECT(ruda_io_stdout_publish_line(
        &denied, line, sizeof(line)
    ) == RUDA_IO_STDOUT_CAPABILITY_DENIED);
    RUDA_EXPECT(sink.calls == 0);

    sink.accept = false;
    RUDA_EXPECT(ruda_io_stdout_publish_line(
        &granted, line, sizeof(line)
    ) == RUDA_IO_STDOUT_WRITE_FAILED);
    RUDA_EXPECT(sink.calls == 1);
    RUDA_EXPECT(sink.length == 0);
}

static void ruda_test_runtime_io_invalid_line(void) {
    static const unsigned char no_lf[] = { 'n', 'o' };
    RudaRuntimeIoSink sink = {0};
    RudaIoStdout stdout_sink = ruda_test_runtime_io_stdout(&sink, true);

    sink.accept = true;
    RUDA_EXPECT(ruda_io_stdout_publish_line(
        &stdout_sink, NULL, 0
    ) == RUDA_IO_STDOUT_INVALID_ARGUMENT);
    RUDA_EXPECT(ruda_io_stdout_publish_line(
        &stdout_sink, no_lf, sizeof(no_lf)
    ) == RUDA_IO_STDOUT_INVALID_ARGUMENT);
    RUDA_EXPECT(sink.calls == 0);
}

void ruda_test_runtime_io(void) {
    ruda_test_runtime_io_exact_line_bytes();
    ruda_test_runtime_io_denial_and_failure();
    ruda_test_runtime_io_invalid_line();
}
