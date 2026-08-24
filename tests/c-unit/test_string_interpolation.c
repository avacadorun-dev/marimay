#include "test_support.h"

#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/base/string_literal.h"

static void mmay_test_interpolated_text_bytes(void) {
    static const unsigned char spelling[] =
        "left{{λ}}\\n\\0right";
    static const unsigned char expected[] =
        "left{λ}\n\0right";
    MarimayOwnedBytes decoded;
    memset(&decoded, 0, sizeof(decoded));
    MMAY_EXPECT(mmay_interpolated_text_decode(
        spelling, sizeof(spelling) - 1, mmay_system_allocator(), &decoded
    ) == MMAY_STRING_LITERAL_OK);
    MMAY_EXPECT(decoded.length == sizeof(expected) - 1);
    MMAY_EXPECT(memcmp(decoded.data, expected, sizeof(expected) - 1) == 0);
    mmay_owned_bytes_dispose(&decoded);
    MMAY_EXPECT(mmay_interpolated_text_decode(
        NULL, 0, mmay_system_allocator(), &decoded
    ) == MMAY_STRING_LITERAL_OK);
    MMAY_EXPECT(decoded.length == 0);
    mmay_owned_bytes_dispose(&decoded);
    MMAY_EXPECT(mmay_interpolated_text_decode(
        (const unsigned char *)"\\q", 2,
        mmay_system_allocator(), &decoded
    ) == MMAY_STRING_LITERAL_INVALID);
    MMAY_EXPECT(mmay_interpolated_text_decode(
        (const unsigned char *)"text", 4,
        (MarimayAllocator){0}, &decoded
    ) == MMAY_STRING_LITERAL_INVALID);
    MMAY_EXPECT(mmay_interpolated_text_decode(
        (const unsigned char *)"text", 4,
        mmay_system_allocator(), NULL
    ) == MMAY_STRING_LITERAL_INVALID);
}

static void mmay_test_interpolated_text_failures(void) {
    static const unsigned char spelling[] = "{{prefix}}\\u{3bb}\\0suffix";
    size_t failure_index;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 64; failure_index += 1U) {
        MarimayFailAllocator state;
        MarimayOwnedBytes decoded;
        MarimayStringLiteralStatus status;
        memset(&decoded, 0, sizeof(decoded));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_interpolated_text_decode(
            spelling, sizeof(spelling) - 1,
            mmay_fail_allocator(&state), &decoded
        );
        MMAY_EXPECT(status == MMAY_STRING_LITERAL_ALLOCATION_FAILED
            || status == MMAY_STRING_LITERAL_OK);
        mmay_owned_bytes_dispose(&decoded);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (status == MMAY_STRING_LITERAL_OK) {
            reached_success = true;
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_string_interpolation(void) {
    mmay_test_interpolated_text_bytes();
    mmay_test_interpolated_text_failures();
}
