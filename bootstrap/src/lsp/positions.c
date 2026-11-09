#include "positions.h"

#include <string.h>

#include "mmay/base/utf8.h"

bool lsp_position_to_offset(const unsigned char *bytes, size_t length,
    size_t line, size_t character, size_t *offset) {
    size_t current_line = 0, cursor = 0, units = 0;
    if ((bytes == NULL && length != 0) || offset == NULL) return false;
    while (current_line < line && cursor < length) {
        if (bytes[cursor] == '\r') {
            cursor += 1;
            if (cursor < length && bytes[cursor] == '\n') cursor += 1;
            current_line += 1;
        } else if (bytes[cursor++] == '\n') {
            current_line += 1;
        }
    }
    if (current_line != line) return false;
    while (units < character && cursor < length
        && bytes[cursor] != '\r' && bytes[cursor] != '\n') {
        MarimayUtf8Scalar scalar;
        size_t width;
        if (!mmay_utf8_decode(bytes + cursor, length - cursor, &scalar)) return false;
        width = scalar.value > 0xffffU ? 2 : 1;
        if (character - units < width) return false;
        units += width;
        cursor += scalar.width;
    }
    if (units != character) return false;
    *offset = cursor;
    return true;
}

bool lsp_offset_to_position(const unsigned char *bytes, size_t length,
    size_t offset, size_t *line, size_t *character) {
    size_t cursor = 0, current_line = 0, units = 0;
    if ((bytes == NULL && length != 0) || line == NULL || character == NULL
        || offset > length) return false;
    while (cursor < offset) {
        MarimayUtf8Scalar scalar;
        if (bytes[cursor] == '\r') {
            cursor += 1;
            if (cursor < length && bytes[cursor] == '\n') {
                if (cursor == offset) return false;
                cursor += 1;
            }
            current_line += 1;
            units = 0;
        } else if (bytes[cursor] == '\n') {
            cursor += 1;
            current_line += 1;
            units = 0;
        } else {
            if (!mmay_utf8_decode(bytes + cursor, length - cursor, &scalar)
                || scalar.width > offset - cursor) return false;
            units += scalar.value > 0xffffU ? 2 : 1;
            cursor += scalar.width;
        }
    }
    *line = current_line;
    *character = units;
    return true;
}

static int lsp_hex(unsigned char byte) {
    if (byte >= '0' && byte <= '9') return byte - '0';
    if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
    if (byte >= 'A' && byte <= 'F') return byte - 'A' + 10;
    return -1;
}

static bool lsp_letter(unsigned char byte) {
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z');
}

LspStatus lsp_uri_to_path(MarimayAllocator allocator,
    const char *uri, size_t length, MarimayOwnedBytes *path) {
    size_t start = 7, cursor, written = 0;
    MarimayOwnedBytes decoded = {0};
    if (path == NULL || uri == NULL || length < 8 || length > 98304
        || memchr(uri, 0, length) != NULL || memcmp(uri, "file://", 7) != 0)
        return LSP_INVALID;
    if (uri[start] != '/') {
        if (length < 17 || memcmp(uri + start, "localhost/", 10) != 0)
            return LSP_INVALID;
        start += 9;
    }
    if (!mmay_owned_bytes_copy(&decoded, allocator, uri + start, length - start))
        return LSP_ALLOCATION_FAILED;
    for (cursor = start; cursor < length; cursor += 1) {
        unsigned char byte = (unsigned char)uri[cursor];
        if (byte == '?' || byte == '#' || byte == '\\') goto invalid;
        if (byte == '%') {
            int high, low;
            if (length - cursor < 3 || (high = lsp_hex((unsigned char)uri[cursor + 1])) < 0
                || (low = lsp_hex((unsigned char)uri[cursor + 2])) < 0) goto invalid;
            byte = (unsigned char)(high * 16 + low);
            cursor += 2;
        }
        if (byte == 0 || byte == '\\') goto invalid;
        decoded.data[written++] = byte;
    }
#ifdef _WIN32
    if (written >= 4 && decoded.data[0] == '/' && lsp_letter(decoded.data[1])
        && decoded.data[2] == ':' && decoded.data[3] == '/') {
        memmove(decoded.data, decoded.data + 1, written - 1);
        written -= 1;
    }
#endif
    if (written == 0 || written > 32768
        || (written >= 2 && decoded.data[0] == '/' && decoded.data[1] == '/')
        || !mmay_utf8_validate((const char *)decoded.data, written, NULL)) goto invalid;
    /* Keep allocation-size metadata exact for allocator-backed teardown. */
    if (!mmay_owned_bytes_copy(path, allocator, decoded.data, written)) {
        mmay_owned_bytes_dispose(&decoded);
        return LSP_ALLOCATION_FAILED;
    }
    mmay_owned_bytes_dispose(&decoded);
    return LSP_OK;
invalid:
    mmay_owned_bytes_dispose(&decoded);
    return LSP_INVALID;
}

LspStatus lsp_path_to_uri(MarimayAllocator allocator,
    const char *path, size_t length, MarimayOwnedBytes *uri) {
    static const char digits[] = "0123456789ABCDEF";
    bool drive;
    size_t index, output_length = 7, written = 0, allocation_size;
    unsigned char *output;
    if (uri == NULL || path == NULL || length == 0 || length > 32768
        || memchr(path, 0, length) != NULL || !mmay_utf8_validate(path, length, NULL))
        return LSP_INVALID;
    drive = length >= 3 && lsp_letter((unsigned char)path[0])
        && path[1] == ':' && (path[2] == '/' || path[2] == '\\');
    if (path[0] != '/' && !drive) return LSP_INVALID;
    if (length >= 2 && path[0] == '/' && path[1] == '/') return LSP_INVALID;
    if (drive) output_length += 1;
    for (index = 0; index < length; index += 1) {
        unsigned char byte = (unsigned char)path[index];
        bool plain = lsp_letter(byte) || (byte >= '0' && byte <= '9')
            || byte == '-' || byte == '_' || byte == '.' || byte == '~'
            || byte == '/' || byte == '\\' || (drive && index == 1 && byte == ':');
        if (!mmay_size_add(output_length, plain ? 1 : 3, &output_length)) return LSP_LIMIT;
    }
    if (!mmay_size_add(output_length, 1, &allocation_size)) return LSP_LIMIT;
    output = mmay_allocate(&allocator, allocation_size);
    if (output == NULL) return LSP_ALLOCATION_FAILED;
    memcpy(output, "file://", 7);
    written = 7;
    if (drive) output[written++] = '/';
    for (index = 0; index < length; index += 1) {
        unsigned char byte = (unsigned char)path[index];
        bool plain = lsp_letter(byte) || (byte >= '0' && byte <= '9')
            || byte == '-' || byte == '_' || byte == '.' || byte == '~'
            || byte == '/' || byte == '\\' || (drive && index == 1 && byte == ':');
        if (byte == '\\') byte = '/';
        if (plain) output[written++] = byte;
        else {
            output[written++] = '%';
            output[written++] = (unsigned char)digits[byte >> 4];
            output[written++] = (unsigned char)digits[byte & 15];
        }
    }
    output[written] = 0;
    uri->allocator = allocator;
    uri->data = output;
    uri->length = written;
    return LSP_OK;
}
