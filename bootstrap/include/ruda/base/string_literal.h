#ifndef RUDA_BASE_STRING_LITERAL_H
#define RUDA_BASE_STRING_LITERAL_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"

typedef enum RudaStringLiteralStatus {
    RUDA_STRING_LITERAL_OK,
    RUDA_STRING_LITERAL_INVALID,
    RUDA_STRING_LITERAL_ALLOCATION_FAILED
} RudaStringLiteralStatus;

/*
 * Decodes one already-lexed ordinary (`"..."`) or raw (`r"..."`) string
 * spelling. The output owns exact UTF-8 bytes and may contain embedded zeroes.
 */
RudaStringLiteralStatus ruda_string_literal_decode(
    const unsigned char *spelling,
    size_t length,
    bool raw,
    RudaAllocator allocator,
    RudaOwnedBytes *decoded
);

#endif
