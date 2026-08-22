#ifndef MMAY_TEST_CHECKER_SUPPORT_H
#define MMAY_TEST_CHECKER_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mmay/ast/ast.h"
#include "mmay/base/allocator.h"
#include "mmay/check/checker.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/lex/lexer.h"
#include "mmay/mir/mir.h"
#include "mmay/parse/parser.h"
#include "mmay/source/source_manager.h"

typedef struct MarimayCheckerFixture {
    MarimaySourceManager sources;
    MarimayLexResult lexed;
    MarimayParseResult parsed;
    MarimayAstTree ast;
    bool sources_initialized;
    bool lexed_initialized;
    bool parsed_initialized;
    bool ast_initialized;
} MarimayCheckerFixture;

bool mmay_checker_fixture_init_path(
    MarimayCheckerFixture *fixture,
    const char *path,
    const char *source
);
bool mmay_checker_fixture_init(MarimayCheckerFixture *fixture, const char *source);
void mmay_checker_fixture_dispose(MarimayCheckerFixture *fixture);
void mmay_checker_expect_one_diagnostic(
    const char *source,
    const char *code,
    const char *marked_text,
    size_t occurrence
);
void mmay_checker_expect_one_diagnostic_path(
    const char *path,
    const char *source,
    const char *code,
    const char *marked_text
);
void mmay_checker_expect_diagnostic_message(
    const char *source,
    const char *expected_message
);
size_t mmay_test_mir_instruction_kind_count(
    const MarimayMirFunction *function,
    MarimayMirInstructionKind kind
);
const MarimayMirInstruction *mmay_test_mir_instruction_kind_at(
    const MarimayMirFunction *function,
    MarimayMirInstructionKind kind,
    size_t occurrence
);

#endif
