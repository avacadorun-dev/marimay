#ifndef MMAY_BASE_TEXT_H
#define MMAY_BASE_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool mmay_text_byte_length(size_t length, int64_t *result);
bool mmay_text_rune_count(
    const unsigned char *bytes, size_t length, int64_t *result
);
bool mmay_text_grapheme_count(
    const unsigned char *bytes, size_t length, int64_t *result
);
bool mmay_text_is_rune_boundary(
    const unsigned char *bytes, size_t length, int64_t offset
);
bool mmay_text_contains(
    const unsigned char *text, size_t text_length,
    const unsigned char *needle, size_t needle_length
);
bool mmay_text_starts_with(
    const unsigned char *text, size_t text_length,
    const unsigned char *prefix, size_t prefix_length
);
bool mmay_text_ends_with(
    const unsigned char *text, size_t text_length,
    const unsigned char *suffix, size_t suffix_length
);
void mmay_text_trim_ascii_range(
    const unsigned char *text, size_t length, size_t *start, size_t *end
);
bool mmay_text_split_count(
    const unsigned char *text, size_t text_length,
    const unsigned char *separator, size_t separator_length,
    int64_t *result
);
bool mmay_text_next_grapheme(
    const unsigned char *text, size_t length, size_t start, size_t *end
);

#endif
