#ifndef RUDA_LEX_TOKEN_H
#define RUDA_LEX_TOKEN_H

#include <stddef.h>

#include "ruda/generated/token_kinds.h"

typedef struct RudaToken {
    RudaTokenKind kind;
    size_t start;
    size_t end;
} RudaToken;

const char *ruda_token_kind_name(RudaTokenKind kind);

/* Returns IDENTIFIER for a non-keyword spelling. */
RudaTokenKind ruda_token_identifier_kind(const unsigned char *bytes, size_t length);

#endif
