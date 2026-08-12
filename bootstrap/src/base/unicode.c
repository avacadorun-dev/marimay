#include "ruda/base/unicode.h"

#include <stddef.h>

#include "ruda/generated/unicode_xid.h"
#include "ruda/generated/unicode_width.h"

static bool ruda_unicode_range_contains(
    const RudaUnicodeRange *ranges,
    size_t count,
    uint32_t scalar
) {
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (scalar < ranges[middle].first) {
            high = middle;
        } else if (scalar > ranges[middle].last) {
            low = middle + 1;
        } else {
            return true;
        }
    }
    return false;
}

const char *ruda_unicode_version(void) {
    return RUDA_UNICODE_VERSION;
}

bool ruda_unicode_is_xid_start(uint32_t scalar) {
    return ruda_unicode_range_contains(
        ruda_unicode_xid_start_ranges,
        RUDA_UNICODE_XID_START_RANGE_COUNT,
        scalar
    );
}

bool ruda_unicode_is_xid_continue(uint32_t scalar) {
    return ruda_unicode_range_contains(
        ruda_unicode_xid_continue_ranges,
        RUDA_UNICODE_XID_CONTINUE_RANGE_COUNT,
        scalar
    );
}

size_t ruda_unicode_formatter_width(uint32_t scalar) {
    if (scalar > 0x10FFFFU || (scalar >= 0xD800U && scalar <= 0xDFFFU)) {
        return 0;
    }
    if (ruda_unicode_range_contains(
            ruda_unicode_zero_width_ranges,
            RUDA_UNICODE_ZERO_WIDTH_RANGE_COUNT,
            scalar
        )) {
        return 0;
    }
    if (ruda_unicode_range_contains(
            ruda_unicode_wide_ranges,
            RUDA_UNICODE_WIDE_RANGE_COUNT,
            scalar
        )) {
        return 2;
    }
    return 1;
}
