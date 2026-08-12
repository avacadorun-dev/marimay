#include "test_support.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/hash.h"
#include "ruda/source/source_manager.h"

static void ruda_expect_position(
    const RudaSourceSnapshot *snapshot,
    size_t offset,
    size_t line,
    size_t column
) {
    RudaSourcePosition position;
    RUDA_EXPECT(ruda_source_position(snapshot, offset, &position));
    RUDA_EXPECT(position.line == line);
    RUDA_EXPECT(position.column == column);
}

static void ruda_test_source_positions(void) {
    static const unsigned char source[] = {
        0xefU, 0xbbU, 0xbfU,
        0xceU, 0xb1U,
        '\r', '\n',
        'b', '\r',
        'c', '\n'
    };
    static const unsigned char invalid[] = {'a', 0xffU, 'b'};
    RudaSourceManager manager;
    const RudaSourceSnapshot *first = NULL;
    const RudaSourceSnapshot *overlay = NULL;
    const RudaSourceSnapshot *invalid_snapshot = NULL;
    RudaSourcePosition position;
    RudaSourceStatus status;
    unsigned char expected_digest[32];

    RUDA_EXPECT(ruda_source_manager_init(&manager, ruda_system_allocator()));
    status = ruda_source_manager_add(
        &manager,
        "src/tour.ruda",
        strlen("src/tour.ruda"),
        source,
        sizeof(source),
        &first
    );
    RUDA_EXPECT(status == RUDA_SOURCE_OK);
    RUDA_EXPECT(first != NULL);
    RUDA_EXPECT(ruda_source_file_id(first) == 1);
    RUDA_EXPECT(ruda_source_snapshot_id(first) == 1);
    RUDA_EXPECT(strcmp(ruda_source_path(first), "src/tour.ruda") == 0);
    RUDA_EXPECT(ruda_source_path_length(first) == strlen("src/tour.ruda"));
    RUDA_EXPECT(ruda_source_length(first) == sizeof(source));
    RUDA_EXPECT(memcmp(ruda_source_bytes(first), source, sizeof(source)) == 0);
    RUDA_EXPECT(ruda_sha256(source, sizeof(source), expected_digest));
    RUDA_EXPECT(memcmp(ruda_source_sha256(first), expected_digest, sizeof(expected_digest)) == 0);
    RUDA_EXPECT(strlen(ruda_source_snapshot_identity(first)) == 71);
    RUDA_EXPECT(strncmp(ruda_source_snapshot_identity(first), "sha256:", 7) == 0);
    RUDA_EXPECT(ruda_source_has_bom(first));
    RUDA_EXPECT(ruda_source_is_valid_utf8(first));
    RUDA_EXPECT(ruda_source_line_count(first) == 4);

    {
        size_t line_start = 0;
        size_t line_end = 0;
        RUDA_EXPECT(ruda_source_line_bounds(first, 1, &line_start, &line_end));
        RUDA_EXPECT(line_start == 3);
        RUDA_EXPECT(line_end == 5);
        RUDA_EXPECT(ruda_source_line_bounds(first, 2, &line_start, &line_end));
        RUDA_EXPECT(line_start == 7);
        RUDA_EXPECT(line_end == 8);
        RUDA_EXPECT(ruda_source_line_bounds(first, 4, &line_start, &line_end));
        RUDA_EXPECT(line_start == sizeof(source));
        RUDA_EXPECT(line_end == sizeof(source));
        RUDA_EXPECT(!ruda_source_line_bounds(first, 0, &line_start, &line_end));
        RUDA_EXPECT(!ruda_source_line_bounds(first, 5, &line_start, &line_end));
    }

    ruda_expect_position(first, 0, 1, 1);
    ruda_expect_position(first, 3, 1, 1);
    ruda_expect_position(first, 4, 1, 1);
    ruda_expect_position(first, 5, 1, 2);
    ruda_expect_position(first, 6, 1, 2);
    ruda_expect_position(first, 7, 2, 1);
    ruda_expect_position(first, 8, 2, 2);
    ruda_expect_position(first, 9, 3, 1);
    ruda_expect_position(first, 10, 3, 2);
    ruda_expect_position(first, 11, 4, 1);
    RUDA_EXPECT(!ruda_source_position(first, 12, &position));

    status = ruda_source_manager_overlay(&manager, 1, "z", 1, &overlay);
    RUDA_EXPECT(status == RUDA_SOURCE_OK);
    RUDA_EXPECT(overlay != NULL);
    RUDA_EXPECT(ruda_source_file_id(overlay) == 1);
    RUDA_EXPECT(ruda_source_snapshot_id(overlay) == 2);
    RUDA_EXPECT(ruda_source_manager_current(&manager, 1) == overlay);
    RUDA_EXPECT(ruda_source_bytes(first)[3] == 0xceU);
    RUDA_EXPECT(ruda_source_length(first) == sizeof(source));

    status = ruda_source_manager_add(
        &manager,
        "src/tour.ruda",
        strlen("src/tour.ruda"),
        "duplicate",
        strlen("duplicate"),
        NULL
    );
    RUDA_EXPECT(status == RUDA_SOURCE_DUPLICATE_PATH);
    RUDA_EXPECT(ruda_source_manager_overlay(&manager, 99, "x", 1, NULL)
        == RUDA_SOURCE_UNKNOWN_FILE);

    status = ruda_source_manager_add(
        &manager,
        "src/invalid.ruda",
        strlen("src/invalid.ruda"),
        invalid,
        sizeof(invalid),
        &invalid_snapshot
    );
    RUDA_EXPECT(status == RUDA_SOURCE_OK);
    RUDA_EXPECT(!ruda_source_is_valid_utf8(invalid_snapshot));
    RUDA_EXPECT(ruda_source_invalid_utf8_offset(invalid_snapshot) == 1);
    ruda_expect_position(invalid_snapshot, 2, 1, 3);

    ruda_source_manager_dispose(&manager);
}

static void ruda_test_source_add_failures(void) {
    static const char many_lines[] =
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n"
        "10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n";
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 32; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaSourceManager manager;
        const RudaSourceSnapshot *snapshot = NULL;
        RudaSourceStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_source_manager_init(&manager, allocator));
        status = ruda_source_manager_add(
            &manager,
            "source.ruda",
            strlen("source.ruda"),
            many_lines,
            strlen(many_lines),
            &snapshot
        );
        if (status == RUDA_SOURCE_OK) {
            reached_success = true;
            RUDA_EXPECT(snapshot != NULL);
        } else {
            RUDA_EXPECT(status == RUDA_SOURCE_ALLOCATION_FAILED);
            RUDA_EXPECT(snapshot == NULL);
            RUDA_EXPECT(manager.files.length == 0);
            RUDA_EXPECT(manager.next_file_id == 1);
            RUDA_EXPECT(manager.next_snapshot_id == 1);
        }
        ruda_source_manager_dispose(&manager);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_source_overlay_failures(void) {
    static const char many_lines[] =
        "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n"
        "10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n";
    size_t relative_failure = 0;
    bool reached_success = false;
    for (relative_failure = 0; relative_failure < 24; relative_failure += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaSourceManager manager;
        const RudaSourceSnapshot *first = NULL;
        const RudaSourceSnapshot *overlay = NULL;
        RudaSourceStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_source_manager_init(&manager, allocator));
        RUDA_EXPECT(ruda_source_manager_add(
            &manager,
            "source.ruda",
            strlen("source.ruda"),
            "first",
            strlen("first"),
            &first
        ) == RUDA_SOURCE_OK);
        state.failure_index = state.attempts + relative_failure;
        status = ruda_source_manager_overlay(
            &manager,
            ruda_source_file_id(first),
            many_lines,
            strlen(many_lines),
            &overlay
        );
        if (status == RUDA_SOURCE_OK) {
            reached_success = true;
            RUDA_EXPECT(overlay != NULL);
            RUDA_EXPECT(ruda_source_manager_current(&manager, 1) == overlay);
        } else {
            RUDA_EXPECT(status == RUDA_SOURCE_ALLOCATION_FAILED);
            RUDA_EXPECT(overlay == NULL);
            RUDA_EXPECT(ruda_source_manager_current(&manager, 1) == first);
            RUDA_EXPECT(manager.next_snapshot_id == 2);
        }
        ruda_source_manager_dispose(&manager);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_source_manager(void) {
    ruda_test_source_positions();
    ruda_test_source_add_failures();
    ruda_test_source_overlay_failures();
}
