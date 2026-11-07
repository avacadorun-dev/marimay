#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/stat.h>
#endif

#include "mmay/base/allocator.h"
#include "mmay/platform/file.h"

typedef struct MarimayFakeFile {
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
} MarimayFakeFile;

typedef struct MarimayFakeReader {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
    size_t calls;
    size_t failure_index;
    size_t maximum_read;
    bool open;
    bool zero_read;
} MarimayFakeReader;

static MarimayFileStatus mmay_fake_reader_open(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayFileHandle *handle
) {
    MarimayFakeReader *reader = context;
    (void)allocator;
    (void)path;
    (void)path_length;
    if (reader->calls == reader->failure_index) {
        reader->calls += 1;
        return MMAY_FILE_IO_FAILED;
    }
    reader->calls += 1;
    reader->open = true;
    *handle = 1;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_fake_reader_read(
    void *context,
    MarimayFileHandle handle,
    unsigned char *bytes,
    size_t capacity,
    size_t *read_count,
    bool *end
) {
    MarimayFakeReader *reader = context;
    size_t count = reader->length - reader->offset;
    (void)handle;
    if (reader->calls == reader->failure_index) {
        reader->calls += 1;
        return MMAY_FILE_IO_FAILED;
    }
    reader->calls += 1;
    if (reader->zero_read) {
        *read_count = 0;
        *end = false;
        return MMAY_FILE_OK;
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
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_fake_reader_close(void *context, MarimayFileHandle handle) {
    MarimayFakeReader *reader = context;
    bool fail = reader->calls == reader->failure_index;
    (void)handle;
    reader->calls += 1;
    reader->open = false;
    return fail ? MMAY_FILE_IO_FAILED : MMAY_FILE_OK;
}

static MarimayFileReadOps mmay_fake_reader_ops(MarimayFakeReader *reader) {
    MarimayFileReadOps ops;
    ops.context = reader;
    ops.open = mmay_fake_reader_open;
    ops.read = mmay_fake_reader_read;
    ops.close = mmay_fake_reader_close;
    return ops;
}

static void mmay_fake_reader_init(
    MarimayFakeReader *reader,
    const unsigned char *bytes,
    size_t length
) {
    memset(reader, 0, sizeof(*reader));
    reader->bytes = bytes;
    reader->length = length;
    reader->failure_index = SIZE_MAX;
    reader->maximum_read = 3;
}

static bool mmay_fake_file_fails(MarimayFakeFile *file) {
    bool fail = file->calls == file->failure_index;
    file->calls += 1;
    return fail;
}

static MarimayFileStatus mmay_fake_file_create(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayFileHandle *handle
) {
    MarimayFakeFile *file = context;
    (void)allocator;
    (void)path;
    (void)path_length;
    if (mmay_fake_file_fails(file)) {
        return MMAY_FILE_IO_FAILED;
    }
    if (file->created) {
        return MMAY_FILE_ALREADY_EXISTS;
    }
    file->created = true;
    file->open = true;
    file->staged_length = 0;
    *handle = 1;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_fake_file_write(
    void *context,
    MarimayFileHandle handle,
    const unsigned char *bytes,
    size_t length,
    size_t *written
) {
    MarimayFakeFile *file = context;
    size_t count = length;
    (void)handle;
    if (mmay_fake_file_fails(file)) {
        return MMAY_FILE_IO_FAILED;
    }
    if (file->zero_write) {
        *written = 0;
        return MMAY_FILE_OK;
    }
    if (count > file->maximum_write) {
        count = file->maximum_write;
    }
    if (!file->open || count > sizeof(file->staged) - file->staged_length) {
        return MMAY_FILE_IO_FAILED;
    }
    memcpy(file->staged + file->staged_length, bytes, count);
    file->staged_length += count;
    *written = count;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_fake_file_sync(void *context, MarimayFileHandle handle) {
    MarimayFakeFile *file = context;
    (void)handle;
    if (mmay_fake_file_fails(file) || !file->open) {
        return MMAY_FILE_IO_FAILED;
    }
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_fake_file_close(void *context, MarimayFileHandle handle) {
    MarimayFakeFile *file = context;
    bool fail = mmay_fake_file_fails(file);
    (void)handle;
    file->open = false;
    return fail ? MMAY_FILE_IO_FAILED : MMAY_FILE_OK;
}

static MarimayFileStatus mmay_fake_file_replace(
    void *context,
    MarimayAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
) {
    MarimayFakeFile *file = context;
    (void)allocator;
    (void)temporary_path;
    (void)temporary_path_length;
    (void)destination_path;
    (void)destination_path_length;
    if (mmay_fake_file_fails(file)) {
        return MMAY_FILE_IO_FAILED;
    }
    memcpy(file->destination, file->staged, file->staged_length);
    file->destination_length = file->staged_length;
    file->created = false;
    file->replaced = true;
    return MMAY_FILE_OK;
}

static void mmay_fake_file_remove(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length
) {
    MarimayFakeFile *file = context;
    (void)allocator;
    (void)path;
    (void)path_length;
    file->created = false;
    file->removed = true;
}

static MarimayFileOps mmay_fake_file_ops(MarimayFakeFile *file) {
    MarimayFileOps ops;
    ops.context = file;
    ops.create_exclusive = mmay_fake_file_create;
    ops.write = mmay_fake_file_write;
    ops.sync = mmay_fake_file_sync;
    ops.close = mmay_fake_file_close;
    ops.replace = mmay_fake_file_replace;
    ops.remove = mmay_fake_file_remove;
    return ops;
}

static void mmay_fake_file_init(MarimayFakeFile *file, size_t failure_index) {
    static const unsigned char original[] = "old";
    memset(file, 0, sizeof(*file));
    file->failure_index = failure_index;
    file->maximum_write = 3;
    memcpy(file->destination, original, sizeof(original) - 1);
    file->destination_length = sizeof(original) - 1;
}

static void mmay_test_file_failure_atomicity(void) {
    static const unsigned char replacement[] = "new-data";
    static const unsigned char original[] = "old";
    size_t failure_index = 0;

    for (failure_index = 0; failure_index < 7; failure_index += 1) {
        MarimayFakeFile file;
        MarimayFileStatus status;
        mmay_fake_file_init(&file, failure_index);
        status = mmay_file_replace_atomic_with_ops(
            mmay_fake_file_ops(&file),
            mmay_system_allocator(),
            "out.tmp",
            strlen("out.tmp"),
            "out.my",
            strlen("out.my"),
            replacement,
            sizeof(replacement) - 1
        );
        MMAY_EXPECT(status == MMAY_FILE_IO_FAILED);
        MMAY_EXPECT(!file.open);
        MMAY_EXPECT(!file.created);
        MMAY_EXPECT(!file.replaced);
        MMAY_EXPECT(file.destination_length == sizeof(original) - 1);
        MMAY_EXPECT(memcmp(file.destination, original, sizeof(original) - 1) == 0);
        if (failure_index != 0) {
            MMAY_EXPECT(file.removed);
        }
    }

    {
        MarimayFakeFile file;
        mmay_fake_file_init(&file, SIZE_MAX);
        MMAY_EXPECT(mmay_file_replace_atomic_with_ops(
            mmay_fake_file_ops(&file),
            mmay_system_allocator(),
            "out.tmp",
            strlen("out.tmp"),
            "out.my",
            strlen("out.my"),
            replacement,
            sizeof(replacement) - 1
        ) == MMAY_FILE_OK);
        MMAY_EXPECT(file.replaced);
        MMAY_EXPECT(!file.removed);
        MMAY_EXPECT(file.destination_length == sizeof(replacement) - 1);
        MMAY_EXPECT(memcmp(
            file.destination,
            replacement,
            sizeof(replacement) - 1
        ) == 0);
    }

    {
        MarimayFakeFile file;
        mmay_fake_file_init(&file, SIZE_MAX);
        file.zero_write = true;
        MMAY_EXPECT(mmay_file_replace_atomic_with_ops(
            mmay_fake_file_ops(&file),
            mmay_system_allocator(),
            "out.tmp",
            strlen("out.tmp"),
            "out.my",
            strlen("out.my"),
            replacement,
            sizeof(replacement) - 1
        ) == MMAY_FILE_IO_FAILED);
        MMAY_EXPECT(file.removed);
        MMAY_EXPECT(!file.replaced);
    }
}

static void mmay_test_file_validation(void) {
    static const unsigned char bytes[] = "x";
    static const char invalid_utf8[] = "x\xff";
    MarimayFakeFile file;
    MarimayFileOps ops;
    mmay_fake_file_init(&file, SIZE_MAX);
    ops = mmay_fake_file_ops(&file);

    MMAY_EXPECT(mmay_file_replace_atomic_with_ops(
        ops,
        mmay_system_allocator(),
        "same.my",
        strlen("same.my"),
        "same.my",
        strlen("same.my"),
        bytes,
        sizeof(bytes) - 1
    ) == MMAY_FILE_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_file_replace_atomic_with_ops(
        ops,
        mmay_system_allocator(),
        "one/out.tmp",
        strlen("one/out.tmp"),
        "two/out.my",
        strlen("two/out.my"),
        bytes,
        sizeof(bytes) - 1
    ) == MMAY_FILE_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_file_replace_atomic_with_ops(
        ops,
        mmay_system_allocator(),
        invalid_utf8,
        sizeof(invalid_utf8) - 1,
        "out.my",
        strlen("out.my"),
        bytes,
        sizeof(bytes) - 1
    ) == MMAY_FILE_INVALID_UTF8);
    MMAY_EXPECT(mmay_file_replace_atomic_with_ops(
        ops,
        mmay_system_allocator(),
        "out.tmp",
        strlen("out.tmp"),
        "out.my",
        strlen("out.my"),
        NULL,
        1
    ) == MMAY_FILE_INVALID_ARGUMENT);
    ops.write = NULL;
    MMAY_EXPECT(mmay_file_replace_atomic_with_ops(
        ops,
        mmay_system_allocator(),
        "out.tmp",
        strlen("out.tmp"),
        "out.my",
        strlen("out.my"),
        bytes,
        sizeof(bytes) - 1
    ) == MMAY_FILE_INVALID_ARGUMENT);
    MMAY_EXPECT(file.calls == 0);
}

static void mmay_test_file_read_all(void) {
    static const unsigned char source[] = {'a', 0, 'b', 'c', 'd'};
    MarimayFakeReader reader;
    MarimayOwnedBytes output = {0};
    size_t failure_index = 0;
    mmay_fake_reader_init(&reader, source, sizeof(source));
    MMAY_EXPECT(mmay_file_read_all_with_ops(
        mmay_fake_reader_ops(&reader),
        mmay_system_allocator(),
        "input.my",
        strlen("input.my"),
        sizeof(source),
        &output
    ) == MMAY_FILE_OK);
    MMAY_EXPECT(output.length == sizeof(source));
    MMAY_EXPECT(memcmp(output.data, source, sizeof(source)) == 0);
    MMAY_EXPECT(!reader.open);
    mmay_owned_bytes_dispose(&output);

    mmay_fake_reader_init(&reader, source, sizeof(source));
    MMAY_EXPECT(mmay_file_read_all_with_ops(
        mmay_fake_reader_ops(&reader),
        mmay_system_allocator(),
        "input.my",
        strlen("input.my"),
        sizeof(source) - 1,
        &output
    ) == MMAY_FILE_LIMIT_EXCEEDED);
    MMAY_EXPECT(output.data == NULL);
    MMAY_EXPECT(!reader.open);

    mmay_fake_reader_init(&reader, source, sizeof(source));
    reader.zero_read = true;
    MMAY_EXPECT(mmay_file_read_all_with_ops(
        mmay_fake_reader_ops(&reader),
        mmay_system_allocator(),
        "input.my",
        strlen("input.my"),
        sizeof(source),
        &output
    ) == MMAY_FILE_IO_FAILED);
    MMAY_EXPECT(!reader.open);

    for (failure_index = 0; failure_index < 4; failure_index += 1) {
        mmay_fake_reader_init(&reader, source, sizeof(source));
        reader.failure_index = failure_index;
        MMAY_EXPECT(mmay_file_read_all_with_ops(
            mmay_fake_reader_ops(&reader),
            mmay_system_allocator(),
            "input.my",
            strlen("input.my"),
            sizeof(source),
            &output
        ) == MMAY_FILE_IO_FAILED);
        MMAY_EXPECT(output.data == NULL);
        MMAY_EXPECT(!reader.open);
    }

    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayFileStatus status;
        mmay_fake_reader_init(&reader, source, sizeof(source));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        status = mmay_file_read_all_with_ops(
            mmay_fake_reader_ops(&reader),
            allocator,
            "input.my",
            strlen("input.my"),
            sizeof(source),
            &output
        );
        if (status == MMAY_FILE_OK) {
            mmay_owned_bytes_dispose(&output);
        } else {
            MMAY_EXPECT(status == MMAY_FILE_ALLOCATION_FAILED);
            MMAY_EXPECT(output.data == NULL);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (status == MMAY_FILE_OK) {
            break;
        }
    }
}

static bool mmay_test_write_file(const char *path, const unsigned char *bytes, size_t length) {
    FILE *file = mmay_test_open_file(path, "wb");
    bool ok = file != NULL;
    if (ok) {
        ok = fwrite(bytes, 1, length, file) == length;
        ok = fclose(file) == 0 && ok;
    }
    return ok;
}

static void mmay_test_real_file_adapter(void) {
    static const char temporary_path[] = "mmay-atomic-α.tmp";
    static const char destination_path[] = "mmay-atomic-α.my";
    static const unsigned char original[] = "original";
    static const unsigned char replacement[] = {'n', 'e', 'w', 0, 'd', 'a', 't', 'a'};
    unsigned char observed[32];
    size_t observed_length = 0;
    FILE *file = NULL;
    size_t failure_index = 0;
    bool reached_success = false;

    (void)remove(temporary_path);
    (void)remove(destination_path);
    MMAY_EXPECT(mmay_test_write_file(
        destination_path,
        original,
        sizeof(original) - 1
    ));
#ifndef _WIN32
    MMAY_EXPECT(chmod(destination_path, 0640) == 0);
#endif

    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayFileStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        status = mmay_file_replace_atomic(
            allocator,
            temporary_path,
            strlen(temporary_path),
            destination_path,
            strlen(destination_path),
            replacement,
            sizeof(replacement)
        );
        if (status == MMAY_FILE_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_FILE_ALLOCATION_FAILED);
            file = mmay_test_open_file(destination_path, "rb");
            MMAY_EXPECT(file != NULL);
            if (file != NULL) {
                observed_length = fread(observed, 1, sizeof(observed), file);
                MMAY_EXPECT(fclose(file) == 0);
                MMAY_EXPECT(observed_length == sizeof(original) - 1);
                MMAY_EXPECT(memcmp(observed, original, sizeof(original) - 1) == 0);
            }
            file = mmay_test_open_file(temporary_path, "rb");
            MMAY_EXPECT(file == NULL);
            if (file != NULL) {
                (void)fclose(file);
            }
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);

    file = mmay_test_open_file(destination_path, "rb");
    MMAY_EXPECT(file != NULL);
    if (file != NULL) {
        observed_length = fread(observed, 1, sizeof(observed), file);
        MMAY_EXPECT(fclose(file) == 0);
        MMAY_EXPECT(observed_length == sizeof(replacement));
        MMAY_EXPECT(memcmp(observed, replacement, sizeof(replacement)) == 0);
    }
#ifndef _WIN32
    {
        struct stat status;
        MMAY_EXPECT(stat(destination_path, &status) == 0);
        MMAY_EXPECT((status.st_mode & 0777) == 0640);
    }
#endif
    file = mmay_test_open_file(temporary_path, "rb");
    MMAY_EXPECT(file == NULL);
    if (file != NULL) {
        (void)fclose(file);
    }

    MMAY_EXPECT(mmay_test_write_file(temporary_path, original, sizeof(original) - 1));
    MMAY_EXPECT(mmay_file_replace_atomic(
        mmay_system_allocator(),
        temporary_path,
        strlen(temporary_path),
        destination_path,
        strlen(destination_path),
        replacement,
        sizeof(replacement)
    ) == MMAY_FILE_ALREADY_EXISTS);
    (void)remove(temporary_path);
    (void)remove(destination_path);
}

static void mmay_test_executable_publication(void) {
    static const char temporary[] = "mmay-executable.tmp";
    static const char destination[] = "mmay-executable.out";
    static const unsigned char original[] = "old";
    static const unsigned char replacement[] = "new executable";
    size_t failure_index = 0;
    bool reached_success = false;
    (void)remove(temporary);
    (void)remove(destination);
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayFileStatus status;
        unsigned char observed[32];
        size_t observed_length = 0;
        FILE *file = NULL;
        MMAY_EXPECT(mmay_test_write_file(
            temporary, replacement, sizeof(replacement) - 1
        ));
        MMAY_EXPECT(mmay_test_write_file(
            destination, original, sizeof(original) - 1
        ));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_file_replace_executable(
            mmay_fail_allocator(&state),
            temporary, sizeof(temporary) - 1,
            destination, sizeof(destination) - 1
        );
        if (status == MMAY_FILE_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_FILE_ALLOCATION_FAILED);
            file = mmay_test_open_file(destination, "rb");
            MMAY_EXPECT(file != NULL);
            if (file != NULL) {
                observed_length = fread(
                    observed, 1, sizeof(observed), file
                );
                MMAY_EXPECT(fclose(file) == 0);
                MMAY_EXPECT(observed_length == sizeof(original) - 1);
                MMAY_EXPECT(memcmp(
                    observed, original, sizeof(original) - 1
                ) == 0);
            }
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        (void)remove(temporary);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
#if !defined(_WIN32)
    {
        struct stat information;
        MMAY_EXPECT(stat(destination, &information) == 0);
        MMAY_EXPECT((information.st_mode & 0111) != 0);
    }
#endif
    MMAY_EXPECT(mmay_file_replace_executable(
        mmay_system_allocator(), NULL, 0,
        destination, sizeof(destination) - 1
    ) == MMAY_FILE_INVALID_ARGUMENT);
    (void)remove(temporary);
    MMAY_EXPECT(remove(destination) == 0);
}

void mmay_test_file(void) {
    mmay_test_file_failure_atomicity();
    mmay_test_file_validation();
    mmay_test_file_read_all();
    mmay_test_real_file_adapter();
    mmay_test_executable_publication();
}
