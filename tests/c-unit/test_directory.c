#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define ruda_test_remove_directory _rmdir
#else
#include <unistd.h>
#define ruda_test_remove_directory rmdir
#endif

#include "ruda/base/allocator.h"
#include "ruda/platform/directory.h"

static void ruda_test_directory_real_adapter(void) {
    RudaOwnedBytes current = {0};
    RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
    RudaDirectoryIterator iterator;
    RudaFileStatus status;
    bool saw_executable = false;
    bool saw_snapshots = false;
    size_t entry_count = 0;

    RUDA_EXPECT(ruda_directory_current(
        ruda_system_allocator(),
        32768,
        &current
    ) == RUDA_FILE_OK);
    RUDA_EXPECT(current.length != 0);
    RUDA_EXPECT(current.data != NULL);
    if (current.data != NULL) {
        RUDA_EXPECT(ruda_directory_kind(
            ruda_system_allocator(),
            (const char *)current.data,
            current.length,
            &kind
        ) == RUDA_FILE_OK);
        RUDA_EXPECT(kind == RUDA_DIRECTORY_ENTRY_DIRECTORY);
    }
    ruda_owned_bytes_dispose(&current);

    RUDA_EXPECT(ruda_directory_kind(
        ruda_system_allocator(),
        ".",
        1,
        &kind
    ) == RUDA_FILE_OK);
    RUDA_EXPECT(kind == RUDA_DIRECTORY_ENTRY_DIRECTORY);
    RUDA_EXPECT(ruda_directory_kind(
        ruda_system_allocator(),
        "ruda_c_unit",
        strlen("ruda_c_unit"),
        &kind
    ) == RUDA_FILE_OK);
    RUDA_EXPECT(kind == RUDA_DIRECTORY_ENTRY_REGULAR);
    RUDA_EXPECT(ruda_directory_kind(
        ruda_system_allocator(),
        "ruda-directory-definitely-missing",
        strlen("ruda-directory-definitely-missing"),
        &kind
    ) == RUDA_FILE_OK);
    RUDA_EXPECT(kind == RUDA_DIRECTORY_ENTRY_MISSING);

    status = ruda_directory_open(
        ruda_system_allocator(),
        ".",
        1,
        &iterator
    );
    RUDA_EXPECT(status == RUDA_FILE_OK);
    if (status == RUDA_FILE_OK) {
        for (;;) {
            RudaOwnedBytes name = {0};
            bool end = false;
            status = ruda_directory_next(
                &iterator,
                ruda_system_allocator(),
                4096,
                &name,
                &kind,
                &end
            );
            RUDA_EXPECT(status == RUDA_FILE_OK);
            if (status != RUDA_FILE_OK || end) {
                ruda_owned_bytes_dispose(&name);
                break;
            }
            entry_count += 1;
            if (strcmp((const char *)name.data, "ruda_c_unit") == 0) {
                saw_executable = kind == RUDA_DIRECTORY_ENTRY_REGULAR;
            } else if (strcmp((const char *)name.data, "snapshots") == 0) {
                saw_snapshots = kind == RUDA_DIRECTORY_ENTRY_DIRECTORY;
            }
            ruda_owned_bytes_dispose(&name);
        }
        RUDA_EXPECT(ruda_directory_close(&iterator) == RUDA_FILE_OK);
    }
    RUDA_EXPECT(entry_count != 0);
    RUDA_EXPECT(saw_executable);
    RUDA_EXPECT(saw_snapshots);
}

static void ruda_test_directory_allocation_failures(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 16; failure_index += 1) {
        RudaFailAllocator state;
        RudaOwnedBytes current = {0};
        RudaFileStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        status = ruda_directory_current(
            ruda_fail_allocator(&state),
            32768,
            &current
        );
        if (status == RUDA_FILE_OK) {
            reached_success = true;
            ruda_owned_bytes_dispose(&current);
        } else {
            RUDA_EXPECT(status == RUDA_FILE_ALLOCATION_FAILED);
            RUDA_EXPECT(current.data == NULL);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 16; failure_index += 1) {
        RudaFailAllocator state;
        RudaDirectoryIterator iterator;
        RudaFileStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        status = ruda_directory_open(
            ruda_fail_allocator(&state),
            ".",
            1,
            &iterator
        );
        if (status == RUDA_FILE_OK) {
            reached_success = true;
            RUDA_EXPECT(ruda_directory_close(&iterator) == RUDA_FILE_OK);
        } else {
            RUDA_EXPECT(status == RUDA_FILE_ALLOCATION_FAILED);
            RUDA_EXPECT(!iterator.open);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);

    {
        RudaDirectoryIterator iterator;
        RudaFailAllocator state;
        RudaOwnedBytes name = {0};
        RudaDirectoryEntryKind kind;
        bool end = false;
        RUDA_EXPECT(ruda_directory_open(
            ruda_system_allocator(),
            ".",
            1,
            &iterator
        ) == RUDA_FILE_OK);
        ruda_fail_allocator_init(&state, ruda_system_allocator(), 0);
        RUDA_EXPECT(ruda_directory_next(
            &iterator,
            ruda_fail_allocator(&state),
            4096,
            &name,
            &kind,
            &end
        ) == RUDA_FILE_ALLOCATION_FAILED);
        RUDA_EXPECT(name.data == NULL);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        RUDA_EXPECT(ruda_directory_close(&iterator) == RUDA_FILE_OK);
    }
}

static void ruda_test_directory_create_owner(void) {
    static const char path[] = "ruda-directory-create-test";
    size_t failure_index = 0;
    bool reached_success = false;
    (void)ruda_test_remove_directory(path);
    for (failure_index = 0; failure_index < 8; failure_index += 1) {
        RudaFailAllocator state;
        RudaFileStatus status;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_directory_create(
            ruda_fail_allocator(&state), path, sizeof(path) - 1
        );
        if (status == RUDA_FILE_OK) reached_success = true;
        else RUDA_EXPECT(status == RUDA_FILE_ALLOCATION_FAILED);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
    RUDA_EXPECT(ruda_directory_create(
        ruda_system_allocator(), path, sizeof(path) - 1
    ) == RUDA_FILE_OK);
    RUDA_EXPECT(ruda_directory_create(
        ruda_system_allocator(), NULL, 0
    ) == RUDA_FILE_INVALID_ARGUMENT);
    RUDA_EXPECT(ruda_test_remove_directory(path) == 0);
}

void ruda_test_directory(void) {
    ruda_test_directory_real_adapter();
    ruda_test_directory_allocation_failures();
    ruda_test_directory_create_owner();
}
