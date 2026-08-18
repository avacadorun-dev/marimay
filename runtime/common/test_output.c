#include "ruda/runtime/test_output.h"

#include <stdint.h>
#include <string.h>

typedef struct RudaTestOutputFrame {
    RudaOwnedBytes expected;
    RudaVector actual;
} RudaTestOutputFrame;

enum {
    RUDA_TEST_OUTPUT_MAX_DEPTH = 64,
    RUDA_TEST_OUTPUT_MAX_CAPTURED_BYTES = 1024 * 1024,
    RUDA_TEST_OUTPUT_DIFF_BYTES = 32
};

static void ruda_test_output_frame_dispose(RudaTestOutputFrame *frame) {
    if (frame == NULL) return;
    ruda_owned_bytes_dispose(&frame->expected);
    ruda_vector_dispose(&frame->actual);
    memset(frame, 0, sizeof(*frame));
}

static bool ruda_test_output_append(
    RudaVector *bytes,
    const void *data,
    size_t length
) {
    return length == 0 || ruda_vector_append(bytes, data, length);
}

static bool ruda_test_output_append_cstr(
    RudaVector *bytes,
    const char *text
) {
    return ruda_test_output_append(bytes, text, strlen(text));
}

static bool ruda_test_output_append_decimal(
    RudaVector *bytes,
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
        if (!ruda_vector_push(bytes, &reversed[--count])) return false;
    }
    return true;
}

static bool ruda_test_output_append_hex(
    RudaVector *bytes,
    const unsigned char *data,
    size_t length
) {
    static const unsigned char digits[] = "0123456789abcdef";
    size_t shown = length < RUDA_TEST_OUTPUT_DIFF_BYTES
        ? length : RUDA_TEST_OUTPUT_DIFF_BYTES;
    size_t index = 0;
    unsigned char encoded[2];
    if (!ruda_test_output_append_cstr(bytes, "0x")) return false;
    for (index = 0; index < shown; index += 1) {
        encoded[0] = digits[data[index] >> 4];
        encoded[1] = digits[data[index] & 0x0fU];
        if (!ruda_test_output_append(bytes, encoded, sizeof(encoded))) {
            return false;
        }
    }
    return shown == length || ruda_test_output_append_cstr(bytes, "...");
}

static bool ruda_test_output_failure_build(
    RudaTestOutput *output,
    const RudaTestOutputFrame *frame
) {
    RudaVector message;
    bool initialized = ruda_vector_init(
        &message, output->allocator, sizeof(unsigned char)
    );
    bool accepted = initialized
        && ruda_test_output_append_cstr(
            &message, "expect.output mismatch: expected "
        )
        && ruda_test_output_append_decimal(
            &message, frame->expected.length
        )
        && ruda_test_output_append_cstr(&message, " bytes ")
        && ruda_test_output_append_hex(
            &message, frame->expected.data, frame->expected.length
        )
        && ruda_test_output_append_cstr(&message, ", captured ")
        && ruda_test_output_append_decimal(&message, frame->actual.length)
        && ruda_test_output_append_cstr(&message, " bytes ")
        && ruda_test_output_append_hex(
            &message, frame->actual.data, frame->actual.length
        );
    if (accepted) {
        accepted = ruda_owned_bytes_copy(
            &output->failure, output->allocator,
            message.data, message.length
        );
    }
    if (initialized) ruda_vector_dispose(&message);
    return accepted;
}

RudaTestOutputStatus ruda_test_output_begin(
    RudaTestOutput *output,
    RudaAllocator allocator,
    const unsigned char *expected,
    size_t expected_length
) {
    RudaTestOutputFrame frame;
    if (output == NULL || allocator.allocate == NULL
        || (expected == NULL && expected_length != 0)) {
        return RUDA_TEST_OUTPUT_INVALID_ARGUMENT;
    }
    if (!output->initialized) {
        memset(output, 0, sizeof(*output));
        output->allocator = allocator;
        output->failure.allocator = allocator;
        if (!ruda_vector_init(
                &output->frames, allocator, sizeof(RudaTestOutputFrame)
            )) {
            return RUDA_TEST_OUTPUT_ALLOCATION_FAILED;
        }
        output->initialized = true;
    } else if (output->allocator.context != allocator.context
        || output->allocator.allocate != allocator.allocate
        || output->allocator.resize != allocator.resize
        || output->allocator.deallocate != allocator.deallocate) {
        return RUDA_TEST_OUTPUT_INVALID_ARGUMENT;
    }
    if (output->frames.length >= RUDA_TEST_OUTPUT_MAX_DEPTH
        || expected_length > RUDA_TEST_OUTPUT_MAX_CAPTURED_BYTES) {
        return RUDA_TEST_OUTPUT_LIMIT_REACHED;
    }
    memset(&frame, 0, sizeof(frame));
    if (!ruda_owned_bytes_copy(
            &frame.expected, allocator, expected, expected_length
        ) || !ruda_vector_init(
            &frame.actual, allocator, sizeof(unsigned char)
        ) || !ruda_vector_push(&output->frames, &frame)) {
        ruda_test_output_frame_dispose(&frame);
        return RUDA_TEST_OUTPUT_ALLOCATION_FAILED;
    }
    memset(&frame, 0, sizeof(frame));
    return RUDA_TEST_OUTPUT_OK;
}

RudaTestOutputStatus ruda_test_output_publish(
    RudaTestOutput *output,
    const RudaIoStdout *stdout_sink,
    const unsigned char *line,
    size_t line_length
) {
    RudaTestOutputFrame *frame = NULL;
    RudaIoStdoutStatus io_status;
    if (line == NULL || line_length == 0) {
        return RUDA_TEST_OUTPUT_INVALID_ARGUMENT;
    }
    if (output != NULL && output->initialized
        && output->frames.length != 0) {
        frame = ruda_vector_at(
            &output->frames, output->frames.length - 1
        );
        if (frame == NULL) return RUDA_TEST_OUTPUT_INVALID_ARGUMENT;
        if (frame->actual.length > RUDA_TEST_OUTPUT_MAX_CAPTURED_BYTES
            || line_length > RUDA_TEST_OUTPUT_MAX_CAPTURED_BYTES
                - frame->actual.length) {
            return RUDA_TEST_OUTPUT_LIMIT_REACHED;
        }
        if (!ruda_vector_append(&frame->actual, line, line_length)) {
            return RUDA_TEST_OUTPUT_ALLOCATION_FAILED;
        }
        output->captured_bytes += line_length;
        return RUDA_TEST_OUTPUT_OK;
    }
    io_status = ruda_io_stdout_publish_line(stdout_sink, line, line_length);
    if (io_status == RUDA_IO_STDOUT_OK) return RUDA_TEST_OUTPUT_OK;
    if (io_status == RUDA_IO_STDOUT_CAPABILITY_DENIED) {
        return RUDA_TEST_OUTPUT_CAPABILITY_DENIED;
    }
    if (io_status == RUDA_IO_STDOUT_WRITE_FAILED) {
        return RUDA_TEST_OUTPUT_WRITE_FAILED;
    }
    return RUDA_TEST_OUTPUT_INVALID_ARGUMENT;
}

RudaTestOutputStatus ruda_test_output_end(RudaTestOutput *output) {
    RudaTestOutputFrame *frame = NULL;
    bool matches = false;
    bool failure_ready = false;
    if (output == NULL || !output->initialized
        || output->frames.length == 0) {
        return RUDA_TEST_OUTPUT_INVALID_ARGUMENT;
    }
    frame = ruda_vector_at(&output->frames, output->frames.length - 1);
    if (frame == NULL) return RUDA_TEST_OUTPUT_INVALID_ARGUMENT;
    matches = frame->expected.length == frame->actual.length
        && (frame->expected.length == 0 || memcmp(
            frame->expected.data, frame->actual.data,
            frame->expected.length
        ) == 0);
    ruda_owned_bytes_dispose(&output->failure);
    if (!matches) failure_ready = ruda_test_output_failure_build(output, frame);
    if (output->captured_bytes >= frame->actual.length) {
        output->captured_bytes -= frame->actual.length;
    } else {
        output->captured_bytes = 0;
    }
    ruda_test_output_frame_dispose(frame);
    output->frames.length -= 1;
    if (matches) return RUDA_TEST_OUTPUT_OK;
    return failure_ready
        ? RUDA_TEST_OUTPUT_MISMATCH
        : RUDA_TEST_OUTPUT_ALLOCATION_FAILED;
}

size_t ruda_test_output_depth(const RudaTestOutput *output) {
    return output != NULL && output->initialized ? output->frames.length : 0;
}

const RudaOwnedBytes *ruda_test_output_failure(const RudaTestOutput *output) {
    return output != NULL && output->initialized ? &output->failure : NULL;
}

void ruda_test_output_dispose(RudaTestOutput *output) {
    if (output == NULL) return;
    if (output->initialized) {
        while (output->frames.length != 0) {
            RudaTestOutputFrame *frame = ruda_vector_at(
                &output->frames, output->frames.length - 1
            );
            ruda_test_output_frame_dispose(frame);
            output->frames.length -= 1;
        }
        ruda_vector_dispose(&output->frames);
        ruda_owned_bytes_dispose(&output->failure);
    }
    memset(output, 0, sizeof(*output));
}
