#include "test_support.h"

#include <string.h>

#include "mmay/runtime/io.h"
#include "mmay/runtime/test_output.h"

typedef struct MarimayRuntimeIoSink {
    unsigned char bytes[64];
    size_t length;
    size_t calls;
    bool accept;
} MarimayRuntimeIoSink;

static bool mmay_test_runtime_io_publish(
    void *context,
    const unsigned char *line_bytes,
    size_t line_length
) {
    MarimayRuntimeIoSink *sink = context;
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

static MarimayIoStdout mmay_test_runtime_io_stdout(
    MarimayRuntimeIoSink *sink,
    bool granted
) {
    MarimayIoStdout stdout_sink;
    stdout_sink.capability_granted = granted;
    stdout_sink.context = sink;
    stdout_sink.publish = mmay_test_runtime_io_publish;
    return stdout_sink;
}

static void mmay_test_runtime_io_exact_line_bytes(void) {
    static const unsigned char embedded_nul[] = { 'a', '\0', 'b', '\n' };
    static const unsigned char payload_newline[] = { 'a', '\n', 'b', '\n' };
    static const unsigned char empty_line[] = { '\n' };
    MarimayRuntimeIoSink sink = {0};
    MarimayIoStdout stdout_sink = mmay_test_runtime_io_stdout(&sink, true);

    sink.accept = true;
    MMAY_EXPECT(mmay_io_stdout_publish_line(
        &stdout_sink, embedded_nul, sizeof(embedded_nul)
    ) == MMAY_IO_STDOUT_OK);
    MMAY_EXPECT(sink.calls == 1);
    MMAY_EXPECT(sink.length == sizeof(embedded_nul));
    MMAY_EXPECT(memcmp(sink.bytes, embedded_nul, sizeof(embedded_nul)) == 0);

    MMAY_EXPECT(mmay_io_stdout_publish_line(
        &stdout_sink, payload_newline, sizeof(payload_newline)
    ) == MMAY_IO_STDOUT_OK);
    MMAY_EXPECT(sink.calls == 2);
    MMAY_EXPECT(sink.length == sizeof(payload_newline));
    MMAY_EXPECT(memcmp(sink.bytes, payload_newline, sizeof(payload_newline)) == 0);

    MMAY_EXPECT(mmay_io_stdout_publish_line(
        &stdout_sink, empty_line, sizeof(empty_line)
    ) == MMAY_IO_STDOUT_OK);
    MMAY_EXPECT(sink.calls == 3);
    MMAY_EXPECT(sink.length == sizeof(empty_line));
    MMAY_EXPECT(memcmp(sink.bytes, empty_line, sizeof(empty_line)) == 0);
}

static void mmay_test_runtime_io_denial_and_failure(void) {
    static const unsigned char line[] = { 'o', 'k', '\n' };
    MarimayRuntimeIoSink sink = {0};
    MarimayIoStdout denied = mmay_test_runtime_io_stdout(&sink, false);
    MarimayIoStdout granted = mmay_test_runtime_io_stdout(&sink, true);

    sink.accept = true;
    MMAY_EXPECT(mmay_io_stdout_publish_line(
        &denied, line, sizeof(line)
    ) == MMAY_IO_STDOUT_CAPABILITY_DENIED);
    MMAY_EXPECT(sink.calls == 0);

    sink.accept = false;
    MMAY_EXPECT(mmay_io_stdout_publish_line(
        &granted, line, sizeof(line)
    ) == MMAY_IO_STDOUT_WRITE_FAILED);
    MMAY_EXPECT(sink.calls == 1);
    MMAY_EXPECT(sink.length == 0);
}

static void mmay_test_runtime_io_invalid_line(void) {
    static const unsigned char no_lf[] = { 'n', 'o' };
    MarimayRuntimeIoSink sink = {0};
    MarimayIoStdout stdout_sink = mmay_test_runtime_io_stdout(&sink, true);

    sink.accept = true;
    MMAY_EXPECT(mmay_io_stdout_publish_line(
        &stdout_sink, NULL, 0
    ) == MMAY_IO_STDOUT_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_io_stdout_publish_line(
        &stdout_sink, no_lf, sizeof(no_lf)
    ) == MMAY_IO_STDOUT_INVALID_ARGUMENT);
    MMAY_EXPECT(sink.calls == 0);
}

static void mmay_test_runtime_output_expectations(void) {
    static const unsigned char expected[] = {'a', '\0', 'b', '\n'};
    static const unsigned char actual[] = {'a', '\0', 'b', '\n'};
    static const unsigned char different[] = {'n', 'o', '\n'};
    MarimayTestOutput output = {0};
    MarimayRuntimeIoSink host = {0};
    MarimayIoStdout sink = mmay_test_runtime_io_stdout(&host, false);
    const MarimayOwnedBytes *failure = NULL;
    host.accept = true;
    MMAY_EXPECT(mmay_test_output_begin(
        &output, mmay_system_allocator(), expected, sizeof(expected)
    ) == MMAY_TEST_OUTPUT_OK);
    MMAY_EXPECT(mmay_test_output_depth(&output) == 1);
    MMAY_EXPECT(mmay_test_output_publish(
        &output, &sink, actual, sizeof(actual)
    ) == MMAY_TEST_OUTPUT_OK);
    MMAY_EXPECT(host.calls == 0);
    MMAY_EXPECT(mmay_test_output_end(&output) == MMAY_TEST_OUTPUT_OK);
    MMAY_EXPECT(mmay_test_output_depth(&output) == 0);

    MMAY_EXPECT(mmay_test_output_begin(
        &output, mmay_system_allocator(), expected, sizeof(expected)
    ) == MMAY_TEST_OUTPUT_OK);
    MMAY_EXPECT(mmay_test_output_publish(
        &output, &sink, different, sizeof(different)
    ) == MMAY_TEST_OUTPUT_OK);
    MMAY_EXPECT(mmay_test_output_end(&output) == MMAY_TEST_OUTPUT_MISMATCH);
    failure = mmay_test_output_failure(&output);
    MMAY_EXPECT(failure != NULL && failure->length != 0);
    if (failure != NULL) {
        MMAY_EXPECT(strstr(
            (const char *)failure->data,
            "expected 4 bytes 0x6100620a, captured 3 bytes 0x6e6f0a"
        ) != NULL);
    }
    mmay_test_output_dispose(&output);
}

static void mmay_test_runtime_output_allocations(void) {
    static const unsigned char line[] = {'o', 'k', '\n'};
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 128; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayTestOutput output = {0};
        MarimayRuntimeIoSink host = {0};
        MarimayIoStdout sink = mmay_test_runtime_io_stdout(&host, true);
        MarimayTestOutputStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        allocator = mmay_fail_allocator(&state);
        host.accept = true;
        status = mmay_test_output_begin(
            &output, allocator, line, sizeof(line)
        );
        if (status == MMAY_TEST_OUTPUT_OK) {
            status = mmay_test_output_publish(
                &output, &sink, line, sizeof(line)
            );
        }
        if (status == MMAY_TEST_OUTPUT_OK) {
            status = mmay_test_output_end(&output);
        }
        if (status == MMAY_TEST_OUTPUT_OK) reached_success = true;
        else MMAY_EXPECT(status == MMAY_TEST_OUTPUT_ALLOCATION_FAILED);
        mmay_test_output_dispose(&output);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_runtime_output_depth_limit(void) {
    MarimayTestOutput output = {0};
    size_t index = 0;
    for (index = 0; index < 64; index += 1) {
        MMAY_EXPECT(mmay_test_output_begin(
            &output, mmay_system_allocator(), NULL, 0
        ) == MMAY_TEST_OUTPUT_OK);
    }
    MMAY_EXPECT(mmay_test_output_depth(&output) == 64);
    MMAY_EXPECT(mmay_test_output_begin(
        &output, mmay_system_allocator(), NULL, 0
    ) == MMAY_TEST_OUTPUT_LIMIT_REACHED);
    for (index = 0; index < 64; index += 1) {
        MMAY_EXPECT(mmay_test_output_end(&output) == MMAY_TEST_OUTPUT_OK);
    }
    MMAY_EXPECT(mmay_test_output_depth(&output) == 0);
    mmay_test_output_dispose(&output);
}

void mmay_test_runtime_io(void) {
    mmay_test_runtime_io_exact_line_bytes();
    mmay_test_runtime_io_denial_and_failure();
    mmay_test_runtime_io_invalid_line();
    mmay_test_runtime_output_expectations();
    mmay_test_runtime_output_allocations();
    mmay_test_runtime_output_depth_limit();
}
