#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/path.h"
#include "ruda/base/stream.h"

typedef struct RudaMemoryReader {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
    size_t maximum_chunk;
    size_t calls;
    size_t failure_index;
    bool zero_success;
} RudaMemoryReader;

static RudaReadStatus ruda_memory_read(
    void *context,
    unsigned char *destination,
    size_t capacity,
    size_t *read_count
) {
    RudaMemoryReader *reader = context;
    size_t remaining = 0;
    size_t count = 0;
    if (reader->calls == reader->failure_index) {
        reader->calls += 1;
        *read_count = 0;
        return RUDA_READ_ERROR;
    }
    reader->calls += 1;
    if (reader->zero_success) {
        *read_count = 0;
        return RUDA_READ_OK;
    }
    if (reader->offset == reader->length) {
        *read_count = 0;
        return RUDA_READ_END;
    }
    remaining = reader->length - reader->offset;
    count = remaining < capacity ? remaining : capacity;
    if (count > reader->maximum_chunk) {
        count = reader->maximum_chunk;
    }
    memcpy(destination, reader->bytes + reader->offset, count);
    reader->offset += count;
    *read_count = count;
    return RUDA_READ_OK;
}

static RudaReadStream ruda_memory_stream(RudaMemoryReader *reader) {
    RudaReadStream stream;
    stream.context = reader;
    stream.read = ruda_memory_read;
    return stream;
}

static void ruda_expect_path(
    RudaPathFlavor flavor,
    const char *input,
    const char *expected,
    bool absolute
) {
    RudaPath path;
    RUDA_EXPECT(ruda_path_normalize(
        &path,
        ruda_system_allocator(),
        flavor,
        input,
        strlen(input)
    ) == RUDA_PATH_OK);
    RUDA_EXPECT(path.text.data != NULL);
    if (path.text.data != NULL) {
        RUDA_EXPECT(strcmp((const char *)path.text.data, expected) == 0);
    }
    RUDA_EXPECT(path.absolute == absolute);
    ruda_path_dispose(&path);
}

static void ruda_test_paths(void) {
    RudaPath path;
    static const char invalid_utf8[] = {'a', (char)0xff, '\0'};
    static const char embedded_zero[] = {'a', 0, 'b'};
    size_t failure_index = 0;
    bool reached_success = false;

    ruda_expect_path(RUDA_PATH_POSIX, "", ".", false);
    ruda_expect_path(RUDA_PATH_POSIX, "a//b/./c/../d", "a/b/d", false);
    ruda_expect_path(RUDA_PATH_POSIX, "/a/../b", "/b", true);
    ruda_expect_path(RUDA_PATH_POSIX, "../../a", "../../a", false);
    ruda_expect_path(RUDA_PATH_POSIX, "α/β/../γ", "α/γ", false);
    RUDA_EXPECT(ruda_path_normalize(
        &path,
        ruda_system_allocator(),
        RUDA_PATH_POSIX,
        "/../../a",
        strlen("/../../a")
    ) == RUDA_PATH_ESCAPES_ROOT);
    RUDA_EXPECT(path.text.data == NULL);
    RUDA_EXPECT(ruda_path_normalize(
        &path,
        ruda_system_allocator(),
        RUDA_PATH_POSIX,
        invalid_utf8,
        sizeof(invalid_utf8) - 1
    ) == RUDA_PATH_INVALID_UTF8);
    RUDA_EXPECT(ruda_path_normalize(
        &path,
        ruda_system_allocator(),
        RUDA_PATH_POSIX,
        embedded_zero,
        sizeof(embedded_zero)
    ) == RUDA_PATH_INVALID_CHARACTER);

    ruda_expect_path(
        RUDA_PATH_WINDOWS,
        "c:\\demo\\me\\..\\x",
        "C:/demo/x",
        true
    );
    ruda_expect_path(
        RUDA_PATH_WINDOWS,
        "\\\\server\\share\\a\\..\\b",
        "//server/share/b",
        true
    );
    ruda_expect_path(RUDA_PATH_WINDOWS, "\\a\\b", "/a/b", true);
    RUDA_EXPECT(ruda_path_normalize(
        &path,
        ruda_system_allocator(),
        RUDA_PATH_WINDOWS,
        "C:relative",
        strlen("C:relative")
    ) == RUDA_PATH_UNSUPPORTED_PREFIX);
    RUDA_EXPECT(ruda_path_normalize(
        &path,
        ruda_system_allocator(),
        RUDA_PATH_WINDOWS,
        "C:\\temp\\con.txt",
        strlen("C:\\temp\\con.txt")
    ) == RUDA_PATH_INVALID_CHARACTER);
    RUDA_EXPECT(ruda_path_normalize(
        &path,
        ruda_system_allocator(),
        RUDA_PATH_WINDOWS,
        "C:\\temp\\trailing. ",
        strlen("C:\\temp\\trailing. ")
    ) == RUDA_PATH_INVALID_CHARACTER);
    RUDA_EXPECT(ruda_path_normalize(
        &path,
        ruda_system_allocator(),
        RUDA_PATH_WINDOWS,
        "C:\\temp\\bad*name",
        strlen("C:\\temp\\bad*name")
    ) == RUDA_PATH_INVALID_CHARACTER);
    RUDA_EXPECT(ruda_path_normalize(
        &path,
        ruda_system_allocator(),
        RUDA_PATH_WINDOWS,
        "\\\\?\\C:\\raw",
        strlen("\\\\?\\C:\\raw")
    ) == RUDA_PATH_UNSUPPORTED_PREFIX);

    for (failure_index = 0; failure_index < 16; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaPathStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_path_normalize(
            &path,
            allocator,
            RUDA_PATH_POSIX,
            "a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p",
            strlen("a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p")
        );
        if (status == RUDA_PATH_OK) {
            reached_success = true;
            ruda_path_dispose(&path);
        } else {
            RUDA_EXPECT(status == RUDA_PATH_ALLOCATION_FAILED);
            RUDA_EXPECT(path.text.data == NULL);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_streams(void) {
    static const unsigned char small[] = "0123456789";
    unsigned char large[10000];
    RudaMemoryReader reader;
    RudaOwnedBytes output;
    size_t index = 0;
    bool reached_success = false;
    FILE *file = NULL;

    reader.bytes = small;
    reader.length = sizeof(small) - 1;
    reader.offset = 0;
    reader.maximum_chunk = 3;
    reader.calls = 0;
    reader.failure_index = SIZE_MAX;
    reader.zero_success = false;
    RUDA_EXPECT(ruda_read_all(
        ruda_memory_stream(&reader),
        ruda_system_allocator(),
        sizeof(small) - 1,
        &output
    ) == RUDA_READ_ALL_OK);
    RUDA_EXPECT(output.length == sizeof(small) - 1);
    RUDA_EXPECT(memcmp(output.data, small, output.length) == 0);
    ruda_owned_bytes_dispose(&output);

    reader.offset = 0;
    reader.calls = 0;
    RUDA_EXPECT(ruda_read_all(
        ruda_memory_stream(&reader),
        ruda_system_allocator(),
        sizeof(small) - 2,
        &output
    ) == RUDA_READ_ALL_LIMIT_EXCEEDED);
    RUDA_EXPECT(output.data == NULL);

    reader.offset = 0;
    reader.calls = 0;
    reader.failure_index = 1;
    RUDA_EXPECT(ruda_read_all(
        ruda_memory_stream(&reader),
        ruda_system_allocator(),
        SIZE_MAX,
        &output
    ) == RUDA_READ_ALL_IO_ERROR);
    RUDA_EXPECT(output.data == NULL);

    reader.failure_index = SIZE_MAX;
    reader.zero_success = true;
    reader.calls = 0;
    RUDA_EXPECT(ruda_read_all(
        ruda_memory_stream(&reader),
        ruda_system_allocator(),
        SIZE_MAX,
        &output
    ) == RUDA_READ_ALL_IO_ERROR);
    reader.zero_success = false;

    for (index = 0; index < sizeof(large); index += 1) {
        large[index] = (unsigned char)(index & 0xffU);
    }
    for (index = 0; index < 32; index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaReadAllStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), index);
        allocator = ruda_fail_allocator(&state);
        reader.bytes = large;
        reader.length = sizeof(large);
        reader.offset = 0;
        reader.maximum_chunk = 137;
        reader.calls = 0;
        reader.failure_index = SIZE_MAX;
        reader.zero_success = false;
        status = ruda_read_all(
            ruda_memory_stream(&reader),
            allocator,
            sizeof(large),
            &output
        );
        if (status == RUDA_READ_ALL_OK) {
            reached_success = true;
            RUDA_EXPECT(output.length == sizeof(large));
            RUDA_EXPECT(memcmp(output.data, large, sizeof(large)) == 0);
            ruda_owned_bytes_dispose(&output);
        } else {
            RUDA_EXPECT(status == RUDA_READ_ALL_ALLOCATION_FAILED);
            RUDA_EXPECT(output.data == NULL);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);

    file = tmpfile();
    RUDA_EXPECT(file != NULL);
    if (file != NULL) {
        RUDA_EXPECT(fwrite(small, 1, sizeof(small) - 1, file) == sizeof(small) - 1);
        rewind(file);
        RUDA_EXPECT(ruda_read_all(
            ruda_file_read_stream(file),
            ruda_system_allocator(),
            100,
            &output
        ) == RUDA_READ_ALL_OK);
        RUDA_EXPECT(output.length == sizeof(small) - 1);
        ruda_owned_bytes_dispose(&output);
        RUDA_EXPECT(fclose(file) == 0);
    }
}

void ruda_test_path_stream(void) {
    ruda_test_paths();
    ruda_test_streams();
}
