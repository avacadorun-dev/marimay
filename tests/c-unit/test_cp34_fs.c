#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/fs.h"

typedef enum MarimayCp34WritePhase {
    MMAY_CP34_WRITE_NONE,
    MMAY_CP34_WRITE_CREATE,
    MMAY_CP34_WRITE_DATA,
    MMAY_CP34_WRITE_SYNC,
    MMAY_CP34_WRITE_CLOSE,
    MMAY_CP34_WRITE_REPLACE,
    MMAY_CP34_WRITE_COLLIDE
} MarimayCp34WritePhase;

typedef struct MarimayCp34Host {
    const unsigned char *read_bytes;
    size_t read_length;
    size_t read_offset;
    size_t maximum_read;
    size_t maximum_write;
    size_t kind_calls;
    size_t open_calls;
    size_t read_calls;
    size_t read_failure_call;
    size_t close_calls;
    size_t metadata_calls;
    size_t directory_open_calls;
    size_t directory_next_calls;
    size_t directory_close_calls;
    size_t directory_index;
    size_t invalid_utf8_index;
    bool read_open;
    bool read_zero_progress;
    bool read_close_failure;
    bool directory_close_failure;
    bool temporary_created;
    bool temporary_open;
    bool temporary_removed;
    bool destination_replaced;
    MarimayCp34WritePhase write_failure;
    unsigned char staged[128];
    size_t staged_length;
    unsigned char destination[128];
    size_t destination_length;
} MarimayCp34Host;

static bool mmay_cp34_path_is(
    const char *path, size_t length, const char *expected
) {
    size_t expected_length = strlen(expected);
    return length == expected_length
        && memcmp(path, expected, expected_length) == 0;
}

static MarimayFileStatus mmay_cp34_current(
    void *context, MarimayAllocator allocator, size_t maximum,
    MarimayOwnedBytes *path
) {
    static const char current[] = "/sandbox";
    (void)context;
    if (sizeof(current) - 1U > maximum) return MMAY_FILE_LIMIT_EXCEEDED;
    return mmay_owned_bytes_copy(
        path, allocator, current, sizeof(current) - 1U
    ) ? MMAY_FILE_OK : MMAY_FILE_ALLOCATION_FAILED;
}

static MarimayFileStatus mmay_cp34_kind(
    void *context, MarimayAllocator allocator,
    const char *path, size_t length, MarimayDirectoryEntryKind *kind
) {
    MarimayCp34Host *host = context;
    (void)allocator;
    host->kind_calls += 1U;
    if (mmay_cp34_path_is(path, length, "/")
        || mmay_cp34_path_is(path, length, "/sandbox")
        || mmay_cp34_path_is(path, length, "/sandbox/read")
        || mmay_cp34_path_is(path, length, "/sandbox/read/dir")
        || mmay_cp34_path_is(path, length, "/sandbox/write")) {
        *kind = MMAY_DIRECTORY_ENTRY_DIRECTORY;
    } else if (mmay_cp34_path_is(path, length, "/sandbox/read/file.txt")
        || mmay_cp34_path_is(path, length, "/sandbox/write/out.txt")) {
        *kind = MMAY_DIRECTORY_ENTRY_REGULAR;
    } else if (mmay_cp34_path_is(path, length, "/sandbox/read/link")) {
        *kind = MMAY_DIRECTORY_ENTRY_SYMLINK;
    } else {
        *kind = MMAY_DIRECTORY_ENTRY_MISSING;
    }
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_cp34_open_read(
    void *context, MarimayAllocator allocator,
    const char *path, size_t length, MarimayFileHandle *handle
) {
    MarimayCp34Host *host = context;
    (void)allocator;
    (void)path;
    (void)length;
    host->open_calls += 1U;
    host->read_open = true;
    host->read_offset = 0;
    *handle = 41U;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_cp34_read(
    void *context, MarimayFileHandle handle, unsigned char *bytes,
    size_t capacity, size_t *count, bool *end
) {
    MarimayCp34Host *host = context;
    size_t available = host->read_length - host->read_offset;
    size_t selected = available;
    (void)handle;
    if (host->read_calls++ == host->read_failure_call) {
        return MMAY_FILE_IO_FAILED;
    }
    if (host->read_zero_progress) {
        *count = 0;
        *end = false;
        return MMAY_FILE_OK;
    }
    if (selected > capacity) selected = capacity;
    if (selected > host->maximum_read) selected = host->maximum_read;
    if (selected != 0) {
        memcpy(bytes, host->read_bytes + host->read_offset, selected);
        host->read_offset += selected;
    }
    *count = selected;
    *end = selected == 0;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_cp34_read_close(
    void *context, MarimayFileHandle handle
) {
    MarimayCp34Host *host = context;
    (void)handle;
    host->close_calls += 1U;
    host->read_open = false;
    return host->read_close_failure
        ? MMAY_FILE_IO_FAILED : MMAY_FILE_OK;
}

static MarimayFileStatus mmay_cp34_create(
    void *context, MarimayAllocator allocator,
    const char *path, size_t length, MarimayFileHandle *handle
) {
    MarimayCp34Host *host = context;
    (void)allocator;
    (void)path;
    (void)length;
    if (host->write_failure == MMAY_CP34_WRITE_COLLIDE) {
        return MMAY_FILE_ALREADY_EXISTS;
    }
    if (host->write_failure == MMAY_CP34_WRITE_CREATE) {
        return MMAY_FILE_IO_FAILED;
    }
    host->temporary_created = true;
    host->temporary_open = true;
    host->staged_length = 0;
    *handle = 73U;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_cp34_write(
    void *context, MarimayFileHandle handle,
    const unsigned char *bytes, size_t length, size_t *written
) {
    MarimayCp34Host *host = context;
    size_t count = length;
    (void)handle;
    if (host->write_failure == MMAY_CP34_WRITE_DATA) {
        return MMAY_FILE_IO_FAILED;
    }
    if (count > host->maximum_write) count = host->maximum_write;
    if (count > sizeof(host->staged) - host->staged_length) {
        return MMAY_FILE_IO_FAILED;
    }
    memcpy(host->staged + host->staged_length, bytes, count);
    host->staged_length += count;
    *written = count;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_cp34_sync(
    void *context, MarimayFileHandle handle
) {
    MarimayCp34Host *host = context;
    (void)handle;
    return host->write_failure == MMAY_CP34_WRITE_SYNC
        ? MMAY_FILE_IO_FAILED : MMAY_FILE_OK;
}

static MarimayFileStatus mmay_cp34_write_close(
    void *context, MarimayFileHandle handle
) {
    MarimayCp34Host *host = context;
    (void)handle;
    host->temporary_open = false;
    return host->write_failure == MMAY_CP34_WRITE_CLOSE
        ? MMAY_FILE_IO_FAILED : MMAY_FILE_OK;
}

static MarimayFileStatus mmay_cp34_replace(
    void *context, MarimayAllocator allocator,
    const char *temporary, size_t temporary_length,
    const char *destination, size_t destination_length
) {
    MarimayCp34Host *host = context;
    (void)allocator;
    (void)temporary;
    (void)temporary_length;
    (void)destination;
    (void)destination_length;
    if (host->write_failure == MMAY_CP34_WRITE_REPLACE) {
        return MMAY_FILE_IO_FAILED;
    }
    memcpy(host->destination, host->staged, host->staged_length);
    host->destination_length = host->staged_length;
    host->temporary_created = false;
    host->destination_replaced = true;
    return MMAY_FILE_OK;
}

static void mmay_cp34_remove(
    void *context, MarimayAllocator allocator,
    const char *path, size_t length
) {
    MarimayCp34Host *host = context;
    (void)allocator;
    (void)path;
    (void)length;
    host->temporary_created = false;
    host->temporary_removed = true;
}

static MarimayFileStatus mmay_cp34_directory_open(
    void *context, MarimayAllocator allocator,
    const char *path, size_t length, MarimayDirectoryHandle *handle
) {
    MarimayCp34Host *host = context;
    (void)allocator;
    (void)path;
    (void)length;
    host->directory_open_calls += 1U;
    host->directory_index = 0;
    *handle = 91U;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_cp34_directory_next(
    void *context, MarimayDirectoryHandle handle,
    MarimayAllocator allocator, size_t maximum,
    MarimayOwnedBytes *name, MarimayDirectoryEntryKind *kind, bool *end
) {
    static const char *const names[] = {
        "zeta.txt", "alpha space", "\303\274nicode"
    };
    static const MarimayDirectoryEntryKind kinds[] = {
        MMAY_DIRECTORY_ENTRY_REGULAR,
        MMAY_DIRECTORY_ENTRY_DIRECTORY,
        MMAY_DIRECTORY_ENTRY_SYMLINK
    };
    MarimayCp34Host *host = context;
    size_t index = host->directory_index++;
    size_t length;
    (void)handle;
    host->directory_next_calls += 1U;
    if (index == host->invalid_utf8_index) return MMAY_FILE_INVALID_UTF8;
    if (index >= sizeof(names) / sizeof(names[0])) {
        *end = true;
        return MMAY_FILE_OK;
    }
    length = strlen(names[index]);
    if (length > maximum) return MMAY_FILE_LIMIT_EXCEEDED;
    if (!mmay_owned_bytes_copy(name, allocator, names[index], length)) {
        return MMAY_FILE_ALLOCATION_FAILED;
    }
    *kind = kinds[index];
    *end = false;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_cp34_directory_close(
    void *context, MarimayDirectoryHandle handle
) {
    MarimayCp34Host *host = context;
    (void)handle;
    host->directory_close_calls += 1U;
    return host->directory_close_failure
        ? MMAY_FILE_IO_FAILED : MMAY_FILE_OK;
}

static MarimayFileStatus mmay_cp34_metadata(
    void *context, MarimayAllocator allocator,
    const char *path, size_t length, MarimayFileMetadata *metadata
) {
    MarimayCp34Host *host = context;
    (void)allocator;
    host->metadata_calls += 1U;
    memset(metadata, 0, sizeof(*metadata));
    if (mmay_cp34_path_is(path, length, "/sandbox/read/dir")) {
        metadata->directory = true;
    } else {
        metadata->regular = true;
        metadata->size = host->read_length;
    }
    return MMAY_FILE_OK;
}

static void mmay_cp34_host_init(MarimayCp34Host *host) {
    static const unsigned char contents[] = "cp34-data";
    static const unsigned char original[] = "old";
    memset(host, 0, sizeof(*host));
    host->read_bytes = contents;
    host->read_length = sizeof(contents) - 1U;
    host->maximum_read = 2;
    host->maximum_write = 2;
    host->read_failure_call = SIZE_MAX;
    host->invalid_utf8_index = SIZE_MAX;
    memcpy(host->destination, original, sizeof(original) - 1U);
    host->destination_length = sizeof(original) - 1U;
}

static MarimayFsOptions mmay_cp34_options(
    MarimayCp34Host *host,
    const MarimayFsPathView *read_roots, size_t read_count,
    const MarimayFsPathView *write_roots, size_t write_count
) {
    MarimayFsOptions options = mmay_fs_default_options();
    options.flavor = MMAY_PATH_POSIX;
    options.working_directory = (MarimayFsPathView){"/sandbox", 8};
    options.read_roots = read_roots;
    options.read_root_count = read_count;
    options.write_roots = write_roots;
    options.write_root_count = write_count;
    options.max_open_files = 2;
    options.read_ops = (MarimayFileReadOps){
        host, mmay_cp34_open_read, mmay_cp34_read, mmay_cp34_read_close
    };
    options.write_ops = (MarimayFileOps){
        host, mmay_cp34_create, mmay_cp34_write, mmay_cp34_sync,
        mmay_cp34_write_close, mmay_cp34_replace, mmay_cp34_remove
    };
    options.directory_ops = (MarimayDirectoryOps){
        host, mmay_cp34_current, mmay_cp34_kind,
        mmay_cp34_directory_open, mmay_cp34_directory_next,
        mmay_cp34_directory_close
    };
    options.metadata_context = host;
    options.metadata = mmay_cp34_metadata;
    return options;
}

static bool mmay_cp34_context_init(
    MarimayFsContext *context, MarimayCp34Host *host,
    MarimayAllocator allocator
) {
    static const MarimayFsPathView read_roots[] = {{"read", 4}};
    static const MarimayFsPathView write_roots[] = {{"write", 5}};
    MarimayFsOptions options = mmay_cp34_options(
        host, read_roots, 1, write_roots, 1
    );
    memset(context, 0, sizeof(*context));
    return mmay_fs_init(context, allocator, &options);
}

static void mmay_cp34_capabilities_and_paths(void) {
    MarimayCp34Host host;
    MarimayFsContext context;
    MarimayOwnedBytes bytes = {0};
    int64_t handle = 0;
    mmay_cp34_host_init(&host);
    MMAY_EXPECT(mmay_cp34_context_init(
        &context, &host, mmay_system_allocator()
    ));
    MMAY_EXPECT(mmay_fs_read_all(
        &context, "read/file.txt", 13, 64, &bytes
    ) == MMAY_FS_OK);
    MMAY_EXPECT(bytes.length == host.read_length
        && memcmp(bytes.data, host.read_bytes, bytes.length) == 0);
    mmay_owned_bytes_dispose(&bytes);
    MMAY_EXPECT(host.open_calls == 1 && host.close_calls == 1);

    host.kind_calls = 0;
    MMAY_EXPECT(mmay_fs_open_read(
        &context, "read/../../outside", 18, &handle
    ) == MMAY_FS_CAPABILITY_DENIED);
    MMAY_EXPECT(host.kind_calls == 0 && host.open_calls == 1);
    MMAY_EXPECT(mmay_fs_open_read(
        &context, "reader/file.txt", 15, &handle
    ) == MMAY_FS_CAPABILITY_DENIED);
    MMAY_EXPECT(host.kind_calls == 0);
    MMAY_EXPECT(mmay_fs_open_read(
        &context, "read/link", 9, &handle
    ) == MMAY_FS_SYMLINK);
    MMAY_EXPECT(mmay_fs_open_read(
        &context, "read/dir", 8, &handle
    ) == MMAY_FS_NOT_FILE);
    MMAY_EXPECT(mmay_fs_open_read(
        &context, "read/missing", 12, &handle
    ) == MMAY_FS_NOT_FOUND);
    MMAY_EXPECT(mmay_fs_open_read(
        &context, "read/file.txt/child", 19, &handle
    ) == MMAY_FS_NOT_DIRECTORY);
    MMAY_EXPECT(mmay_fs_read(&context, 999, 4, &bytes) == MMAY_FS_CLOSED);
    MMAY_EXPECT(mmay_fs_read(&context, 1, -1, &bytes)
        == MMAY_FS_INVALID_LIMIT);
    MMAY_EXPECT(mmay_fs_read_all(
        &context, "read/file.txt", 13, 3, &bytes
    ) == MMAY_FS_LIMIT_EXCEEDED);
    MMAY_EXPECT(bytes.data == NULL && !host.read_open);
    mmay_fs_dispose(&context);
}

static void mmay_cp34_handles_and_cleanup(void) {
    MarimayCp34Host host;
    MarimayFsContext context;
    MarimayOwnedBytes bytes = {0};
    int64_t first = 0;
    int64_t second = 0;
    int64_t third = 0;
    mmay_cp34_host_init(&host);
    MMAY_EXPECT(mmay_cp34_context_init(
        &context, &host, mmay_system_allocator()
    ));
    MMAY_EXPECT(mmay_fs_open_read(
        &context, "read/file.txt", 13, &first
    ) == MMAY_FS_OK);
    MMAY_EXPECT(mmay_fs_open_read(
        &context, "read/file.txt", 13, &second
    ) == MMAY_FS_OK);
    MMAY_EXPECT(mmay_fs_open_read(
        &context, "read/file.txt", 13, &third
    ) == MMAY_FS_LIMIT_EXCEEDED);
    MMAY_EXPECT(mmay_fs_close(&context, first) == MMAY_FS_OK);
    MMAY_EXPECT(mmay_fs_close(&context, first) == MMAY_FS_OK);
    host.read_zero_progress = true;
    MMAY_EXPECT(mmay_fs_read(&context, second, 64, &bytes)
        == MMAY_FS_READ_FAILED);
    host.read_zero_progress = false;
    host.read_close_failure = true;
    MMAY_EXPECT(mmay_fs_close(&context, second) == MMAY_FS_CLOSE_FAILED);
    MMAY_EXPECT(mmay_fs_close(&context, second) == MMAY_FS_OK);
    host.read_close_failure = false;
    MMAY_EXPECT(mmay_fs_open_read(
        &context, "read/file.txt", 13, &third
    ) == MMAY_FS_OK);
    host.read_close_failure = true;
    mmay_fs_close_best_effort(&context, third);
    MMAY_EXPECT(host.close_calls == 3);
    host.read_close_failure = false;
    MMAY_EXPECT(mmay_fs_read_all(
        &context, "read/file.txt", 13, 64, &bytes
    ) == MMAY_FS_OK);
    mmay_owned_bytes_dispose(&bytes);
    MMAY_EXPECT(host.read_open == false);
    MMAY_EXPECT(mmay_fs_open_read(
        &context, "read/file.txt", 13, &third
    ) == MMAY_FS_OK);
    MMAY_EXPECT(mmay_fs_close(&context, third) == MMAY_FS_OK);
    mmay_fs_dispose(&context);
    MMAY_EXPECT(!host.read_open);
}

static MarimayFsStatus mmay_cp34_write_case(
    MarimayCp34WritePhase failure, MarimayCp34Host *host
) {
    static const unsigned char replacement[] = "new-value";
    MarimayFsContext context;
    MarimayFsStatus status;
    mmay_cp34_host_init(host);
    host->write_failure = failure;
    MMAY_EXPECT(mmay_cp34_context_init(
        &context, host, mmay_system_allocator()
    ));
    status = mmay_fs_write_atomic(
        &context, "write/out.txt", 13,
        replacement, sizeof(replacement) - 1U
    );
    mmay_fs_dispose(&context);
    return status;
}

static void mmay_cp34_atomic_writes(void) {
    static const unsigned char replacement[] = "new-value";
    static const unsigned char original[] = "old";
    static const MarimayFsStatus expected[] = {
        MMAY_FS_OK, MMAY_FS_WRITE_FAILED, MMAY_FS_WRITE_FAILED,
        MMAY_FS_SYNC_FAILED, MMAY_FS_CLOSE_FAILED,
        MMAY_FS_REPLACE_FAILED, MMAY_FS_WRITE_FAILED
    };
    size_t phase = 0;
    for (phase = 0; phase < sizeof(expected) / sizeof(expected[0]);
        phase += 1U) {
        MarimayCp34Host host;
        MMAY_EXPECT(mmay_cp34_write_case(
            (MarimayCp34WritePhase)phase, &host
        ) == expected[phase]);
        MMAY_EXPECT(!host.temporary_open && !host.temporary_created);
        if (phase == MMAY_CP34_WRITE_NONE) {
            MMAY_EXPECT(host.destination_replaced);
            MMAY_EXPECT(host.destination_length == sizeof(replacement) - 1U
                && memcmp(host.destination, replacement,
                    sizeof(replacement) - 1U) == 0);
        } else {
            MMAY_EXPECT(!host.destination_replaced);
            MMAY_EXPECT(host.destination_length == sizeof(original) - 1U
                && memcmp(host.destination, original,
                    sizeof(original) - 1U) == 0);
            if (phase > MMAY_CP34_WRITE_CREATE
                && phase < MMAY_CP34_WRITE_COLLIDE) {
                MMAY_EXPECT(host.temporary_removed);
            }
        }
    }
    {
        MarimayCp34Host host;
        MarimayFsContext context;
        mmay_cp34_host_init(&host);
        MMAY_EXPECT(mmay_cp34_context_init(
            &context, &host, mmay_system_allocator()
        ));
        host.kind_calls = 0;
        MMAY_EXPECT(mmay_fs_write_atomic(
            &context, "read/file.txt", 13, replacement,
            sizeof(replacement) - 1U
        ) == MMAY_FS_CAPABILITY_DENIED);
        MMAY_EXPECT(host.kind_calls == 0 && !host.temporary_created);
        MMAY_EXPECT(mmay_fs_write_atomic(
            &context, "write/out.txt", 13, replacement,
            MMAY_FS_MAX_FILE_BYTES + 1U
        ) == MMAY_FS_LIMIT_EXCEEDED);
        mmay_fs_dispose(&context);
    }
}

static void mmay_cp34_metadata_and_directory(void) {
    MarimayCp34Host host;
    MarimayFsContext context;
    MarimayFsMetadata metadata;
    MarimayVector entries;
    const MarimayFsEntry *first;
    const MarimayFsEntry *second;
    const MarimayFsEntry *third;
    mmay_cp34_host_init(&host);
    MMAY_EXPECT(mmay_cp34_context_init(
        &context, &host, mmay_system_allocator()
    ));
    MMAY_EXPECT(mmay_fs_metadata(
        &context, "read/file.txt", 13, &metadata
    ) == MMAY_FS_OK);
    MMAY_EXPECT(!metadata.directory && metadata.size == host.read_length);
    MMAY_EXPECT(mmay_fs_metadata(
        &context, "read/dir", 8, &metadata
    ) == MMAY_FS_OK);
    MMAY_EXPECT(metadata.directory && metadata.size == 0);
    MMAY_EXPECT(mmay_fs_metadata(
        &context, "read/link", 9, &metadata
    ) == MMAY_FS_SYMLINK);
    MMAY_EXPECT(host.metadata_calls == 2);
    MMAY_EXPECT(mmay_fs_read_directory(
        &context, "read/dir", 8, 3, &entries
    ) == MMAY_FS_OK);
    MMAY_EXPECT(entries.length == 3);
    first = mmay_vector_at_const(&entries, 0);
    second = mmay_vector_at_const(&entries, 1);
    third = mmay_vector_at_const(&entries, 2);
    MMAY_EXPECT(first != NULL && second != NULL && third != NULL);
    if (first != NULL && second != NULL && third != NULL) {
        MMAY_EXPECT(mmay_cp34_path_is(
            (const char *)first->name.data, first->name.length, "alpha space"
        ));
        MMAY_EXPECT(mmay_cp34_path_is(
            (const char *)second->name.data, second->name.length, "zeta.txt"
        ));
        MMAY_EXPECT(mmay_cp34_path_is(
            (const char *)third->name.data, third->name.length,
            "\303\274nicode"
        ));
    }
    mmay_fs_entries_dispose(&entries);
    MMAY_EXPECT(mmay_fs_read_directory(
        &context, "read/file.txt", 13, 3, &entries
    ) == MMAY_FS_NOT_DIRECTORY);
    MMAY_EXPECT(mmay_fs_read_directory(
        &context, "read/dir", 8, 2, &entries
    ) == MMAY_FS_TOO_MANY_ENTRIES);
    MMAY_EXPECT(entries.data == NULL);
    host.invalid_utf8_index = 0;
    MMAY_EXPECT(mmay_fs_read_directory(
        &context, "read/dir", 8, 3, &entries
    ) == MMAY_FS_INVALID_UTF8);
    host.invalid_utf8_index = SIZE_MAX;
    host.directory_close_failure = true;
    MMAY_EXPECT(mmay_fs_read_directory(
        &context, "read/dir", 8, 3, &entries
    ) == MMAY_FS_CLOSE_FAILED);
    MMAY_EXPECT(entries.data == NULL);
    MMAY_EXPECT(mmay_fs_read_directory(
        &context, "read/dir", 8, -1, &entries
    ) == MMAY_FS_INVALID_LIMIT);
    mmay_fs_dispose(&context);
}

typedef enum MarimayCp34AllocationOperation {
    MMAY_CP34_ALLOC_INIT,
    MMAY_CP34_ALLOC_READ,
    MMAY_CP34_ALLOC_WRITE,
    MMAY_CP34_ALLOC_DIRECTORY
} MarimayCp34AllocationOperation;

static bool mmay_cp34_allocation_attempt(
    size_t failure_index, MarimayCp34AllocationOperation operation
) {
    static const unsigned char replacement[] = "new-value";
    MarimayFailAllocator state;
    MarimayAllocator allocator;
    MarimayCp34Host host;
    MarimayFsContext context;
    MarimayOwnedBytes bytes = {0};
    MarimayVector entries;
    MarimayFsStatus status = MMAY_FS_ALLOCATION_FAILED;
    bool initialized;
    bool success = false;
    mmay_cp34_host_init(&host);
    mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
    allocator = mmay_fail_allocator(&state);
    initialized = mmay_cp34_context_init(&context, &host, allocator);
    if (operation == MMAY_CP34_ALLOC_INIT) {
        success = initialized;
    } else if (initialized && operation == MMAY_CP34_ALLOC_READ) {
        status = mmay_fs_read_all(
            &context, "read/file.txt", 13, 64, &bytes
        );
        success = status == MMAY_FS_OK;
        MMAY_EXPECT(success || status == MMAY_FS_ALLOCATION_FAILED);
    } else if (initialized && operation == MMAY_CP34_ALLOC_WRITE) {
        status = mmay_fs_write_atomic(
            &context, "write/out.txt", 13,
            replacement, sizeof(replacement) - 1U
        );
        success = status == MMAY_FS_OK;
        MMAY_EXPECT(success || status == MMAY_FS_ALLOCATION_FAILED);
        if (!success) {
            MMAY_EXPECT(!host.destination_replaced);
            MMAY_EXPECT(!host.temporary_created && !host.temporary_open);
        }
    } else if (initialized && operation == MMAY_CP34_ALLOC_DIRECTORY) {
        status = mmay_fs_read_directory(
            &context, "read/dir", 8, 3, &entries
        );
        success = status == MMAY_FS_OK;
        MMAY_EXPECT(success || status == MMAY_FS_ALLOCATION_FAILED);
        if (success) mmay_fs_entries_dispose(&entries);
    }
    mmay_owned_bytes_dispose(&bytes);
    if (initialized) mmay_fs_dispose(&context);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
    return success;
}

static void mmay_cp34_allocation_failures(void) {
    MarimayCp34AllocationOperation operation;
    for (operation = MMAY_CP34_ALLOC_INIT;
        operation <= MMAY_CP34_ALLOC_DIRECTORY; operation += 1) {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 256; failure_index += 1U) {
            if (mmay_cp34_allocation_attempt(failure_index, operation)) {
                reached_success = true;
                break;
            }
        }
        MMAY_EXPECT(reached_success);
    }
}

void mmay_test_cp34_fs(void) {
    mmay_cp34_capabilities_and_paths();
    mmay_cp34_handles_and_cleanup();
    mmay_cp34_atomic_writes();
    mmay_cp34_metadata_and_directory();
    mmay_cp34_allocation_failures();
}
