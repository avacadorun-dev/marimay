#include "mmay/lex/token.h"

#include <stdbool.h>
#include <string.h>

typedef struct MarimayKeyword {
    const char *text;
    MarimayTokenKind kind;
} MarimayKeyword;

#include "../generated/token_data.inc"

static bool mmay_token_text_equal(
    const unsigned char *bytes,
    size_t length,
    const char *text
) {
    size_t text_length = strlen(text);
    return length == text_length && memcmp(bytes, text, length) == 0;
}

const char *mmay_token_kind_name(MarimayTokenKind kind) {
    size_t index = (size_t)kind;
    if (index >= sizeof(mmay_token_names) / sizeof(mmay_token_names[0])) {
        return "UNKNOWN";
    }
    return mmay_token_names[index];
}

MarimayTokenKind mmay_token_identifier_kind(const unsigned char *bytes, size_t length) {
    static const char *const reserved[] = {
        "null", "undefined"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(mmay_keywords) / sizeof(mmay_keywords[0]); index += 1) {
        if (mmay_token_text_equal(bytes, length, mmay_keywords[index].text)) {
            return mmay_keywords[index].kind;
        }
    }
    for (index = 0; index < sizeof(reserved) / sizeof(reserved[0]); index += 1) {
        if (mmay_token_text_equal(bytes, length, reserved[index])) {
            return MMAY_TOKEN_RESERVED_WORD;
        }
    }
    return MMAY_TOKEN_IDENTIFIER;
}
