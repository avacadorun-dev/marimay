#include "ruda/base/utf8.h"

static bool ruda_utf8_continuation(unsigned char byte) {
    return byte >= 0x80U && byte <= 0xbfU;
}

static bool ruda_utf8_fail(RudaUtf8Scalar *scalar) {
    if (scalar != NULL) {
        scalar->value = 0xfffdU;
        scalar->width = 1;
    }
    return false;
}

bool ruda_utf8_decode(const unsigned char *bytes, size_t length, RudaUtf8Scalar *scalar) {
    unsigned char first = 0;
    if (bytes == NULL || scalar == NULL || length == 0) {
        return ruda_utf8_fail(scalar);
    }
    first = bytes[0];
    if (first <= 0x7fU) {
        scalar->value = first;
        scalar->width = 1;
        return true;
    }
    if (first >= 0xc2U && first <= 0xdfU) {
        if (length < 2 || !ruda_utf8_continuation(bytes[1])) {
            return ruda_utf8_fail(scalar);
        }
        scalar->value = ((uint32_t)(first & 0x1fU) << 6)
            | (uint32_t)(bytes[1] & 0x3fU);
        scalar->width = 2;
        return true;
    }
    if (first >= 0xe0U && first <= 0xefU) {
        if (length < 3 || !ruda_utf8_continuation(bytes[1])
            || !ruda_utf8_continuation(bytes[2])) {
            return ruda_utf8_fail(scalar);
        }
        if ((first == 0xe0U && bytes[1] < 0xa0U)
            || (first == 0xedU && bytes[1] > 0x9fU)) {
            return ruda_utf8_fail(scalar);
        }
        scalar->value = ((uint32_t)(first & 0x0fU) << 12)
            | ((uint32_t)(bytes[1] & 0x3fU) << 6)
            | (uint32_t)(bytes[2] & 0x3fU);
        scalar->width = 3;
        return true;
    }
    if (first >= 0xf0U && first <= 0xf4U) {
        if (length < 4 || !ruda_utf8_continuation(bytes[1])
            || !ruda_utf8_continuation(bytes[2])
            || !ruda_utf8_continuation(bytes[3])) {
            return ruda_utf8_fail(scalar);
        }
        if ((first == 0xf0U && bytes[1] < 0x90U)
            || (first == 0xf4U && bytes[1] > 0x8fU)) {
            return ruda_utf8_fail(scalar);
        }
        scalar->value = ((uint32_t)(first & 0x07U) << 18)
            | ((uint32_t)(bytes[1] & 0x3fU) << 12)
            | ((uint32_t)(bytes[2] & 0x3fU) << 6)
            | (uint32_t)(bytes[3] & 0x3fU);
        scalar->width = 4;
        return true;
    }
    return ruda_utf8_fail(scalar);
}

bool ruda_utf8_encode(uint32_t value, unsigned char output[4], size_t *width) {
    if (output == NULL || width == NULL || value > 0x10ffffU
        || (value >= 0xd800U && value <= 0xdfffU)) {
        return false;
    }
    if (value <= 0x7fU) {
        output[0] = (unsigned char)value;
        *width = 1;
    } else if (value <= 0x7ffU) {
        output[0] = (unsigned char)(0xc0U | (value >> 6));
        output[1] = (unsigned char)(0x80U | (value & 0x3fU));
        *width = 2;
    } else if (value <= 0xffffU) {
        output[0] = (unsigned char)(0xe0U | (value >> 12));
        output[1] = (unsigned char)(0x80U | ((value >> 6) & 0x3fU));
        output[2] = (unsigned char)(0x80U | (value & 0x3fU));
        *width = 3;
    } else {
        output[0] = (unsigned char)(0xf0U | (value >> 18));
        output[1] = (unsigned char)(0x80U | ((value >> 12) & 0x3fU));
        output[2] = (unsigned char)(0x80U | ((value >> 6) & 0x3fU));
        output[3] = (unsigned char)(0x80U | (value & 0x3fU));
        *width = 4;
    }
    return true;
}

bool ruda_utf8_validate(const char *text, size_t length, size_t *invalid_offset) {
    size_t offset = 0;
    if (text == NULL && length != 0) {
        if (invalid_offset != NULL) {
            *invalid_offset = 0;
        }
        return false;
    }
    while (offset < length) {
        RudaUtf8Scalar scalar;
        if (!ruda_utf8_decode(
                (const unsigned char *)text + offset,
                length - offset,
                &scalar
            )) {
            if (invalid_offset != NULL) {
                *invalid_offset = offset;
            }
            return false;
        }
        offset += scalar.width;
    }
    if (invalid_offset != NULL) {
        *invalid_offset = length;
    }
    return true;
}
