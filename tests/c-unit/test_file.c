#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/stat.h>
#endif

#include "ruda/base/allocator.h"
#include "ruda/platform/file.h"

typedef struct RudaFakeFile {
    size_t calls;
    size_t failure_index;
    size_t maximum_write;
    bool zero_write;
    bool created;
    bool open;
    bool removed;
    bool replaced;
    unsigned char staged[64];
    size_t staged_length;
    unsigned char destination[64];
    size_t destination_length;
} RudaFakeFile;

typedef struct RudaFakeReader {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
    size_t calls;
    size_t failure_index;
    size_t maximum_read;
    bool open;
    bool zero_read;
} RudaFakeReader;

static RudaFileStatus ruda_fake_reader_open(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaFileHandle *handle
) {
    RudaFakeReader *reader = context;
    (void)allocator;
    (void)path;
    (void)path_length;
    if (reader->calls == reader->failure_index) {
        reader->calls += 1;
        return RUDA_FILE_IO_FAILED;
    }
    reader->calls += 1;
    reader->open = true;
    *handle = 1;
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_fake_reader_read(
    void *context,
    RudaFileHandle handle,
    unsigned char *bytes,
    size_t capacity,
    size_t *read_count,
    bool *end
) {
    RudaFakeReader *reader = context;
    size_t count = reader->length - reader->offset;
    (void)handle;
    if (reader->calls == reader->failure_index) {
        reader->calls += 1;
        return RUDA_FILE_IO_FAILED;
    }
    reader->calls += 1;
    if (reader->zero_read) {
        *read_count = 0;
        *end = false;
        return RUDA_FILE_OK;
    }
    if (count > capacity) {
        count = capacity;
    }
    if (count > reader->maximum_read) {
        count = reader->maximum_read;
    }
    if (count != 0) {
        memcpy(bytes, reader->bytes + reader->offset, count);
        reader->offset += count;
    }
    *read_count = count;
    *end = count == 0;
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_fake_reader_close(void *context, RudaFileHandle handle) {
    RudaFakeReader *reader = context;
    bool fail = reader->calls == reader->failure_index;
    (void)handle;
    reader->calls += 1;
    reader->open = false;
    return fail ? RUDA_FILE_IO_FAILED : RUDA_FILE_OK;
}

static RudaFileReadOps ruda_fake_reader_ops(RudaFakeReader *reader) {
    RudaFileReadOps ops;
    ops.context = reader;
    ops.open = ruda_fake_reader_open;
    ops.read = ruda_fake_reader_read;
    ops.close = ruda_fake_reader_close;
    return ops;
}

static void ruda_fake_reader_init(
    RudaFakeReader *reader,
    const unsigned char *bytes,
    size_t length
) {
    memset(reader, 0, sizeof(*reader));
    reader->bytes = bytes;
    reader->length = length;
    reader->failure_index = SIZE_MAX;
    reader->maximum_read = 3;
}

static bool ruda_fake_file_fails(RudaFakeFile *file) {
    bool fail = file->calls == file->failure_index;
    file->calls += 1;
    return fail;
}

static RudaFileStatus ruda_fake_file_create(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaFileHandle *handle
) {
    RudaFakeFile *file = context;
    (void)allocator;
    (void)path;
    (void)path_length;
    if (ruda_fake_file_fails(file)) {
        return RUDA_FILE_IO_FAILED;
    }
    if (file->created) {
        return RUDA_FILE_ALREADY_EXISTS;
    }
    file->created = true;
    file->open = true;
    file->staged_length = 0;
    *handle = 1;
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_fake_file_write(
    void *context,
    RudaFileHandle handle,
    const unsigned char *bytes,
    size_t length,
    size_t *written
) {
    RudaFakeFile *file = context;
    size_t count = length;
    (void)handle;
    if (ruda_fake_file_fails(file)) {
        return RUDA_FILE_IO_FAILED;
    }
    if (file->zero_write) {
        *written = 0;
        return RUDA_FILE_OK;
    }
    if (count > file->maximum_write) {
        count = file->maximum_write;
    }
    if (!file->open || count > sizeof(file->staged) - file->staged_length) {
        return RUDA_FILE_IO_FAILED;
    }
    memcpy(file->staged + file->staged_length, bytes, count);
    file->staged_length += count;
    *written = count;
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_fake_file_sync(void *context, RudaFileHandle handle) {
    RudaFakeFile *file = context;
    (void)handle;
    if (ruda_fake_file_fails(file) || !file->open) {
        return RUDA_FILE_IO_FAILED;
    }
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_fake_file_close(void *context, RudaFileHandle handle) {
    RudaFakeFile *file = context;
    bool fail = ruda_fake_file_fails(file);
    (void)handle;
    file->open = false;
    return fail ? RUDA_FILE_IO_FAILED : RUDA_FILE_OK;
}

static RudaFileStatus ruda_fake_file_replace(
    void *context,
    RudaAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
) {
    RudaFakeFile *file = context;
    (void)allocator;
    (void)temporary_path;
    (void)temporary_path_length;
    (void)destination_path;
    (void)destination_path_length;
    if (ruda_fake_file_fails(file)) {
        return RUDA_FILE_IO_FAILED;
    }
    memcpy(file->destination, file->staged, file->staged_length);
    file->destination_length = file->staged_length;
    file->created = false;
    file->replaced = true;
    return RUDA_FILE_OK;
}

static void ruda_fake_file_remove(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length
) {
    RudaFakeFile *file = context;
    (void)allocator;
    (void)path;
    (void)path_length;
    file->created = false;
    file->removed = true;
}

static RudaFileOps ruda_fake_file_ops(RudaFakeFile *file) {
    RudaFileOps ops;
    ops.context = file;
    ops.create_exclusive = ruda_fake_file_create;
    ops.write = ruda_fake_file_write;
    ops.sync = ruda_fake_file_sync;
    ops.close = ruda_fake_file_close;
    ops.replace = ruda_fake_file_replace;
    ops.remove = ruda_fake_file_remove;
    return ops;
}

static void ruda_fake_file_init(RudaFakeFile *file, size_t failure_index) {
    static const unsigned char original[] = "old";
    memset(file, 0, sizeof(*file));
    file->failure_index = failure_index;
    file->maximum_write = 3;
    memcpy(file->destination, original, sizeof(original) - 1);
    file->destination_length = sizeof(original) - 1;
}

static void ruda_test_file_failure_atomicity(void) {
    static const unsigned char replacement[] = "new-data";
    static const unsigned char original[] = "old";
    size_t failure_index = 0;

    for (failure_index = 0; failure_index < 7; failure_index += 1) {
        RudaFakeFile file;
        RudaFileStatus status;
        ruda_fake_file_init(&file, failure_index);
        status = ruda_file_replace_atomic_with_ops(
            ruda_fake_file_ops(&file),
            ruda_system_allocator(),
            "out.tmp",
            strlen("out.tmp"),
            "out.ruda",
            strlen("out.ruda"),
            replacement,
            sizeof(replacement) - 1
        );
        RUDA_EXPECT(status == RUDA_FILE_IO_FAILED);
        RUDA_EXPECT(!file.open);
        RUDA_EXPECT(!file.created);
        RUDA_EXPECT(!file.replaced);
        RUDA_EXPECT(file.destination_length == sizeof(original) - 1);
        RUDA_EXPECT(memcmp(file.destination, original, sizeof(original) - 1) == 0);
        if (failure_index != 0) {
            RUDA_EXPECT(file.removed);
        }
    }

    {
        RudaFakeFile file;
        ruda_fake_file_init(&file, SIZE_MAX);
        RUDA_EXPECT(ruda_file_replace_atomic_with_ops(
            ruda_fake_file_ops(&file),
            ruda_system_allocator(),
            "out.tmp",
            strlen("out.tmp"),
            "out.ruda",
            strlen("out.ruda"),
            replacement,
            sizeof(replacement) - 1
        ) == RUDA_FILE_OK);
        RUDA_EXPECT(file.replaced);
        RUDA_EXPECT(!file.removed);
        RUDA_EXPECT(file.destination_length == sizeof(replacement) - 1);
        RUDA_EXPECT(memcmp(
            file.destination,
            replacement,
            sizeof(replacement) - 1
        ) == 0);
    }

    {
        RudaFakeFile file;
        ruda_fake_file_init(&file, SIZE_MAX);
        file.zero_write = true;
        RUDA_EXPECT(ruda_file_replace_atomic_with_ops(
            ruda_fake_file_ops(&file),
            ruda_system_allocator(),
            "out.tmp",
            strlen("out.tmp"),
            "out.ruda",
            strlen("out.ruda"),
            replacement,
            sizeof(replacement) - 1
        ) == RUDA_FILE_IO_FAILED);
        RUDA_EXPECT(file.removed);
        RUDA_EXPECT(!file.replaced);
    }
}

static void ruda_test_file_validation(void) {
    static const unsigned char bytes[] = "x";
    static const char invalid_utf8[] = {'x', (char)0xff};
    RudaFakeFile file;
    RudaFileOps ops;
    ruda_fake_file_init(&file, SIZE_MAX);
    ops = ruda_fake_file_ops(&file);

    RUDA_EXPECT(ruda_file_replace_atomic_with_ops(
        ops,
        ruda_system_allocator(),
        "same.ruda",
        strlen("same.ruda"),
        "same.ruda",
        strlen("same.ruda"),
        bytes,
        sizeof(bytes) - 1
    ) == RUDA_FILE_INVALID_ARGUMENT);
    RUDA_EXPECT(ruda_file_replace_atomic_with_ops(
        ops,
        ruda_system_allocator(),
        "one/out.tmp",
        strlen("one/out.tmp"),
        "two/out.ruda",
        strlen("two/out.ruda"),
        bytes,
        sizeof(bytes) - 1
    ) == RUDA_FILE_INVALID_ARGUMENT);
    RUDA_EXPECT(ruda_file_replace_atomic_with_ops(
        ops,
        ruda_system_allocator(),
        invalid_utf8,
        sizeof(invalid_utf8),
        "out.ruda",
        strlen("out.ruda"),
        bytes,
        sizeof(bytes) - 1
    ) == RUDA_FILE_INVALID_UTF8);
    RUDA_EXPECT(ruda_file_replace_atomic_with_ops(
        ops,
        ruda_system_allocator(),
        "out.tmp",
        strlen("out.tmp"),
        "out.ruda",
        strlen("out.ruda"),
        NULL,
        1
    ) == RUDA_FILE_INVALID_ARGUMENT);
    ops.write = NULL;
    RUDA_EXPECT(ruda_file_replace_atomic_with_ops(
        ops,
        ruda_system_allocator(),
        "out.tmp",
        strlen("out.tmp"),
        "out.ruda",
        strlen("out.ruda"),
        bytes,
        sizeof(bytes) - 1
    ) == RUDA_FILE_INVALID_ARGUMENT);
    RUDA_EXPECT(file.calls == 0);
}

static void ruda_test_file_read_all(void) {
    static const unsigned char source[] = {'a', 0, 'b', 'c', 'd'};
    RudaFakeReader reader;
    RudaOwnedBytes output = {0};
    size_t failure_index = 0;
    ruda_fake_reader_init(&reader, source, sizeof(source));
    RUDA_EXPECT(ruda_file_read_all_with_ops(
        ruda_fake_reader_ops(&reader),
        ruda_system_allocator(),
        "input.ruda",
        strlen("input.ruda"),
        sizeof(source),
        &output
    ) == RUDA_FILE_OK);
    RUDA_EXPECT(output.length == sizeof(source));
    RUDA_EXPECT(memcmp(output.data, source, sizeof(source)) == 0);
    RUDA_EXPECT(!reader.open);
    ruda_owned_bytes_dispose(&output);

    ruda_fake_reader_init(&reader, source, sizeof(source));
    RUDA_EXPECT(ruda_file_read_all_with_ops(
        ruda_fake_reader_ops(&reader),
        ruda_system_allocator(),
        "input.ruda",
        strlen("input.ruda"),
        sizeof(source) - 1,
        &output
    ) == RUDA_FILE_LIMIT_EXCEEDED);
    RUDA_EXPECT(output.data == NULL);
    RUDA_EXPECT(!reader.open);

    ruda_fake_reader_init(&reader, source, sizeof(source));
    reader.zero_read = true;
    RUDA_EXPECT(ruda_file_read_all_with_ops(
        ruda_fake_reader_ops(&reader),
        ruda_system_allocator(),
        "input.ruda",
        strlen("input.ruda"),
        sizeof(source),
        &output
    ) == RUDA_FILE_IO_FAILED);
    RUDA_EXPECT(!reader.open);

    for (failure_index = 0; failure_index < 4; failure_index += 1) {
        ruda_fake_reader_init(&reader, source, sizeof(source));
        reader.failure_index = failure_index;
        RUDA_EXPECT(ruda_file_read_all_with_ops(
            ruda_fake_reader_ops(&reader),
            ruda_system_allocator(),
            "input.ruda",
            strlen("input.ruda"),
            sizeof(source),
            &output
        ) == RUDA_FILE_IO_FAILED);
        RUDA_EXPECT(output.data == NULL);
        RUDA_EXPECT(!reader.open);
    }

    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaFileStatus status;
        ruda_fake_reader_init(&reader, source, sizeof(source));
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_file_read_all_with_ops(
            ruda_fake_reader_ops(&reader),
            allocator,
            "input.ruda",
            strlen("input.ruda"),
            sizeof(source),
            &output
        );
        if (status == RUDA_FILE_OK) {
            ruda_owned_bytes_dispose(&output);
        } else {
            RUDA_EXPECT(status == RUDA_FILE_ALLOCATION_FAILED);
            RUDA_EXPECT(output.data == NULL);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (status == RUDA_FILE_OK) {
            break;
        }
    }
}

static bool ruda_test_write_file(const char *path, const unsigned char *bytes, size_t length) {
    FILE *file = fopen(path, "wb");
    bool ok = file != NULL;
    if (ok) {
        ok = fwrite(bytes, 1, length, file) == length;
        ok = fclose(file) == 0 && ok;
    }
    return ok;
}

static void ruda_test_real_file_adapter(void) {
    static const char temporary_path[] = "ruda-atomic-α.tmp";
    static const char destination_path[] = "ruda-atomic-α.ruda";
    static const unsigned char original[] = "original";
    static const unsigned char replacement[] = {'n', 'e', 'w', 0, 'd', 'a', 't', 'a'};
    unsigned char observed[32];
    size_t observed_length = 0;
    FILE *file = NULL;
    size_t failure_index = 0;
    bool reached_success = false;

    (void)remove(temporary_path);
    (void)remove(destination_path);
    RUDA_EXPECT(ruda_test_write_file(
        destination_path,
        original,
        sizeof(original) - 1
    ));
#ifndef _WIN32
    RUDA_EXPECT(chmod(destination_path, 0640) == 0);
#endif

    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaFileStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_file_replace_atomic(
            allocator,
            temporary_path,
            strlen(temporary_path),
            destination_path,
            strlen(destination_path),
            replacement,
            sizeof(replacement)
        );
        if (status == RUDA_FILE_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_FILE_ALLOCATION_FAILED);
            file = fopen(destination_path, "rb");
            RUDA_EXPECT(file != NULL);
            if (file != NULL) {
                observed_length = fread(observed, 1, sizeof(observed), file);
                RUDA_EXPECT(fclose(file) == 0);
                RUDA_EXPECT(observed_length == sizeof(original) - 1);
                RUDA_EXPECT(memcmp(observed, original, sizeof(original) - 1) == 0);
            }
            file = fopen(temporary_path, "rb");
            RUDA_EXPECT(file == NULL);
            if (file != NULL) {
                (void)fclose(file);
            }
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);

    file = fopen(destination_path, "rb");
    RUDA_EXPECT(file != NULL);
    if (file != NULL) {
        observed_length = fread(observed, 1, sizeof(observed), file);
        RUDA_EXPECT(fclose(file) == 0);
        RUDA_EXPECT(observed_length == sizeof(replacement));
        RUDA_EXPECT(memcmp(observed, replacement, sizeof(replacement)) == 0);
    }
#ifndef _WIN32
    {
        struct stat status;
        RUDA_EXPECT(stat(destination_path, &status) == 0);
        RUDA_EXPECT((status.st_mode & 0777) == 0640);
    }
#endif
    file = fopen(temporary_path, "rb");
    RUDA_EXPECT(file == NULL);
    if (file != NULL) {
        (void)fclose(file);
    }

    RUDA_EXPECT(ruda_test_write_file(temporary_path, original, sizeof(original) - 1));
    RUDA_EXPECT(ruda_file_replace_atomic(
        ruda_system_allocator(),
        temporary_path,
        strlen(temporary_path),
        destination_path,
        strlen(destination_path),
        replacement,
        sizeof(replacement)
    ) == RUDA_FILE_ALREADY_EXISTS);
    (void)remove(temporary_path);
    (void)remove(destination_path);
}

static void ruda_test_executable_publication(void) {
    static const char temporary[] = "ruda-executable.tmp";
    static const char destination[] = "ruda-executable.out";
    static const unsigned char original[] = "old";
    static const unsigned char replacement[] = "new executable";
    size_t failure_index = 0;
    bool reached_success = false;
    (void)remove(temporary);
    (void)remove(destination);
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        RudaFailAllocator state;
        RudaFileStatus status;
        unsigned char observed[32];
        size_t observed_length = 0;
        FILE *file = NULL;
        RUDA_EXPECT(ruda_test_write_file(
            temporary, replacement, sizeof(replacement) - 1
        ));
        RUDA_EXPECT(ruda_test_write_file(
            destination, original, sizeof(original) - 1
        ));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_file_replace_executable(
            ruda_fail_allocator(&state),
            temporary, sizeof(temporary) - 1,
            destination, sizeof(destination) - 1
        );
        if (status == RUDA_FILE_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_FILE_ALLOCATION_FAILED);
            file = fopen(destination, "rb");
            RUDA_EXPECT(file != NULL);
            if (file != NULL) {
                observed_length = fread(
                    observed, 1, sizeof(observed), file
                );
                RUDA_EXPECT(fclose(file) == 0);
                RUDA_EXPECT(observed_length == sizeof(original) - 1);
                RUDA_EXPECT(memcmp(
                    observed, original, sizeof(original) - 1
                ) == 0);
            }
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        (void)remove(temporary);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
#if !defined(_WIN32)
    {
        struct stat information;
        RUDA_EXPECT(stat(destination, &information) == 0);
        RUDA_EXPECT((information.st_mode & 0111) != 0);
    }
#endif
    RUDA_EXPECT(ruda_file_replace_executable(
        ruda_system_allocator(), NULL, 0,
        destination, sizeof(destination) - 1
    ) == RUDA_FILE_INVALID_ARGUMENT);
    (void)remove(temporary);
    RUDA_EXPECT(remove(destination) == 0);
}

void ruda_test_file(void) {
    ruda_test_file_failure_atomicity();
    ruda_test_file_validation();
    ruda_test_file_read_all();
    ruda_test_real_file_adapter();
    ruda_test_executable_publication();
}
