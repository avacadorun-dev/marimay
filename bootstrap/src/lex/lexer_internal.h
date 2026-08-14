#ifndef RUDA_LEX_LEXER_INTERNAL_H
#define RUDA_LEX_LEXER_INTERNAL_H

#include "ruda/lex/lexer.h"

typedef enum RudaLexContextKind {
    RUDA_LEX_CONTEXT_ROOT,
    RUDA_LEX_CONTEXT_INTERPOLATED_TEXT,
    RUDA_LEX_CONTEXT_INTERPOLATION_EXPRESSION
} RudaLexContextKind;

typedef struct RudaLexContext {
    RudaLexContextKind kind;
    size_t literal_start;
    size_t brace_depth;
} RudaLexContext;

typedef struct RudaLexer {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
    RudaLexOptions options;
    RudaLexResult *result;
    RudaVector contexts;
    bool allocation_failed;
    bool limit_reached;
} RudaLexer;

typedef enum RudaLiteralMode {
    RUDA_LITERAL_NORMAL,
    RUDA_LITERAL_RAW,
    RUDA_LITERAL_BYTE,
    RUDA_LITERAL_RUNE
} RudaLiteralMode;

#endif
