#include "mmay/base/integer_text.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static uint64_t mmay_integer_text_mask(unsigned int width) {
    return width == 64U ? UINT64_MAX
        : (UINT64_C(1) << width) - UINT64_C(1);
}

MarimayIntegerTextStatus mmay_format_bool(
    bool value, char *buffer, size_t capacity, size_t *length
) {
    const char *text = value ? "true" : "false";
    size_t required = value ? 4U : 5U;
    if (length == NULL || (buffer == NULL && capacity != 0)) {
        return MMAY_INTEGER_TEXT_INVALID_ARGUMENT;
    }
    *length = required;
    if (buffer == NULL || capacity <= required) {
        return MMAY_INTEGER_TEXT_BUFFER_TOO_SMALL;
    }
    memcpy(buffer, text, required + 1U);
    return MMAY_INTEGER_TEXT_OK;
}

MarimayIntegerTextStatus mmay_format_integer(
    MarimayIntegerValue value,
    unsigned int radix,
    size_t minimum_digits,
    char *buffer,
    size_t capacity,
    size_t *length
) {
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char reverse[64];
    unsigned int width = mmay_integer_bit_width(value.kind);
    uint64_t bits;
    uint64_t magnitude;
    size_t digit_count = 0;
    size_t actual_digits = 0;
    size_t required;
    size_t cursor = 0;
    bool negative = false;
    if (length == NULL || (buffer == NULL && capacity != 0)
        || width == 0 || radix < 2U || radix > 36U
        || minimum_digits > 64U) {
        return MMAY_INTEGER_TEXT_INVALID_ARGUMENT;
    }
    bits = value.bits & mmay_integer_text_mask(width);
    if (mmay_integer_kind_is_signed(value.kind)
        && (bits & (UINT64_C(1) << (width - 1U))) != 0) {
        negative = true;
        magnitude = ((~bits) + UINT64_C(1)) & mmay_integer_text_mask(width);
    } else {
        magnitude = bits;
    }
    do {
        reverse[digit_count] = digits[magnitude % radix];
        digit_count += 1;
        magnitude /= radix;
    } while (magnitude != 0);
    actual_digits = digit_count;
    if (digit_count < minimum_digits) {
        digit_count = minimum_digits;
    }
    required = digit_count + (negative ? 1U : 0U);
    *length = required;
    if (buffer == NULL || capacity <= required) {
        return MMAY_INTEGER_TEXT_BUFFER_TOO_SMALL;
    }
    if (negative) {
        buffer[cursor++] = '-';
    }
    while (digit_count != 0) {
        size_t source;
        digit_count -= 1;
        source = digit_count;
        buffer[cursor++] = source < actual_digits ? reverse[source] : '0';
    }
    buffer[cursor] = '\0';
    return MMAY_INTEGER_TEXT_OK;
}

static unsigned int mmay_integer_digit(unsigned char byte) {
    if (byte >= (unsigned char)'0' && byte <= (unsigned char)'9') {
        return (unsigned int)(byte - (unsigned char)'0');
    }
    if (byte >= (unsigned char)'a' && byte <= (unsigned char)'z') {
        return 10U + (unsigned int)(byte - (unsigned char)'a');
    }
    if (byte >= (unsigned char)'A' && byte <= (unsigned char)'Z') {
        return 10U + (unsigned int)(byte - (unsigned char)'A');
    }
    return UINT_MAX;
}

MarimayIntegerParseStatus mmay_parse_i64(
    const unsigned char *text,
    size_t length,
    int64_t radix,
    int64_t *result,
    size_t *error_offset
) {
    uint64_t magnitude = 0;
    uint64_t limit;
    size_t index = 0;
    bool negative = false;
    if (result != NULL) *result = 0;
    if (error_offset != NULL) *error_offset = 0;
    if ((text == NULL && length != 0) || result == NULL
        || error_offset == NULL) {
        return MMAY_INTEGER_PARSE_INVALID_DIGIT;
    }
    if (radix < 2 || radix > 36) {
        return MMAY_INTEGER_PARSE_INVALID_RADIX;
    }
    if (length == 0) return MMAY_INTEGER_PARSE_EMPTY;
    if (text[0] == (unsigned char)'-') {
        negative = true;
        index = 1;
        if (index == length) {
            *error_offset = index;
            return MMAY_INTEGER_PARSE_INVALID_DIGIT;
        }
    }
    limit = negative
        ? (uint64_t)INT64_MAX + UINT64_C(1) : (uint64_t)INT64_MAX;
    for (; index < length; index += 1U) {
        unsigned int digit = mmay_integer_digit(text[index]);
        if (digit >= (unsigned int)radix) {
            *error_offset = index;
            return MMAY_INTEGER_PARSE_INVALID_DIGIT;
        }
        if (magnitude > (limit - digit) / (uint64_t)radix) {
            return MMAY_INTEGER_PARSE_OUT_OF_RANGE;
        }
        magnitude = magnitude * (uint64_t)radix + digit;
    }
    if (negative) {
        *result = magnitude == (uint64_t)INT64_MAX + UINT64_C(1)
            ? INT64_MIN : -(int64_t)magnitude;
    } else {
        *result = (int64_t)magnitude;
    }
    return MMAY_INTEGER_PARSE_OK;
}
