#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/base/path.h"
#include "mmay/base/stream.h"

typedef struct MarimayMemoryReader {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
    size_t maximum_chunk;
    size_t calls;
    size_t failure_index;
    bool zero_success;
} MarimayMemoryReader;

static MarimayReadStatus mmay_memory_read(
    void *context,
    unsigned char *destination,
    size_t capacity,
    size_t *read_count
) {
    MarimayMemoryReader *reader = context;
    size_t remaining = 0;
    size_t count = 0;
    if (reader->calls == reader->failure_index) {
        reader->calls += 1;
        *read_count = 0;
        return MMAY_READ_ERROR;
    }
    reader->calls += 1;
    if (reader->zero_success) {
        *read_count = 0;
        return MMAY_READ_OK;
    }
    if (reader->offset == reader->length) {
        *read_count = 0;
        return MMAY_READ_END;
    }
    remaining = reader->length - reader->offset;
    count = remaining < capacity ? remaining : capacity;
    if (count > reader->maximum_chunk) {
        count = reader->maximum_chunk;
    }
    memcpy(destination, reader->bytes + reader->offset, count);
    reader->offset += count;
    *read_count = count;
    return MMAY_READ_OK;
}

static MarimayReadStream mmay_memory_stream(MarimayMemoryReader *reader) {
    MarimayReadStream stream;
    stream.context = reader;
    stream.read = mmay_memory_read;
    return stream;
}

static void mmay_expect_path(
    MarimayPathFlavor flavor,
    const char *input,
    const char *expected,
    bool absolute
) {
    MarimayPath path;
    MMAY_EXPECT(mmay_path_normalize(
        &path,
        mmay_system_allocator(),
        flavor,
        input,
        strlen(input)
    ) == MMAY_PATH_OK);
    MMAY_EXPECT(path.text.data != NULL);
    if (path.text.data != NULL) {
        MMAY_EXPECT(strcmp((const char *)path.text.data, expected) == 0);
    }
    MMAY_EXPECT(path.absolute == absolute);
    mmay_path_dispose(&path);
}

static void mmay_test_paths(void) {
    MarimayPath path;
    static const char invalid_utf8[] = {'a', (char)0xff, '\0'};
    static const char embedded_zero[] = {'a', 0, 'b'};
    size_t failure_index = 0;
    bool reached_success = false;

    mmay_expect_path(MMAY_PATH_POSIX, "", ".", false);
    mmay_expect_path(MMAY_PATH_POSIX, "a//b/./c/../d", "a/b/d", false);
    mmay_expect_path(MMAY_PATH_POSIX, "/a/../b", "/b", true);
    mmay_expect_path(MMAY_PATH_POSIX, "../../a", "../../a", false);
    mmay_expect_path(MMAY_PATH_POSIX, "α/β/../γ", "α/γ", false);
    MMAY_EXPECT(mmay_path_normalize(
        &path,
        mmay_system_allocator(),
        MMAY_PATH_POSIX,
        "/../../a",
        strlen("/../../a")
    ) == MMAY_PATH_ESCAPES_ROOT);
    MMAY_EXPECT(path.text.data == NULL);
    MMAY_EXPECT(mmay_path_normalize(
        &path,
        mmay_system_allocator(),
        MMAY_PATH_POSIX,
        invalid_utf8,
        sizeof(invalid_utf8) - 1
    ) == MMAY_PATH_INVALID_UTF8);
    MMAY_EXPECT(mmay_path_normalize(
        &path,
        mmay_system_allocator(),
        MMAY_PATH_POSIX,
        embedded_zero,
        sizeof(embedded_zero)
    ) == MMAY_PATH_INVALID_CHARACTER);

    mmay_expect_path(
        MMAY_PATH_WINDOWS,
        "c:\\demo\\me\\..\\x",
        "C:/demo/x",
        true
    );
    mmay_expect_path(
        MMAY_PATH_WINDOWS,
        "\\\\server\\share\\a\\..\\b",
        "//server/share/b",
        true
    );
    mmay_expect_path(MMAY_PATH_WINDOWS, "\\a\\b", "/a/b", true);
    MMAY_EXPECT(mmay_path_normalize(
        &path,
        mmay_system_allocator(),
        MMAY_PATH_WINDOWS,
        "C:relative",
        strlen("C:relative")
    ) == MMAY_PATH_UNSUPPORTED_PREFIX);
    MMAY_EXPECT(mmay_path_normalize(
        &path,
        mmay_system_allocator(),
        MMAY_PATH_WINDOWS,
        "C:\\temp\\con.txt",
        strlen("C:\\temp\\con.txt")
    ) == MMAY_PATH_INVALID_CHARACTER);
    MMAY_EXPECT(mmay_path_normalize(
        &path,
        mmay_system_allocator(),
        MMAY_PATH_WINDOWS,
        "C:\\temp\\trailing. ",
        strlen("C:\\temp\\trailing. ")
    ) == MMAY_PATH_INVALID_CHARACTER);
    MMAY_EXPECT(mmay_path_normalize(
        &path,
        mmay_system_allocator(),
        MMAY_PATH_WINDOWS,
        "C:\\temp\\bad*name",
        strlen("C:\\temp\\bad*name")
    ) == MMAY_PATH_INVALID_CHARACTER);
    MMAY_EXPECT(mmay_path_normalize(
        &path,
        mmay_system_allocator(),
        MMAY_PATH_WINDOWS,
        "\\\\?\\C:\\raw",
        strlen("\\\\?\\C:\\raw")
    ) == MMAY_PATH_UNSUPPORTED_PREFIX);

    for (failure_index = 0; failure_index < 16; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayPathStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        status = mmay_path_normalize(
            &path,
            allocator,
            MMAY_PATH_POSIX,
            "a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p",
            strlen("a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p")
        );
        if (status == MMAY_PATH_OK) {
            reached_success = true;
            mmay_path_dispose(&path);
        } else {
            MMAY_EXPECT(status == MMAY_PATH_ALLOCATION_FAILED);
            MMAY_EXPECT(path.text.data == NULL);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_streams(void) {
    static const unsigned char small[] = "0123456789";
    unsigned char large[10000];
    MarimayMemoryReader reader;
    MarimayOwnedBytes output;
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
    MMAY_EXPECT(mmay_read_all(
        mmay_memory_stream(&reader),
        mmay_system_allocator(),
        sizeof(small) - 1,
        &output
    ) == MMAY_READ_ALL_OK);
    MMAY_EXPECT(output.length == sizeof(small) - 1);
    MMAY_EXPECT(memcmp(output.data, small, output.length) == 0);
    mmay_owned_bytes_dispose(&output);

    reader.offset = 0;
    reader.calls = 0;
    MMAY_EXPECT(mmay_read_all(
        mmay_memory_stream(&reader),
        mmay_system_allocator(),
        sizeof(small) - 2,
        &output
    ) == MMAY_READ_ALL_LIMIT_EXCEEDED);
    MMAY_EXPECT(output.data == NULL);

    reader.offset = 0;
    reader.calls = 0;
    reader.failure_index = 1;
    MMAY_EXPECT(mmay_read_all(
        mmay_memory_stream(&reader),
        mmay_system_allocator(),
        SIZE_MAX,
        &output
    ) == MMAY_READ_ALL_IO_ERROR);
    MMAY_EXPECT(output.data == NULL);

    reader.failure_index = SIZE_MAX;
    reader.zero_success = true;
    reader.calls = 0;
    MMAY_EXPECT(mmay_read_all(
        mmay_memory_stream(&reader),
        mmay_system_allocator(),
        SIZE_MAX,
        &output
    ) == MMAY_READ_ALL_IO_ERROR);
    reader.zero_success = false;

    for (index = 0; index < sizeof(large); index += 1) {
        large[index] = (unsigned char)(index & 0xffU);
    }
    for (index = 0; index < 32; index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayReadAllStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        allocator = mmay_fail_allocator(&state);
        reader.bytes = large;
        reader.length = sizeof(large);
        reader.offset = 0;
        reader.maximum_chunk = 137;
        reader.calls = 0;
        reader.failure_index = SIZE_MAX;
        reader.zero_success = false;
        status = mmay_read_all(
            mmay_memory_stream(&reader),
            allocator,
            sizeof(large),
            &output
        );
        if (status == MMAY_READ_ALL_OK) {
            reached_success = true;
            MMAY_EXPECT(output.length == sizeof(large));
            MMAY_EXPECT(memcmp(output.data, large, sizeof(large)) == 0);
            mmay_owned_bytes_dispose(&output);
        } else {
            MMAY_EXPECT(status == MMAY_READ_ALL_ALLOCATION_FAILED);
            MMAY_EXPECT(output.data == NULL);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);

    file = tmpfile();
    MMAY_EXPECT(file != NULL);
    if (file != NULL) {
        MMAY_EXPECT(fwrite(small, 1, sizeof(small) - 1, file) == sizeof(small) - 1);
        rewind(file);
        MMAY_EXPECT(mmay_read_all(
            mmay_file_read_stream(file),
            mmay_system_allocator(),
            100,
            &output
        ) == MMAY_READ_ALL_OK);
        MMAY_EXPECT(output.length == sizeof(small) - 1);
        mmay_owned_bytes_dispose(&output);
        MMAY_EXPECT(fclose(file) == 0);
    }
}

void mmay_test_path_stream(void) {
    mmay_test_paths();
    mmay_test_streams();
}
