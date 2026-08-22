#include "cli_internal.h"

#include <string.h>

#include "mmay/base/utf8.h"

void mmay_display_argument(const char *argument, char *output, size_t capacity) {
    const unsigned char *cursor = (const unsigned char *)argument;
    size_t remaining = strlen(argument);
    size_t length = 0;
    if (capacity == 0) {
        return;
    }
    while (remaining != 0 && length + 1 < capacity) {
        unsigned char byte = *cursor;
        if (byte == '\n' || byte == '\r' || byte == '\t' || byte == '\\' || byte == '\'') {
            if (length + 2 >= capacity) {
                break;
            }
            output[length] = '\\';
            length += 1;
            output[length] = byte == '\n' ? 'n' : byte == '\r' ? 'r' : byte == '\t' ? 't' : (char)byte;
            length += 1;
            cursor += 1;
            remaining -= 1;
            continue;
        } else if (byte < 0x20U || byte == 0x7fU) {
            if (length + 1 >= capacity) {
                break;
            }
            output[length] = '?';
            length += 1;
            cursor += 1;
            remaining -= 1;
            continue;
        } else if (byte < 0x80U) {
            output[length] = (char)byte;
            length += 1;
            cursor += 1;
            remaining -= 1;
            continue;
        } else {
            MarimayUtf8Scalar scalar;
            if (mmay_utf8_decode(cursor, remaining, &scalar)) {
                if (scalar.width > capacity - length - 1) {
                    break;
                }
                memcpy(output + length, cursor, scalar.width);
                length += scalar.width;
                cursor += scalar.width;
                remaining -= scalar.width;
                continue;
            }
            output[length] = '?';
            length += 1;
            cursor += 1;
            remaining -= 1;
        }
    }
    output[length] = '\0';
}
