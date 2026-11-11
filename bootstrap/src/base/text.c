#include "mmay/base/text.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/unicode.h"
#include "mmay/base/utf8.h"

static bool mmay_text_valid_input(const unsigned char *bytes, size_t length) {
    return bytes != NULL || length == 0;
}

bool mmay_text_byte_length(size_t length, int64_t *result) {
    if (result == NULL || (uintmax_t)length > INT64_MAX) return false;
    *result = (int64_t)length;
    return true;
}

bool mmay_text_rune_count(
    const unsigned char *bytes, size_t length, int64_t *result
) {
    size_t offset = 0;
    int64_t count = 0;
    if (!mmay_text_valid_input(bytes, length) || result == NULL) return false;
    while (offset < length) {
        MarimayUtf8Scalar scalar;
        if (!mmay_utf8_decode(bytes + offset, length - offset, &scalar)
            || count == INT64_MAX) return false;
        offset += scalar.width;
        count += 1;
    }
    *result = count;
    return true;
}

bool mmay_text_next_grapheme(
    const unsigned char *text, size_t length, size_t start, size_t *end
) {
    MarimayUtf8Scalar first;
    size_t offset = start;
    if (!mmay_text_valid_input(text, length) || end == NULL || start >= length
        || !mmay_utf8_decode(text + start, length - start, &first)) return false;
    offset += first.width;
    if (first.value == 0x0dU && offset < length && text[offset] == 0x0aU) {
        *end = offset + 1U;
        return true;
    }
    while (offset < length) {
        MarimayUtf8Scalar scalar;
        if (!mmay_utf8_decode(text + offset, length - offset, &scalar)) {
            return false;
        }
        if (mmay_unicode_formatter_width(scalar.value) != 0) break;
        offset += scalar.width;
    }
    *end = offset;
    return true;
}

bool mmay_text_grapheme_count(
    const unsigned char *bytes, size_t length, int64_t *result
) {
    size_t offset = 0;
    int64_t count = 0;
    if (!mmay_text_valid_input(bytes, length) || result == NULL) return false;
    while (offset < length) {
        if (!mmay_text_next_grapheme(bytes, length, offset, &offset)
            || count == INT64_MAX) return false;
        count += 1;
    }
    *result = count;
    return true;
}

bool mmay_text_is_rune_boundary(
    const unsigned char *bytes, size_t length, int64_t offset
) {
    if (!mmay_text_valid_input(bytes, length) || offset < 0
        || (uint64_t)offset > (uintmax_t)length) return false;
    return (size_t)offset == 0 || (size_t)offset == length
        || (bytes[(size_t)offset] & 0xc0U) != 0x80U;
}

bool mmay_text_starts_with(
    const unsigned char *text, size_t text_length,
    const unsigned char *prefix, size_t prefix_length
) {
    return mmay_text_valid_input(text, text_length)
        && mmay_text_valid_input(prefix, prefix_length)
        && prefix_length <= text_length
        && (prefix_length == 0 || memcmp(text, prefix, prefix_length) == 0);
}

bool mmay_text_ends_with(
    const unsigned char *text, size_t text_length,
    const unsigned char *suffix, size_t suffix_length
) {
    return mmay_text_valid_input(text, text_length)
        && mmay_text_valid_input(suffix, suffix_length)
        && suffix_length <= text_length
        && (suffix_length == 0 || memcmp(
            text + text_length - suffix_length, suffix, suffix_length
        ) == 0);
}

bool mmay_text_contains(
    const unsigned char *text, size_t text_length,
    const unsigned char *needle, size_t needle_length
) {
    size_t offset = 0;
    if (!mmay_text_valid_input(text, text_length)
        || !mmay_text_valid_input(needle, needle_length)) return false;
    if (needle_length == 0) return true;
    if (needle_length > text_length) return false;
    mmay_text_find_separator(text, text_length, 0, needle, needle_length,
        &offset);
    return offset != text_length;
}

static bool mmay_text_ascii_space(unsigned char value) {
    return value == 0x20U || (value >= 0x09U && value <= 0x0dU);
}

void mmay_text_trim_ascii_range(
    const unsigned char *text, size_t length, size_t *start, size_t *end
) {
    size_t first = 0;
    size_t last = length;
    if (start == NULL || end == NULL || !mmay_text_valid_input(text, length)) {
        return;
    }
    while (first < last && mmay_text_ascii_space(text[first])) first += 1U;
    while (last > first && mmay_text_ascii_space(text[last - 1U])) last -= 1U;
    *start = first;
    *end = last;
}

/* One separator search owns every split loop. The scalar scan compares the
 * first separator byte directly and only then calls memcmp, so a one-byte
 * separator never leaves this translation unit. */
void mmay_text_find_separator(
    const unsigned char *text, size_t text_length, size_t start,
    const unsigned char *separator, size_t separator_length, size_t *end
) {
    size_t offset = 0;
    if (end == NULL) return;
    *end = text_length;
    if (!mmay_text_valid_input(text, text_length)
        || !mmay_text_valid_input(separator, separator_length)
        || separator_length == 0 || separator_length > text_length
        || start > text_length || start == text_length) return;
    if (separator_length == 1U) {
        const unsigned char *found = memchr(
            mmay_text_range_start(text, start), separator[0],
            text_length - start
        );
        if (found != NULL) *end = (size_t)(found - text);
        return;
    }
    for (offset = start; offset <= text_length - separator_length;
        offset += 1U) {
        if (text[offset] != separator[0]) continue;
        if (memcmp(text + offset, separator, separator_length) == 0) {
            *end = offset;
            return;
        }
    }
}

bool mmay_text_split_count(
    const unsigned char *text, size_t text_length,
    const unsigned char *separator, size_t separator_length,
    int64_t *result
) {
    size_t offset = 0;
    size_t end = 0;
    int64_t count = 1;
    if (!mmay_text_valid_input(text, text_length)
        || !mmay_text_valid_input(separator, separator_length)
        || separator_length == 0 || result == NULL) return false;
    for (;;) {
        mmay_text_find_separator(text, text_length, offset, separator,
            separator_length, &end);
        if (end == text_length) break;
        if (count == INT64_MAX) return false;
        count += 1;
        offset = end + separator_length;
    }
    *result = count;
    return true;
}
