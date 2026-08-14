#ifndef RUDA_TEST_CHECKER_SUPPORT_H
#define RUDA_TEST_CHECKER_SUPPORT_H

#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/check/checker.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/lex/lexer.h"
#include "ruda/mir/mir.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"

typedef struct RudaCheckerFixture {
    RudaSourceManager sources;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaAstTree ast;
    bool sources_initialized;
    bool lexed_initialized;
    bool parsed_initialized;
    bool ast_initialized;
} RudaCheckerFixture;

bool ruda_checker_fixture_init_path(
    RudaCheckerFixture *fixture,
    const char *path,
    const char *source
);
bool ruda_checker_fixture_init(RudaCheckerFixture *fixture, const char *source);
void ruda_checker_fixture_dispose(RudaCheckerFixture *fixture);
void ruda_checker_expect_one_diagnostic(
    const char *source,
    const char *code,
    const char *marked_text,
    size_t occurrence
);
void ruda_checker_expect_one_diagnostic_path(
    const char *path,
    const char *source,
    const char *code,
    const char *marked_text
);
void ruda_checker_expect_diagnostic_message(
    const char *source,
    const char *expected_message
);
size_t ruda_test_mir_instruction_kind_count(
    const RudaMirFunction *function,
    RudaMirInstructionKind kind
);
const RudaMirInstruction *ruda_test_mir_instruction_kind_at(
    const RudaMirFunction *function,
    RudaMirInstructionKind kind,
    size_t occurrence
);

#endif
