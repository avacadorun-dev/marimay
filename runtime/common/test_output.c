#include "mmay/runtime/test_output.h"

#include <stdint.h>
#include <string.h>

typedef struct MarimayTestOutputFrame {
    MarimayOwnedBytes expected;
    MarimayVector actual;
} MarimayTestOutputFrame;

enum {
    MMAY_TEST_OUTPUT_MAX_DEPTH = 64,
    MMAY_TEST_OUTPUT_MAX_CAPTURED_BYTES = 1024 * 1024,
    MMAY_TEST_OUTPUT_DIFF_BYTES = 32
};

static void mmay_test_output_frame_dispose(MarimayTestOutputFrame *frame) {
    if (frame == NULL) return;
    mmay_owned_bytes_dispose(&frame->expected);
    mmay_vector_dispose(&frame->actual);
    memset(frame, 0, sizeof(*frame));
}

static bool mmay_test_output_append(
    MarimayVector *bytes,
    const void *data,
    size_t length
) {
    return length == 0 || mmay_vector_append(bytes, data, length);
}

static bool mmay_test_output_append_cstr(
    MarimayVector *bytes,
    const char *text
) {
    return mmay_test_output_append(bytes, text, strlen(text));
}

static bool mmay_test_output_append_decimal(
    MarimayVector *bytes,
    size_t value
) {
    unsigned char reversed[3 * sizeof(size_t)];
    unsigned char digit = 0;
    size_t count = 0;
    do {
        digit = (unsigned char)('0' + value % 10);
        reversed[count++] = digit;
        value /= 10;
    } while (value != 0);
    while (count != 0) {
        if (!mmay_vector_push(bytes, &reversed[--count])) return false;
    }
    return true;
}

static bool mmay_test_output_append_hex(
    MarimayVector *bytes,
    const unsigned char *data,
    size_t length
) {
    static const unsigned char digits[] = "0123456789abcdef";
    size_t shown = length < MMAY_TEST_OUTPUT_DIFF_BYTES
        ? length : MMAY_TEST_OUTPUT_DIFF_BYTES;
    size_t index = 0;
    unsigned char encoded[2];
    if (!mmay_test_output_append_cstr(bytes, "0x")) return false;
    for (index = 0; index < shown; index += 1) {
        encoded[0] = digits[data[index] >> 4];
        encoded[1] = digits[data[index] & 0x0fU];
        if (!mmay_test_output_append(bytes, encoded, sizeof(encoded))) {
            return false;
        }
    }
    return shown == length || mmay_test_output_append_cstr(bytes, "...");
}

static bool mmay_test_output_failure_build(
    MarimayTestOutput *output,
    const MarimayTestOutputFrame *frame
) {
    MarimayVector message;
    bool initialized = mmay_vector_init(
        &message, output->allocator, sizeof(unsigned char)
    );
    bool accepted = initialized
        && mmay_test_output_append_cstr(
            &message, "expect.output mismatch: expected "
        )
        && mmay_test_output_append_decimal(
            &message, frame->expected.length
        )
        && mmay_test_output_append_cstr(&message, " bytes ")
        && mmay_test_output_append_hex(
            &message, frame->expected.data, frame->expected.length
        )
        && mmay_test_output_append_cstr(&message, ", captured ")
        && mmay_test_output_append_decimal(&message, frame->actual.length)
        && mmay_test_output_append_cstr(&message, " bytes ")
        && mmay_test_output_append_hex(
            &message, frame->actual.data, frame->actual.length
        );
    if (accepted) {
        accepted = mmay_owned_bytes_copy(
            &output->failure, output->allocator,
            message.data, message.length
        );
    }
    if (initialized) mmay_vector_dispose(&message);
    return accepted;
}

MarimayTestOutputStatus mmay_test_output_begin(
    MarimayTestOutput *output,
    MarimayAllocator allocator,
    const unsigned char *expected,
    size_t expected_length
) {
    MarimayTestOutputFrame frame;
    if (output == NULL || allocator.allocate == NULL
        || (expected == NULL && expected_length != 0)) {
        return MMAY_TEST_OUTPUT_INVALID_ARGUMENT;
    }
    if (!output->initialized) {
        memset(output, 0, sizeof(*output));
        output->allocator = allocator;
        output->failure.allocator = allocator;
        if (!mmay_vector_init(
                &output->frames, allocator, sizeof(MarimayTestOutputFrame)
            )) {
            return MMAY_TEST_OUTPUT_ALLOCATION_FAILED;
        }
        output->initialized = true;
    } else if (output->allocator.context != allocator.context
        || output->allocator.allocate != allocator.allocate
        || output->allocator.resize != allocator.resize
        || output->allocator.deallocate != allocator.deallocate) {
        return MMAY_TEST_OUTPUT_INVALID_ARGUMENT;
    }
    if (output->frames.length >= MMAY_TEST_OUTPUT_MAX_DEPTH
        || expected_length > MMAY_TEST_OUTPUT_MAX_CAPTURED_BYTES) {
        return MMAY_TEST_OUTPUT_LIMIT_REACHED;
    }
    memset(&frame, 0, sizeof(frame));
    if (!mmay_owned_bytes_copy(
            &frame.expected, allocator, expected, expected_length
        ) || !mmay_vector_init(
            &frame.actual, allocator, sizeof(unsigned char)
        ) || !mmay_vector_push(&output->frames, &frame)) {
        mmay_test_output_frame_dispose(&frame);
        return MMAY_TEST_OUTPUT_ALLOCATION_FAILED;
    }
    memset(&frame, 0, sizeof(frame));
    return MMAY_TEST_OUTPUT_OK;
}

MarimayTestOutputStatus mmay_test_output_publish(
    MarimayTestOutput *output,
    const MarimayIoStdout *stdout_sink,
    const unsigned char *line,
    size_t line_length
) {
    MarimayTestOutputFrame *frame = NULL;
    MarimayIoStdoutStatus io_status;
    if (line == NULL || line_length == 0) {
        return MMAY_TEST_OUTPUT_INVALID_ARGUMENT;
    }
    if (output != NULL && output->initialized
        && output->frames.length != 0) {
        frame = mmay_vector_at(
            &output->frames, output->frames.length - 1
        );
        if (frame == NULL) return MMAY_TEST_OUTPUT_INVALID_ARGUMENT;
        if (frame->actual.length > MMAY_TEST_OUTPUT_MAX_CAPTURED_BYTES
            || line_length > MMAY_TEST_OUTPUT_MAX_CAPTURED_BYTES
                - frame->actual.length) {
            return MMAY_TEST_OUTPUT_LIMIT_REACHED;
        }
        if (!mmay_vector_append(&frame->actual, line, line_length)) {
            return MMAY_TEST_OUTPUT_ALLOCATION_FAILED;
        }
        output->captured_bytes += line_length;
        return MMAY_TEST_OUTPUT_OK;
    }
    io_status = mmay_io_stdout_publish_line(stdout_sink, line, line_length);
    if (io_status == MMAY_IO_STDOUT_OK) return MMAY_TEST_OUTPUT_OK;
    if (io_status == MMAY_IO_STDOUT_CAPABILITY_DENIED) {
        return MMAY_TEST_OUTPUT_CAPABILITY_DENIED;
    }
    if (io_status == MMAY_IO_STDOUT_WRITE_FAILED) {
        return MMAY_TEST_OUTPUT_WRITE_FAILED;
    }
    return MMAY_TEST_OUTPUT_INVALID_ARGUMENT;
}

MarimayTestOutputStatus mmay_test_output_end(MarimayTestOutput *output) {
    MarimayTestOutputFrame *frame = NULL;
    bool matches = false;
    bool failure_ready = false;
    if (output == NULL || !output->initialized
        || output->frames.length == 0) {
        return MMAY_TEST_OUTPUT_INVALID_ARGUMENT;
    }
    frame = mmay_vector_at(&output->frames, output->frames.length - 1);
    if (frame == NULL) return MMAY_TEST_OUTPUT_INVALID_ARGUMENT;
    matches = frame->expected.length == frame->actual.length
        && (frame->expected.length == 0 || memcmp(
            frame->expected.data, frame->actual.data,
            frame->expected.length
        ) == 0);
    mmay_owned_bytes_dispose(&output->failure);
    if (!matches) failure_ready = mmay_test_output_failure_build(output, frame);
    if (output->captured_bytes >= frame->actual.length) {
        output->captured_bytes -= frame->actual.length;
    } else {
        output->captured_bytes = 0;
    }
    mmay_test_output_frame_dispose(frame);
    output->frames.length -= 1;
    if (matches) return MMAY_TEST_OUTPUT_OK;
    return failure_ready
        ? MMAY_TEST_OUTPUT_MISMATCH
        : MMAY_TEST_OUTPUT_ALLOCATION_FAILED;
}

size_t mmay_test_output_depth(const MarimayTestOutput *output) {
    return output != NULL && output->initialized ? output->frames.length : 0;
}

const MarimayOwnedBytes *mmay_test_output_failure(const MarimayTestOutput *output) {
    return output != NULL && output->initialized ? &output->failure : NULL;
}

void mmay_test_output_dispose(MarimayTestOutput *output) {
    if (output == NULL) return;
    if (output->initialized) {
        while (output->frames.length != 0) {
            MarimayTestOutputFrame *frame = mmay_vector_at(
                &output->frames, output->frames.length - 1
            );
            mmay_test_output_frame_dispose(frame);
            output->frames.length -= 1;
        }
        mmay_vector_dispose(&output->frames);
        mmay_owned_bytes_dispose(&output->failure);
    }
    memset(output, 0, sizeof(*output));
}
