#ifndef MMAY_TEST_PARSER_SUPPORT_H
#define MMAY_TEST_PARSER_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/lex/lexer.h"
#include "mmay/parse/parser.h"
#include "mmay/source/source_manager.h"
#include "mmay/syntax/syntax.h"

typedef struct MarimayParseFixture {
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *snapshot;
    MarimayLexResult lexed;
    MarimayParseResult parsed;
} MarimayParseFixture;

bool mmay_parse_fixture_init_bytes(
    MarimayParseFixture *fixture,
    const unsigned char *bytes,
    size_t length
);
bool mmay_parse_fixture_init(MarimayParseFixture *fixture, const char *text);
void mmay_parse_fixture_dispose(MarimayParseFixture *fixture);
bool mmay_parse_has_diagnostic(
    const MarimayParseResult *result,
    const char *code
);
size_t mmay_syntax_count_kind(const MarimayGreenNode *node, MarimaySyntaxKind kind);
size_t mmay_syntax_count_synthetic_tokens(const MarimayGreenNode *node);
const MarimayGreenNode *mmay_syntax_direct_nth_kind(
    const MarimayGreenNode *node,
    MarimaySyntaxKind kind,
    size_t ordinal
);
bool mmay_syntax_has_direct_node(
    const MarimayGreenNode *node,
    MarimaySyntaxKind kind
);
const MarimayGreenNode *mmay_syntax_find_kind_with_direct_token(
    const MarimayGreenNode *node,
    MarimaySyntaxKind kind,
    MarimayTokenKind token_kind
);

#endif
