#ifndef MMAY_LEX_LEXER_INTERNAL_H
#define MMAY_LEX_LEXER_INTERNAL_H

#include "mmay/lex/lexer.h"

typedef enum MarimayLexContextKind {
    MMAY_LEX_CONTEXT_ROOT,
    MMAY_LEX_CONTEXT_INTERPOLATED_TEXT,
    MMAY_LEX_CONTEXT_INTERPOLATION_EXPRESSION
} MarimayLexContextKind;

typedef struct MarimayLexContext {
    MarimayLexContextKind kind;
    size_t literal_start;
    size_t brace_depth;
} MarimayLexContext;

typedef struct MarimayLexer {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
    MarimayLexOptions options;
    MarimayLexResult *result;
    MarimayVector contexts;
    bool allocation_failed;
    bool limit_reached;
} MarimayLexer;

typedef enum MarimayLiteralMode {
    MMAY_LITERAL_NORMAL,
    MMAY_LITERAL_RAW,
    MMAY_LITERAL_BYTE,
    MMAY_LITERAL_RUNE
} MarimayLiteralMode;

#endif
