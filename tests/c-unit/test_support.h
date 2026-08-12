#ifndef RUDA_TEST_SUPPORT_H
#define RUDA_TEST_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/writer.h"

typedef struct RudaTestBuffer {
    char bytes[16384];
    size_t length;
    size_t calls;
    size_t failure_index;
} RudaTestBuffer;

void ruda_test_buffer_init(RudaTestBuffer *buffer);
RudaWriter ruda_test_buffer_writer(RudaTestBuffer *buffer);

void ruda_test_fail(const char *file, int line, const char *expression);
void ruda_test_expect_snapshot(
    const char *file,
    int line,
    const char *snapshot,
    const char *actual
);
void ruda_test_expect_file(
    const char *file,
    int line,
    const char *expected_path,
    const char *actual
);
int ruda_test_failure_count(void);
void ruda_test_set_toolchain_executable(const char *path);
const char *ruda_test_toolchain_executable(void);

#define RUDA_EXPECT(condition) \
    do { \
        if (!(condition)) { \
            ruda_test_fail(__FILE__, __LINE__, #condition); \
        } \
    } while (0)

#define RUDA_EXPECT_SNAPSHOT(snapshot, actual) \
    ruda_test_expect_snapshot(__FILE__, __LINE__, (snapshot), (actual))

#define RUDA_EXPECT_FILE(expected_path, actual) \
    ruda_test_expect_file(__FILE__, __LINE__, (expected_path), (actual))

#endif
