#ifndef MMAY_TEST_FORMATTER_SUPPORT_H
#define MMAY_TEST_FORMATTER_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/ast/ast.h"
#include "mmay/base/allocator.h"
#include "mmay/base/utf8.h"
#include "mmay/format/formatter.h"
#include "mmay/lex/lexer.h"
#include "mmay/parse/parser.h"
#include "mmay/source/source_manager.h"

#include "semantic_oracle.h"

typedef struct MarimayFormatterFixture {
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *snapshot;
    MarimayLexResult lexed;
    MarimayParseResult parsed;
} MarimayFormatterFixture;

bool mmay_formatter_fixture_init(
    MarimayFormatterFixture *fixture,
    const char *path,
    const unsigned char *bytes,
    size_t length
);
void mmay_formatter_fixture_dispose(MarimayFormatterFixture *fixture);
bool mmay_formatter_ast_semantically_equal(
    const MarimayAstNode *left,
    const MarimaySourceSnapshot *left_source,
    const MarimayAstNode *right,
    const MarimaySourceSnapshot *right_source
);
bool mmay_formatter_comments_equal(
    const MarimayLexResult *left,
    const MarimayLexResult *right
);
bool mmay_formatter_read_file(
    const char *path,
    unsigned char *bytes,
    size_t capacity,
    size_t *length
);

#endif
