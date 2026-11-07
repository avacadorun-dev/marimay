#include "test_support.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static int mmay_failures = 0;
static const char *mmay_executable = NULL;

FILE *mmay_test_open_file(const char *path, const char *mode) {
#if defined(_MSC_VER)
    FILE *file = NULL;
    return fopen_s(&file, path, mode) == 0 ? file : NULL;
#else
    return fopen(path, mode);
#endif
}

FILE *mmay_test_temporary_file(void) {
#if defined(_MSC_VER)
    static size_t next_file = 0;
    size_t attempt = 0;
    /* tmpfile_s writes to the drive root. Each test owner already has a
       private writable working directory; x prevents replacing any file and
       D binds deletion to the last stream/child handle closing. */
    for (attempt = 0; attempt < 64 && next_file != SIZE_MAX; attempt += 1) {
        char path[64];
        FILE *file = NULL;
        int written = snprintf(path, sizeof(path), "mmay-test-stream-%zu.tmp", next_file);
        errno_t status;
        next_file += 1;
        if (written < 0 || (size_t)written >= sizeof(path)) {
            return NULL;
        }
        status = fopen_s(&file, path, "w+bxD");
        if (status == 0) {
            return file;
        }
        if (status != EEXIST) {
            return NULL;
        }
    }
    return NULL;
#else
    return tmpfile();
#endif
}

static bool mmay_test_buffer_write(void *context, const char *bytes, size_t length) {
    MarimayTestBuffer *buffer = context;
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

void mmay_test_buffer_init(MarimayTestBuffer *buffer) {
    buffer->bytes[0] = '\0';
    buffer->length = 0;
    buffer->calls = 0;
    buffer->failure_index = SIZE_MAX;
}

MarimayWriter mmay_test_buffer_writer(MarimayTestBuffer *buffer) {
    MarimayWriter writer;
    writer.context = buffer;
    writer.write = mmay_test_buffer_write;
    writer.flush = NULL;
    return writer;
}

void mmay_test_fail(const char *file, int line, const char *expression) {
    mmay_failures += 1;
    fprintf(stderr, "%s:%d: expectation failed: %s\n", file, line, expression);
}

void mmay_test_expect_snapshot(
    const char *file,
    int line,
    const char *snapshot,
    const char *actual
) {
    char path[256];
    int path_length = snprintf(path, sizeof(path), "snapshots/%s", snapshot);
    if (path_length < 0 || (size_t)path_length >= sizeof(path)) {
        mmay_test_fail(file, line, "snapshot path fits buffer");
        return;
    }
    mmay_test_expect_file(file, line, path, actual);
}

void mmay_test_expect_file(
    const char *file,
    int line,
    const char *expected_path,
    const char *actual
) {
    char expected[16384];
    FILE *input = NULL;
    size_t length = 0;
    input = mmay_test_open_file(expected_path, "rb");
    if (input == NULL) {
        mmay_test_fail(file, line, "expected file opens");
        return;
    }
    length = fread(expected, 1, sizeof(expected) - 1, input);
    if (ferror(input) != 0 || !feof(input)) {
        fclose(input);
        mmay_test_fail(file, line, "snapshot read completes");
        return;
    }
    if (fclose(input) != 0) {
        mmay_test_fail(file, line, "snapshot closes");
        return;
    }
    expected[length] = '\0';
    if (strcmp(expected, actual) != 0) {
        mmay_failures += 1;
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

int mmay_test_failure_count(void) {
    return mmay_failures;
}

void mmay_test_set_toolchain_executable(const char *path) {
    mmay_executable = path;
}

const char *mmay_test_toolchain_executable(void) {
    return mmay_executable;
}
