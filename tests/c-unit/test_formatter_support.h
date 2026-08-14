#ifndef RUDA_TEST_FORMATTER_SUPPORT_H
#define RUDA_TEST_FORMATTER_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/base/utf8.h"
#include "ruda/format/formatter.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"

#include "semantic_oracle.h"

typedef struct RudaFormatterFixture {
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot;
    RudaLexResult lexed;
    RudaParseResult parsed;
} RudaFormatterFixture;

bool ruda_formatter_fixture_init(
    RudaFormatterFixture *fixture,
    const char *path,
    const unsigned char *bytes,
    size_t length
);
void ruda_formatter_fixture_dispose(RudaFormatterFixture *fixture);
bool ruda_formatter_ast_semantically_equal(
    const RudaAstNode *left,
    const RudaSourceSnapshot *left_source,
    const RudaAstNode *right,
    const RudaSourceSnapshot *right_source
);
bool ruda_formatter_comments_equal(
    const RudaLexResult *left,
    const RudaLexResult *right
);
bool ruda_formatter_read_file(
    const char *path,
    unsigned char *bytes,
    size_t capacity,
    size_t *length
);

#endif
