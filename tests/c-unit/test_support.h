#ifndef MMAY_TEST_SUPPORT_H
#define MMAY_TEST_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/writer.h"

typedef struct MarimayTestBuffer {
    char bytes[16384];
    size_t length;
    size_t calls;
    size_t failure_index;
} MarimayTestBuffer;

void mmay_test_buffer_init(MarimayTestBuffer *buffer);
MarimayWriter mmay_test_buffer_writer(MarimayTestBuffer *buffer);

void mmay_test_fail(const char *file, int line, const char *expression);
void mmay_test_expect_snapshot(
    const char *file,
    int line,
    const char *snapshot,
    const char *actual
);
void mmay_test_expect_file(
    const char *file,
    int line,
    const char *expected_path,
    const char *actual
);
int mmay_test_failure_count(void);
void mmay_test_set_toolchain_executable(const char *path);
const char *mmay_test_toolchain_executable(void);

#define MMAY_EXPECT(condition) \
    do { \
        if (!(condition)) { \
            mmay_test_fail(__FILE__, __LINE__, #condition); \
        } \
    } while (0)

#define MMAY_EXPECT_SNAPSHOT(snapshot, actual) \
    mmay_test_expect_snapshot(__FILE__, __LINE__, (snapshot), (actual))

#define MMAY_EXPECT_FILE(expected_path, actual) \
    mmay_test_expect_file(__FILE__, __LINE__, (expected_path), (actual))

#endif
