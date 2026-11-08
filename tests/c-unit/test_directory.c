#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define mmay_test_remove_directory _rmdir
#else
#include <unistd.h>
#define mmay_test_remove_directory rmdir
#endif

#include "mmay/base/allocator.h"
#include "mmay/platform/directory.h"

static void mmay_test_directory_real_adapter(void) {
#if defined(_WIN32)
    static const char executable[] = "mmay_c_unit.exe";
#else
    static const char executable[] = "mmay_c_unit";
#endif
    MarimayOwnedBytes current = {0};
    MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
    MarimayDirectoryIterator iterator;
    MarimayFileStatus status;
    bool saw_executable = false;
    bool saw_snapshots = false;
    size_t entry_count = 0;

    MMAY_EXPECT(mmay_directory_current(
        mmay_system_allocator(),
        32768,
        &current
    ) == MMAY_FILE_OK);
    MMAY_EXPECT(current.length != 0);
    MMAY_EXPECT(current.data != NULL);
    if (current.data != NULL) {
        MMAY_EXPECT(mmay_directory_kind(
            mmay_system_allocator(),
            (const char *)current.data,
            current.length,
            &kind
        ) == MMAY_FILE_OK);
        MMAY_EXPECT(kind == MMAY_DIRECTORY_ENTRY_DIRECTORY);
    }
    mmay_owned_bytes_dispose(&current);

    MMAY_EXPECT(mmay_directory_kind(
        mmay_system_allocator(),
        ".",
        1,
        &kind
    ) == MMAY_FILE_OK);
    MMAY_EXPECT(kind == MMAY_DIRECTORY_ENTRY_DIRECTORY);
    MMAY_EXPECT(mmay_directory_kind(
        mmay_system_allocator(),
        executable,
        strlen(executable),
        &kind
    ) == MMAY_FILE_OK);
    MMAY_EXPECT(kind == MMAY_DIRECTORY_ENTRY_REGULAR);
    MMAY_EXPECT(mmay_directory_kind(
        mmay_system_allocator(),
        "mmay-directory-definitely-missing",
        strlen("mmay-directory-definitely-missing"),
        &kind
    ) == MMAY_FILE_OK);
    MMAY_EXPECT(kind == MMAY_DIRECTORY_ENTRY_MISSING);

    status = mmay_directory_open(
        mmay_system_allocator(),
        ".",
        1,
        &iterator
    );
    MMAY_EXPECT(status == MMAY_FILE_OK);
    if (status == MMAY_FILE_OK) {
        for (;;) {
            MarimayOwnedBytes name = {0};
            bool end = false;
            status = mmay_directory_next(
                &iterator,
                mmay_system_allocator(),
                4096,
                &name,
                &kind,
                &end
            );
            MMAY_EXPECT(status == MMAY_FILE_OK);
            if (status != MMAY_FILE_OK || end) {
                mmay_owned_bytes_dispose(&name);
                break;
            }
            entry_count += 1;
            if (strcmp((const char *)name.data, executable) == 0) {
                saw_executable = kind == MMAY_DIRECTORY_ENTRY_REGULAR;
            } else if (strcmp((const char *)name.data, "snapshots") == 0) {
                saw_snapshots = kind == MMAY_DIRECTORY_ENTRY_DIRECTORY;
            }
            mmay_owned_bytes_dispose(&name);
        }
        MMAY_EXPECT(mmay_directory_close(&iterator) == MMAY_FILE_OK);
    }
    MMAY_EXPECT(entry_count != 0);
    MMAY_EXPECT(saw_executable);
    MMAY_EXPECT(saw_snapshots);
}

static void mmay_test_directory_allocation_failures(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 16; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes current = {0};
        MarimayFileStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        status = mmay_directory_current(
            mmay_fail_allocator(&state),
            32768,
            &current
        );
        if (status == MMAY_FILE_OK) {
            reached_success = true;
            mmay_owned_bytes_dispose(&current);
        } else {
            MMAY_EXPECT(status == MMAY_FILE_ALLOCATION_FAILED);
            MMAY_EXPECT(current.data == NULL);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 16; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayDirectoryIterator iterator;
        MarimayFileStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        status = mmay_directory_open(
            mmay_fail_allocator(&state),
            ".",
            1,
            &iterator
        );
        if (status == MMAY_FILE_OK) {
            reached_success = true;
            MMAY_EXPECT(mmay_directory_close(&iterator) == MMAY_FILE_OK);
        } else {
            MMAY_EXPECT(status == MMAY_FILE_ALLOCATION_FAILED);
            MMAY_EXPECT(!iterator.open);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);

    {
        MarimayDirectoryIterator iterator;
        MarimayFailAllocator state;
        MarimayOwnedBytes name = {0};
        MarimayDirectoryEntryKind kind;
        bool end = false;
        MMAY_EXPECT(mmay_directory_open(
            mmay_system_allocator(),
            ".",
            1,
            &iterator
        ) == MMAY_FILE_OK);
        mmay_fail_allocator_init(&state, mmay_system_allocator(), 0);
        MMAY_EXPECT(mmay_directory_next(
            &iterator,
            mmay_fail_allocator(&state),
            4096,
            &name,
            &kind,
            &end
        ) == MMAY_FILE_ALLOCATION_FAILED);
        MMAY_EXPECT(name.data == NULL);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        MMAY_EXPECT(mmay_directory_close(&iterator) == MMAY_FILE_OK);
    }
}

static void mmay_test_directory_create_owner(void) {
    static const char path[] = "mmay-directory-create-test";
    size_t failure_index = 0;
    bool reached_success = false;
    (void)mmay_test_remove_directory(path);
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayFileStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_directory_create(
            mmay_fail_allocator(&state), path, sizeof(path) - 1
        );
        if (status == MMAY_FILE_OK) reached_success = true;
        else MMAY_EXPECT(status == MMAY_FILE_ALLOCATION_FAILED);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
    MMAY_EXPECT(mmay_directory_create(
        mmay_system_allocator(), path, sizeof(path) - 1
    ) == MMAY_FILE_OK);
    MMAY_EXPECT(mmay_directory_create(
        mmay_system_allocator(), NULL, 0
    ) == MMAY_FILE_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_test_remove_directory(path) == 0);
}

static void mmay_test_directory_publish_owner(void) {
    static const char staging[] = "mmay-directory-publish-stage";
    static const char destination[] = "mmay-directory-publish-destination";
    size_t failure_index = 0;
    bool reached_success = false;
    (void)mmay_test_remove_directory(staging);
    (void)mmay_test_remove_directory(destination);
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayFileStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_directory_create_exclusive(
            mmay_fail_allocator(&state), staging, sizeof(staging) - 1
        );
        if (status == MMAY_FILE_OK) reached_success = true;
        else MMAY_EXPECT(status == MMAY_FILE_ALLOCATION_FAILED);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
    MMAY_EXPECT(mmay_directory_create_exclusive(
        mmay_system_allocator(), staging, sizeof(staging) - 1
    ) == MMAY_FILE_ALREADY_EXISTS);
    MMAY_EXPECT(mmay_directory_create_exclusive(
        mmay_system_allocator(), NULL, 0
    ) == MMAY_FILE_INVALID_ARGUMENT);

    reached_success = false;
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayFileStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_directory_publish(
            mmay_fail_allocator(&state),
            staging, sizeof(staging) - 1,
            destination, sizeof(destination) - 1
        );
        if (status == MMAY_FILE_OK) reached_success = true;
        else MMAY_EXPECT(status == MMAY_FILE_ALLOCATION_FAILED);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
    {
        MarimayFailAllocator state;
        size_t attempts = 0;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_directory_publish(
            mmay_fail_allocator(&state), destination, sizeof(destination) - 1,
            destination, sizeof(destination) - 1
        ) == MMAY_FILE_ALREADY_EXISTS);
        attempts = state.attempts;
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        for (failure_index = 0; failure_index < attempts; failure_index += 1) {
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
            MMAY_EXPECT(mmay_directory_publish(
                mmay_fail_allocator(&state), destination, sizeof(destination) - 1,
                destination, sizeof(destination) - 1
            ) == MMAY_FILE_ALLOCATION_FAILED);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
    }
    reached_success = false;
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayFileStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_directory_remove_empty(
            mmay_fail_allocator(&state),
            destination, sizeof(destination) - 1
        );
        if (status == MMAY_FILE_OK) reached_success = true;
        else MMAY_EXPECT(status == MMAY_FILE_ALLOCATION_FAILED);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
    MMAY_EXPECT(mmay_directory_remove_empty(
        mmay_system_allocator(), destination, sizeof(destination) - 1
    ) == MMAY_FILE_OK);
}

void mmay_test_directory(void) {
    mmay_test_directory_real_adapter();
    mmay_test_directory_allocation_failures();
    mmay_test_directory_create_owner();
    mmay_test_directory_publish_owner();
}
