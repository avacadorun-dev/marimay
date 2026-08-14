#ifndef RUDA_TEST_PARSER_SUPPORT_H
#define RUDA_TEST_PARSER_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"
#include "ruda/syntax/syntax.h"

typedef struct RudaParseFixture {
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot;
    RudaLexResult lexed;
    RudaParseResult parsed;
} RudaParseFixture;

bool ruda_parse_fixture_init_bytes(
    RudaParseFixture *fixture,
    const unsigned char *bytes,
    size_t length
);
bool ruda_parse_fixture_init(RudaParseFixture *fixture, const char *text);
void ruda_parse_fixture_dispose(RudaParseFixture *fixture);
bool ruda_parse_has_diagnostic(
    const RudaParseResult *result,
    const char *code
);
size_t ruda_syntax_count_kind(const RudaGreenNode *node, RudaSyntaxKind kind);
size_t ruda_syntax_count_synthetic_tokens(const RudaGreenNode *node);
const RudaGreenNode *ruda_syntax_direct_nth_kind(
    const RudaGreenNode *node,
    RudaSyntaxKind kind,
    size_t ordinal
);
bool ruda_syntax_has_direct_node(
    const RudaGreenNode *node,
    RudaSyntaxKind kind
);
const RudaGreenNode *ruda_syntax_find_kind_with_direct_token(
    const RudaGreenNode *node,
    RudaSyntaxKind kind,
    RudaTokenKind token_kind
);

#endif
