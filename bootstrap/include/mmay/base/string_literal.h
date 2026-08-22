#ifndef MMAY_BASE_STRING_LITERAL_H
#define MMAY_BASE_STRING_LITERAL_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"

typedef enum MarimayStringLiteralStatus {
    MMAY_STRING_LITERAL_OK,
    MMAY_STRING_LITERAL_INVALID,
    MMAY_STRING_LITERAL_ALLOCATION_FAILED
} MarimayStringLiteralStatus;

/*
 * Decodes one already-lexed ordinary (`"..."`) or raw (`r"..."`) string
 * spelling. The output owns exact UTF-8 bytes and may contain embedded zeroes.
 */
MarimayStringLiteralStatus mmay_string_literal_decode(
    const unsigned char *spelling,
    size_t length,
    bool raw,
    MarimayAllocator allocator,
    MarimayOwnedBytes *decoded
);

#endif
