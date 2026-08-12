#include "ruda/base/string_literal.h"

#include <stdint.h>
#include <string.h>

#include "ruda/base/utf8.h"
#include "ruda/base/vector.h"

static int ruda_string_literal_hex_digit(unsigned char byte) {
    if (byte >= '0' && byte <= '9') {
        return (int)(byte - '0');
    }
    if (byte >= 'a' && byte <= 'f') {
        return (int)(byte - 'a') + 10;
    }
    if (byte >= 'A' && byte <= 'F') {
        return (int)(byte - 'A') + 10;
    }
    return -1;
}

RudaStringLiteralStatus ruda_string_literal_decode(
    const unsigned char *spelling,
    size_t length,
    bool raw,
    RudaAllocator allocator,
    RudaOwnedBytes *decoded
) {
    RudaVector bytes;
    size_t cursor = raw ? 2 : 1;
    size_t end = length > 0 ? length - 1 : 0;
    RudaStringLiteralStatus status = RUDA_STRING_LITERAL_OK;
    if (decoded == NULL) {
        return RUDA_STRING_LITERAL_INVALID;
    }
    memset(decoded, 0, sizeof(*decoded));
    if (spelling == NULL || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL
        || (!raw && (length < 2 || spelling[0] != '"'))
        || (raw && (length < 3 || spelling[0] != 'r'
            || spelling[1] != '"'))
        || spelling[end] != '"') {
        return RUDA_STRING_LITERAL_INVALID;
    }
    if (!ruda_vector_init(&bytes, allocator, sizeof(unsigned char))) {
        return RUDA_STRING_LITERAL_ALLOCATION_FAILED;
    }
    while (cursor < end) {
        unsigned char byte = spelling[cursor];
        if (raw || byte != '\\') {
            RudaUtf8Scalar scalar;
            size_t width = 1;
            if (byte >= 0x80U) {
                if (!ruda_utf8_decode(
                        spelling + cursor,
                        end - cursor,
                        &scalar
                    )) {
                    status = RUDA_STRING_LITERAL_INVALID;
                    break;
                }
                width = scalar.width;
            }
            if (!ruda_vector_append(&bytes, spelling + cursor, width)) {
                status = RUDA_STRING_LITERAL_ALLOCATION_FAILED;
                break;
            }
            cursor += width;
            continue;
        }
        cursor += 1;
        if (cursor >= end) {
            status = RUDA_STRING_LITERAL_INVALID;
            break;
        }
        byte = spelling[cursor];
        if (byte == '\\' || byte == '"' || byte == '\'') {
            if (!ruda_vector_push(&bytes, &byte)) {
                status = RUDA_STRING_LITERAL_ALLOCATION_FAILED;
                break;
            }
            cursor += 1;
        } else if (byte == 'n' || byte == 'r' || byte == 't'
            || byte == '0') {
            unsigned char value = byte == 'n' ? '\n'
                : (byte == 'r' ? '\r' : (byte == 't' ? '\t' : 0));
            if (!ruda_vector_push(&bytes, &value)) {
                status = RUDA_STRING_LITERAL_ALLOCATION_FAILED;
                break;
            }
            cursor += 1;
        } else if (byte == 'u') {
            uint32_t value = 0;
            size_t digits = 0;
            unsigned char encoded[4];
            size_t encoded_width = 0;
            cursor += 1;
            if (cursor >= end || spelling[cursor] != '{') {
                status = RUDA_STRING_LITERAL_INVALID;
                break;
            }
            cursor += 1;
            while (cursor < end && spelling[cursor] != '}') {
                int digit = ruda_string_literal_hex_digit(spelling[cursor]);
                if (digit < 0 || digits >= 6) {
                    status = RUDA_STRING_LITERAL_INVALID;
                    break;
                }
                value = (value << 4) | (uint32_t)digit;
                digits += 1;
                cursor += 1;
            }
            if (status != RUDA_STRING_LITERAL_OK) {
                break;
            }
            if (digits == 0 || cursor >= end || spelling[cursor] != '}'
                || !ruda_utf8_encode(value, encoded, &encoded_width)) {
                status = RUDA_STRING_LITERAL_INVALID;
                break;
            }
            if (!ruda_vector_append(&bytes, encoded, encoded_width)) {
                status = RUDA_STRING_LITERAL_ALLOCATION_FAILED;
                break;
            }
            cursor += 1;
        } else {
            status = RUDA_STRING_LITERAL_INVALID;
            break;
        }
    }
    if (status == RUDA_STRING_LITERAL_OK && !ruda_owned_bytes_copy(
            decoded,
            allocator,
            bytes.data,
            bytes.length
        )) {
        status = RUDA_STRING_LITERAL_ALLOCATION_FAILED;
    }
    ruda_vector_dispose(&bytes);
    return status;
}
