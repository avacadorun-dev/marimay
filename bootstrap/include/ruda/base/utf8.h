#ifndef RUDA_BASE_UTF8_H
#define RUDA_BASE_UTF8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct RudaUtf8Scalar {
    uint32_t value;
    size_t width;
} RudaUtf8Scalar;

/*
 * Strictly decodes one Unicode scalar from bytes. On malformed or truncated
 * input it returns false and sets width to one so a bounded caller progresses.
 */
bool ruda_utf8_decode(const unsigned char *bytes, size_t length, RudaUtf8Scalar *scalar);

/* Encodes one Unicode scalar into output and reports 1..4 bytes. */
bool ruda_utf8_encode(uint32_t value, unsigned char output[4], size_t *width);

/* Returns false and reports the first malformed byte offset when requested. */
bool ruda_utf8_validate(const char *text, size_t length, size_t *invalid_offset);

#endif
