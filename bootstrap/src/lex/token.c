#include "ruda/lex/token.h"

#include <stdbool.h>
#include <string.h>

typedef struct RudaKeyword {
    const char *text;
    RudaTokenKind kind;
} RudaKeyword;

#include "../generated/token_data.inc"

static bool ruda_token_text_equal(
    const unsigned char *bytes,
    size_t length,
    const char *text
) {
    size_t text_length = strlen(text);
    return length == text_length && memcmp(bytes, text, length) == 0;
}

const char *ruda_token_kind_name(RudaTokenKind kind) {
    size_t index = (size_t)kind;
    if (index >= sizeof(ruda_token_names) / sizeof(ruda_token_names[0])) {
        return "UNKNOWN";
    }
    return ruda_token_names[index];
}

RudaTokenKind ruda_token_identifier_kind(const unsigned char *bytes, size_t length) {
    static const char *const reserved[] = {
        "null", "undefined"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(ruda_keywords) / sizeof(ruda_keywords[0]); index += 1) {
        if (ruda_token_text_equal(bytes, length, ruda_keywords[index].text)) {
            return ruda_keywords[index].kind;
        }
    }
    for (index = 0; index < sizeof(reserved) / sizeof(reserved[0]); index += 1) {
        if (ruda_token_text_equal(bytes, length, reserved[index])) {
            return RUDA_TOKEN_RESERVED_WORD;
        }
    }
    return RUDA_TOKEN_IDENTIFIER;
}
