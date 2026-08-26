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

typedef struct MarimayRuntimeIoStream {
    const unsigned char *input;
    size_t input_length;
    size_t input_offset;
    unsigned char output[64];
    size_t output_length;
    size_t write_chunk;
    size_t read_retries;
    size_t write_retries;
    size_t flush_retries;
    size_t read_calls;
    size_t write_calls;
    size_t flush_calls;
    bool fail_read;
    bool fail_write;
    bool fail_flush;
    bool zero_write;
} MarimayRuntimeIoStream;

static MarimayIoTransferStatus mmay_test_runtime_io_read(
    void *context, unsigned char *destination, size_t capacity, size_t *count
) {
    MarimayRuntimeIoStream *stream = context;
    if (stream != NULL) stream->read_calls += 1;
    if (stream == NULL || destination == NULL || count == NULL || capacity == 0
        || stream->fail_read) return MMAY_IO_TRANSFER_FAILED;
    if (stream->read_retries != 0) {
        stream->read_retries -= 1;
        *count = 0;
        return MMAY_IO_TRANSFER_RETRY;
    }
    if (stream->input_offset == stream->input_length) {
        *count = 0;
        return MMAY_IO_TRANSFER_END;
    }
    destination[0] = stream->input[stream->input_offset++];
    *count = 1;
    return MMAY_IO_TRANSFER_OK;
}

static MarimayIoTransferStatus mmay_test_runtime_io_write(
    void *context, const unsigned char *bytes, size_t length, size_t *count
) {
    MarimayRuntimeIoStream *stream = context;
    size_t amount = length;
    if (stream != NULL) stream->write_calls += 1;
    if (stream == NULL || bytes == NULL || count == NULL || stream->fail_write) {
        return MMAY_IO_TRANSFER_FAILED;
    }
    if (stream->write_retries != 0) {
        stream->write_retries -= 1;
        *count = 0;
        return MMAY_IO_TRANSFER_RETRY;
    }
    if (stream->zero_write) {
        *count = 0;
        return MMAY_IO_TRANSFER_OK;
    }
    if (stream->write_chunk != 0 && amount > stream->write_chunk) amount = stream->write_chunk;
    if (amount > sizeof(stream->output) - stream->output_length) {
        return MMAY_IO_TRANSFER_FAILED;
    }
    memcpy(stream->output + stream->output_length, bytes, amount);
    stream->output_length += amount;
    *count = amount;
    return MMAY_IO_TRANSFER_OK;
}

static MarimayIoTransferStatus mmay_test_runtime_io_flush(void *context) {
    MarimayRuntimeIoStream *stream = context;
    if (stream != NULL) stream->flush_calls += 1;
    if (stream != NULL && stream->flush_retries != 0) {
        stream->flush_retries -= 1;
        return MMAY_IO_TRANSFER_RETRY;
    }
    return stream != NULL && !stream->fail_flush
        ? MMAY_IO_TRANSFER_OK : MMAY_IO_TRANSFER_FAILED;
}

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
    MarimayIoStdout stdout_sink = {0};
    stdout_sink.capability_granted = granted;
    stdout_sink.context = sink;
    stdout_sink.publish = mmay_test_runtime_io_publish;
    stdout_sink.stderr_capability_granted = granted;
    stdout_sink.stderr_context = sink;
    stdout_sink.publish_stderr = mmay_test_runtime_io_publish;
    stdout_sink.flush_stdout = NULL;
    stdout_sink.flush_stderr = NULL;
    stdout_sink.stdin_capability_granted = false;
    stdout_sink.stdin_context = NULL;
    stdout_sink.read_stdin = NULL;
    stdout_sink.write_stdout = NULL;
    stdout_sink.write_stderr = NULL;
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

static void mmay_test_runtime_io_stream_contract(void) {
    static const unsigned char crlf[] = {'a', '\0', 'b', '\r', '\n'};
    static const unsigned char invalid_utf8[] = {0xff, '\n'};
    static const unsigned char long_line[] = {'a', 'b', '\n'};
    MarimayRuntimeIoStream stream = {0};
    MarimayIoInput input = {true, &stream, mmay_test_runtime_io_read};
    MarimayIoOutput output = {true, &stream, mmay_test_runtime_io_write, mmay_test_runtime_io_flush};
    MarimayOwnedBytes line = {0};
    MarimayOwnedBytes bytes = {0};
    bool eof = false;
    stream.input = crlf;
    stream.input_length = sizeof(crlf);
    MMAY_EXPECT(mmay_io_read_line(&input, mmay_system_allocator(), 8, &line, &eof) == MMAY_IO_OK);
    MMAY_EXPECT(!eof && line.length == 3 && memcmp(line.data, "a\0b", 3) == 0);
    mmay_owned_bytes_dispose(&line);
    MMAY_EXPECT(mmay_io_read_line(&input, mmay_system_allocator(), 8, &line, &eof) == MMAY_IO_OK);
    MMAY_EXPECT(eof && line.length == 0);
    mmay_owned_bytes_dispose(&line);
    stream.input = invalid_utf8; stream.input_length = sizeof(invalid_utf8); stream.input_offset = 0;
    MMAY_EXPECT(mmay_io_read_line(&input, mmay_system_allocator(), 8, &line, &eof) == MMAY_IO_INVALID_UTF8);
    stream.input = long_line; stream.input_length = sizeof(long_line); stream.input_offset = 0;
    MMAY_EXPECT(mmay_io_read_line(&input, mmay_system_allocator(), 1, &line, &eof) == MMAY_IO_LINE_TOO_LONG);
    stream.input = crlf; stream.input_length = sizeof(crlf); stream.input_offset = 0;
    MMAY_EXPECT(mmay_io_read_bytes(
        &input, mmay_system_allocator(), sizeof(crlf), &bytes
    ) == MMAY_IO_OK);
    MMAY_EXPECT(bytes.length == sizeof(crlf));
    MMAY_EXPECT(memcmp(bytes.data, crlf, sizeof(crlf)) == 0);
    mmay_owned_bytes_dispose(&bytes);
    stream.input_offset = 0;
    MMAY_EXPECT(mmay_io_read_bytes(
        &input, mmay_system_allocator(), sizeof(crlf) - 1, &bytes
    ) == MMAY_IO_INPUT_TOO_LONG);
    stream.write_chunk = 1;
    MMAY_EXPECT(mmay_io_write(&output, (const unsigned char *)"abc", 3) == MMAY_IO_OK);
    MMAY_EXPECT(stream.output_length == 3 && memcmp(stream.output, "abc", 3) == 0);
    MMAY_EXPECT(mmay_io_flush(&output) == MMAY_IO_OK);
    stream.fail_flush = true;
    MMAY_EXPECT(mmay_io_flush(&output) == MMAY_IO_FLUSH_FAILED);
    output.capability_granted = false;
    MMAY_EXPECT(mmay_io_write(&output, (const unsigned char *)"x", 1) == MMAY_IO_CAPABILITY_DENIED);
    MMAY_EXPECT(mmay_io_flush(&output) == MMAY_IO_CAPABILITY_DENIED);
}

static void mmay_test_runtime_io_partial_retry_and_failures(void) {
    static const unsigned char unterminated[] = {'x', '\0', 'y'};
    static const unsigned char empty_line[] = {'\n'};
    static const unsigned char empty_crlf[] = {'\r', '\n'};
    static const unsigned char exact_crlf[] = {'x', '\r', '\n'};
    static const unsigned char cr_at_eof[] = {'\r'};
    static const unsigned char too_long_at_zero[] = {'x', '\n'};
    MarimayRuntimeIoStream stream = {0};
    MarimayIoInput input = {true, &stream, mmay_test_runtime_io_read};
    MarimayIoOutput output = {
        true, &stream, mmay_test_runtime_io_write,
        mmay_test_runtime_io_flush
    };
    MarimayOwnedBytes line = {0};
    MarimayOwnedBytes bytes = {0};
    bool eof = false;

    stream.input = unterminated;
    stream.input_length = sizeof(unterminated);
    stream.read_retries = 3;
    MMAY_EXPECT(mmay_io_read_line(
        &input, mmay_system_allocator(), 3, &line, &eof
    ) == MMAY_IO_OK);
    MMAY_EXPECT(!eof && line.length == sizeof(unterminated));
    MMAY_EXPECT(memcmp(line.data, unterminated, sizeof(unterminated)) == 0);
    MMAY_EXPECT(stream.read_calls == sizeof(unterminated) + 4);
    mmay_owned_bytes_dispose(&line);

    stream = (MarimayRuntimeIoStream){0};
    input.context = &stream;
    output.context = &stream;
    stream.input = empty_line;
    stream.input_length = sizeof(empty_line);
    MMAY_EXPECT(mmay_io_read_line(
        &input, mmay_system_allocator(), 0, &line, &eof
    ) == MMAY_IO_OK);
    MMAY_EXPECT(!eof && line.length == 0);
    mmay_owned_bytes_dispose(&line);

    stream.input = empty_crlf;
    stream.input_length = sizeof(empty_crlf);
    stream.input_offset = 0;
    MMAY_EXPECT(mmay_io_read_line(
        &input, mmay_system_allocator(), 0, &line, &eof
    ) == MMAY_IO_OK);
    MMAY_EXPECT(!eof && line.length == 0);
    mmay_owned_bytes_dispose(&line);

    stream.input = exact_crlf;
    stream.input_length = sizeof(exact_crlf);
    stream.input_offset = 0;
    MMAY_EXPECT(mmay_io_read_line(
        &input, mmay_system_allocator(), 1, &line, &eof
    ) == MMAY_IO_OK);
    MMAY_EXPECT(!eof && line.length == 1 && line.data[0] == 'x');
    mmay_owned_bytes_dispose(&line);

    stream.input = cr_at_eof;
    stream.input_length = sizeof(cr_at_eof);
    stream.input_offset = 0;
    MMAY_EXPECT(mmay_io_read_line(
        &input, mmay_system_allocator(), 0, &line, &eof
    ) == MMAY_IO_LINE_TOO_LONG);

    stream.input = too_long_at_zero;
    stream.input_length = sizeof(too_long_at_zero);
    stream.input_offset = 0;
    MMAY_EXPECT(mmay_io_read_line(
        &input, mmay_system_allocator(), 0, &line, &eof
    ) == MMAY_IO_LINE_TOO_LONG);

    stream = (MarimayRuntimeIoStream){0};
    input.context = &stream;
    output.context = &stream;
    stream.write_chunk = 1;
    stream.write_retries = 2;
    MMAY_EXPECT(mmay_io_write(
        &output, (const unsigned char *)"partial", 7
    ) == MMAY_IO_OK);
    MMAY_EXPECT(stream.write_calls == 9);
    MMAY_EXPECT(stream.output_length == 7);
    stream.flush_retries = 2;
    MMAY_EXPECT(mmay_io_flush(&output) == MMAY_IO_OK);
    MMAY_EXPECT(stream.flush_calls == 3);

    stream.zero_write = true;
    MMAY_EXPECT(mmay_io_write(
        &output, (const unsigned char *)"x", 1
    ) == MMAY_IO_WRITE_FAILED);
    stream.zero_write = false;
    stream.write_retries = MMAY_IO_MAX_CONSECUTIVE_RETRIES + 1;
    MMAY_EXPECT(mmay_io_write(
        &output, (const unsigned char *)"x", 1
    ) == MMAY_IO_WRITE_FAILED);
    stream.flush_retries = MMAY_IO_MAX_CONSECUTIVE_RETRIES + 1;
    MMAY_EXPECT(mmay_io_flush(&output) == MMAY_IO_FLUSH_FAILED);

    stream = (MarimayRuntimeIoStream){0};
    input.context = &stream;
    stream.read_retries = MMAY_IO_MAX_CONSECUTIVE_RETRIES + 1;
    MMAY_EXPECT(mmay_io_read_line(
        &input, mmay_system_allocator(), 1, &line, &eof
    ) == MMAY_IO_READ_FAILED);
    input.capability_granted = false;
    MMAY_EXPECT(mmay_io_read_line(
        &input, mmay_system_allocator(), 1, &line, &eof
    ) == MMAY_IO_CAPABILITY_DENIED);
    MMAY_EXPECT(stream.read_calls == MMAY_IO_MAX_CONSECUTIVE_RETRIES + 1);
    mmay_owned_bytes_dispose(&line);

    MMAY_EXPECT(mmay_io_read_bytes(
        &input, mmay_system_allocator(), 1, &bytes
    ) == MMAY_IO_CAPABILITY_DENIED);
    input.capability_granted = true;
    stream.read_retries = MMAY_IO_MAX_CONSECUTIVE_RETRIES + 1;
    MMAY_EXPECT(mmay_io_read_bytes(
        &input, mmay_system_allocator(), 1, &bytes
    ) == MMAY_IO_READ_FAILED);
    MMAY_EXPECT(mmay_io_read_bytes(
        &input, mmay_system_allocator(), MMAY_IO_MAX_BYTES + 1, &bytes
    ) == MMAY_IO_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_io_read_bytes(NULL, mmay_system_allocator(), 1, &bytes)
        == MMAY_IO_INVALID_ARGUMENT);
}

static void mmay_test_runtime_io_stream_allocations(void) {
    static const unsigned char input_bytes[] = {'o', 'k', '\n'};
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 16; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayRuntimeIoStream stream = {0};
        MarimayIoInput input = {true, &stream, mmay_test_runtime_io_read};
        MarimayOwnedBytes line = {0};
        MarimayAllocator allocator;
        MarimayIoStatus status;
        stream.input = input_bytes;
        stream.input_length = sizeof(input_bytes);
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        status = mmay_io_read_line(&input, allocator, 8, &line, &(bool){false});
        if (status == MMAY_IO_OK) {
            reached_success = true;
            MMAY_EXPECT(line.length == 2 && memcmp(line.data, "ok", 2) == 0);
        } else {
            MMAY_EXPECT(status == MMAY_IO_ALLOCATION_FAILED);
        }
        mmay_owned_bytes_dispose(&line);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 16; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayRuntimeIoStream stream = {0};
        MarimayIoInput input = {true, &stream, mmay_test_runtime_io_read};
        MarimayOwnedBytes bytes = {0};
        MarimayAllocator allocator;
        MarimayIoStatus status;
        stream.input = input_bytes;
        stream.input_length = sizeof(input_bytes);
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        allocator = mmay_fail_allocator(&state);
        status = mmay_io_read_bytes(
            &input, allocator, sizeof(input_bytes), &bytes
        );
        if (status == MMAY_IO_OK) {
            reached_success = true;
            MMAY_EXPECT(bytes.length == sizeof(input_bytes));
            MMAY_EXPECT(memcmp(
                bytes.data, input_bytes, sizeof(input_bytes)
            ) == 0);
        } else {
            MMAY_EXPECT(status == MMAY_IO_ALLOCATION_FAILED);
        }
        mmay_owned_bytes_dispose(&bytes);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
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
    mmay_test_runtime_io_stream_contract();
    mmay_test_runtime_io_partial_retry_and_failures();
    mmay_test_runtime_io_stream_allocations();
    mmay_test_runtime_output_expectations();
    mmay_test_runtime_output_allocations();
    mmay_test_runtime_output_depth_limit();
}
