/* LSP JSON owner: bounded syntax, token lookup, and JSON string encoding. */
#include "json.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/utf8.h"

enum { MMAY_LSP_JSON_MAX_DEPTH = 64, MMAY_LSP_JSON_MAX_TOKENS = 262144 };

typedef struct MarimayLspJsonParser {
    MarimayLspJson *json;
    size_t cursor;
} MarimayLspJsonParser;

static int mmay_lsp_json_hex(unsigned char value) {
    if (value >= '0' && value <= '9') return (int)(value - '0');
    if (value >= 'a' && value <= 'f') return (int)(value - 'a') + 10;
    if (value >= 'A' && value <= 'F') return (int)(value - 'A') + 10;
    return -1;
}

static bool mmay_lsp_json_hex4(const unsigned char *text, size_t length,
    size_t *cursor, uint32_t *value) {
    size_t index;
    uint32_t result = 0;
    if (*cursor > length || length - *cursor < 4) return false;
    for (index = 0; index < 4; index += 1) {
        int digit = mmay_lsp_json_hex(text[*cursor + index]);
        if (digit < 0) return false;
        result = result * 16U + (uint32_t)digit;
    }
    *cursor += 4;
    *value = result;
    return true;
}

/* Decode one string scalar. cursor excludes the opening and closing quotes. */
static bool mmay_lsp_json_scalar(const unsigned char *text, size_t length,
    size_t *cursor, uint32_t *value) {
    MarimayUtf8Scalar decoded;
    unsigned char byte;
    if (*cursor >= length) return false;
    byte = text[*cursor];
    *cursor += 1;
    if (byte == '\\') {
        if (*cursor >= length) return false;
        byte = text[*cursor];
        *cursor += 1;
        switch (byte) {
            case '"': case '\\': case '/': *value = byte; return true;
            case 'b': *value = 8; return true;
            case 'f': *value = 12; return true;
            case 'n': *value = 10; return true;
            case 'r': *value = 13; return true;
            case 't': *value = 9; return true;
            case 'u': {
                uint32_t first = 0;
                uint32_t second = 0;
                if (!mmay_lsp_json_hex4(text, length, cursor, &first)) return false;
                if (first >= 0xdc00U && first <= 0xdfffU) return false;
                if (first >= 0xd800U && first <= 0xdbffU) {
                    if (*cursor > length || length - *cursor < 6
                        || text[*cursor] != '\\' || text[*cursor + 1] != 'u') return false;
                    *cursor += 2;
                    if (!mmay_lsp_json_hex4(text, length, cursor, &second)
                        || second < 0xdc00U || second > 0xdfffU) return false;
                    first = 0x10000U + (first - 0xd800U) * 0x400U + second - 0xdc00U;
                }
                *value = first;
                return true;
            }
            default: return false;
        }
    }
    if (byte < 0x20U || byte == '"') return false;
    *cursor -= 1;
    if (!mmay_utf8_decode(text + *cursor, length - *cursor, &decoded)) return false;
    *cursor += decoded.width;
    *value = decoded.value;
    return true;
}

static bool mmay_lsp_json_strings_equal(const MarimayLspJson *json,
    size_t left_index, size_t right_index) {
    const MarimayLspJsonToken *left = mmay_lsp_json_token(json, left_index);
    const MarimayLspJsonToken *right = mmay_lsp_json_token(json, right_index);
    size_t a;
    size_t b;
    if (left == NULL || right == NULL || left->kind != MMAY_LSP_JSON_STRING
        || right->kind != MMAY_LSP_JSON_STRING) return false;
    a = left->start + 1;
    b = right->start + 1;
    while (a < left->end - 1 && b < right->end - 1) {
        uint32_t x = 0;
        uint32_t y = 0;
        if (!mmay_lsp_json_scalar(json->source, left->end - 1, &a, &x)
            || !mmay_lsp_json_scalar(json->source, right->end - 1, &b, &y)
            || x != y) return false;
    }
    return a == left->end - 1 && b == right->end - 1;
}

static void mmay_lsp_json_whitespace(MarimayLspJsonParser *parser) {
    while (parser->cursor < parser->json->length) {
        unsigned char byte = parser->json->source[parser->cursor];
        if (byte != ' ' && byte != '\t' && byte != '\r' && byte != '\n') break;
        parser->cursor += 1;
    }
}

static bool mmay_lsp_json_parse_string(MarimayLspJsonParser *parser) {
    const unsigned char *source = parser->json->source;
    size_t length = parser->json->length;
    parser->cursor += 1;
    while (parser->cursor < length) {
        uint32_t scalar = 0;
        if (source[parser->cursor] == '"') {
            parser->cursor += 1;
            return true;
        }
        if (!mmay_lsp_json_scalar(source, length, &parser->cursor, &scalar)) return false;
    }
    return false;
}

static bool mmay_lsp_json_digit(unsigned char byte) {
    return byte >= '0' && byte <= '9';
}

static bool mmay_lsp_json_parse_number(MarimayLspJsonParser *parser) {
    const unsigned char *source = parser->json->source;
    size_t length = parser->json->length;
    size_t cursor = parser->cursor;
    if (source[cursor] == '-') cursor += 1;
    if (cursor >= length) return false;
    if (source[cursor] == '0') cursor += 1;
    else {
        if (source[cursor] < '1' || source[cursor] > '9') return false;
        do { cursor += 1; } while (cursor < length && mmay_lsp_json_digit(source[cursor]));
    }
    if (cursor < length && source[cursor] == '.') {
        cursor += 1;
        if (cursor >= length || !mmay_lsp_json_digit(source[cursor])) return false;
        do { cursor += 1; } while (cursor < length && mmay_lsp_json_digit(source[cursor]));
    }
    if (cursor < length && (source[cursor] == 'e' || source[cursor] == 'E')) {
        cursor += 1;
        if (cursor < length && (source[cursor] == '+' || source[cursor] == '-')) cursor += 1;
        if (cursor >= length || !mmay_lsp_json_digit(source[cursor])) return false;
        do { cursor += 1; } while (cursor < length && mmay_lsp_json_digit(source[cursor]));
    }
    parser->cursor = cursor;
    return true;
}

static bool mmay_lsp_json_parse_value(MarimayLspJsonParser *parser, size_t depth);

static bool mmay_lsp_json_parse_container(MarimayLspJsonParser *parser,
    size_t depth, size_t root, bool object) {
    unsigned char closing = object ? '}' : ']';
    size_t members = 0;
    parser->cursor += 1;
    mmay_lsp_json_whitespace(parser);
    if (parser->cursor < parser->json->length
        && parser->json->source[parser->cursor] == closing) {
        parser->cursor += 1;
        return true;
    }
    for (;;) {
        if (object) {
            size_t key = parser->json->tokens.length;
            size_t previous = root + 1;
            if (members >= 4096 || parser->cursor >= parser->json->length
                || parser->json->source[parser->cursor] != '"'
                || !mmay_lsp_json_parse_value(parser, depth + 1)) return false;
            while (previous < key) {
                const MarimayLspJsonToken *value;
                if (mmay_lsp_json_strings_equal(parser->json, previous, key)) return false;
                value = mmay_lsp_json_token(parser->json, previous + 1);
                if (value == NULL) return false;
                previous = value->next;
            }
            mmay_lsp_json_whitespace(parser);
            if (parser->cursor >= parser->json->length
                || parser->json->source[parser->cursor] != ':') return false;
            parser->cursor += 1;
        }
        if (!mmay_lsp_json_parse_value(parser, depth + 1)) return false;
        members += 1;
        mmay_lsp_json_whitespace(parser);
        if (parser->cursor >= parser->json->length) return false;
        if (parser->json->source[parser->cursor] == closing) {
            parser->cursor += 1;
            return true;
        }
        if (parser->json->source[parser->cursor] != ',') return false;
        parser->cursor += 1;
        mmay_lsp_json_whitespace(parser);
    }
}

static bool mmay_lsp_json_parse_value(MarimayLspJsonParser *parser, size_t depth) {
    MarimayLspJsonToken token;
    size_t index = parser->json->tokens.length;
    unsigned char byte;
    const char *literal = NULL;
    mmay_lsp_json_whitespace(parser);
    if (depth > MMAY_LSP_JSON_MAX_DEPTH || parser->cursor >= parser->json->length
        || index >= MMAY_LSP_JSON_MAX_TOKENS) return false;
    byte = parser->json->source[parser->cursor];
    memset(&token, 0, sizeof(token));
    token.start = parser->cursor;
    switch (byte) {
        case '{': token.kind = MMAY_LSP_JSON_OBJECT; break;
        case '[': token.kind = MMAY_LSP_JSON_ARRAY; break;
        case '"': token.kind = MMAY_LSP_JSON_STRING; break;
        case 't': token.kind = MMAY_LSP_JSON_TRUE; literal = "true"; break;
        case 'f': token.kind = MMAY_LSP_JSON_FALSE; literal = "false"; break;
        case 'n': token.kind = MMAY_LSP_JSON_NULL; literal = "null"; break;
        default:
            if (byte != '-' && !mmay_lsp_json_digit(byte)) return false;
            token.kind = MMAY_LSP_JSON_NUMBER;
            break;
    }
    if (!mmay_vector_push(&parser->json->tokens, &token)) return false;
    if (token.kind == MMAY_LSP_JSON_STRING) {
        if (!mmay_lsp_json_parse_string(parser)) return false;
    } else if (token.kind == MMAY_LSP_JSON_NUMBER) {
        if (!mmay_lsp_json_parse_number(parser)) return false;
    } else if (literal != NULL) {
        size_t count = strlen(literal);
        if (count > parser->json->length - parser->cursor
            || memcmp(parser->json->source + parser->cursor, literal, count) != 0) return false;
        parser->cursor += count;
    } else if (!mmay_lsp_json_parse_container(parser, depth, index,
        token.kind == MMAY_LSP_JSON_OBJECT)) return false;
    {
        MarimayLspJsonToken *stored = mmay_vector_at(&parser->json->tokens, index);
        stored->end = parser->cursor;
        stored->next = parser->json->tokens.length;
    }
    return true;
}

bool mmay_lsp_json_parse(MarimayLspJson *json, MarimayAllocator allocator,
    const void *source, size_t length) {
    MarimayLspJsonParser parser;
    if (json == NULL) return false;
    memset(json, 0, sizeof(*json));
    if (source == NULL || length > 16U * 1024U * 1024U
        || !mmay_utf8_validate(source, length, NULL)
        || !mmay_vector_init(&json->tokens, allocator, sizeof(MarimayLspJsonToken))) return false;
    json->allocator = allocator;
    json->source = source;
    json->length = length;
    parser.json = json;
    parser.cursor = 0;
    if (mmay_lsp_json_parse_value(&parser, 0)) {
        mmay_lsp_json_whitespace(&parser);
        if (parser.cursor == length) return true;
    }
    mmay_lsp_json_dispose(json);
    return false;
}

void mmay_lsp_json_dispose(MarimayLspJson *json) {
    if (json == NULL) return;
    mmay_vector_dispose(&json->tokens);
    memset(json, 0, sizeof(*json));
}

const MarimayLspJsonToken *mmay_lsp_json_token(const MarimayLspJson *json,
    size_t index) {
    return json == NULL ? NULL : mmay_vector_at_const(&json->tokens, index);
}

size_t mmay_lsp_json_get(const MarimayLspJson *json, size_t object,
    const char *key) {
    const MarimayLspJsonToken *root = mmay_lsp_json_token(json, object);
    size_t index = object;
    if (root == NULL || root->kind != MMAY_LSP_JSON_OBJECT || key == NULL) return SIZE_MAX;
    index += 1;
    while (index < root->next) {
        const MarimayLspJsonToken *value = mmay_lsp_json_token(json, index + 1);
        if (value == NULL) return SIZE_MAX;
        if (mmay_lsp_json_equal(json, index, key)) return index + 1;
        index = value->next;
    }
    return SIZE_MAX;
}

size_t mmay_lsp_json_at(const MarimayLspJson *json, size_t array, size_t wanted) {
    const MarimayLspJsonToken *root = mmay_lsp_json_token(json, array);
    size_t index = array;
    if (root == NULL || root->kind != MMAY_LSP_JSON_ARRAY) return SIZE_MAX;
    index += 1;
    while (index < root->next) {
        const MarimayLspJsonToken *value = mmay_lsp_json_token(json, index);
        if (value == NULL) return SIZE_MAX;
        if (wanted == 0) return index;
        wanted -= 1;
        index = value->next;
    }
    return SIZE_MAX;
}

bool mmay_lsp_json_string(const MarimayLspJson *json, size_t index,
    MarimayOwnedBytes *output) {
    const MarimayLspJsonToken *token = mmay_lsp_json_token(json, index);
    size_t cursor;
    size_t length = 0;
    size_t capacity;
    unsigned char *bytes;
    if (output == NULL) return false;
    memset(output, 0, sizeof(*output));
    if (token == NULL || token->kind != MMAY_LSP_JSON_STRING) return false;
    cursor = token->start + 1;
    while (cursor < token->end - 1) {
        uint32_t scalar = 0;
        unsigned char encoded[4];
        size_t width = 0;
        if (!mmay_lsp_json_scalar(json->source, token->end - 1, &cursor, &scalar)
            || !mmay_utf8_encode(scalar, encoded, &width)
            || !mmay_size_add(length, width, &length)) return false;
    }
    if (!mmay_size_add(length, 1, &capacity)) return false;
    bytes = mmay_allocate(&json->allocator, capacity);
    if (bytes == NULL) return false;
    cursor = token->start + 1;
    length = 0;
    while (cursor < token->end - 1) {
        uint32_t scalar = 0;
        unsigned char encoded[4];
        size_t width = 0;
        if (!mmay_lsp_json_scalar(json->source, token->end - 1, &cursor, &scalar)
            || !mmay_utf8_encode(scalar, encoded, &width)) {
            mmay_deallocate(&json->allocator, bytes, capacity);
            return false;
        }
        memcpy(bytes + length, encoded, width);
        length += width;
    }
    bytes[length] = 0;
    output->allocator = json->allocator;
    output->data = bytes;
    output->length = length;
    return true;
}

bool mmay_lsp_json_equal(const MarimayLspJson *json, size_t index,
    const char *text) {
    const MarimayLspJsonToken *token = mmay_lsp_json_token(json, index);
    size_t cursor;
    size_t target = 0;
    size_t length;
    if (token == NULL || token->kind != MMAY_LSP_JSON_STRING || text == NULL) return false;
    length = strlen(text);
    cursor = token->start + 1;
    while (cursor < token->end - 1 && target < length) {
        MarimayUtf8Scalar decoded;
        uint32_t scalar = 0;
        if (!mmay_lsp_json_scalar(json->source, token->end - 1, &cursor, &scalar)
            || !mmay_utf8_decode((const unsigned char *)text + target,
                length - target, &decoded) || scalar != decoded.value) return false;
        target += decoded.width;
    }
    return cursor == token->end - 1 && target == length;
}

bool mmay_lsp_json_integer(const MarimayLspJson *json, size_t index,
    int64_t *value) {
    const MarimayLspJsonToken *token = mmay_lsp_json_token(json, index);
    uint64_t result = 0;
    uint64_t limit;
    size_t cursor;
    bool negative;
    if (value == NULL || token == NULL || token->kind != MMAY_LSP_JSON_NUMBER) return false;
    cursor = token->start;
    negative = json->source[cursor] == '-';
    if (negative) cursor += 1;
    limit = negative ? (uint64_t)INT64_MAX + 1U : (uint64_t)INT64_MAX;
    for (; cursor < token->end; cursor += 1) {
        unsigned char byte = json->source[cursor];
        uint64_t digit;
        if (!mmay_lsp_json_digit(byte)) return false;
        digit = (uint64_t)(byte - '0');
        if (result > (limit - digit) / 10U) return false;
        result = result * 10U + digit;
    }
    *value = negative ? (result == (uint64_t)INT64_MAX + 1U
        ? INT64_MIN : -(int64_t)result) : (int64_t)result;
    return true;
}

bool mmay_lsp_json_write_string(MarimayWriter writer, const void *bytes,
    size_t length) {
    const unsigned char *source = bytes;
    size_t index;
    size_t start = 0;
    static const char hex[] = "0123456789abcdef";
    if ((bytes == NULL && length != 0)
        || !mmay_utf8_validate(bytes, length, NULL)
        || !mmay_writer_write_cstr(writer, "\"")) return false;
    for (index = 0; index < length; index += 1) {
        unsigned char byte = source[index];
        if (byte == '"' || byte == '\\' || byte < 0x20U) {
            char escape[6] = {'\\', 'u', '0', '0', '0', '0'};
            size_t count = 6;
            if (!mmay_writer_write(writer, (const char *)source + start, index - start)) return false;
            if (byte == '"' || byte == '\\') { escape[1] = (char)byte; count = 2; }
            else { escape[4] = hex[byte >> 4]; escape[5] = hex[byte & 15U]; }
            if (!mmay_writer_write(writer, escape, count)) return false;
            start = index + 1;
        }
    }
    return mmay_writer_write(writer, bytes == NULL ? "" : (const char *)source + start,
        length - start) && mmay_writer_write_cstr(writer, "\"");
}
