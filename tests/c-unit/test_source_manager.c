#include "test_support.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/base/hash.h"
#include "mmay/source/source_manager.h"

static void mmay_expect_position(
    const MarimaySourceSnapshot *snapshot,
    size_t offset,
    size_t line,
    size_t column
) {
    MarimaySourcePosition position;
    MMAY_EXPECT(mmay_source_position(snapshot, offset, &position));
    MMAY_EXPECT(position.line == line);
    MMAY_EXPECT(position.column == column);
}

static void mmay_test_source_positions(void) {
    static const unsigned char source[] = {
        0xefU, 0xbbU, 0xbfU,
        0xceU, 0xb1U,
        '\r', '\n',
        'b', '\r',
        'c', '\n'
    };
    static const unsigned char invalid[] = {'a', 0xffU, 'b'};
    MarimaySourceManager manager;
    const MarimaySourceSnapshot *first = NULL;
    const MarimaySourceSnapshot *overlay = NULL;
    const MarimaySourceSnapshot *invalid_snapshot = NULL;
    MarimaySourcePosition position;
    MarimaySourceStatus status;
    unsigned char expected_digest[32];

    MMAY_EXPECT(mmay_source_manager_init(&manager, mmay_system_allocator()));
    status = mmay_source_manager_add(
        &manager,
        "src/tour.my",
        strlen("src/tour.my"),
        source,
        sizeof(source),
        &first
    );
    MMAY_EXPECT(status == MMAY_SOURCE_OK);
    MMAY_EXPECT(first != NULL);
    MMAY_EXPECT(mmay_source_file_id(first) == 1);
    MMAY_EXPECT(mmay_source_snapshot_id(first) == 1);
    MMAY_EXPECT(strcmp(mmay_source_path(first), "src/tour.my") == 0);
    MMAY_EXPECT(mmay_source_path_length(first) == strlen("src/tour.my"));
    MMAY_EXPECT(mmay_source_length(first) == sizeof(source));
    MMAY_EXPECT(memcmp(mmay_source_bytes(first), source, sizeof(source)) == 0);
    MMAY_EXPECT(mmay_sha256(source, sizeof(source), expected_digest));
    MMAY_EXPECT(memcmp(mmay_source_sha256(first), expected_digest, sizeof(expected_digest)) == 0);
    MMAY_EXPECT(strlen(mmay_source_snapshot_identity(first)) == 71);
    MMAY_EXPECT(strncmp(mmay_source_snapshot_identity(first), "sha256:", 7) == 0);
    MMAY_EXPECT(mmay_source_has_bom(first));
    MMAY_EXPECT(mmay_source_is_valid_utf8(first));
    MMAY_EXPECT(mmay_source_line_count(first) == 4);

    {
        size_t line_start = 0;
        size_t line_end = 0;
        MMAY_EXPECT(mmay_source_line_bounds(first, 1, &line_start, &line_end));
        MMAY_EXPECT(line_start == 3);
        MMAY_EXPECT(line_end == 5);
        MMAY_EXPECT(mmay_source_line_bounds(first, 2, &line_start, &line_end));
        MMAY_EXPECT(line_start == 7);
        MMAY_EXPECT(line_end == 8);
        MMAY_EXPECT(mmay_source_line_bounds(first, 4, &line_start, &line_end));
        MMAY_EXPECT(line_start == sizeof(source));
        MMAY_EXPECT(line_end == sizeof(source));
        MMAY_EXPECT(!mmay_source_line_bounds(first, 0, &line_start, &line_end));
        MMAY_EXPECT(!mmay_source_line_bounds(first, 5, &line_start, &line_end));
    }

    mmay_expect_position(first, 0, 1, 1);
    mmay_expect_position(first, 3, 1, 1);
    mmay_expect_position(first, 4, 1, 1);
    mmay_expect_position(first, 5, 1, 2);
    mmay_expect_position(first, 6, 1, 2);
    mmay_expect_position(first, 7, 2, 1);
    mmay_expect_position(first, 8, 2, 2);
    mmay_expect_position(first, 9, 3, 1);
    mmay_expect_position(first, 10, 3, 2);
    mmay_expect_position(first, 11, 4, 1);
    MMAY_EXPECT(!mmay_source_position(first, 12, &position));

    status = mmay_source_manager_overlay(&manager, 1, "z", 1, &overlay);
    MMAY_EXPECT(status == MMAY_SOURCE_OK);
    MMAY_EXPECT(overlay != NULL);
    MMAY_EXPECT(mmay_source_file_id(overlay) == 1);
    MMAY_EXPECT(mmay_source_snapshot_id(overlay) == 2);
    MMAY_EXPECT(mmay_source_manager_current(&manager, 1) == overlay);
    MMAY_EXPECT(mmay_source_bytes(first)[3] == 0xceU);
    MMAY_EXPECT(mmay_source_length(first) == sizeof(source));

    status = mmay_source_manager_add(
        &manager,
        "src/tour.my",
        strlen("src/tour.my"),
        "duplicate",
        strlen("duplicate"),
        NULL
    );
    MMAY_EXPECT(status == MMAY_SOURCE_DUPLICATE_PATH);
    MMAY_EXPECT(mmay_source_manager_overlay(&manager, 99, "x", 1, NULL)
        == MMAY_SOURCE_UNKNOWN_FILE);

    status = mmay_source_manager_add(
        &manager,
        "src/invalid.my",
        strlen("src/invalid.my"),
        invalid,
        sizeof(invalid),
        &invalid_snapshot
    );
    MMAY_EXPECT(status == MMAY_SOURCE_OK);
    MMAY_EXPECT(!mmay_source_is_valid_utf8(invalid_snapshot));
    MMAY_EXPECT(mmay_source_invalid_utf8_offset(invalid_snapshot) == 1);
    mmay_expect_position(invalid_snapshot, 2, 1, 3);

    mmay_source_manager_dispose(&manager);
}

static void mmay_test_source_add_failures(void) {
    static const char many_lines[] =
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n"
        "10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n";
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 32; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimaySourceManager manager;
        const MarimaySourceSnapshot *snapshot = NULL;
        MarimaySourceStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_source_manager_init(&manager, allocator));
        status = mmay_source_manager_add(
            &manager,
            "source.my",
            strlen("source.my"),
            many_lines,
            strlen(many_lines),
            &snapshot
        );
        if (status == MMAY_SOURCE_OK) {
            reached_success = true;
            MMAY_EXPECT(snapshot != NULL);
        } else {
            MMAY_EXPECT(status == MMAY_SOURCE_ALLOCATION_FAILED);
            MMAY_EXPECT(snapshot == NULL);
            MMAY_EXPECT(manager.files.length == 0);
            MMAY_EXPECT(manager.next_file_id == 1);
            MMAY_EXPECT(manager.next_snapshot_id == 1);
        }
        mmay_source_manager_dispose(&manager);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_source_overlay_failures(void) {
    static const char many_lines[] =
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n"
        "10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n";
    size_t relative_failure = 0;
    bool reached_success = false;
    for (relative_failure = 0; relative_failure < 24; relative_failure += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimaySourceManager manager;
        const MarimaySourceSnapshot *first = NULL;
        const MarimaySourceSnapshot *overlay = NULL;
        MarimaySourceStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_source_manager_init(&manager, allocator));
        MMAY_EXPECT(mmay_source_manager_add(
            &manager,
            "source.my",
            strlen("source.my"),
            "first",
            strlen("first"),
            &first
        ) == MMAY_SOURCE_OK);
        state.failure_index = state.attempts + relative_failure;
        status = mmay_source_manager_overlay(
            &manager,
            mmay_source_file_id(first),
            many_lines,
            strlen(many_lines),
            &overlay
        );
        if (status == MMAY_SOURCE_OK) {
            reached_success = true;
            MMAY_EXPECT(overlay != NULL);
            MMAY_EXPECT(mmay_source_manager_current(&manager, 1) == overlay);
        } else {
            MMAY_EXPECT(status == MMAY_SOURCE_ALLOCATION_FAILED);
            MMAY_EXPECT(overlay == NULL);
            MMAY_EXPECT(mmay_source_manager_current(&manager, 1) == first);
            MMAY_EXPECT(manager.next_snapshot_id == 2);
        }
        mmay_source_manager_dispose(&manager);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_source_manager(void) {
    mmay_test_source_positions();
    mmay_test_source_add_failures();
    mmay_test_source_overlay_failures();
}
