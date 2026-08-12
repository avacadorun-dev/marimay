#include "test_support.h"

#include <stdio.h>
#include <string.h>

static int ruda_failures = 0;
static const char *ruda_executable = NULL;

static bool ruda_test_buffer_write(void *context, const char *bytes, size_t length) {
    RudaTestBuffer *buffer = context;
    if (buffer->calls == buffer->failure_index) {
        buffer->calls += 1;
        return false;
    }
    buffer->calls += 1;
    if (length > sizeof(buffer->bytes) - buffer->length - 1) {
        return false;
    }
    memcpy(buffer->bytes + buffer->length, bytes, length);
    buffer->length += length;
    buffer->bytes[buffer->length] = '\0';
    return true;
}

void ruda_test_buffer_init(RudaTestBuffer *buffer) {
    buffer->bytes[0] = '\0';
    buffer->length = 0;
    buffer->calls = 0;
    buffer->failure_index = SIZE_MAX;
}

RudaWriter ruda_test_buffer_writer(RudaTestBuffer *buffer) {
    RudaWriter writer;
    writer.context = buffer;
    writer.write = ruda_test_buffer_write;
    return writer;
}

void ruda_test_fail(const char *file, int line, const char *expression) {
    ruda_failures += 1;
    fprintf(stderr, "%s:%d: expectation failed: %s\n", file, line, expression);
}

void ruda_test_expect_snapshot(
    const char *file,
    int line,
    const char *snapshot,
    const char *actual
) {
    char path[256];
    int path_length = snprintf(path, sizeof(path), "snapshots/%s", snapshot);
    if (path_length < 0 || (size_t)path_length >= sizeof(path)) {
        ruda_test_fail(file, line, "snapshot path fits buffer");
        return;
    }
    ruda_test_expect_file(file, line, path, actual);
}

void ruda_test_expect_file(
    const char *file,
    int line,
    const char *expected_path,
    const char *actual
) {
    char expected[16384];
    FILE *input = NULL;
    size_t length = 0;
    input = fopen(expected_path, "rb");
    if (input == NULL) {
        ruda_test_fail(file, line, "expected file opens");
        return;
    }
    length = fread(expected, 1, sizeof(expected) - 1, input);
    if (ferror(input) != 0 || !feof(input)) {
        fclose(input);
        ruda_test_fail(file, line, "snapshot read completes");
        return;
    }
    if (fclose(input) != 0) {
        ruda_test_fail(file, line, "snapshot closes");
        return;
    }
    expected[length] = '\0';
    if (strcmp(expected, actual) != 0) {
        ruda_failures += 1;
        fprintf(
            stderr,
            "%s:%d: snapshot %s differs\n--- expected ---\n%s--- actual ---\n%s",
            file,
            line,
            expected_path,
            expected,
            actual
        );
    }
}

int ruda_test_failure_count(void) {
    return ruda_failures;
}

void ruda_test_set_toolchain_executable(const char *path) {
    ruda_executable = path;
}

const char *ruda_test_toolchain_executable(void) {
    return ruda_executable;
}
