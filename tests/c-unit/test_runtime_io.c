#include "test_support.h"

#include <string.h>

#include "ruda/runtime/io.h"
#include "ruda/runtime/test_output.h"

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

static void ruda_test_runtime_output_expectations(void) {
    static const unsigned char expected[] = {'a', '\0', 'b', '\n'};
    static const unsigned char actual[] = {'a', '\0', 'b', '\n'};
    static const unsigned char different[] = {'n', 'o', '\n'};
    RudaTestOutput output = {0};
    RudaRuntimeIoSink host = {0};
    RudaIoStdout sink = ruda_test_runtime_io_stdout(&host, false);
    const RudaOwnedBytes *failure = NULL;
    host.accept = true;
    RUDA_EXPECT(ruda_test_output_begin(
        &output, ruda_system_allocator(), expected, sizeof(expected)
    ) == RUDA_TEST_OUTPUT_OK);
    RUDA_EXPECT(ruda_test_output_depth(&output) == 1);
    RUDA_EXPECT(ruda_test_output_publish(
        &output, &sink, actual, sizeof(actual)
    ) == RUDA_TEST_OUTPUT_OK);
    RUDA_EXPECT(host.calls == 0);
    RUDA_EXPECT(ruda_test_output_end(&output) == RUDA_TEST_OUTPUT_OK);
    RUDA_EXPECT(ruda_test_output_depth(&output) == 0);

    RUDA_EXPECT(ruda_test_output_begin(
        &output, ruda_system_allocator(), expected, sizeof(expected)
    ) == RUDA_TEST_OUTPUT_OK);
    RUDA_EXPECT(ruda_test_output_publish(
        &output, &sink, different, sizeof(different)
    ) == RUDA_TEST_OUTPUT_OK);
    RUDA_EXPECT(ruda_test_output_end(&output) == RUDA_TEST_OUTPUT_MISMATCH);
    failure = ruda_test_output_failure(&output);
    RUDA_EXPECT(failure != NULL && failure->length != 0);
    if (failure != NULL) {
        RUDA_EXPECT(strstr(
            (const char *)failure->data,
            "expected 4 bytes 0x6100620a, captured 3 bytes 0x6e6f0a"
        ) != NULL);
    }
    ruda_test_output_dispose(&output);
}

static void ruda_test_runtime_output_allocations(void) {
    static const unsigned char line[] = {'o', 'k', '\n'};
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 128; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaTestOutput output = {0};
        RudaRuntimeIoSink host = {0};
        RudaIoStdout sink = ruda_test_runtime_io_stdout(&host, true);
        RudaTestOutputStatus status;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        allocator = ruda_fail_allocator(&state);
        host.accept = true;
        status = ruda_test_output_begin(
            &output, allocator, line, sizeof(line)
        );
        if (status == RUDA_TEST_OUTPUT_OK) {
            status = ruda_test_output_publish(
                &output, &sink, line, sizeof(line)
            );
        }
        if (status == RUDA_TEST_OUTPUT_OK) {
            status = ruda_test_output_end(&output);
        }
        if (status == RUDA_TEST_OUTPUT_OK) reached_success = true;
        else RUDA_EXPECT(status == RUDA_TEST_OUTPUT_ALLOCATION_FAILED);
        ruda_test_output_dispose(&output);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_runtime_output_depth_limit(void) {
    RudaTestOutput output = {0};
    size_t index = 0;
    for (index = 0; index < 64; index += 1) {
        RUDA_EXPECT(ruda_test_output_begin(
            &output, ruda_system_allocator(), NULL, 0
        ) == RUDA_TEST_OUTPUT_OK);
    }
    RUDA_EXPECT(ruda_test_output_depth(&output) == 64);
    RUDA_EXPECT(ruda_test_output_begin(
        &output, ruda_system_allocator(), NULL, 0
    ) == RUDA_TEST_OUTPUT_LIMIT_REACHED);
    for (index = 0; index < 64; index += 1) {
        RUDA_EXPECT(ruda_test_output_end(&output) == RUDA_TEST_OUTPUT_OK);
    }
    RUDA_EXPECT(ruda_test_output_depth(&output) == 0);
    ruda_test_output_dispose(&output);
}

void ruda_test_runtime_io(void) {
    ruda_test_runtime_io_exact_line_bytes();
    ruda_test_runtime_io_denial_and_failure();
    ruda_test_runtime_io_invalid_line();
    ruda_test_runtime_output_expectations();
    ruda_test_runtime_output_allocations();
    ruda_test_runtime_output_depth_limit();
}
