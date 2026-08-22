#ifndef MMAY_LEX_TOKEN_H
#define MMAY_LEX_TOKEN_H

#include <stddef.h>

#include "mmay/generated/token_kinds.h"

typedef struct MarimayToken {
    MarimayTokenKind kind;
    size_t start;
    size_t end;
} MarimayToken;

const char *mmay_token_kind_name(MarimayTokenKind kind);

/* Returns IDENTIFIER for a non-keyword spelling. */
MarimayTokenKind mmay_token_identifier_kind(const unsigned char *bytes, size_t length);

#endif
