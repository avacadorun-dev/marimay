#ifndef MMAY_BASE_UTF8_H
#define MMAY_BASE_UTF8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct MarimayUtf8Scalar {
    uint32_t value;
    size_t width;
} MarimayUtf8Scalar;

/*
 * Strictly decodes one Unicode scalar from bytes. On malformed or truncated
 * input it returns false and sets width to one so a bounded caller progresses.
 */
bool mmay_utf8_decode(const unsigned char *bytes, size_t length, MarimayUtf8Scalar *scalar);

/* Encodes one Unicode scalar into output and reports 1..4 bytes. */
bool mmay_utf8_encode(uint32_t value, unsigned char output[4], size_t *width);

/* Returns false and reports the first malformed byte offset when requested. */
bool mmay_utf8_validate(const char *text, size_t length, size_t *invalid_offset);

#endif
