/* LSP transport owner: bounded Content-Length framing over caller streams. */
#include "transport.h"

#include <stdio.h>
#include <string.h>

enum { MMAY_LSP_HEADER_MAX = 8192, MMAY_LSP_BODY_MAX = 16 * 1024 * 1024 };

static bool mmay_lsp_header_equal(const unsigned char *left, size_t length,
    const char *right) {
    size_t index;
    if (length != strlen(right)) return false;
    for (index = 0; index < length; index += 1) {
        unsigned char byte = left[index];
        if (byte >= 'A' && byte <= 'Z') byte = (unsigned char)(byte - 'A' + 'a');
        if (byte != (unsigned char)right[index]) return false;
    }
    return true;
}

static bool mmay_lsp_content_length(const unsigned char *header, size_t length,
    size_t *body_length) {
    size_t line = 0;
    bool found = false;
    while (line < length) {
        size_t end = line;
        size_t colon = line;
        size_t cursor;
        size_t value = 0;
        while (end + 1 < length && !(header[end] == '\r' && header[end + 1] == '\n')) end += 1;
        if (end + 1 >= length) return false;
        if (end == line) return found && end + 2 == length;
        while (colon < end && header[colon] != ':') {
            if (!((header[colon] >= 'a' && header[colon] <= 'z')
                || (header[colon] >= 'A' && header[colon] <= 'Z')
                || (header[colon] >= '0' && header[colon] <= '9')
                || (header[colon] != 0
                    && strchr("!#$%&'*+-.^_`|~", header[colon]) != NULL))) return false;
            colon += 1;
        }
        if (colon == line || colon == end) return false;
        for (cursor = colon + 1; cursor < end; cursor += 1) {
            if ((header[cursor] < 0x20U && header[cursor] != '\t')
                || header[cursor] >= 0x7fU) return false;
        }
        if (mmay_lsp_header_equal(header + line, colon - line, "content-length")) {
            size_t last = end;
            if (found) return false;
            found = true;
            cursor = colon + 1;
            while (cursor < end && (header[cursor] == ' ' || header[cursor] == '\t')) cursor += 1;
            while (last > cursor && (header[last - 1] == ' ' || header[last - 1] == '\t')) last -= 1;
            if (cursor == last) return false;
            for (; cursor < last; cursor += 1) {
                size_t digit;
                if (header[cursor] < '0' || header[cursor] > '9') return false;
                digit = (size_t)(header[cursor] - '0');
                if (value > ((size_t)MMAY_LSP_BODY_MAX - digit) / 10U) return false;
                value = value * 10U + digit;
            }
            *body_length = value;
        }
        line = end + 2;
    }
    return false;
}

MarimayLspReadStatus mmay_lsp_read_message(MarimayReadStream input,
    MarimayAllocator allocator, MarimayOwnedBytes *message) {
    unsigned char header[MMAY_LSP_HEADER_MAX];
    size_t header_length = 0;
    size_t body_length = 0;
    size_t received = 0;
    unsigned char *bytes;
    if (message == NULL) return MMAY_LSP_READ_INVALID;
    memset(message, 0, sizeof(*message));
    if (input.read == NULL) return MMAY_LSP_READ_INVALID;
    for (;;) {
        size_t count = 0;
        MarimayReadStatus status;
        if (header_length == sizeof(header)) return MMAY_LSP_READ_INVALID;
        status = input.read(input.context, header + header_length, 1, &count);
        if (status == MMAY_READ_END && count == 0) return header_length == 0
            ? MMAY_LSP_READ_END : MMAY_LSP_READ_INVALID;
        if (status != MMAY_READ_OK || count != 1) return MMAY_LSP_READ_IO_ERROR;
        header_length += 1;
        if (header_length >= 4
            && memcmp(header + header_length - 4, "\r\n\r\n", 4) == 0) break;
    }
    if (!mmay_lsp_content_length(header, header_length, &body_length)) return MMAY_LSP_READ_INVALID;
    bytes = mmay_allocate(&allocator, body_length + 1);
    if (bytes == NULL) return MMAY_LSP_READ_ALLOCATION_FAILED;
    while (received < body_length) {
        size_t count = 0;
        MarimayReadStatus status = input.read(input.context, bytes + received,
            body_length - received, &count);
        if (status != MMAY_READ_OK || count == 0 || count > body_length - received) {
            mmay_deallocate(&allocator, bytes, body_length + 1);
            return status == MMAY_READ_END && count == 0
                ? MMAY_LSP_READ_INVALID : MMAY_LSP_READ_IO_ERROR;
        }
        received += count;
    }
    bytes[body_length] = 0;
    message->allocator = allocator;
    message->data = bytes;
    message->length = body_length;
    return MMAY_LSP_READ_OK;
}

bool mmay_lsp_write_message(MarimayWriter output, const void *bytes,
    size_t length) {
    char header[64];
    int count;
    if ((bytes == NULL && length != 0) || length > MMAY_LSP_BODY_MAX) return false;
    count = snprintf(header, sizeof(header), "Content-Length: %zu\r\n\r\n", length);
    return count > 0 && (size_t)count < sizeof(header)
        && mmay_writer_write(output, header, (size_t)count)
        && mmay_writer_write(output, bytes, length)
        && mmay_writer_flush(output);
}
