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

static bool ruda_checker_fixture_init_path(
    RudaCheckerFixture *fixture,
    const char *path,
    const char *source
) {
    const RudaSourceSnapshot *snapshot = NULL;
    memset(fixture, 0, sizeof(*fixture));
    if (!ruda_source_manager_init(
            &fixture->sources,
            ruda_system_allocator()
        )) {
        return false;
    }
    fixture->sources_initialized = true;
    if (ruda_source_manager_add(
            &fixture->sources,
            path,
            strlen(path),
            source,
            strlen(source),
            &snapshot
        ) != RUDA_SOURCE_OK) {
        return false;
    }
    if (ruda_lex_snapshot(
            snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &fixture->lexed
        ) != RUDA_LEX_OK) {
        return false;
    }
    fixture->lexed_initialized = true;
    if (fixture->lexed.diagnostics.length != 0
        || ruda_parse_file(
            &fixture->lexed,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &fixture->parsed
        ) != RUDA_PARSE_OK) {
        return false;
    }
    fixture->parsed_initialized = true;
    if (fixture->parsed.diagnostics.length != 0
        || ruda_ast_project(
            &fixture->parsed.tree,
            ruda_system_allocator(),
            ruda_ast_default_options(),
            &fixture->ast
        ) != RUDA_AST_OK) {
        return false;
    }
    fixture->ast_initialized = true;
    return true;
}

static bool ruda_checker_fixture_init(
    RudaCheckerFixture *fixture,
    const char *source
) {
    return ruda_checker_fixture_init_path(fixture, "checker.ruda", source);
}

static void ruda_checker_fixture_dispose(RudaCheckerFixture *fixture) {
    if (fixture->ast_initialized) {
        ruda_ast_tree_dispose(&fixture->ast);
    }
    if (fixture->parsed_initialized) {
        ruda_parse_result_dispose(&fixture->parsed);
    }
    if (fixture->lexed_initialized) {
        ruda_lex_result_dispose(&fixture->lexed);
    }
    if (fixture->sources_initialized) {
        ruda_source_manager_dispose(&fixture->sources);
    }
    memset(fixture, 0, sizeof(*fixture));
}

static void ruda_checker_expect_one_diagnostic(
    const char *source,
    const char *code,
    const char *marked_text,
    size_t occurrence
) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckStatus status;
    const RudaCheckDiagnostic *diagnostic = NULL;
    const char *marked = source;
    size_t index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    for (index = 0; index <= occurrence && marked != NULL; index += 1) {
        marked = strstr(marked, marked_text);
        if (index != occurrence && marked != NULL) {
            marked += strlen(marked_text);
        }
    }
    RUDA_EXPECT(marked != NULL);
    status = ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    );
    if (status != RUDA_CHECK_OK) {
        fprintf(
            stderr,
            "checker diagnostic fixture for %s returned status %d\n",
            code,
            (int)status
        );
    }
    RUDA_EXPECT(status == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        RUDA_EXPECT(checked.constants.length == 0);
        RUDA_EXPECT(checked.type_aliases.length == 0);
        RUDA_EXPECT(checked.records.length == 0);
        RUDA_EXPECT(checked.enums.length == 0);
        RUDA_EXPECT(checked.function_index.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL && marked != NULL) {
            if (diagnostic->start != (size_t)(marked - source)
                || diagnostic->end
                    != (size_t)(marked - source) + strlen(marked_text)) {
                fprintf(
                    stderr,
                    "checker diagnostic span mismatch for %s: expected %zu..%zu, got %zu..%zu\n",
                    code,
                    (size_t)(marked - source),
                    (size_t)(marked - source) + strlen(marked_text),
                    diagnostic->start,
                    diagnostic->end
                );
            }
            RUDA_EXPECT(diagnostic->snapshot == fixture.ast.snapshot);
            RUDA_EXPECT(strcmp(diagnostic->code, code) == 0);
            RUDA_EXPECT(diagnostic->start == (size_t)(marked - source));
            RUDA_EXPECT(diagnostic->end
                == (size_t)(marked - source) + strlen(marked_text));
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_checker_expect_one_diagnostic_path(
    const char *path,
    const char *source,
    const char *code,
    const char *marked_text
) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckDiagnostic *diagnostic = NULL;
    const char *marked = strstr(source, marked_text);
    RUDA_EXPECT(ruda_checker_fixture_init_path(&fixture, path, source));
    RUDA_EXPECT(marked != NULL);
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker-path",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL && marked != NULL) {
            RUDA_EXPECT(strcmp(diagnostic->code, code) == 0);
            RUDA_EXPECT(diagnostic->start == (size_t)(marked - source));
            RUDA_EXPECT(diagnostic->end
                == (size_t)(marked - source) + strlen(marked_text));
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_checker_expect_diagnostic_message(
    const char *source,
    const char *expected_message
) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckDiagnostic *diagnostic = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker-message",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH
            ) == 0);
            RUDA_EXPECT(diagnostic->message != NULL);
            if (diagnostic->message != NULL) {
                RUDA_EXPECT(strcmp(
                    diagnostic->message,
                    expected_message
                ) == 0);
            }
            RUDA_EXPECT(diagnostic->owned_message.data != NULL);
            RUDA_EXPECT(diagnostic->owned_message.length
                == strlen(expected_message));
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_vertical_slice(void) {
    static const char source[] =
        "fn calc(a: int, b: int, c: int): int {\n"
        "  return a + b * c;\n"
        "}\n"
        "fn atLeast(a: int, b: int): bool {\n"
        "  return a >= b;\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *calc = NULL;
    const RudaCheckedFunction *at_least = NULL;
    const RudaTypeDescriptor *signature = NULL;
    RudaMirValue arguments[3];
    RudaMirValue result;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 2);
        calc = ruda_checked_function(&checked, 0);
        at_least = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(calc != NULL);
        RUDA_EXPECT(at_least != NULL);
        if (calc != NULL) {
            RUDA_EXPECT(calc->name.length == strlen("calc"));
            RUDA_EXPECT(memcmp(calc->name.data, "calc", strlen("calc")) == 0);
            signature = ruda_type_descriptor(
                &checked.types,
                calc->signature_type
            );
            RUDA_EXPECT(signature != NULL);
            if (signature != NULL) {
                RUDA_EXPECT(signature->kind == RUDA_TYPE_KIND_FUNCTION);
                RUDA_EXPECT(signature->parameter_count == 3);
                RUDA_EXPECT(signature->argument_count == 4);
                RUDA_EXPECT(signature->arguments[0] == RUDA_TYPE_I64);
                RUDA_EXPECT(signature->arguments[1] == RUDA_TYPE_I64);
                RUDA_EXPECT(signature->arguments[2] == RUDA_TYPE_I64);
                RUDA_EXPECT(signature->arguments[3] == RUDA_TYPE_I64);
            }
            arguments[0].type = RUDA_TYPE_I64;
            arguments[0].as.i64 = 2;
            arguments[1].type = RUDA_TYPE_I64;
            arguments[1].as.i64 = 3;
            arguments[2].type = RUDA_TYPE_I64;
            arguments[2].as.i64 = 4;
            RUDA_EXPECT(ruda_mir_interpret(
                &calc->mir,
                arguments,
                3,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 14);
        }
        if (at_least != NULL) {
            arguments[0].type = RUDA_TYPE_I64;
            arguments[0].as.i64 = 4;
            arguments[1].type = RUDA_TYPE_I64;
            arguments[1].as.i64 = 3;
            RUDA_EXPECT(ruda_mir_interpret(
                &at_least->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_BOOL);
            RUDA_EXPECT(result.as.boolean);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_diagnostics(void) {
    static const char unknown[] =
        "fn bad(): int { return missing; }\n";
    static const char duplicate[] =
        "fn bad(): int { const value: int = 1; let value: int = 2; return value; }\n";
    static const char mismatch[] =
        "fn bad(): int { return true; }\n";
    static const char immutable[] =
        "fn bad(): int { const value: int = 1; value = 2; return value; }\n";
    static const char uninitialized[] =
        "fn bad(): int { let value: int; return value; }\n";
    static const char out_of_range[] =
        "fn bad(): int { return 9223372036854775808; }\n";
    static const char unknown_type[] =
        "fn bad(value: Mystery): int { return 0; }\n";
    static const char invalid_operator[] =
        "fn bad(): int { return true + false; }\n";
    static const char duplicate_parameter[] =
        "fn bad(value: int, value: int): int { return value; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckDiagnostic *diagnostic = NULL;

    ruda_checker_expect_one_diagnostic(
        unknown,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        duplicate,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "value",
        1
    );
    ruda_checker_expect_one_diagnostic(
        mismatch,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        immutable,
        RUDA_DIAGNOSTIC_ASSIGN_IMMUTABLE,
        "value",
        1
    );
    ruda_checker_expect_one_diagnostic(
        uninitialized,
        RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
        "value",
        1
    );
    ruda_checker_expect_one_diagnostic(
        out_of_range,
        RUDA_DIAGNOSTIC_INTEGER_LITERAL_RANGE,
        "9223372036854775808",
        0
    );
    ruda_checker_expect_one_diagnostic(
        unknown_type,
        RUDA_DIAGNOSTIC_UNKNOWN_TYPE,
        "Mystery",
        0
    );
    ruda_checker_expect_one_diagnostic(
        invalid_operator,
        RUDA_DIAGNOSTIC_INVALID_OPERATOR,
        "+",
        0
    );
    ruda_checker_expect_one_diagnostic(
        duplicate_parameter,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "value",
        1
    );

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn bad(): int { const value: int = 1; }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_MISSING_RETURN
            ) == 0);
            RUDA_EXPECT(diagnostic->start == diagnostic->end);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_signature_phase(void) {
    static const char duplicate_before_body[] =
        "fn repeated(): int { return unavailable(); }\n"
        "fn repeated(): int { return 1; }\n";
    static const char canonical[] =
        "fn first(value: int): int { return value; }\n"
        "fn second(value: int): int { return value; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *first = NULL;
    const RudaCheckedFunction *second = NULL;
    ruda_checker_expect_one_diagnostic(
        duplicate_before_body,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "repeated",
        1
    );
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, canonical));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 2);
        first = ruda_checked_function(&checked, 0);
        second = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(first != NULL);
        RUDA_EXPECT(second != NULL);
        if (first != NULL && second != NULL) {
            RUDA_EXPECT(first->signature_type == second->signature_type);
            RUDA_EXPECT(!ruda_symbol_id_equal(
                first->mir.symbol,
                second->mir.symbol
            ));
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_function_attributes_and_panic_shadow(void) {
    static const char attributed[] =
        "@test\n"
        "fn verifies(): void { return; }\n"
        "@fuzz\n"
        "fn fuzzInput(input: int): void { const copied = input; }\n";
    static const char shadow[] =
        "fn panic(message: string): string { return message; }\n"
        "fn callShadow(): string { return panic(\"ordinary\"); }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *test_function = NULL;
    const RudaCheckedFunction *fuzz_function = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &fixture,
        "attributes_test.ruda",
        attributed
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/attributes",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 2);
        test_function = ruda_checked_function(&checked, 0);
        fuzz_function = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(test_function != NULL && test_function->is_test);
        RUDA_EXPECT(test_function != NULL && !test_function->is_fuzz);
        RUDA_EXPECT(fuzz_function != NULL && !fuzz_function->is_test);
        RUDA_EXPECT(fuzz_function != NULL && fuzz_function->is_fuzz);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, shadow));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/panic-shadow",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 2);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic_path(
        "invalid_test.ruda",
        "@test(1) fn invalid(): void {}\n",
        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "@test(1)"
    );
    ruda_checker_expect_one_diagnostic_path(
        "invalid_test.ruda",
        "@fuzz fn invalid(): void {}\n",
        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "invalid"
    );
    ruda_checker_expect_one_diagnostic_path(
        "invalid_test.ruda",
        "@test public fn invalid(): void {}\n",
        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "invalid"
    );
    ruda_checker_expect_one_diagnostic_path(
        "invalid_test.ruda",
        "@mystery fn invalid(): void {}\n",
        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "@mystery"
    );
    ruda_checker_expect_one_diagnostic_path(
        "invalid.ruda",
        "@test fn invalid(): void {}\n",
        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "invalid"
    );
}

static void ruda_test_checker_contextual_word_symbols(void) {
    static const char source[] =
        "type Context { as: int; break: int; case: int; const: int; "
        "continue: int; else: int; enum: int; extern: int; fn: int; "
        "for: int; from: int; import: int; interface: int; let: int; "
        "mutable: int; of: int; public: int; scope: int; type: int; "
        "unsafe: int; yield: int; }\n"
        "type C17Words { auto: int; break: int; case: int; char: int; "
        "const: int; continue: int; default: int; do: int; double: int; "
        "else: int; enum: int; extern: int; float: int; for: int; "
        "goto: int; inline: int; int: int; long: int; register: int; "
        "restrict: int; short: int; signed: int; sizeof: int; static: int; "
        "struct: int; switch: int; typedef: int; union: int; unsigned: int; "
        "void: int; volatile: int; }\n"
        "fn as(): int { return 1; }\n"
        "fn break(): int { return 2; }\n"
        "fn case(): int { return 3; }\n"
        "fn const(): int { return 4; }\n"
        "fn continue(): int { return 5; }\n"
        "fn else(): int { return 6; }\n"
        "fn enum(): int { return 7; }\n"
        "fn extern(): int { return 8; }\n"
        "fn fn(): int { return 9; }\n"
        "fn for(): int { return 10; }\n"
        "fn from(): int { return 11; }\n"
        "fn import(): int { return 12; }\n"
        "fn interface(): int { return 13; }\n"
        "fn let(): int { return 14; }\n"
        "fn mutable(): int { return 15; }\n"
        "fn of(): int { return 16; }\n"
        "fn public(): int { return 17; }\n"
        "fn scope(): int { return 18; }\n"
        "fn type(): int { return 19; }\n"
        "fn unsafe(): int { return 20; }\n"
        "fn yield(): int { return 21; }\n"
        "fn total(): int {\n"
        "  return as() + break() + case() + const() + continue() + else() "
        "+ enum() + extern() + fn() + for() + from() + import() "
        "+ interface() + let() + mutable() + of() + public() + scope() "
        "+ type() + unsafe() + yield();\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue result;
    const RudaCheckedFunction *total = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/contextual-word-symbols",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.records.length == 2);
        RUDA_EXPECT(checked.functions.length == 22);
        total = ruda_checked_function(&checked, 21);
        RUDA_EXPECT(total != NULL);
        if (total != NULL) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                21,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 231);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_package_sources(void) {
    static const char entry_source[] =
        "fn entry(value: Counter): Counter { return helper(value) + offset; }\n";
    static const char helper_source[] =
        "type Base = int;\n"
        "type Counter = Base;\n"
        "const offset: Counter = 1;\n"
        "fn helper(value: Counter): Base { return value; }\n";
    static const char first_duplicate[] =
        "fn repeated(): int { return 1; }\n";
    static const char second_duplicate[] =
        "fn repeated(): int { return 2; }\n";
    RudaCheckerFixture entry;
    RudaCheckerFixture helper;
    const RudaAstTree *asts[2];
    RudaCheckResult checked;
    const RudaCheckedFunction *entry_function = NULL;
    const RudaCheckedFunction *helper_function = NULL;
    const RudaCheckedConstant *offset = NULL;
    const RudaCheckedTypeAlias *base = NULL;
    const RudaCheckedTypeAlias *counter = NULL;
    const RudaCheckDiagnostic *diagnostic = NULL;
    RudaSymbolId entry_symbol;
    RudaSymbolId helper_symbol;
    RudaSymbolId offset_symbol;
    RudaSymbolId base_symbol;
    RudaSymbolId counter_symbol;
    RudaMirValue argument;
    RudaMirValue value;
    RudaCheckOptions options;

    memset(&entry_symbol, 0, sizeof(entry_symbol));
    memset(&helper_symbol, 0, sizeof(helper_symbol));
    memset(&offset_symbol, 0, sizeof(offset_symbol));
    memset(&base_symbol, 0, sizeof(base_symbol));
    memset(&counter_symbol, 0, sizeof(counter_symbol));
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &entry,
        "a_entry.ruda",
        entry_source
    ));
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &helper,
        "z_helper.ruda",
        helper_source
    ));
    asts[0] = &helper.ast;
    asts[1] = &entry.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.snapshot == entry.ast.snapshot);
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 2);
        RUDA_EXPECT(checked.constants.length == 1);
        RUDA_EXPECT(checked.type_aliases.length == 2);
        RUDA_EXPECT(checked.function_index.length == 2);
        entry_function = ruda_checked_function(&checked, 0);
        helper_function = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(entry_function != NULL);
        RUDA_EXPECT(helper_function != NULL);
        offset = ruda_checked_constant(&checked, 0);
        base = ruda_checked_type_alias(&checked, 0);
        counter = ruda_checked_type_alias(&checked, 1);
        RUDA_EXPECT(offset != NULL);
        RUDA_EXPECT(base != NULL);
        RUDA_EXPECT(counter != NULL);
        if (entry_function != NULL && helper_function != NULL) {
            RUDA_EXPECT(entry_function->name.length == strlen("entry"));
            RUDA_EXPECT(memcmp(
                entry_function->name.data,
                "entry",
                strlen("entry")
            ) == 0);
            RUDA_EXPECT(helper_function->name.length == strlen("helper"));
            entry_symbol = entry_function->mir.symbol;
            helper_symbol = helper_function->mir.symbol;
            if (offset != NULL) {
                RUDA_EXPECT(offset->name.length == strlen("offset"));
                RUDA_EXPECT(offset->value.type == RUDA_TYPE_I64);
                RUDA_EXPECT(offset->value.as.i64 == 1);
                offset_symbol = offset->symbol;
            }
            if (base != NULL && counter != NULL) {
                RUDA_EXPECT(base->target_type == RUDA_TYPE_I64);
                RUDA_EXPECT(counter->target_type == RUDA_TYPE_I64);
                base_symbol = base->symbol;
                counter_symbol = counter->symbol;
            }
            argument.type = RUDA_TYPE_I64;
            argument.as.i64 = 41;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                0,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &value
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(value.type == RUDA_TYPE_I64);
            RUDA_EXPECT(value.as.i64 == 42);
        }
        ruda_check_result_dispose(&checked);
    }

    options = ruda_check_default_options();
    options.max_source_files = 1;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/package",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);

    asts[0] = &entry.ast;
    asts[1] = &helper.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        entry_function = ruda_checked_function(&checked, 0);
        helper_function = ruda_checked_function(&checked, 1);
        offset = ruda_checked_constant(&checked, 0);
        base = ruda_checked_type_alias(&checked, 0);
        counter = ruda_checked_type_alias(&checked, 1);
        RUDA_EXPECT(entry_function != NULL);
        RUDA_EXPECT(helper_function != NULL);
        RUDA_EXPECT(offset != NULL);
        RUDA_EXPECT(base != NULL);
        RUDA_EXPECT(counter != NULL);
        if (entry_function != NULL && helper_function != NULL) {
            RUDA_EXPECT(ruda_symbol_id_equal(
                entry_function->mir.symbol,
                entry_symbol
            ));
            RUDA_EXPECT(ruda_symbol_id_equal(
                helper_function->mir.symbol,
                helper_symbol
            ));
            if (offset != NULL) {
                RUDA_EXPECT(ruda_symbol_id_equal(
                    offset->symbol,
                    offset_symbol
                ));
            }
            if (base != NULL && counter != NULL) {
                RUDA_EXPECT(ruda_symbol_id_equal(base->symbol, base_symbol));
                RUDA_EXPECT(ruda_symbol_id_equal(
                    counter->symbol,
                    counter_symbol
                ));
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&helper);
    ruda_checker_fixture_dispose(&entry);

    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &entry,
        "a_first.ruda",
        first_duplicate
    ));
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &helper,
        "z_second.ruda",
        second_duplicate
    ));
    asts[0] = &helper.ast;
    asts[1] = &entry.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        RUDA_EXPECT(checked.function_index.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME
            ) == 0);
            RUDA_EXPECT(diagnostic->snapshot == helper.ast.snapshot);
            RUDA_EXPECT(strcmp(
                ruda_source_path(diagnostic->snapshot),
                "z_second.ruda"
            ) == 0);
        }
        ruda_check_result_dispose(&checked);
    }

    asts[0] = &entry.ast;
    asts[1] = &entry.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INVALID_ARGUMENT);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&helper);
    ruda_checker_fixture_dispose(&entry);
}

static void ruda_test_checker_declaration_index_growth(void) {
    char source[32768];
    size_t length = 0;
    size_t index = 0;
    int written = 0;
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue value;
    const RudaCheckDiagnostic *diagnostic = NULL;

    written = snprintf(
        source,
        sizeof(source),
        "fn entry(): int { return function_255(); }\n"
    );
    RUDA_EXPECT(written > 0 && (size_t)written < sizeof(source));
    if (written <= 0 || (size_t)written >= sizeof(source)) {
        return;
    }
    length = (size_t)written;
    for (index = 0; index < 256; index += 1) {
        written = snprintf(
            source + length,
            sizeof(source) - length,
            "fn function_%03zu(): int { return %zu; }\n",
            index,
            index
        );
        RUDA_EXPECT(written > 0
            && (size_t)written < sizeof(source) - length);
        if (written <= 0 || (size_t)written >= sizeof(source) - length) {
            return;
        }
        length += (size_t)written;
    }
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/index-growth",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 257);
        RUDA_EXPECT(checked.function_index.length == 257);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(value.type == RUDA_TYPE_I64);
        RUDA_EXPECT(value.as.i64 == 255);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    written = snprintf(
        source + length,
        sizeof(source) - length,
        "fn function_127(): int { return 999; }\n"
    );
    RUDA_EXPECT(written > 0 && (size_t)written < sizeof(source) - length);
    if (written <= 0 || (size_t)written >= sizeof(source) - length) {
        return;
    }
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/index-growth",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME
            ) == 0);
            RUDA_EXPECT(diagnostic->snapshot == fixture.ast.snapshot);
            RUDA_EXPECT(diagnostic->end - diagnostic->start
                == strlen("function_127"));
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_type_aliases(void) {
    static const char source[] =
        "type UserId = Later;\n"
        "type Later = int;\n"
        "const answer: UserId = 42;\n"
        "fn identity(value: UserId): Later { return value; }\n";
    static const char cycle[] =
        "type First = Second;\n"
        "type Second = First;\n";
    static const char unknown[] =
        "type Known = Missing;\n";
    static const char primitive_redefinition[] =
        "type int = bool;\n";
    static const char duplicate_value[] =
        "fn Shared(): int { return 1; }\n"
        "type Shared = int;\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckOptions options;
    const RudaCheckedTypeAlias *user_id = NULL;
    const RudaCheckedTypeAlias *later = NULL;
    const RudaCheckedConstant *answer = NULL;
    const RudaCheckedFunction *identity = NULL;
    const RudaTypeDescriptor *signature = NULL;
    RudaMirValue argument;
    RudaMirValue result;

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/type-aliases",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.type_aliases.length == 2);
        RUDA_EXPECT(checked.constants.length == 1);
        RUDA_EXPECT(checked.functions.length == 1);
        user_id = ruda_checked_type_alias(&checked, 0);
        later = ruda_checked_type_alias(&checked, 1);
        answer = ruda_checked_constant(&checked, 0);
        identity = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(user_id != NULL);
        RUDA_EXPECT(later != NULL);
        RUDA_EXPECT(answer != NULL);
        RUDA_EXPECT(identity != NULL);
        if (user_id != NULL && later != NULL) {
            RUDA_EXPECT(user_id->target_type == RUDA_TYPE_I64);
            RUDA_EXPECT(later->target_type == RUDA_TYPE_I64);
            RUDA_EXPECT(!ruda_symbol_id_equal(
                user_id->symbol,
                later->symbol
            ));
        }
        if (answer != NULL) {
            RUDA_EXPECT(answer->value.type == RUDA_TYPE_I64);
            RUDA_EXPECT(answer->value.as.i64 == 42);
        }
        if (identity != NULL) {
            signature = ruda_type_descriptor(
                &checked.types,
                identity->signature_type
            );
            RUDA_EXPECT(signature != NULL);
            if (signature != NULL) {
                RUDA_EXPECT(signature->argument_count == 2);
                RUDA_EXPECT(signature->arguments[0] == RUDA_TYPE_I64);
                RUDA_EXPECT(signature->arguments[1] == RUDA_TYPE_I64);
            }
            argument.type = RUDA_TYPE_I64;
            argument.as.i64 = 17;
            RUDA_EXPECT(ruda_mir_interpret(
                &identity->mir,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 17);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        cycle,
        RUDA_DIAGNOSTIC_TYPE_ALIAS_CYCLE,
        "First",
        1
    );
    ruda_checker_expect_one_diagnostic(
        unknown,
        RUDA_DIAGNOSTIC_UNKNOWN_TYPE,
        "Missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        primitive_redefinition,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "int",
        0
    );
    ruda_checker_expect_one_diagnostic(
        duplicate_value,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "Shared",
        0
    );

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    options = ruda_check_default_options();
    options.max_type_aliases = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/type-aliases",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.max_type_alias_depth = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/type-aliases",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "type Generic<T> = T;\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/type-aliases",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

}

static void ruda_test_checker_record_declarations(void) {
    static const char source[] =
        "public type UserId = int;\n"
        "public type User {\n"
        "  public id: UserId;\n"
        "  mutable parent: User;\n"
        "}\n"
        "public type Shadow { public id: UserId; mutable parent: User; }\n"
        "fn identity(value: User): User { return value; }\n";
    static const char duplicate_field[] =
        "type User { id: int; id: string; }\n";
    static const char unknown_field_type[] =
        "type User { value: Missing; }\n";
    static const char primitive_redefinition[] =
        "type int { value: int; }\n";
    static const char duplicate_top_level[] =
        "type Shared = int;\n"
        "type Shared { value: int; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckOptions options;
    const RudaCheckedRecord *user = NULL;
    const RudaCheckedRecord *shadow = NULL;
    const RudaCheckedRecordField *id = NULL;
    const RudaCheckedRecordField *parent = NULL;
    const RudaCheckedFunction *identity = NULL;
    const RudaTypeDescriptor *nominal = NULL;
    const RudaTypeDescriptor *signature = NULL;

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/records",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.records.length == 2);
        RUDA_EXPECT(checked.type_aliases.length == 1);
        RUDA_EXPECT(checked.functions.length == 1);
        user = ruda_checked_record(&checked, 0);
        shadow = ruda_checked_record(&checked, 1);
        identity = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(user != NULL);
        RUDA_EXPECT(shadow != NULL);
        RUDA_EXPECT(identity != NULL);
        if (user != NULL) {
            RUDA_EXPECT(user->name.length == strlen("User"));
            RUDA_EXPECT(memcmp(user->name.data, "User", strlen("User")) == 0);
            RUDA_EXPECT(user->is_public);
            RUDA_EXPECT(user->fields.length == 2);
            RUDA_EXPECT(ruda_type_kind(&checked.types, user->type)
                == RUDA_TYPE_KIND_NOMINAL);
            nominal = ruda_type_descriptor(&checked.types, user->type);
            RUDA_EXPECT(nominal != NULL);
            if (nominal != NULL) {
                RUDA_EXPECT(ruda_symbol_id_equal(nominal->symbol, user->symbol));
                RUDA_EXPECT(nominal->argument_count == 0);
            }
            id = ruda_vector_at_const(&user->fields, 0);
            parent = ruda_vector_at_const(&user->fields, 1);
            RUDA_EXPECT(id != NULL);
            RUDA_EXPECT(parent != NULL);
            if (id != NULL) {
                RUDA_EXPECT(id->name.length == strlen("id"));
                RUDA_EXPECT(id->type == RUDA_TYPE_I64);
                RUDA_EXPECT(id->is_public);
                RUDA_EXPECT(!id->is_mutable);
            }
            if (parent != NULL) {
                RUDA_EXPECT(parent->name.length == strlen("parent"));
                RUDA_EXPECT(parent->type == user->type);
                RUDA_EXPECT(!parent->is_public);
                RUDA_EXPECT(parent->is_mutable);
            }
        }
        if (user != NULL && shadow != NULL) {
            RUDA_EXPECT(shadow->is_public);
            RUDA_EXPECT(shadow->fields.length == user->fields.length);
            RUDA_EXPECT(shadow->type != user->type);
            RUDA_EXPECT(!ruda_symbol_id_equal(shadow->symbol, user->symbol));
        }
        if (user != NULL && identity != NULL) {
            signature = ruda_type_descriptor(
                &checked.types,
                identity->signature_type
            );
            RUDA_EXPECT(signature != NULL);
            if (signature != NULL) {
                RUDA_EXPECT(signature->argument_count == 2);
                RUDA_EXPECT(signature->arguments[0] == user->type);
                RUDA_EXPECT(signature->arguments[1] == user->type);
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "type First { second: Second; }\n"
        "type Second { first: First; }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/records",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedRecord *first = ruda_checked_record(&checked, 0);
        const RudaCheckedRecord *second = ruda_checked_record(&checked, 1);
        const RudaCheckedRecordField *first_field = first != NULL
            ? ruda_vector_at_const(&first->fields, 0)
            : NULL;
        const RudaCheckedRecordField *second_field = second != NULL
            ? ruda_vector_at_const(&second->fields, 0)
            : NULL;
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(first != NULL);
        RUDA_EXPECT(second != NULL);
        RUDA_EXPECT(first_field != NULL);
        RUDA_EXPECT(second_field != NULL);
        if (first != NULL && second != NULL && first_field != NULL
            && second_field != NULL) {
            RUDA_EXPECT(first_field->type == second->type);
            RUDA_EXPECT(second_field->type == first->type);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        duplicate_field,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "id",
        1
    );
    ruda_checker_expect_one_diagnostic(
        unknown_field_type,
        RUDA_DIAGNOSTIC_UNKNOWN_TYPE,
        "Missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        primitive_redefinition,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "int",
        0
    );
    ruda_checker_expect_one_diagnostic(
        duplicate_top_level,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "Shared",
        1
    );

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    options = ruda_check_default_options();
    options.max_records = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/records",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.max_fields_per_record = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/records",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "type Generic<T> { value: T; }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/records",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "@derive(Debug) type User { id: int; }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/records",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

}

static void ruda_test_checker_record_package_order(void) {
    static const char entry_source[] =
        "fn keep(value: Node): Node { return value; }\n"
        "fn build(value: int): int {\n"
        "  const leaf = Leaf { value };\n"
        "  return leaf.value;\n"
        "}\n";
    static const char types_source[] =
        "type Alias = Node;\n"
        "type Node { public id: int; next: Alias; }\n"
        "type Leaf { value: int; }\n";
    RudaCheckerFixture entry;
    RudaCheckerFixture types;
    const RudaAstTree *asts[2];
    RudaCheckResult checked;
    const RudaCheckedRecord *node = NULL;
    const RudaCheckedTypeAlias *alias = NULL;
    const RudaCheckedFunction *keep = NULL;
    RudaMirValue argument;
    RudaMirValue result;
    const RudaTypeDescriptor *signature = NULL;
    RudaSymbolId symbol;
    RudaTypeId type = RUDA_TYPE_INVALID;
    memset(&symbol, 0, sizeof(symbol));

    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &entry,
        "a_entry.ruda",
        entry_source
    ));
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &types,
        "z_types.ruda",
        types_source
    ));
    asts[0] = &types.ast;
    asts[1] = &entry.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/record-package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.records.length == 2);
        RUDA_EXPECT(checked.type_aliases.length == 1);
        RUDA_EXPECT(checked.functions.length == 2);
        node = ruda_checked_record(&checked, 0);
        alias = ruda_checked_type_alias(&checked, 0);
        keep = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(node != NULL);
        RUDA_EXPECT(alias != NULL);
        RUDA_EXPECT(keep != NULL);
        if (node != NULL) {
            RUDA_EXPECT(node->fields.length == 2);
            symbol = node->symbol;
            type = node->type;
        }
        if (node != NULL && alias != NULL) {
            RUDA_EXPECT(alias->target_type == node->type);
        }
        if (node != NULL && keep != NULL) {
            signature = ruda_type_descriptor(&checked.types, keep->signature_type);
            RUDA_EXPECT(signature != NULL);
            if (signature != NULL) {
                RUDA_EXPECT(signature->arguments[0] == node->type);
                RUDA_EXPECT(signature->arguments[1] == node->type);
            }
        }
        argument.type = RUDA_TYPE_I64;
        argument.as.i64 = 42;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64);
        RUDA_EXPECT(result.as.i64 == 42);
        ruda_check_result_dispose(&checked);
    }

    asts[0] = &entry.ast;
    asts[1] = &types.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/record-package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        node = ruda_checked_record(&checked, 0);
        RUDA_EXPECT(node != NULL);
        if (node != NULL) {
            RUDA_EXPECT(ruda_symbol_id_equal(node->symbol, symbol));
            RUDA_EXPECT(node->type == type);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&types);
    ruda_checker_fixture_dispose(&entry);
}

static void ruda_test_checker_record_execution(void) {
    static const char source[] =
        "type Counter {\n"
        "  seed: int;\n"
        "  mutable value: int;\n"
        "}\n"
        "type CounterAlias = Counter;\n"
        "fn make(seed: int): Counter {\n"
        "  return CounterAlias { value: seed, seed };\n"
        "}\n"
        "fn read(counter: Counter): int { return counter.value; }\n"
        "fn aliasing(seed: int): int {\n"
        "  const counter = Counter { seed, value: 1 };\n"
        "  const alias = counter;\n"
        "  counter.value += seed;\n"
        "  return alias.value;\n"
        "}\n"
        "fn adjust(counter: Counter, delta: int): int {\n"
        "  counter.value = counter.value + delta;\n"
        "  return counter.value;\n"
        "}\n"
        "fn acrossCall(seed: int): int {\n"
        "  const counter = make(seed);\n"
        "  const alias = counter;\n"
        "  adjust(counter, 1);\n"
        "  return alias.value;\n"
        "}\n"
        "fn overflow(): int { return 9223372036854775807 + 1; }\n"
        "fn divideZero(): int { return 1 / 0; }\n"
        "fn order(): int {\n"
        "  const counter = Counter { value: divideZero(), seed: overflow() };\n"
        "  return counter.value;\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *make = NULL;
    const RudaCheckedFunction *read = NULL;
    const RudaCheckedFunction *aliasing = NULL;
    const RudaCheckedFunction *across_call = NULL;
    const RudaCheckedFunction *order = NULL;
    RudaMirValue argument;
    RudaMirValue result;
    RudaMirValue record_argument;
    RudaCheckInterpretOptions interpret_options =
        ruda_check_interpret_default_options();
    RudaMirExecution execution;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/record-execution",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.records.length == 1);
        RUDA_EXPECT(checked.type_aliases.length == 1);
        RUDA_EXPECT(checked.functions.length == 8);
        make = ruda_checked_function(&checked, 0);
        read = ruda_checked_function(&checked, 1);
        aliasing = ruda_checked_function(&checked, 2);
        across_call = ruda_checked_function(&checked, 4);
        order = ruda_checked_function(&checked, 7);
        RUDA_EXPECT(make != NULL);
        RUDA_EXPECT(read != NULL);
        RUDA_EXPECT(aliasing != NULL);
        RUDA_EXPECT(across_call != NULL);
        RUDA_EXPECT(order != NULL);
        if (make != NULL) {
            RUDA_EXPECT(make->mir.records.length == 1);
            RUDA_EXPECT(make->mir.record_fields.length == 2);
            RUDA_EXPECT(make->mir.record_constructions.length == 1);
            RUDA_EXPECT(make->mir.record_arguments.length == 2);
        }
        if (read != NULL) {
            RUDA_EXPECT(read->mir.records.length == 1);
            RUDA_EXPECT(read->mir.record_constructions.length == 0);
        }
        argument.type = RUDA_TYPE_I64;
        argument.as.i64 = 4;
        if (aliasing != NULL) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                2,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 5);
            {
                size_t failure_index = 0;
                bool reached_success = false;
                for (failure_index = 0; failure_index < 32;
                    failure_index += 1) {
                    RudaFailAllocator state;
                    RudaAllocator allocator;
                    RudaMirInterpretStatus status;
                    ruda_fail_allocator_init(
                        &state,
                        ruda_system_allocator(),
                        failure_index
                    );
                    allocator = ruda_fail_allocator(&state);
                    status = ruda_check_interpret_function(
                        &checked,
                        2,
                        &argument,
                        1,
                        allocator,
                        ruda_check_interpret_default_options(),
                        &result
                    );
                    if (status == RUDA_MIR_INTERPRET_OK) {
                        reached_success = true;
                        RUDA_EXPECT(result.as.i64 == 5);
                    } else {
                        RUDA_EXPECT(
                            status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED
                        );
                    }
                    RUDA_EXPECT(state.live_allocations == 0);
                    RUDA_EXPECT(state.live_bytes == 0);
                    if (reached_success) {
                        break;
                    }
                }
                RUDA_EXPECT(reached_success);
            }
        }
        if (across_call != NULL) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                4,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 5);
        }
        if (order != NULL) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                7,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        }
        if (make != NULL && read != NULL) {
            argument.as.i64 = 7;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                0,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
            RUDA_EXPECT(ruda_mir_execution_init(
                &execution,
                ruda_system_allocator(),
                ruda_mir_execution_default_options()
            ));
            interpret_options.execution = &execution;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                0,
                &argument,
                1,
                ruda_system_allocator(),
                interpret_options,
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == make->mir.result_type);
            RUDA_EXPECT(ruda_mir_execution_owns_reference(
                &execution,
                result
            ));
            RUDA_EXPECT(execution.objects.length == 1);
            record_argument = result;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                1,
                &record_argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                1,
                &record_argument,
                1,
                ruda_system_allocator(),
                interpret_options,
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 7);
            record_argument.as.reference = RUDA_MIR_INVALID_REFERENCE;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                1,
                &record_argument,
                1,
                ruda_system_allocator(),
                interpret_options,
                &result
            ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
            ruda_mir_execution_dispose(&execution);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "fn bad(): Counter { return Counter { other: 1 }; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
        "other",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "fn bad(): Counter { return Counter { value: 1, value: 2 }; }\n",
        RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
        "value",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { seed: int; value: int; }\n"
        "fn bad(): Counter { return Counter { value: 1 }; }\n",
        RUDA_DIAGNOSTIC_MISSING_FIELD,
        "Counter { value: 1 }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "fn bad(): Counter { return Counter { value: true }; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { seed: int; }\n"
        "fn bad(counter: Counter): int { counter.seed = 1; return 0; }\n",
        RUDA_DIAGNOSTIC_ASSIGN_IMMUTABLE,
        "seed",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "fn bad(counter: Counter): int { return counter.other; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
        "other",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "fn bad(): Counter { return Counter { value }; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "value",
        1
    );
}

static size_t ruda_test_mir_instruction_kind_count(
    const RudaMirFunction *function,
    RudaMirInstructionKind kind
);

static void ruda_test_checker_record_identity(void) {
    static const char source[] =
        "type Point { mutable x: int; }\n"
        "type Other { x: int; }\n"
        "type PointAlias = Point;\n"
        "fn make(x: int): Point { return Point { x }; }\n"
        "fn same(left: PointAlias, right: Point): bool {\n"
        "  return identity.same(left, right);\n"
        "}\n"
        "fn self(x: int): bool {\n"
        "  const point = Point { x };\n"
        "  return identity.same(point, point);\n"
        "}\n"
        "fn distinct(x: int): bool {\n"
        "  return identity.same(Point { x }, Point { x });\n"
        "}\n"
        "fn aliasing(x: int): bool {\n"
        "  const point = Point { x };\n"
        "  const alias = point;\n"
        "  point.x += 1;\n"
        "  return identity.same(point, alias);\n"
        "}\n"
        "fn overflow(): int { return 9223372036854775807 + 1; }\n"
        "fn divideZero(): int { return 1 / 0; }\n"
        "fn order(): bool {\n"
        "  return identity.same(\n"
        "    Point { x: divideZero() },\n"
        "    Point { x: overflow() }\n"
        "  );\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue argument;
    RudaMirValue first;
    RudaMirValue second;
    RudaMirValue comparison_arguments[2];
    RudaMirValue result;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options =
        ruda_check_interpret_default_options();
    const RudaCheckedFunction *same = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/record-identity",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.records.length == 2);
        RUDA_EXPECT(checked.type_aliases.length == 1);
        RUDA_EXPECT(checked.functions.length == 8);
        same = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(same != NULL);
        if (same != NULL) {
            RUDA_EXPECT(same->mir.records.length == 1);
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &same->mir,
                RUDA_MIR_RECORD_IDENTITY_SAME
            ) == 1);
        }
        argument.type = RUDA_TYPE_I64;
        argument.as.i64 = 7;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_BOOL);
        RUDA_EXPECT(result.as.boolean);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(!result.as.boolean);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.boolean);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);

        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &first
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &second
        ) == RUDA_MIR_INTERPRET_OK);
        comparison_arguments[0] = first;
        comparison_arguments[1] = first;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            comparison_arguments,
            2,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.boolean);
        comparison_arguments[1] = second;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            comparison_arguments,
            2,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(!result.as.boolean);
        comparison_arguments[1].as.reference = RUDA_MIR_INVALID_REFERENCE;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            comparison_arguments,
            2,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "type Other { x: int; }\n"
        "fn bad(): bool { return identity.same(Point { x: 1 }, Other { x: 1 }); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Other { x: 1 }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): bool { return identity.same(1, 1); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): bool { return identity.same(\"a\", \"a\"); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "\"a\"",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { A, }\n"
        "fn bad(): bool { return identity.same(Choice.A, Choice.A); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Choice.A",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(value: Point): bool { return identity.same(value); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "(value)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(value: Point): bool { return identity.same(value, value, value); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "(value, value, value)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(value: Point): bool { return identity.same<Point>(value, value); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "identity.same<Point>(value, value)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(value: Point): bool { return identity.same(value, 1); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "const same: bool = identity.same(Point { x: 1 }, Point { x: 1 });\n",
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "identity.same(Point { x: 1 }, Point { x: 1 })",
        0
    );
}

static void ruda_test_checker_record_match(void) {
    static const char source[] =
        "type Point { x: int; y: int; }\n"
        "type Empty {}\n"
        "type PointAlias = Point;\n"
        "fn make(x: int, y: int): Point { return Point { x, y }; }\n"
        "fn sum(point: Point): int {\n"
        "  return match point { case Point { x, y } => x + y; };\n"
        "}\n"
        "fn renamed(point: Point): int {\n"
        "  return match point { case PointAlias { x: left, y: right } => left - right; };\n"
        "}\n"
        "fn ignored(point: Point): int {\n"
        "  return match point { case Point { x: _, y } => y; };\n"
        "}\n"
        "fn guarded(point: Point, ready: bool): int {\n"
        "  return match point {\n"
        "    case Point { x, y } if ready => x;\n"
        "    case Point { x: _, y } => y;\n"
        "  };\n"
        "}\n"
        "fn whole(point: Point): Point {\n"
        "  return match point { case value => value; };\n"
        "}\n"
        "fn block(point: Point): int {\n"
        "  return match point { case Point { x, y } => { yield x + y; } };\n"
        "}\n"
        "fn makeEmpty(): Empty { return Empty {}; }\n"
        "fn emptyValue(value: Empty): int {\n"
        "  return match value { case Empty {} => 14; };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirValue arguments[2];
    RudaMirValue point;
    RudaMirValue result;
    size_t index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/record-match",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedFunction *sum = ruda_checked_function(&checked, 1);
        const RudaCheckedFunction *ignored = ruda_checked_function(&checked, 3);
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 9);
        RUDA_EXPECT(checked.records.length == 2);
        for (index = 0; index < checked.functions.length; index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        RUDA_EXPECT(sum != NULL && ignored != NULL);
        if (sum != NULL) {
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &sum->mir,
                RUDA_MIR_RECORD_GET
            ) == 2);
        }
        if (ignored != NULL) {
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &ignored->mir,
                RUDA_MIR_RECORD_GET
            ) == 1);
        }
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        arguments[0].type = RUDA_TYPE_I64;
        arguments[0].as.i64 = 9;
        arguments[1].type = RUDA_TYPE_I64;
        arguments[1].as.i64 = 4;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, arguments, 2, ruda_system_allocator(), options,
            &point
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, &point, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 13);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, &point, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 5);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 3, &point, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 4);
        arguments[0] = point;
        arguments[1].type = RUDA_TYPE_BOOL;
        arguments[1].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, arguments, 2, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 9);
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, arguments, 2, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 4);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, &point, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == point.type
            && ruda_mir_reference_equal(
                result.as.reference,
                point.as.reference
            ));
        arguments[0] = point;
        arguments[0].as.reference = RUDA_MIR_INVALID_REFERENCE;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 1, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, &point, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 13);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, NULL, 0, ruda_system_allocator(), options, &point
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 8, &point, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 14);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; y: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { x } => x; }; }\n",
        RUDA_DIAGNOSTIC_MISSING_FIELD,
        "{ x }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { z } => z; }; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
        "z",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { x, x } => x; }; }\n",
        RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
        "x",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; y: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { x: value, y: value } => value; }; }\n",
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "value",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "type Other { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Other { x } => x; }; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Other",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { x } | _ => x; }; }\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "_",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { x } if true => x; }; }\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match point { case Point { x } if true => x; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Point(x) => x; }; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Point(x)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "type Alias = int;\n"
        "fn bad(point: Point): int { return match point { case Alias { x } => x; }; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Alias",
        1
    );
    ruda_checker_expect_diagnostic_message(
        "type Empty {}\n"
        "fn bad(value: Empty): int { return match value {}; }\n",
        "record match is missing witness: Empty {}"
    );
    ruda_checker_expect_diagnostic_message(
        "type Point { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { x } if true => x; }; }\n",
        "record match is missing witness: Point { x: _ }; guarded patterns do not count toward exhaustiveness"
    );
}

static void ruda_test_checker_public_type_visibility(void) {
    static const char source[] =
        "public type PublicId = int;\n"
        "type PrivateId = int;\n"
        "public type PublicUser {\n"
        "  public id: PublicId;\n"
        "  internal: PrivateId;\n"
        "}\n"
        "type PrivateUser { public id: int; }\n"
        "public const answer: PublicId = 42;\n"
        "const hidden: PrivateId = 1;\n"
        "public fn identity(value: PublicUser): PublicUser { return value; }\n"
        "fn local(value: PrivateUser): PrivateUser { return value; }\n";
    static const char public_alias_private[] =
        "type Hidden = int;\n"
        "public type Exposed = Hidden;\n";
    static const char public_parameter_private[] =
        "type Hidden {}\n"
        "public fn expose(value: Hidden): void {}\n";
    static const char public_return_private[] =
        "type Hidden {}\n"
        "public fn expose(): Hidden {}\n";
    static const char public_field_private[] =
        "type Hidden {}\n"
        "public type Visible { public value: Hidden; internal: Hidden; }\n";
    static const char public_constant_private[] =
        "type Hidden = int;\n"
        "public const answer: Hidden = 1;\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedTypeAlias *public_id = NULL;
    const RudaCheckedTypeAlias *private_id = NULL;
    const RudaCheckedRecord *public_user = NULL;
    const RudaCheckedRecord *private_user = NULL;
    const RudaCheckedConstant *answer = NULL;
    const RudaCheckedConstant *hidden = NULL;
    const RudaCheckedFunction *identity = NULL;
    const RudaCheckedFunction *local = NULL;

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/visibility",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.type_aliases.length == 2);
        RUDA_EXPECT(checked.records.length == 2);
        RUDA_EXPECT(checked.constants.length == 2);
        RUDA_EXPECT(checked.functions.length == 2);
        public_id = ruda_checked_type_alias(&checked, 0);
        private_id = ruda_checked_type_alias(&checked, 1);
        public_user = ruda_checked_record(&checked, 0);
        private_user = ruda_checked_record(&checked, 1);
        answer = ruda_checked_constant(&checked, 0);
        hidden = ruda_checked_constant(&checked, 1);
        identity = ruda_checked_function(&checked, 0);
        local = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(public_id != NULL && public_id->is_public);
        RUDA_EXPECT(private_id != NULL && !private_id->is_public);
        RUDA_EXPECT(public_user != NULL && public_user->is_public);
        RUDA_EXPECT(private_user != NULL && !private_user->is_public);
        RUDA_EXPECT(answer != NULL && answer->is_public);
        RUDA_EXPECT(hidden != NULL && !hidden->is_public);
        RUDA_EXPECT(identity != NULL && identity->is_public);
        RUDA_EXPECT(local != NULL && !local->is_public);
        if (public_user != NULL) {
            const RudaCheckedRecordField *id = ruda_vector_at_const(
                &public_user->fields,
                0
            );
            const RudaCheckedRecordField *internal = ruda_vector_at_const(
                &public_user->fields,
                1
            );
            RUDA_EXPECT(id != NULL && id->is_public);
            RUDA_EXPECT(internal != NULL && !internal->is_public);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        public_alias_private,
        RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
    ruda_checker_expect_one_diagnostic(
        public_parameter_private,
        RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
    ruda_checker_expect_one_diagnostic(
        public_return_private,
        RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
    ruda_checker_expect_one_diagnostic(
        public_field_private,
        RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
    ruda_checker_expect_one_diagnostic(
        public_constant_private,
        RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
}

static void ruda_test_checker_enum_declarations(void) {
    static const char source[] =
        "public type Code = int;\n"
        "public type Alias = Message;\n"
        "public enum Message {\n"
        "  Quit,\n"
        "  Write(string, Code),\n"
        "  Move { x: int; y: Code; },\n"
        "  Chain(Message),\n"
        "}\n"
        "enum Shadow {\n"
        "  Quit,\n"
        "  Write(string, Code),\n"
        "  Move { x: int; y: Code; },\n"
        "  Chain(Message),\n"
        "}\n"
        "public fn keep(value: Message): Alias { return value; }\n";
    static const char mutual[] =
        "type Node { event: Event; }\n"
        "enum Event { Attach(Node), Next(Event), }\n";
    static const char duplicate_variant[] =
        "enum State { Ready, Ready, }\n";
    static const char duplicate_payload_field[] =
        "enum Event { Move { x: int; x: int; }, }\n";
    static const char unknown_payload_type[] =
        "enum Event { Value(Missing), }\n";
    static const char primitive_redefinition[] =
        "enum int { Value, }\n";
    static const char duplicate_top_level[] =
        "type Shared {}\n"
        "enum Shared { Value, }\n";
    static const char public_private_payload[] =
        "type Hidden {}\n"
        "public enum Exposed { Value(Hidden), }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckOptions options;
    const RudaCheckedEnum *message = NULL;
    const RudaCheckedEnum *shadow = NULL;
    const RudaCheckedEnumVariant *quit = NULL;
    const RudaCheckedEnumVariant *write = NULL;
    const RudaCheckedEnumVariant *move = NULL;
    const RudaCheckedEnumVariant *chain = NULL;
    const RudaCheckedTypeAlias *alias = NULL;
    const RudaCheckedFunction *keep = NULL;
    const RudaTypeDescriptor *signature = NULL;

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.enums.length == 2);
        RUDA_EXPECT(checked.type_aliases.length == 2);
        RUDA_EXPECT(checked.functions.length == 1);
        message = ruda_checked_enum(&checked, 0);
        shadow = ruda_checked_enum(&checked, 1);
        alias = ruda_checked_type_alias(&checked, 1);
        keep = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(message != NULL);
        RUDA_EXPECT(shadow != NULL);
        RUDA_EXPECT(alias != NULL);
        RUDA_EXPECT(keep != NULL);
        if (message != NULL) {
            const RudaTypeDescriptor *nominal = ruda_type_descriptor(
                &checked.types,
                message->type
            );
            RUDA_EXPECT(message->is_public);
            RUDA_EXPECT(message->variants.length == 4);
            RUDA_EXPECT(nominal != NULL);
            if (nominal != NULL) {
                RUDA_EXPECT(nominal->kind == RUDA_TYPE_KIND_NOMINAL);
                RUDA_EXPECT(ruda_symbol_id_equal(
                    nominal->symbol,
                    message->symbol
                ));
            }
            quit = ruda_vector_at_const(&message->variants, 0);
            write = ruda_vector_at_const(&message->variants, 1);
            move = ruda_vector_at_const(&message->variants, 2);
            chain = ruda_vector_at_const(&message->variants, 3);
            RUDA_EXPECT(quit != NULL);
            RUDA_EXPECT(write != NULL);
            RUDA_EXPECT(move != NULL);
            RUDA_EXPECT(chain != NULL);
            if (quit != NULL) {
                RUDA_EXPECT(quit->kind == RUDA_CHECKED_ENUM_VARIANT_UNIT);
                RUDA_EXPECT(quit->payload.length == 0);
            }
            if (write != NULL) {
                const RudaCheckedEnumPayloadItem *text =
                    ruda_vector_at_const(&write->payload, 0);
                const RudaCheckedEnumPayloadItem *code =
                    ruda_vector_at_const(&write->payload, 1);
                RUDA_EXPECT(write->kind
                    == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL);
                RUDA_EXPECT(write->payload.length == 2);
                RUDA_EXPECT(text != NULL && !text->has_name
                    && text->type == RUDA_TYPE_STRING);
                RUDA_EXPECT(code != NULL && !code->has_name
                    && code->type == RUDA_TYPE_I64);
            }
            if (move != NULL) {
                const RudaCheckedEnumPayloadItem *x = ruda_vector_at_const(
                    &move->payload,
                    0
                );
                const RudaCheckedEnumPayloadItem *y = ruda_vector_at_const(
                    &move->payload,
                    1
                );
                RUDA_EXPECT(move->kind == RUDA_CHECKED_ENUM_VARIANT_NAMED);
                RUDA_EXPECT(move->payload.length == 2);
                RUDA_EXPECT(x != NULL && x->has_name
                    && x->name.length == strlen("x")
                    && x->type == RUDA_TYPE_I64);
                RUDA_EXPECT(y != NULL && y->has_name
                    && y->name.length == strlen("y")
                    && y->type == RUDA_TYPE_I64);
            }
            if (chain != NULL) {
                const RudaCheckedEnumPayloadItem *next =
                    ruda_vector_at_const(&chain->payload, 0);
                RUDA_EXPECT(next != NULL && next->type == message->type);
            }
        }
        if (message != NULL && shadow != NULL) {
            RUDA_EXPECT(!shadow->is_public);
            RUDA_EXPECT(shadow->variants.length == message->variants.length);
            RUDA_EXPECT(shadow->type != message->type);
            RUDA_EXPECT(!ruda_symbol_id_equal(shadow->symbol, message->symbol));
        }
        if (message != NULL && alias != NULL) {
            RUDA_EXPECT(alias->target_type == message->type);
            RUDA_EXPECT(alias->is_public);
        }
        if (message != NULL && keep != NULL) {
            RUDA_EXPECT(keep->is_public);
            signature = ruda_type_descriptor(&checked.types, keep->signature_type);
            RUDA_EXPECT(signature != NULL);
            if (signature != NULL) {
                RUDA_EXPECT(signature->arguments[0] == message->type);
                RUDA_EXPECT(signature->arguments[1] == message->type);
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, mutual));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedRecord *node = ruda_checked_record(&checked, 0);
        const RudaCheckedEnum *event = ruda_checked_enum(&checked, 0);
        const RudaCheckedRecordField *event_field = node != NULL
            ? ruda_vector_at_const(&node->fields, 0)
            : NULL;
        const RudaCheckedEnumVariant *attach = event != NULL
            ? ruda_vector_at_const(&event->variants, 0)
            : NULL;
        const RudaCheckedEnumVariant *next = event != NULL
            ? ruda_vector_at_const(&event->variants, 1)
            : NULL;
        const RudaCheckedEnumPayloadItem *attached = attach != NULL
            ? ruda_vector_at_const(&attach->payload, 0)
            : NULL;
        const RudaCheckedEnumPayloadItem *continued = next != NULL
            ? ruda_vector_at_const(&next->payload, 0)
            : NULL;
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(node != NULL && event != NULL);
        RUDA_EXPECT(event_field != NULL && attached != NULL
            && continued != NULL);
        if (node != NULL && event != NULL && event_field != NULL
            && attached != NULL && continued != NULL) {
            RUDA_EXPECT(event_field->type == event->type);
            RUDA_EXPECT(attached->type == node->type);
            RUDA_EXPECT(continued->type == event->type);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        duplicate_variant,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "Ready",
        1
    );
    ruda_checker_expect_one_diagnostic(
        duplicate_payload_field,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "x",
        1
    );
    ruda_checker_expect_one_diagnostic(
        unknown_payload_type,
        RUDA_DIAGNOSTIC_UNKNOWN_TYPE,
        "Missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        primitive_redefinition,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "int",
        0
    );
    ruda_checker_expect_one_diagnostic(
        duplicate_top_level,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "Shared",
        1
    );
    ruda_checker_expect_one_diagnostic(
        public_private_payload,
        RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    options = ruda_check_default_options();
    options.max_enums = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.max_variants_per_enum = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.max_payload_items_per_variant = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "enum Generic<T> { Value(T), }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "@derive(Debug) enum State { Ready, }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_enum_package_order(void) {
    static const char entry_source[] =
        "fn keep(value: Event): Event { return value; }\n"
        "fn empty(): Event { return Event.Empty; }\n"
        "fn kind(value: Event): int {\n"
        "  return match value {\n"
        "    case Empty => 0;\n"
        "    case Link(node) | Peer(node) => 1;\n"
        "  };\n"
        "}\n";
    static const char types_source[] =
        "enum Event { Empty, Link(Node), Peer(Node), }\n"
        "type Node { event: Event; }\n";
    RudaCheckerFixture entry;
    RudaCheckerFixture types;
    RudaCheckerFixture missing_entry;
    const RudaAstTree *asts[2];
    RudaCheckResult checked;
    const RudaCheckedEnum *event = NULL;
    const RudaCheckedRecord *node = NULL;
    RudaSymbolId symbol;
    RudaTypeId type = RUDA_TYPE_INVALID;
    memset(&symbol, 0, sizeof(symbol));

    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &entry,
        "a_entry.ruda",
        entry_source
    ));
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &types,
        "z_types.ruda",
        types_source
    ));
    asts[0] = &types.ast;
    asts[1] = &entry.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/enum-package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedEnumVariant *link = NULL;
        const RudaCheckedEnumPayloadItem *linked = NULL;
        const RudaCheckedRecordField *field = NULL;
        event = ruda_checked_enum(&checked, 0);
        node = ruda_checked_record(&checked, 0);
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 3);
        RUDA_EXPECT(event != NULL && node != NULL);
        if (event != NULL && node != NULL) {
            link = ruda_vector_at_const(&event->variants, 1);
            linked = link != NULL
                ? ruda_vector_at_const(&link->payload, 0)
                : NULL;
            field = ruda_vector_at_const(&node->fields, 0);
            RUDA_EXPECT(linked != NULL && linked->type == node->type);
            RUDA_EXPECT(field != NULL && field->type == event->type);
            symbol = event->symbol;
            type = event->type;
        }
        {
            const RudaCheckedFunction *empty = ruda_checked_function(
                &checked,
                1
            );
            RUDA_EXPECT(empty != NULL);
            if (empty != NULL) {
                RUDA_EXPECT(empty->mir.enums.length == 1);
                RUDA_EXPECT(empty->mir.enum_constructions.length == 1);
            }
            {
                const RudaCheckedFunction *kind = ruda_checked_function(
                    &checked,
                    2
                );
                RUDA_EXPECT(kind != NULL);
                if (kind != NULL) {
                    RUDA_EXPECT(kind->mir.enums.length == 1);
                    RUDA_EXPECT(ruda_mir_verify(&kind->mir)
                        == RUDA_MIR_VERIFY_OK);
                }
            }
        }
        ruda_check_result_dispose(&checked);
    }

    asts[0] = &entry.ast;
    asts[1] = &types.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/enum-package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        event = ruda_checked_enum(&checked, 0);
        RUDA_EXPECT(event != NULL);
        RUDA_EXPECT(checked.functions.length == 3);
        if (event != NULL) {
            RUDA_EXPECT(ruda_symbol_id_equal(event->symbol, symbol));
            RUDA_EXPECT(event->type == type);
        }
        ruda_check_result_dispose(&checked);
    }
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &missing_entry,
        "b_missing.ruda",
        "type Alias = Event;\n"
        "fn kind(value: Alias): int {\n"
        "  return match value { case Empty => 0; case Link(_) => 1; };\n"
        "}\n"
    ));
    asts[0] = &types.ast;
    asts[1] = &missing_entry.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/enum-package-witness",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckDiagnostic *diagnostic = ruda_check_diagnostic(
            &checked,
            0
        );
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->message,
                "enum match is missing witness: Peer(_)"
            ) == 0);
            RUDA_EXPECT(diagnostic->owned_message.data != NULL);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&missing_entry);
    ruda_checker_fixture_dispose(&types);
    ruda_checker_fixture_dispose(&entry);
}

static void ruda_test_checker_enum_execution(void) {
    static const char source[] =
        "enum Message {\n"
        "  Quit,\n"
        "  Write(int, int),\n"
        "  Move { x: int; y: int; },\n"
        "}\n"
        "type MessageAlias = Message;\n"
        "fn quit(): Message { return Message.Quit; }\n"
        "fn write(x: int, y: int): Message {\n"
        "  return Message.Write(x, y);\n"
        "}\n"
        "fn move(x: int, y: int): MessageAlias {\n"
        "  return MessageAlias.Move { y, x };\n"
        "}\n"
        "fn relay(x: int, y: int): Message { return write(x, y); }\n"
        "fn keep(message: Message): Message { return message; }\n"
        "fn overflow(): int { return 9223372036854775807 + 1; }\n"
        "fn divideZero(): int { return 1 / 0; }\n"
        "fn positionalOrder(): Message {\n"
        "  return Message.Write(divideZero(), overflow());\n"
        "}\n"
        "fn namedOrder(): Message {\n"
        "  return Message.Move { y: divideZero(), x: overflow() };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedEnum *message = NULL;
    const RudaCheckedFunction *quit = NULL;
    const RudaCheckedFunction *write = NULL;
    const RudaCheckedFunction *move = NULL;
    RudaMirExecution execution;
    RudaCheckInterpretOptions interpret_options =
        ruda_check_interpret_default_options();
    RudaMirValue arguments[2];
    RudaMirValue result;
    RudaMirValue payload;
    RudaMirValue enum_argument;
    size_t selected_variant = 0;
    size_t payload_count = 0;
    size_t retained_count = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enum-execution",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.enums.length == 1);
        RUDA_EXPECT(checked.type_aliases.length == 1);
        RUDA_EXPECT(checked.functions.length == 9);
        message = ruda_checked_enum(&checked, 0);
        quit = ruda_checked_function(&checked, 0);
        write = ruda_checked_function(&checked, 1);
        move = ruda_checked_function(&checked, 2);
        RUDA_EXPECT(message != NULL);
        RUDA_EXPECT(quit != NULL && write != NULL && move != NULL);
        if (quit != NULL) {
            RUDA_EXPECT(quit->mir.enums.length == 1);
            RUDA_EXPECT(quit->mir.enum_variants.length == 3);
            RUDA_EXPECT(quit->mir.enum_payloads.length == 4);
            RUDA_EXPECT(quit->mir.enum_constructions.length == 1);
            RUDA_EXPECT(quit->mir.enum_arguments.length == 0);
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                0,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
        }
        if (write != NULL) {
            RUDA_EXPECT(write->mir.enums.length == 1);
            RUDA_EXPECT(write->mir.enum_constructions.length == 1);
            RUDA_EXPECT(write->mir.enum_arguments.length == 2);
        }
        if (move != NULL) {
            RUDA_EXPECT(move->mir.enums.length == 1);
            RUDA_EXPECT(move->mir.enum_constructions.length == 1);
            RUDA_EXPECT(move->mir.enum_arguments.length == 2);
        }
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        interpret_options.execution = &execution;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(message != NULL && result.type == message->type);
        RUDA_EXPECT(ruda_mir_execution_owns_reference(&execution, result));
        RUDA_EXPECT(ruda_mir_execution_inspect_enum(
            &execution,
            result,
            &selected_variant,
            &payload_count
        ));
        RUDA_EXPECT(selected_variant == 0 && payload_count == 0);

        arguments[0].type = RUDA_TYPE_I64;
        arguments[0].as.i64 = 10;
        arguments[1].type = RUDA_TYPE_I64;
        arguments[1].as.i64 = 20;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            arguments,
            2,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_execution_owns_reference(&execution, result));
        RUDA_EXPECT(ruda_mir_execution_inspect_enum(
            &execution,
            result,
            &selected_variant,
            &payload_count
        ));
        RUDA_EXPECT(selected_variant == 1 && payload_count == 2);
        RUDA_EXPECT(ruda_mir_execution_enum_payload(
            &execution,
            result,
            0,
            &payload
        ));
        RUDA_EXPECT(payload.type == RUDA_TYPE_I64 && payload.as.i64 == 10);
        RUDA_EXPECT(ruda_mir_execution_enum_payload(
            &execution,
            result,
            1,
            &payload
        ));
        RUDA_EXPECT(payload.as.i64 == 20);

        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            arguments,
            2,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_execution_owns_reference(&execution, result));
        RUDA_EXPECT(ruda_mir_execution_inspect_enum(
            &execution,
            result,
            &selected_variant,
            &payload_count
        ));
        RUDA_EXPECT(selected_variant == 2 && payload_count == 2);
        RUDA_EXPECT(ruda_mir_execution_enum_payload(
            &execution,
            result,
            0,
            &payload
        ));
        RUDA_EXPECT(payload.as.i64 == 10);
        RUDA_EXPECT(ruda_mir_execution_enum_payload(
            &execution,
            result,
            1,
            &payload
        ));
        RUDA_EXPECT(payload.as.i64 == 20);

        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            arguments,
            2,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_execution_owns_reference(&execution, result));
        enum_argument = result;
        retained_count = execution.enums.length;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            &enum_argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            &enum_argument,
            1,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_reference_equal(
            result.as.reference,
            enum_argument.as.reference
        ));
        RUDA_EXPECT(execution.enums.length == retained_count);
        enum_argument.as.reference = RUDA_MIR_INVALID_REFERENCE;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            &enum_argument,
            1,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);

        retained_count = execution.enums.length;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            6,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        RUDA_EXPECT(execution.enums.length == retained_count);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        RUDA_EXPECT(execution.enums.length == retained_count);
        RUDA_EXPECT(execution.objects.length == 0);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(): Message { return Message.Missing; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_VARIANT,
        "Missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Write(int), }\n"
        "fn bad(): Message { return Message.Write; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Message.Write",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(): Message { return Message.Quit(); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Message.Quit()",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; }, }\n"
        "fn bad(): Message { return Message.Move(1); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Message.Move(1)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Write(int, int), }\n"
        "fn bad(): Message { return Message.Write(1); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "(1)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; }, }\n"
        "fn bad(): Message { return Message.Move { y: 1 }; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
        "y",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; }, }\n"
        "fn bad(): Message { return Message.Move { x: 1, x: 2 }; }\n",
        RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
        "x",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; y: int; }, }\n"
        "fn bad(): Message { return Message.Move { x: 1 }; }\n",
        RUDA_DIAGNOSTIC_MISSING_FIELD,
        "Message.Move { x: 1 }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; }, }\n"
        "fn bad(): Message { return Message.Move { x: true }; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; }, }\n"
        "fn bad(): Message { return Message.Move { x }; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "x",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(): Message { return Quit; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "Quit",
        1
    );
}

static void ruda_test_checker_enum_match(void) {
    static const char source[] =
        "enum Message {\n"
        "  Quit,\n"
        "  Write(int, int),\n"
        "  Move { x: int; y: int; },\n"
        "}\n"
        "fn quit(): Message { return Message.Quit; }\n"
        "fn write(x: int, y: int): Message {\n"
        "  return Message.Write(x, y);\n"
        "}\n"
        "fn move(x: int, y: int): Message {\n"
        "  return Message.Move { y, x };\n"
        "}\n"
        "fn describe(value: Message): int {\n"
        "  return match value {\n"
        "    case Quit => 0;\n"
        "    case Write(number, _) => number;\n"
        "    case Move { x, y: vertical } => x + vertical;\n"
        "  };\n"
        "}\n"
        "fn fallback(value: Message): int {\n"
        "  return match value {\n"
        "    case Quit => 1;\n"
        "    case other => 2;\n"
        "  };\n"
        "}\n"
        "fn divideZero(): int { return 1 / 0; }\n"
        "fn overflow(): int { return 9223372036854775807 + 1; }\n"
        "fn lazy(value: Message): int {\n"
        "  return match value {\n"
        "    case Quit => 1;\n"
        "    case Write(_, _) => divideZero();\n"
        "    case Move { x: _, y: _ } => overflow();\n"
        "  };\n"
        "}\n"
        "fn guarded(value: Message): int {\n"
        "  return match value {\n"
        "    case Write(number, _) if number > 0 => number;\n"
        "    case Write(_, _) => 0;\n"
        "    case Quit => -1;\n"
        "    case Move { x, y } if x > y => x;\n"
        "    case Move { x, y } => y;\n"
        "  };\n"
        "}\n"
        "fn guardedCatch(value: Message): int {\n"
        "  return match value {\n"
        "    case other if false => 9;\n"
        "    case _ => 4;\n"
        "  };\n"
        "}\n"
        "fn guardBomb(): bool { return 1 / 0 == 0; }\n"
        "fn guardOrder(value: Message): int {\n"
        "  return match value {\n"
        "    case Write(_, _) if guardBomb() => 1;\n"
        "    case _ => 2;\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *describe = NULL;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirValue constructor_arguments[2];
    RudaMirValue enum_value;
    RudaMirValue result;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enum-match",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.enums.length == 1);
        RUDA_EXPECT(checked.functions.length == 12);
        describe = ruda_checked_function(&checked, 3);
        RUDA_EXPECT(describe != NULL);
        if (describe != NULL) {
            RUDA_EXPECT(describe->mir.enums.length == 1);
            RUDA_EXPECT(describe->mir.enum_constructions.length == 0);
            RUDA_EXPECT(describe->mir.blocks.length >= 6);
            RUDA_EXPECT(ruda_mir_verify(&describe->mir) == RUDA_MIR_VERIFY_OK);
        }
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            NULL,
            0,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 0);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == -1);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            9,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 4);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            11,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);

        constructor_arguments[0].type = RUDA_TYPE_I64;
        constructor_arguments[0].as.i64 = 41;
        constructor_arguments[1].type = RUDA_TYPE_I64;
        constructor_arguments[1].as.i64 = 99;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            constructor_arguments,
            2,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 41);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 41);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            11,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        constructor_arguments[0].as.i64 = -1;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            constructor_arguments,
            2,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);

        constructor_arguments[0].as.i64 = 10;
        constructor_arguments[1].as.i64 = 20;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            constructor_arguments,
            2,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 30);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OVERFLOW);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 20);
        constructor_arguments[0].as.i64 = 30;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            constructor_arguments,
            2,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 30);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &enum_value,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
        enum_value.as.reference = RUDA_MIR_INVALID_REFERENCE;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case Quit => 0; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; case Quit => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Quit",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case _ => 0; case Quit => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Quit",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Write => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Write",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Missing(x) => x; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_UNKNOWN_VARIANT,
        "Missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Write(int, int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Write(x) => x; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "(x)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Write(int, int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Write(x, x) => x; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "x",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; case Write(x) => true; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; y: int; }, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Move { z } => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
        "z",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; y: int; }, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Move { x, x } => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
        "x",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; y: int; }, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Move { x } => x; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_MISSING_FIELD,
        "{ x }",
        0
    );

    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn guarded(value: Message): int {\n"
        "  return match value { case Quit if true => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case Quit if true => 0; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit if 1 => 0; case _ => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; case Quit if false => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Quit",
        2
    );
}

static size_t ruda_test_mir_instruction_kind_count(
    const RudaMirFunction *function,
    RudaMirInstructionKind kind
) {
    size_t block_index = 0;
    size_t count = 0;
    if (function == NULL) {
        return 0;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) {
            continue;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            if (instruction != NULL && instruction->kind == kind) {
                count += 1;
            }
        }
    }
    return count;
}

static const RudaMirInstruction *ruda_test_mir_instruction_kind_at(
    const RudaMirFunction *function,
    RudaMirInstructionKind kind,
    size_t occurrence
) {
    size_t block_index = 0;
    size_t found = 0;
    if (function == NULL) {
        return NULL;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) {
            continue;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            if (instruction != NULL && instruction->kind == kind) {
                if (found == occurrence) {
                    return instruction;
                }
                found += 1;
            }
        }
    }
    return NULL;
}

static void ruda_test_checker_enum_or_patterns(void) {
    static const char source[] =
        "enum Choice {\n"
        "  Idle, Stop, Pause,\n"
        "  Left(int), Right(int),\n"
        "  Point { x: int; y: int; },\n"
        "  Pixel { x: int; y: int; },\n"
        "}\n"
        "fn idle(): Choice { return Choice.Idle; }\n"
        "fn stop(): Choice { return Choice.Stop; }\n"
        "fn pause(): Choice { return Choice.Pause; }\n"
        "fn left(value: int): Choice { return Choice.Left(value); }\n"
        "fn right(value: int): Choice { return Choice.Right(value); }\n"
        "fn point(x: int, y: int): Choice {\n"
        "  return Choice.Point { x, y };\n"
        "}\n"
        "fn pixel(x: int, y: int): Choice {\n"
        "  return Choice.Pixel { y, x };\n"
        "}\n"
        "fn classify(value: Choice): int {\n"
        "  return match value {\n"
        "    case Idle | Stop | Pause => 1;\n"
        "    case Left(_) | Right(_) => 2;\n"
        "    case Point { x: _, y: _ } | Pixel { x: _, y: _ } => 3;\n"
        "  };\n"
        "}\n"
        "fn payload(value: Choice): int {\n"
        "  return match value {\n"
        "    case Left(number) | Right(number) => number;\n"
        "    case _ => -1;\n"
        "  };\n"
        "}\n"
        "fn named(value: Choice): int {\n"
        "  return match value {\n"
        "    case Point { x: horizontal, y: vertical }\n"
        "      | Pixel { y: vertical, x: horizontal }\n"
        "      => horizontal * 10 + vertical;\n"
        "    case _ => -1;\n"
        "  };\n"
        "}\n"
        "fn guarded(value: Choice): int {\n"
        "  return match value {\n"
        "    case Left(number) | Right(number) if number > 0 => number;\n"
        "    case Left(_) | Right(_) => 0;\n"
        "    case _ => -1;\n"
        "  };\n"
        "}\n"
        "fn guardBomb(): bool { return 1 / 0 == 0; }\n"
        "fn guardOrder(value: Choice): int {\n"
        "  return match value {\n"
        "    case Left(_) | Right(_) if guardBomb() => 1;\n"
        "    case _ => 2;\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirValue arguments[2];
    RudaMirValue enum_value;
    RudaMirValue result;
    size_t constructor_index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enum-or-patterns",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedFunction *guard_order = NULL;
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.enums.length == 1);
        RUDA_EXPECT(checked.functions.length == 13);
        guard_order = ruda_checked_function(&checked, 12);
        RUDA_EXPECT(guard_order != NULL);
        if (guard_order != NULL) {
            RUDA_EXPECT(ruda_mir_verify(&guard_order->mir)
                == RUDA_MIR_VERIFY_OK);
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &guard_order->mir,
                RUDA_MIR_CALL
            ) == 1);
        }
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        for (constructor_index = 0; constructor_index < 3;
            constructor_index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                constructor_index,
                NULL,
                0,
                ruda_system_allocator(),
                options,
                &enum_value
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                7,
                &enum_value,
                1,
                ruda_system_allocator(),
                options,
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 1);
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            12,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);

        arguments[0].type = RUDA_TYPE_I64;
        arguments[0].as.i64 = 41;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            arguments,
            1,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 41);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            10,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 41);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            12,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);

        arguments[0].as.i64 = -7;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            arguments,
            1,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == -7);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            10,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);

        arguments[0].as.i64 = 3;
        arguments[1].type = RUDA_TYPE_I64;
        arguments[1].as.i64 = 4;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            5,
            arguments,
            2,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            9,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 34);
        arguments[0].as.i64 = 5;
        arguments[1].as.i64 = 6;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            6,
            arguments,
            2,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            9,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 56);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "enum Choice { Left(int), Right(int), }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Left(x) | Right(y) => x; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Right(y)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Number(int), Flag(bool), }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Number(item) | Flag(item) => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Flag(item)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Idle, Left(int), }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Left(item) | Idle => item; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Idle",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Idle, Stop, }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Idle | Idle => 0; case Stop => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Idle",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Idle, Stop, }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Idle => 0; case Idle | Stop => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Idle",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Idle, Stop, }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case _ | Idle => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Idle",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Idle, Stop, }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Idle | _ => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Idle",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Idle, Stop, }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Idle | Stop if true => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case Idle | Stop if true => 0; }",
        0
    );
}

static void ruda_test_checker_boolean_match(void) {
    static const char source[] =
        "fn choose(value: bool): int {\n"
        "  return match value { case true => 1; case false => 0; };\n"
        "}\n"
        "fn reverse(value: bool): int {\n"
        "  return match value { case false => 0; case true => 1; };\n"
        "}\n"
        "fn bind(value: bool): bool {\n"
        "  return match value { case observed => observed; };\n"
        "}\n"
        "fn guarded(value: bool): int {\n"
        "  return match value {\n"
        "    case true if false => 9;\n"
        "    case true => 1;\n"
        "    case false => 0;\n"
        "  };\n"
        "}\n"
        "fn boundGuard(value: bool): int {\n"
        "  return match value {\n"
        "    case observed if observed => 1;\n"
        "    case _ => 0;\n"
        "  };\n"
        "}\n"
        "fn guardBomb(): bool { return 1 / 0 == 0; }\n"
        "fn guardOrder(value: bool): int {\n"
        "  return match value {\n"
        "    case true if guardBomb() => 1;\n"
        "    case _ => 2;\n"
        "  };\n"
        "}\n"
        "fn union(value: bool): int {\n"
        "  return match value { case true | false => 3; };\n"
        "}\n"
        "fn identity(value: bool): bool { return value; }\n"
        "fn viaCall(value: bool): int {\n"
        "  return match identity(value) {\n"
        "    case true => 1;\n"
        "    case false => 0;\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue argument;
    RudaMirValue result;
    size_t function_index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/boolean-match",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 10);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        {
            const RudaCheckedFunction *guard_order = ruda_checked_function(
                &checked,
                6
            );
            const RudaCheckedFunction *via_call = ruda_checked_function(
                &checked,
                9
            );
            RUDA_EXPECT(guard_order != NULL && via_call != NULL);
            if (guard_order != NULL) {
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &guard_order->mir,
                    RUDA_MIR_CALL
                ) == 1);
            }
            if (via_call != NULL) {
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &via_call->mir,
                    RUDA_MIR_CALL
                ) == 1);
            }
        }
        argument.type = RUDA_TYPE_BOOL;
        argument.as.boolean = false;
        for (function_index = 0; function_index < 2;
            function_index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                function_index,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 0);
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_BOOL && !result.as.boolean);
        for (function_index = 3; function_index <= 4;
            function_index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                function_index,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 0);
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            6,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 3);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            9,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);

        argument.as.boolean = true;
        for (function_index = 0; function_index < 2;
            function_index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                function_index,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 1);
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_BOOL && result.as.boolean);
        for (function_index = 3; function_index <= 4;
            function_index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                function_index,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 1);
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            6,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 3);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            9,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case true => 1; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true if true => 1; case false => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case true if true => 1; case false => 0; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true => 1; case true => 2; case false => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "true",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case _ => 0; case false => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "false",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true | true => 1; case false => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "true",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true | _ => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true | false if true => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case true | false if true => 1; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true if 1 => 1; case true => 2; case false => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true => 1; case false => false; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "false",
        1
    );

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn bad(value: bool): int {\n"
        "  return match value { case 1 => 1; case _ => 0; };\n"
        "}\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/boolean-match-boundary",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_match_witnesses(void) {
    ruda_checker_expect_diagnostic_message(
        "enum Message {\n"
        "  Quit, Write(int, int), Move { x: int; y: int; },\n"
        "  Pause, Stop, Wait,\n"
        "}\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; };\n"
        "}\n",
        "enum match is missing witnesses: Write(_, _), "
        "Move { x: _, y: _ }, Pause, Stop; +1 more"
    );
    ruda_checker_expect_diagnostic_message(
        "enum Message { Quit, Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value {\n"
        "    case Quit if true => 0;\n"
        "    case Write(_) => 1;\n"
        "  };\n"
        "}\n",
        "enum match is missing witness: Quit; guarded patterns do not count "
        "toward exhaustiveness"
    );
    ruda_checker_expect_diagnostic_message(
        "enum Message { Quit, Write(int), Move, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value {\n"
        "    case Quit if true => 0; case Quit => 1; case Write(_) => 2;\n"
        "  };\n"
        "}\n",
        "enum match is missing witness: Move"
    );
    ruda_checker_expect_diagnostic_message(
        "enum Message { Quit, Many(int, int, int, int, int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; };\n"
        "}\n",
        "enum match is missing witness: Many(_, _, _, _, ...)"
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: bool): int {\n"
        "  return match value { case true => 1; };\n"
        "}\n",
        "boolean match is missing witness: false"
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: bool): int {\n"
        "  return match value { case true if true => 1; case true => 2; };\n"
        "}\n",
        "boolean match is missing witness: false"
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: bool): int {\n"
        "  return match value { case observed if observed => 1; };\n"
        "}\n",
        "boolean match is missing witnesses: false, true; guarded patterns "
        "do not count toward exhaustiveness"
    );
    ruda_checker_expect_diagnostic_message(
        "enum Message { Quit, Write(int), }\n"
        "fn bad(value: Message): int { return match value {}; }\n",
        "enum match is missing witnesses: Quit, Write(_)"
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: bool): int { return match value {}; }\n",
        "boolean match is missing witnesses: false, true"
    );
}

static void ruda_test_checker_i64_match(void) {
    static const char source[] =
        "fn classify(value: int): int {\n"
        "  return match value {\n"
        "    case 0 => 10; case 1 | 2 => 20; case other => other;\n"
        "  };\n"
        "}\n"
        "fn guarded(value: int): int {\n"
        "  return match value {\n"
        "    case 1 | 2 if value > 1 => 9;\n"
        "    case 1 | 2 => 8; case _ => 0;\n"
        "  };\n"
        "}\n"
        "fn boundGuard(value: int): int {\n"
        "  return match value {\n"
        "    case observed if observed > 0 => observed; case _ => 0;\n"
        "  };\n"
        "}\n"
        "fn guardBomb(): bool { return 1 / 0 == 0; }\n"
        "fn guardOrder(value: int): int {\n"
        "  return match value { case 1 if guardBomb() => 1; case _ => 2; };\n"
        "}\n"
        "fn maximum(value: int): int {\n"
        "  return match value {\n"
        "    case 9223372036854775807 => 1; case _ => 0;\n"
        "  };\n"
        "}\n"
        "fn identity(value: int): int { return value; }\n"
        "fn viaCall(value: int): int {\n"
        "  return match identity(value) { case 0 => 1; case other => other; };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue argument;
    RudaMirValue result;
    size_t function_index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/i64-match",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedFunction *via_call = NULL;
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 8);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        via_call = ruda_checked_function(&checked, 7);
        RUDA_EXPECT(via_call != NULL);
        if (via_call != NULL) {
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &via_call->mir,
                RUDA_MIR_CALL
            ) == 1);
        }
        argument.type = RUDA_TYPE_I64;
        argument.as.i64 = 0;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 10);
        argument.as.i64 = 1;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 20);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 8);
        argument.as.i64 = 2;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 9);
        argument.as.i64 = 7;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 7);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 7);
        argument.as.i64 = -7;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);
        argument.as.i64 = 0;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);
        argument.as.i64 = 1;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        argument.as.i64 = INT64_MAX;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);
        argument.as.i64 = 0;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);
        argument.as.i64 = 9;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 9);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_diagnostic_message(
        "fn bad(value: int): int {\n"
        "  return match value { case 0 => 1; case 1 => 2; case 2 => 3; };\n"
        "}\n",
        "integer match is missing witness: 3"
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: int): int {\n"
        "  return match value { case observed if observed > 0 => 1; };\n"
        "}\n",
        "integer match is missing witness: 0; guarded patterns do not count "
        "toward exhaustiveness"
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: int): int { return match value {}; }\n",
        "integer match is missing witness: 0"
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case 1 => 1; case 1 => 2; case _ => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "1",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case 1 | 1 => 1; case _ => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "1",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case _ => 0; case 1 => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case 0 | _ => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "0",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case true => 1; case _ => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case 9223372036854775808 => 1; case _ => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_INTEGER_LITERAL_RANGE,
        "9223372036854775808",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case 1 if 1 => 1; case _ => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case 1 => 1; case _ => true; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
}

static void ruda_test_checker_match_block_arms(void) {
    static const char source[] =
        "enum Choice { None, Value(int), }\n"
        "fn none(): Choice { return Choice.None; }\n"
        "fn value(item: int): Choice { return Choice.Value(item); }\n"
        "fn enumBlock(choice: Choice, flag: bool): int {\n"
        "  return match choice {\n"
        "    case None => { const base = 10; yield base; }\n"
        "    case Value(item) if item > 0 => {\n"
        "      if flag { yield item; }\n"
        "      yield item + 1;\n"
        "    }\n"
        "    case Value(item) => { yield item; }\n"
        "  };\n"
        "}\n"
        "fn boolBlock(value: bool): int {\n"
        "  return match value {\n"
        "    case true | false => {\n"
        "      if value { yield 1; } else { yield 0; }\n"
        "    }\n"
        "  };\n"
        "}\n"
        "fn intBlock(value: int): int {\n"
        "  return match value {\n"
        "    case 0 | 1 => { const next = value + 10; yield next; }\n"
        "    case other => { yield other; }\n"
        "  };\n"
        "}\n"
        "fn nested(value: bool, other: bool): int {\n"
        "  return match value {\n"
        "    case true => {\n"
        "      const inner = match other {\n"
        "        case true => { yield 1; }\n"
        "        case false => { yield 2; }\n"
        "      };\n"
        "      yield inner + 10;\n"
        "    }\n"
        "    case false => { yield 0; }\n"
        "  };\n"
        "}\n"
        "fn mixed(value: bool): int {\n"
        "  return match value { case true => 1; case false => { yield 0; } };\n"
        "}\n"
        "fn mixedReturn(value: bool): int {\n"
        "  return match value {\n"
        "    case true => { if value { return 9; } yield 1; }\n"
        "    case false => { yield 0; }\n"
        "  };\n"
        "}\n"
        "fn blockBomb(): int { return 1 / 0; }\n"
        "fn lazyBlock(value: bool): int {\n"
        "  return match value {\n"
        "    case true => { yield 1; }\n"
        "    case false => { yield blockBomb(); }\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirValue arguments[2];
    RudaMirValue enum_value;
    RudaMirValue result;
    size_t function_index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/match-block-arms",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.enums.length == 1);
        RUDA_EXPECT(checked.functions.length == 10);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, NULL, 0, ruda_system_allocator(), options, &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        arguments[0] = enum_value;
        arguments[1].type = RUDA_TYPE_BOOL;
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, arguments, 2, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 10);

        arguments[0].type = RUDA_TYPE_I64;
        arguments[0].as.i64 = 5;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, arguments, 1, ruda_system_allocator(), options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        arguments[0] = enum_value;
        arguments[1].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, arguments, 2, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 5);
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, arguments, 2, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 6);

        arguments[0].type = RUDA_TYPE_BOOL;
        arguments[0].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 3, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);
        arguments[0].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 3, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);

        arguments[0].type = RUDA_TYPE_I64;
        arguments[0].as.i64 = 0;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 10);
        arguments[0].as.i64 = 1;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 11);
        arguments[0].as.i64 = 7;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 7);

        arguments[0].type = RUDA_TYPE_BOOL;
        arguments[0].as.boolean = true;
        arguments[1].type = RUDA_TYPE_BOOL;
        arguments[1].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 2, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 11);
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 2, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 12);
        arguments[0].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 2, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);

        arguments[0].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 9);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 9, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);
        arguments[0].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 9, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "fn bad(): int { yield 1; }\n",
        RUDA_DIAGNOSTIC_YIELD_OUTSIDE_MATCH_ARM,
        "yield",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value {\n"
        "    case true => { const item = 1; } case false => { yield 0; }\n"
        "  };\n"
        "}\n",
        RUDA_DIAGNOSTIC_MISSING_MATCH_YIELD,
        "{ const item = 1; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value {\n"
        "    case true => { if value { yield 1; } }\n"
        "    case false => { yield 0; }\n"
        "  };\n"
        "}\n",
        RUDA_DIAGNOSTIC_MISSING_MATCH_YIELD,
        "{ if value { yield 1; } }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value {\n"
        "    case true => { yield 1; } case false => { yield false; }\n"
        "  };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "false",
        1
    );

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn pending(value: bool): int {\n"
        "  return match value {\n"
        "    case true => { return 1; } case false => { return 0; }\n"
        "  };\n"
        "}\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/match-block-never",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedFunction *pending = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 1);
        RUDA_EXPECT(pending != NULL);
        if (pending != NULL) {
            RUDA_EXPECT(ruda_mir_verify(&pending->mir) == RUDA_MIR_VERIFY_OK);
        }
        arguments[0].type = RUDA_TYPE_BOOL;
        arguments[0].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, arguments, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 1);
        arguments[0].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, arguments, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_match_return_arms(void) {
    static const char source[] =
        "enum Decision { Stop, Value(int), }\n"
        "fn stop(): Decision { return Decision.Stop; }\n"
        "fn decisionValue(item: int): Decision { return Decision.Value(item); }\n"
        "fn enumReturn(value: Decision): int {\n"
        "  return match value {\n"
        "    case Stop => { return 9; } case Value(item) => { yield item; }\n"
        "  };\n"
        "}\n"
        "fn firstReturn(value: bool): int {\n"
        "  return match value {\n"
        "    case true => { return 7; } case false => { yield 0; }\n"
        "  };\n"
        "}\n"
        "fn lastReturn(value: bool): int {\n"
        "  return match value {\n"
        "    case true => { yield 1; } case false => { return 8; }\n"
        "  };\n"
        "}\n"
        "fn branching(value: bool, condition: bool): int {\n"
        "  return match value {\n"
        "    case true => {\n"
        "      if condition { return 11; } else { return 12; }\n"
        "    }\n"
        "    case false => { yield 0; }\n"
        "  };\n"
        "}\n"
        "fn binding(value: bool): int {\n"
        "  const selected = match value {\n"
        "    case true => { return 9; } case false => { yield 2; }\n"
        "  };\n"
        "  return selected + 1;\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirValue arguments[2];
    RudaMirValue enum_value;
    RudaMirValue result;
    size_t function_index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/match-return-arms",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 7);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, NULL, 0, ruda_system_allocator(), options, &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, &enum_value, 1, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 9);

        arguments[0].type = RUDA_TYPE_I64;
        arguments[0].as.i64 = 4;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, arguments, 1, ruda_system_allocator(), options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, &enum_value, 1, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 4);

        arguments[0].type = RUDA_TYPE_BOOL;
        arguments[0].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 3, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 7);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 9);

        arguments[1].type = RUDA_TYPE_BOOL;
        arguments[1].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 2, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 11);
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 2, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 12);

        arguments[0].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 3, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 8);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 2, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, arguments, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 3);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_all_never_match(void) {
    static const char source[] =
        "fn allReturn(value: bool): int {\n"
        "  return match value {\n"
        "    case true => { return 1; } case false => { return 2; }\n"
        "  };\n"
        "}\n"
        "fn bindingNever(value: bool): int {\n"
        "  const unreachable = match value {\n"
        "    case true => { return 11; } case false => { return 12; }\n"
        "  };\n"
        "}\n"
        "fn binaryNever(value: bool): int {\n"
        "  return (match value {\n"
        "    case true => { return 21; } case false => { return 22; }\n"
        "  }) + 100;\n"
        "}\n"
        "fn consume(value: int): int { return value + 1000; }\n"
        "fn callNever(value: bool): int {\n"
        "  return consume(match value {\n"
        "    case true => { return 31; } case false => { return 32; }\n"
        "  });\n"
        "}\n"
        "fn unaryNever(value: bool): int {\n"
        "  return -(match value {\n"
        "    case true => { return 41; } case false => { return 42; }\n"
        "  });\n"
        "}\n"
        "fn shortNever(flag: bool, value: bool): bool {\n"
        "  return flag || match value {\n"
        "    case true => { return true; } case false => { return false; }\n"
        "  };\n"
        "}\n"
        "fn leftNever(flag: bool, value: bool): bool {\n"
        "  return (match value {\n"
        "    case true => { return true; } case false => { return false; }\n"
        "  }) || flag;\n"
        "}\n"
        "fn voidNever(value: bool): void {\n"
        "  return match value {\n"
        "    case true => { return; } case false => { return; }\n"
        "  };\n"
        "}\n"
        "type Box { item: int; }\n"
        "fn recordNever(value: bool): int {\n"
        "  const box = Box { item: match value {\n"
        "    case true => { return 51; } case false => { return 52; }\n"
        "  } };\n"
        "}\n"
        "enum Wrap { Value(int), }\n"
        "fn enumNever(value: bool): int {\n"
        "  const wrapped = Wrap.Value(match value {\n"
        "    case true => { return 61; } case false => { return 62; }\n"
        "  });\n"
        "}\n"
        "fn assignmentNever(value: bool): int {\n"
        "  let selected: int = 0;\n"
        "  selected = match value {\n"
        "    case true => { return 71; } case false => { return 72; }\n"
        "  };\n"
        "}\n"
        "fn nestedNever(value: bool, other: bool): int {\n"
        "  return match value {\n"
        "    case true => match other {\n"
        "      case true => { return 81; } case false => { return 82; }\n"
        "    };\n"
        "    case false => { return 80; }\n"
        "  };\n"
        "}\n"
        "fn intAllNever(value: int): int {\n"
        "  return match value { case _ => { return 91; } };\n"
        "}\n"
        "fn enumAllNever(value: Wrap): int {\n"
        "  return match value { case Value(_) => { return 101; } };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue arguments[2];
    RudaMirValue result;
    size_t function_index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/all-never-match",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedFunction *all_return = ruda_checked_function(
            &checked,
            0
        );
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 15);
        RUDA_EXPECT(checked.records.length == 1);
        RUDA_EXPECT(checked.enums.length == 1);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        RUDA_EXPECT(all_return != NULL);
        if (all_return != NULL) {
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &all_return->mir,
                RUDA_MIR_COPY
            ) == 0);
        }
        arguments[0].type = RUDA_TYPE_BOOL;
        arguments[1].type = RUDA_TYPE_BOOL;
        for (function_index = 0; function_index < 2; function_index += 1) {
            arguments[0].as.boolean = function_index != 0;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, 0, arguments, 1, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == (function_index != 0 ? 1 : 2));
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, 1, arguments, 1, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == (function_index != 0 ? 11 : 12));
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, 2, arguments, 1, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == (function_index != 0 ? 21 : 22));
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, 4, arguments, 1, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == (function_index != 0 ? 31 : 32));
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, 5, arguments, 1, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == (function_index != 0 ? 41 : 42));
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, 8, arguments, 1, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_VOID);
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, 9, arguments, 1, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == (function_index != 0 ? 51 : 52));
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, 10, arguments, 1, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == (function_index != 0 ? 61 : 62));
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, 11, arguments, 1, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == (function_index != 0 ? 71 : 72));
        }
        arguments[0].as.boolean = true;
        arguments[1].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_BOOL && result.as.boolean);
        arguments[0].as.boolean = false;
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_BOOL && !result.as.boolean);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_BOOL && !result.as.boolean);
        arguments[0].as.boolean = true;
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 12, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 82);
        arguments[0].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 12, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 80);
        arguments[0].type = RUDA_TYPE_I64;
        arguments[0].as.i64 = 123;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 13, arguments, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 91);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_never_conditions_and_guards(void) {
    static const char source[] =
        "fn ifNever(value: bool): int {\n"
        "  if match value {\n"
        "    case true => { return 1; } case false => { return 2; }\n"
        "  } { return 99; }\n"
        "}\n"
        "fn i64Guard(value: int, decision: bool): int {\n"
        "  return match value {\n"
        "    case 0 if match decision {\n"
        "      case true => { return 11; } case false => { return 12; }\n"
        "    } => 99;\n"
        "    case _ => 1;\n"
        "  };\n"
        "}\n"
        "fn boolGuard(value: bool, decision: bool): int {\n"
        "  return match value {\n"
        "    case true if match decision {\n"
        "      case true => { return 21; } case false => { return 22; }\n"
        "    } => 99;\n"
        "    case true => 1; case false => 0;\n"
        "  };\n"
        "}\n"
        "enum Signal { A, B, }\n"
        "fn makeA(): Signal { return Signal.A; }\n"
        "fn makeB(): Signal { return Signal.B; }\n"
        "fn enumGuard(value: Signal, decision: bool): int {\n"
        "  return match value {\n"
        "    case A if match decision {\n"
        "      case true => { return 31; } case false => { return 32; }\n"
        "    } => 99;\n"
        "    case A => 1; case B => 2;\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirValue arguments[2];
    RudaMirValue enum_value;
    RudaMirValue result;
    size_t function_index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/never-conditions-guards",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 6);
        RUDA_EXPECT(checked.enums.length == 1);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        arguments[0].type = RUDA_TYPE_BOOL;
        arguments[0].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, arguments, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 1);
        arguments[0].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, arguments, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);

        arguments[0].type = RUDA_TYPE_I64;
        arguments[0].as.i64 = 1;
        arguments[1].type = RUDA_TYPE_BOOL;
        arguments[1].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);
        arguments[0].as.i64 = 0;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 11);
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 12);

        arguments[0].type = RUDA_TYPE_BOOL;
        arguments[0].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);
        arguments[0].as.boolean = true;
        arguments[1].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 21);
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 22);

        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 3, NULL, 0, ruda_system_allocator(), options, &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        arguments[0] = enum_value;
        arguments[1].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 2, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 31);
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 2, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 32);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, NULL, 0, ruda_system_allocator(), options, &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        arguments[0] = enum_value;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 2, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn whileNever(value: bool): int {\n"
        "  while match value {\n"
        "    case true => { return 1; } case false => { return 2; }\n"
        "  } {}\n"
        "}\n"
        "fn whileMixed(value: bool): int {\n"
        "  while match value {\n"
        "    case true => { return 3; }\n"
        "    case false => { yield false; }\n"
        "  } {}\n"
        "  return 4;\n"
        "}\n"
        "fn forConditionNever(value: bool): int {\n"
        "  for let index = 0; match value {\n"
        "    case true => { return 5; } case false => { return 6; }\n"
        "  }; index += 1 {}\n"
        "}\n"
        "fn forConditionMixed(value: bool): int {\n"
        "  for let index = 0; match value {\n"
        "    case true => { return 7; }\n"
        "    case false => { yield false; }\n"
        "  }; index += 1 {}\n"
        "  return 8;\n"
        "}\n"
        "fn forInitializerNever(value: bool): int {\n"
        "  for let index = match value {\n"
        "    case true => { return 9; } case false => { return 10; }\n"
        "  }; false; index += 1 {}\n"
        "}\n"
        "fn forStepNever(value: bool, ready: bool): int {\n"
        "  for let index = 0; ready; index = match value {\n"
        "    case true => { return 11; } case false => { return 12; }\n"
        "  } {}\n"
        "  return 13;\n"
        "}\n"
        "fn forStepMixed(value: bool): int {\n"
        "  for let index = 0; index < 1; index = match value {\n"
        "    case true => { return 14; }\n"
        "    case false => { yield index + 1; }\n"
        "  } {}\n"
        "  return 15;\n"
        "}\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/never-loop-conditions",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 7);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        arguments[0].type = RUDA_TYPE_BOOL;
        for (function_index = 0; function_index < 5; function_index += 1) {
            arguments[0].as.boolean = true;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, function_index, arguments, 1, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == (int64_t)(function_index * 2 + 1));
            arguments[0].as.boolean = false;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked, function_index, arguments, 1, ruda_system_allocator(),
                ruda_check_interpret_default_options(), &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == (int64_t)(function_index * 2 + 2));
        }
        arguments[1].type = RUDA_TYPE_BOOL;
        arguments[1].as.boolean = true;
        arguments[0].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 11);
        arguments[0].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 12);
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 2, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 13);
        arguments[0].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, arguments, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 14);
        arguments[0].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, arguments, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 15);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_strings_and_panic(void) {
    static const char source[] =
        "fn direct(): int { panic(\"broken\\n\\u{26cf}\"); }\n"
        "fn selected(value: bool): int {\n"
        "  return match value {\n"
        "    case true => panic(r\"path\\\\raw\"); case false => 7;\n"
        "  };\n"
        "}\n"
        "fn conditional(value: bool): int {\n"
        "  if value { panic(\"nested\"); }\n"
        "  return 9;\n"
        "}\n"
        "fn text(): string { return \"line\\n\\u{26cf}\\0end\"; }\n"
        "fn callee(): int { panic(\"callee\"); }\n"
        "fn caller(): int { return callee(); }\n"
        "fn accept(value: int): int { return value; }\n"
        "fn argument(): int { return accept(panic(\"argument\")); }\n"
        "fn allPanic(value: bool): int {\n"
        "  return match value {\n"
        "    case true => panic(\"yes\"); case false => panic(\"no\");\n"
        "  };\n"
        "}\n"
        "fn message(): string { return \"dynamic\"; }\n"
        "fn indirect(): int { panic(message()); }\n";
    static const unsigned char text_value[] = "line\n\xe2\x9b\x8f\0end";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirExecution execution;
    RudaMirPanic panic;
    RudaMirValue argument;
    RudaMirValue result;
    const unsigned char *bytes = NULL;
    size_t length = 0;
    size_t function_index = 0;
    const char *direct_panic = strstr(source, "panic(\"broken");
    memset(&panic, 0, sizeof(panic));
    RUDA_EXPECT(direct_panic != NULL);
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/strings-panic",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 11);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        options.panic = &panic;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, NULL, 0, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
        RUDA_EXPECT(ruda_symbol_id_equal(
            panic.symbol,
            ruda_checked_function(&checked, 0)->mir.symbol
        ));
        RUDA_EXPECT(panic.message.length == 10);
        RUDA_EXPECT(panic.message.data != NULL && memcmp(
            panic.message.data,
            "broken\n\xe2\x9b\x8f",
            10
        ) == 0);
        if (direct_panic != NULL) {
            size_t start = (size_t)(direct_panic - source);
            RUDA_EXPECT(panic.span.start == start);
            RUDA_EXPECT(panic.span.end == start + strlen(
                "panic(\"broken\\n\\u{26cf}\")"
            ));
        }

        argument.type = RUDA_TYPE_BOOL;
        argument.as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, &argument, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 7);
        RUDA_EXPECT(panic.message.data == NULL);
        argument.as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, &argument, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 9 && memcmp(
            panic.message.data,
            "path\\\\raw",
            9
        ) == 0);
        argument.as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, &argument, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 9);
        argument.as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, &argument, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 6 && memcmp(
            panic.message.data,
            "nested",
            6
        ) == 0);

        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 3, NULL, 0, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_STRING);
        RUDA_EXPECT(ruda_mir_execution_string_bytes(
            &execution,
            result,
            &bytes,
            &length
        ));
        RUDA_EXPECT(length == sizeof(text_value) - 1);
        RUDA_EXPECT(bytes != NULL && memcmp(
            bytes,
            text_value,
            sizeof(text_value) - 1
        ) == 0);
        options.execution = NULL;
        ruda_mir_execution_dispose(&execution);

        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, NULL, 0, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 6 && memcmp(
            panic.message.data,
            "callee",
            6
        ) == 0);
        RUDA_EXPECT(ruda_symbol_id_equal(
            panic.symbol,
            ruda_checked_function(&checked, 4)->mir.symbol
        ));
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, NULL, 0, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 8 && memcmp(
            panic.message.data,
            "argument",
            8
        ) == 0);
        argument.as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 8, &argument, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 3
            && memcmp(panic.message.data, "yes", 3) == 0);
        argument.as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 8, &argument, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 2
            && memcmp(panic.message.data, "no", 2) == 0);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 10, NULL, 0, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 7
            && memcmp(panic.message.data, "dynamic", 7) == 0);
        RUDA_EXPECT(ruda_symbol_id_equal(
            panic.symbol,
            ruda_checked_function(&checked, 10)->mir.symbol
        ));
        ruda_mir_panic_dispose(&panic);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "fn bad(): int { panic( ); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "( )",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): int { panic(\"a\", \"b\"); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "(\"a\", \"b\")",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): int { panic(1); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): int { panic; }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "panic",
        0
    );
}

static void ruda_test_checker_string_match(void) {
    static const char source[] =
        "fn empty(): string { return \"\"; }\n"
        "fn ready(): string { return \"ready\"; }\n"
        "fn embedded(): string { return \"a\\0b\"; }\n"
        "fn other(): string { return \"other\"; }\n"
        "fn classify(value: string): int {\n"
        "  return match value {\n"
        "    case \"\" => 0; case \"ready\" => 1;\n"
        "    case \"a\\0b\" => 2; case remaining => 3;\n"
        "  };\n"
        "}\n"
        "fn rawClassify(value: string): int {\n"
        "  return match value { case r\"ready\" => 4; case _ => 5; };\n"
        "}\n"
        "fn guarded(value: string, flag: bool): int {\n"
        "  return match value {\n"
        "    case \"ready\" if flag => 6; case _ => 7;\n"
        "  };\n"
        "}\n"
        "fn block(value: string): int {\n"
        "  return match value {\n"
        "    case \"ready\" => { yield 8; } case _ => 9;\n"
        "  };\n"
        "}\n"
        "fn composed(): string { return \"\\u{e9}\"; }\n"
        "fn decomposed(): string { return \"e\\u{301}\"; }\n"
        "fn accents(value: string): int {\n"
        "  return match value {\n"
        "    case \"\\u{e9}\" => 10; case \"e\\u{301}\" => 11; case _ => 12;\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirValue arguments[2];
    RudaMirValue strings[4];
    RudaMirValue result;
    size_t index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/string-match",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedFunction *classify = ruda_checked_function(
            &checked,
            4
        );
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 11);
        for (index = 0; index < checked.functions.length; index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        RUDA_EXPECT(classify != NULL);
        if (classify != NULL) {
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &classify->mir,
                RUDA_MIR_EQUAL_STRING
            ) == 3);
        }
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        for (index = 0; index < 4; index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                index,
                NULL,
                0,
                ruda_system_allocator(),
                options,
                &strings[index]
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(strings[index].type == RUDA_TYPE_STRING);
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                4,
                &strings[index],
                1,
                ruda_system_allocator(),
                options,
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == (int64_t)index);
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, &strings[1], 1, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 4);
        arguments[0] = strings[1];
        arguments[1].type = RUDA_TYPE_BOOL;
        arguments[1].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, arguments, 2, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 6);
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, arguments, 2, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 7);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, &strings[1], 1, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 8);
        for (index = 0; index < 2; index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                8 + index,
                NULL,
                0,
                ruda_system_allocator(),
                options,
                &arguments[0]
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                10,
                arguments,
                1,
                ruda_system_allocator(),
                options,
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == (int64_t)(10 + index));
        }
        arguments[0] = strings[1];
        arguments[0].as.reference = RUDA_MIR_INVALID_REFERENCE;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, arguments, 1, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "fn bad(value: string): int {\n"
        "  return match value { case \"ready\" => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case \"ready\" => 1; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: string): int {\n"
        "  return match value { case \"a\" => 1; case r\"a\" => 2; case _ => 3; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "r\"a\"",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: string): int {\n"
        "  return match value { case \"a\" | \"\\u{61}\" => 1; case _ => 2; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "\"\\u{61}\"",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: string): int {\n"
        "  return match value { case 1 => 1; case _ => 2; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: string): int {\n"
        "  return match value { case \"a\" if true => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case \"a\" if true => 1; }",
        0
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: string): int {\n"
        "  return match value { case \"ruda#0\" => 1; };\n"
        "}\n",
        "string match is missing witness: \"ruda#1\""
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: string): int {\n"
        "  return match value { case \"ruda#0\" if true => 1; };\n"
        "}\n",
        "string match is missing witness: \"ruda#0\"; guarded patterns do not count toward exhaustiveness"
    );
}

static void ruda_test_checker_declared_never_calls(void) {
    static const char source[] =
        "fn stop(message: string): never { panic(message); }\n"
        "fn direct(): int { stop(\"direct\"); }\n"
        "fn selected(value: bool): int {\n"
        "  return match value {\n"
        "    case true => stop(\"selected\"); case false => 7;\n"
        "  };\n"
        "}\n"
        "fn nested(): never { return stop(\"nested\"); }\n"
        "fn caller(): int { nested(); }\n"
        "fn accept(value: int): int { return value; }\n"
        "fn argument(): int { return accept(stop(\"argument\")); }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirPanic panic;
    RudaMirValue argument;
    RudaMirValue result;
    size_t function_index = 0;
    memset(&panic, 0, sizeof(panic));
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/declared-never",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 7);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        RUDA_EXPECT(ruda_checked_function(&checked, 0)->mir.result_type
            == RUDA_TYPE_NEVER);
        RUDA_EXPECT(ruda_checked_function(&checked, 3)->mir.result_type
            == RUDA_TYPE_NEVER);
        options.panic = &panic;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            NULL,
            0,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 6
            && memcmp(panic.message.data, "direct", 6) == 0);
        RUDA_EXPECT(ruda_symbol_id_equal(
            panic.symbol,
            ruda_checked_function(&checked, 0)->mir.symbol
        ));

        argument.type = RUDA_TYPE_BOOL;
        argument.as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 7);
        argument.as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 8
            && memcmp(panic.message.data, "selected", 8) == 0);

        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            NULL,
            0,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 6
            && memcmp(panic.message.data, "nested", 6) == 0);
        RUDA_EXPECT(ruda_symbol_id_equal(
            panic.symbol,
            ruda_checked_function(&checked, 0)->mir.symbol
        ));

        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            6,
            NULL,
            0,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 8
            && memcmp(panic.message.data, "argument", 8) == 0);
        ruda_mir_panic_dispose(&panic);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn bad(): never {}\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/declared-never-missing",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckDiagnostic *diagnostic = ruda_check_diagnostic(
            &checked,
            0
        );
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_MISSING_RETURN
            ) == 0);
            RUDA_EXPECT(diagnostic->start == diagnostic->end);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
    ruda_checker_expect_one_diagnostic(
        "fn bad(): never { return; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "return;",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): never { return 1; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
}

static void ruda_test_checker_one_witness_allocation_failure(
    const char *source,
    const char *expected_message
) {
    RudaCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(source != NULL && expected_message != NULL);
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCheckResult checked;
        RudaCheckStatus status;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        status = ruda_check_ast(
            &fixture.ast,
            "test/witness-allocation",
            allocator,
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            const RudaCheckDiagnostic *diagnostic = NULL;
            reached_success = true;
            RUDA_EXPECT(checked.initialized);
            RUDA_EXPECT(checked.diagnostics.length == 1);
            diagnostic = ruda_check_diagnostic(&checked, 0);
            RUDA_EXPECT(diagnostic != NULL);
            if (diagnostic != NULL) {
                RUDA_EXPECT(strcmp(
                    diagnostic->message,
                    expected_message
                ) == 0);
            }
            ruda_check_result_dispose(&checked);
        } else {
            RUDA_EXPECT(status == RUDA_CHECK_ALLOCATION_FAILED);
            RUDA_EXPECT(!checked.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_witness_allocation_failures(void) {
    static const char enum_source[] =
        "enum Message { Quit, Write(int), Move { x: int; y: int; }, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; };\n"
        "}\n";
    static const char string_source[] =
        "fn bad(value: string): int {\n"
        "  return match value { case \"ruda#0\" => 0; };\n"
        "}\n";
    static const char record_source[] =
        "type Point { x: int; y: int; }\n"
        "fn bad(value: Point): int {\n"
        "  return match value { case Point { x, y } if true => x + y; };\n"
        "}\n";
    ruda_test_checker_one_witness_allocation_failure(
        enum_source,
        "enum match is missing witnesses: Write(_), Move { x: _, y: _ }"
    );
    ruda_test_checker_one_witness_allocation_failure(
        string_source,
        "string match is missing witness: \"ruda#1\""
    );
    ruda_test_checker_one_witness_allocation_failure(
        record_source,
        "record match is missing witness: Point { x: _, y: _ }; guarded patterns do not count toward exhaustiveness"
    );
}

static void ruda_test_checker_calls(void) {
    static const char source[] =
        "fn caller(value: int): int { return double(value) + one(); }\n"
        "fn one(): int { return 1; }\n"
        "fn double(value: int): int { return value + value; }\n"
        "fn factorial(value: int): int {\n"
        "  if value <= 1 { return 1; }\n"
        "  return value * factorial(value - 1);\n"
        "}\n"
        "fn noop(): void {}\n"
        "fn entry(): void { noop(); }\n";
    static const char wrong_arity[] =
        "fn target(value: int): int { return value; }\n"
        "fn bad(): int { return target(); }\n";
    static const char wrong_type[] =
        "fn target(value: int): int { return value; }\n"
        "fn bad(): int { return target(true); }\n";
    static const char unknown[] =
        "fn bad(): int { return absent(1); }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue argument;
    RudaMirValue result;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    const RudaCheckedFunction *caller = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 6);
        RUDA_EXPECT(checked.function_index.length == 6);
        caller = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(caller != NULL);
        if (caller != NULL) {
            RUDA_EXPECT(caller->mir.calls.length == 2);
            RUDA_EXPECT(caller->mir.call_arguments.length == 1);
        }
        argument.type = RUDA_TYPE_I64;
        argument.as.i64 = 5;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64);
        RUDA_EXPECT(result.as.i64 == 11);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 120);
        options.max_total_steps = 2;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_STEP_LIMIT);
        options = ruda_check_interpret_default_options();
        options.max_call_depth = 2;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_STEP_LIMIT);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            5,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_VOID);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
    ruda_checker_expect_one_diagnostic(
        wrong_arity,
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "()",
        1
    );
    ruda_checker_expect_one_diagnostic(
        wrong_type,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        unknown,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "absent",
        0
    );
}

static void ruda_test_checker_top_level_constants(void) {
    static const char source[] =
        "const answer: int = later + 1;\n"
        "const later = 41;\n"
        "const ready = answer == 42;\n"
        "fn entry(): int {\n"
        "  if ready { return answer; }\n"
        "  return 0;\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedConstant *answer = NULL;
    const RudaCheckedConstant *later = NULL;
    const RudaCheckedConstant *ready = NULL;
    RudaMirValue result;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/constants",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.constants.length == 3);
        RUDA_EXPECT(checked.functions.length == 1);
        answer = ruda_checked_constant(&checked, 0);
        later = ruda_checked_constant(&checked, 1);
        ready = ruda_checked_constant(&checked, 2);
        RUDA_EXPECT(answer != NULL);
        RUDA_EXPECT(later != NULL);
        RUDA_EXPECT(ready != NULL);
        if (answer != NULL && later != NULL && ready != NULL) {
            RUDA_EXPECT(answer->name.length == strlen("answer"));
            RUDA_EXPECT(answer->value.type == RUDA_TYPE_I64);
            RUDA_EXPECT(answer->value.as.i64 == 42);
            RUDA_EXPECT(later->value.type == RUDA_TYPE_I64);
            RUDA_EXPECT(later->value.as.i64 == 41);
            RUDA_EXPECT(ready->value.type == RUDA_TYPE_BOOL);
            RUDA_EXPECT(ready->value.as.boolean);
            RUDA_EXPECT(!ruda_symbol_id_equal(
                answer->symbol,
                later->symbol
            ));
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64);
        RUDA_EXPECT(result.as.i64 == 42);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_constant_diagnostics(void) {
    static const char cycle[] =
        "const first: int = second;\n"
        "const second: int = first;\n"
        "fn entry(): int { return first; }\n";
    static const char overflow[] =
        "const bad: int = 9223372036854775807 + 1;\n"
        "fn entry(): int { return bad; }\n";
    static const char division_by_zero[] =
        "const bad: int = 1 / 0;\n"
        "fn entry(): int { return bad; }\n";
    static const char call[] =
        "fn one(): int { return 1; }\n"
        "const bad: int = one();\n";
    static const char mismatch[] =
        "const bad: bool = 1;\n"
        "fn entry(): bool { return bad; }\n";
    static const char unknown[] =
        "const bad: int = missing;\n"
        "fn entry(): int { return bad; }\n";
    static const char duplicate[] =
        "const repeated: int = 1;\n"
        "fn repeated(): int { return 2; }\n";
    static const char called_constant[] =
        "const value: int = 1;\n"
        "fn bad(): int { return value(); }\n";
    static const char body_error_after_constant[] =
        "const value: int = 1;\n"
        "fn bad(): int { return missing; }\n";
    static const char invalid_logical_constant[] =
        "const bad = true || 1;\n";
    static const char limits[] =
        "const first: int = second;\n"
        "const second: int = 1;\n"
        "fn entry(): int { return first; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckOptions options;
    const RudaCheckDiagnostic *diagnostic = NULL;

    ruda_checker_expect_one_diagnostic(
        cycle,
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "first",
        1
    );
    ruda_checker_expect_one_diagnostic(
        overflow,
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "9223372036854775807 + 1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        division_by_zero,
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "1 / 0",
        0
    );
    ruda_checker_expect_one_diagnostic(
        call,
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "one()",
        1
    );
    ruda_checker_expect_one_diagnostic(
        mismatch,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        unknown,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        duplicate,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "repeated",
        1
    );
    ruda_checker_expect_one_diagnostic(
        called_constant,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "value",
        1
    );
    ruda_checker_expect_one_diagnostic(
        body_error_after_constant,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        invalid_logical_constant,
        RUDA_DIAGNOSTIC_INVALID_OPERATOR,
        "||",
        0
    );

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, limits));
    options = ruda_check_default_options();
    options.max_constant_depth = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/constants",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.constants.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_CONSTANT_EVALUATION
            ) == 0);
        }
        ruda_check_result_dispose(&checked);
    }
    options = ruda_check_default_options();
    options.max_constant_steps = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/constants",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.constants.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_CONSTANT_EVALUATION
            ) == 0);
        }
        ruda_check_result_dispose(&checked);
    }
    options = ruda_check_default_options();
    options.max_constants = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/constants",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

}

static void ruda_test_checker_short_circuit(void) {
    static const char source[] =
        "const safeAnd = false && (1 / 0 == 0);\n"
        "const safeOr = true || (1 / 0 == 0);\n"
        "fn andSkip(): bool { return false && (1 / 0 == 0); }\n"
        "fn orSkip(): bool { return true || (1 / 0 == 0); }\n"
        "fn andEvaluate(): bool { return true && (6 / 2 == 3); }\n"
        "fn orEvaluate(): bool { return false || (6 / 2 == 3); }\n"
        "fn nestedRight(): bool { return true && (false || true); }\n"
        "fn nestedLeft(): bool { return (false || true) && true; }\n";
    static const char invalid[] =
        "fn bad(): bool { return 1 && true; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedConstant *safe_and = NULL;
    const RudaCheckedConstant *safe_or = NULL;
    size_t index = 0;
    RudaMirValue result;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/short-circuit",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.constants.length == 2);
        RUDA_EXPECT(checked.functions.length == 6);
        safe_and = ruda_checked_constant(&checked, 0);
        safe_or = ruda_checked_constant(&checked, 1);
        RUDA_EXPECT(safe_and != NULL);
        RUDA_EXPECT(safe_or != NULL);
        if (safe_and != NULL && safe_or != NULL) {
            RUDA_EXPECT(safe_and->value.type == RUDA_TYPE_BOOL);
            RUDA_EXPECT(!safe_and->value.as.boolean);
            RUDA_EXPECT(safe_or->value.type == RUDA_TYPE_BOOL);
            RUDA_EXPECT(safe_or->value.as.boolean);
        }
        for (index = 0; index < checked.functions.length; index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                index
            );
            RUDA_EXPECT(function != NULL);
            if (function == NULL) {
                continue;
            }
            RUDA_EXPECT(function->mir.blocks.length == (index < 4 ? 4 : 7));
            RUDA_EXPECT(ruda_mir_interpret(
                &function->mir,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_BOOL);
            if (index == 0) {
                RUDA_EXPECT(!result.as.boolean);
            } else {
                RUDA_EXPECT(result.as.boolean);
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
    ruda_checker_expect_one_diagnostic(
        invalid,
        RUDA_DIAGNOSTIC_INVALID_OPERATOR,
        "&&",
        0
    );
}

static void ruda_test_checker_bitwise(void) {
    static const char source[] =
        "const mask: int = (((~0) & 255) ^ 15) | 256;\n"
        "fn apply(value: int): int {\n"
        "  let current: int = value;\n"
        "  current &= 255;\n"
        "  current ^= 15;\n"
        "  current |= 256;\n"
        "  return current;\n"
        "}\n"
        "fn negative(): int { return ~0; }\n";
    static const char invalid[] =
        "fn bad(): bool { return true & false; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedConstant *mask = NULL;
    const RudaCheckedFunction *apply = NULL;
    const RudaCheckedFunction *negative = NULL;
    RudaMirValue argument;
    RudaMirValue result;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/bitwise",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.constants.length == 1);
        RUDA_EXPECT(checked.functions.length == 2);
        mask = ruda_checked_constant(&checked, 0);
        apply = ruda_checked_function(&checked, 0);
        negative = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(mask != NULL);
        RUDA_EXPECT(apply != NULL);
        RUDA_EXPECT(negative != NULL);
        if (mask != NULL) {
            RUDA_EXPECT(mask->value.type == RUDA_TYPE_I64);
            RUDA_EXPECT(mask->value.as.i64 == 496);
        }
        if (apply != NULL) {
            argument.type = RUDA_TYPE_I64;
            argument.as.i64 = 291;
            RUDA_EXPECT(ruda_mir_interpret(
                &apply->mir,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 300);
        }
        if (negative != NULL) {
            RUDA_EXPECT(ruda_mir_interpret(
                &negative->mir,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == -1);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
    ruda_checker_expect_one_diagnostic(
        invalid,
        RUDA_DIAGNOSTIC_INVALID_OPERATOR,
        "&",
        0
    );

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn bad(left: i32, right: i32): i32 { return left & right; }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/bitwise",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_shifts(void) {
    static const char source[] =
        "const lowest: int = -1 << 63;\n"
        "const signed: int = -7 >> 1;\n"
        "const precedence: int = 1 + 1 << 2;\n"
        "fn compound(value: int, count: int): int {\n"
        "  let current: int = value;\n"
        "  current <<= count;\n"
        "  current >>= 1;\n"
        "  return current;\n"
        "}\n"
        "fn right(value: int, count: int): int { return value >> count; }\n";
    static const char overflow[] =
        "const bad: int = 1 << 63;\n";
    static const char negative_count[] =
        "const bad: int = 1 << -1;\n";
    static const char large_count[] =
        "const bad: int = 1 >> 64;\n";
    static const char invalid[] =
        "fn bad(): bool { return true << false; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedConstant *lowest = NULL;
    const RudaCheckedConstant *signed_value = NULL;
    const RudaCheckedConstant *precedence = NULL;
    const RudaCheckedFunction *compound = NULL;
    const RudaCheckedFunction *right = NULL;
    RudaMirValue arguments[2];
    RudaMirValue result;

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/shifts",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.constants.length == 3);
        RUDA_EXPECT(checked.functions.length == 2);
        lowest = ruda_checked_constant(&checked, 0);
        signed_value = ruda_checked_constant(&checked, 1);
        precedence = ruda_checked_constant(&checked, 2);
        compound = ruda_checked_function(&checked, 0);
        right = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(lowest != NULL);
        RUDA_EXPECT(signed_value != NULL);
        RUDA_EXPECT(precedence != NULL);
        RUDA_EXPECT(compound != NULL);
        RUDA_EXPECT(right != NULL);
        if (lowest != NULL && signed_value != NULL && precedence != NULL) {
            RUDA_EXPECT(lowest->value.as.i64 == INT64_MIN);
            RUDA_EXPECT(signed_value->value.as.i64 == -4);
            RUDA_EXPECT(precedence->value.as.i64 == 8);
        }
        arguments[0].type = RUDA_TYPE_I64;
        arguments[1].type = RUDA_TYPE_I64;
        if (compound != NULL) {
            arguments[0].as.i64 = 3;
            arguments[1].as.i64 = 2;
            RUDA_EXPECT(ruda_mir_interpret(
                &compound->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 6);
            arguments[0].as.i64 = 1;
            arguments[1].as.i64 = 63;
            RUDA_EXPECT(ruda_mir_interpret(
                &compound->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OVERFLOW);
        }
        if (right != NULL) {
            arguments[0].as.i64 = -7;
            arguments[1].as.i64 = 1;
            RUDA_EXPECT(ruda_mir_interpret(
                &right->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == -4);
            arguments[1].as.i64 = 64;
            RUDA_EXPECT(ruda_mir_interpret(
                &right->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_INVALID_SHIFT);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        overflow,
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "1 << 63",
        0
    );
    ruda_checker_expect_one_diagnostic(
        negative_count,
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "1 << -1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        large_count,
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "1 >> 64",
        0
    );
    ruda_checker_expect_one_diagnostic(
        invalid,
        RUDA_DIAGNOSTIC_INVALID_OPERATOR,
        "<<",
        0
    );

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn bad(left: i32, right: i32): i32 { return left << right; }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/shifts",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_division_and_remainder(void) {
    static const char source[] =
        "fn quotient(left: int, right: int): int { return left / right; }\n"
        "fn remainder(left: int, right: int): int { return left % right; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue arguments[2];
    RudaMirValue result;
    const RudaCheckedFunction *quotient = NULL;
    const RudaCheckedFunction *remainder = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/division-remainder",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        quotient = ruda_checked_function(&checked, 0);
        remainder = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(quotient != NULL);
        RUDA_EXPECT(remainder != NULL);
        arguments[0].type = RUDA_TYPE_I64;
        arguments[1].type = RUDA_TYPE_I64;

        if (quotient != NULL) {
            arguments[0].as.i64 = -7;
            arguments[1].as.i64 = 3;
            RUDA_EXPECT(ruda_mir_interpret(
                &quotient->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == -2);
            arguments[0].as.i64 = 7;
            arguments[1].as.i64 = -3;
            RUDA_EXPECT(ruda_mir_interpret(
                &quotient->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == -2);
            arguments[0].as.i64 = INT64_MIN;
            arguments[1].as.i64 = -1;
            RUDA_EXPECT(ruda_mir_interpret(
                &quotient->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OVERFLOW);
            arguments[0].as.i64 = 1;
            arguments[1].as.i64 = 0;
            RUDA_EXPECT(ruda_mir_interpret(
                &quotient->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        }

        if (remainder != NULL) {
            arguments[0].as.i64 = -7;
            arguments[1].as.i64 = 3;
            RUDA_EXPECT(ruda_mir_interpret(
                &remainder->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == -1);
            arguments[0].as.i64 = 7;
            arguments[1].as.i64 = -3;
            RUDA_EXPECT(ruda_mir_interpret(
                &remainder->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 1);
            arguments[0].as.i64 = INT64_MIN;
            arguments[1].as.i64 = -1;
            RUDA_EXPECT(ruda_mir_interpret(
                &remainder->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OVERFLOW);
            arguments[0].as.i64 = 1;
            arguments[1].as.i64 = 0;
            RUDA_EXPECT(ruda_mir_interpret(
                &remainder->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_f64_literals_and_operators(void) {
    static const char half_min_subnormal[] =
        "24703282292062327208828439643411068618252990130716238221279284125033775363510437593264991818081799618989828234772285886546332835517796989819938739800539093906315035659515570226392290858392449105184435931802849936536152500319370457678249219365623669863658480757001585769269903706311928279558551332927834338409351978015531246597263579574622766465272827220056374006485499977096599470454020828166226237857393450736339007967761930577506740176324673600968951340535537458516661134223766678604162159680461914467291840300530057530849048765391711386591646239524912623653881879636239373280423891018672348497668235089863388587925628302755995657524455507255189313690836254779186948667994968324049705821028513185451396213837722826145437693412532098591327667236328125";
    static const char beyond_half_min_subnormal[] =
        "24703282292062327208828439643411068618252990130716238221279284125033775363510437593264991818081799618989828234772285886546332835517796989819938739800539093906315035659515570226392290858392449105184435931802849936536152500319370457678249219365623669863658480757001585769269903706311928279558551332927834338409351978015531246597263579574622766465272827220056374006485499977096599470454020828166226237857393450736339007967761930577506740176324673600968951340535537458516661134223766678604162159680461914467291840300530057530849048765391711386591646239524912623653881879636239373280423891018672348497668235089863388587925628302755995657524455507255189313690836254779186948667994968324049705821028513185451396213837722826145437693412532098591327667236328126";
    static const uint64_t expected_f64[] = {
        UINT64_C(0x4340000000000000),
        UINT64_C(0x0000000000000001),
        UINT64_C(0x0000000000000000),
        UINT64_C(0x0000000000000001),
        UINT64_C(0x7fefffffffffffff),
        UINT64_C(0x7ff0000000000000),
        UINT64_C(0x4029000000000000),
        UINT64_C(0x8000000000000000),
        UINT64_C(0x400e000000000000),
        UINT64_C(0x4008000000000000),
        UINT64_C(0xc018000000000000),
        UINT64_C(0x7ff0000000000000)
    };
    static const RudaMirInstructionKind arithmetic_kinds[] = {
        RUDA_MIR_ADD_F64,
        RUDA_MIR_SUBTRACT_F64,
        RUDA_MIR_MULTIPLY_F64,
        RUDA_MIR_DIVIDE_F64
    };
    static const RudaMirInstructionKind comparison_kinds[] = {
        RUDA_MIR_EQUAL_F64,
        RUDA_MIR_EQUAL_F64,
        RUDA_MIR_LESS_F64,
        RUDA_MIR_LESS_EQUAL_F64,
        RUDA_MIR_LESS_F64,
        RUDA_MIR_LESS_EQUAL_F64
    };
    char source[8192];
    int written = snprintf(
        source,
        sizeof(source),
        "const topF64: f64 = -0.0;\n"
        "fn rounded(): f64 { return 9007199254740993.0; }\n"
        "fn minSubnormal(): f64 { return 5e-324; }\n"
        "fn halfTie(): f64 { return %se-1075; }\n"
        "fn beyondHalfTie(): f64 { return %se-1075; }\n"
        "fn maxFinite(): f64 { return 1.7976931348623157e308; }\n"
        "fn infinity(): f64 { return 1.7976931348623159e308; }\n"
        "fn separated(): f64 { return 1_2.5_0; }\n"
        "fn negativeZero(): f64 { return -0.0; }\n"
        "fn add(): f64 { return 1.5 + 2.25; }\n"
        "fn subtract(): f64 { return 5.0 - 2.0; }\n"
        "fn multiply(): f64 { return -2.0 * 3.0; }\n"
        "fn divide(): f64 { return 1.0 / 0.0; }\n"
        "fn equal(): bool { return 0.0 == -0.0; }\n"
        "fn notEqual(): bool { return 1.0 != 2.0; }\n"
        "fn less(): bool { return -1.0 < 0.0; }\n"
        "fn lessEqual(): bool { return 1.0 <= 1.0; }\n"
        "fn greater(): bool { return 2.0 > 1.0; }\n"
        "fn greaterEqual(): bool { return 2.0 >= 2.0; }\n"
        "fn identity(value: f64): f64 { return value; }\n"
        "fn direct(): f64 { return identity(-0.0); }\n"
        "fn negate(value: f64): f64 { return -value; }\n"
        "fn compound(value: f64): f64 {\n"
        "  let result: f64 = value;\n"
        "  result += 1.0; result -= 2.0;\n"
        "  result *= 4.0; result /= 2.0;\n"
        "  return result;\n"
        "}\n"
        "fn fromTop(): f64 { return topF64; }\n",
        half_min_subnormal,
        beyond_half_min_subnormal
    );
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue argument;
    RudaMirValue result;
    size_t function_index = 0;

    RUDA_EXPECT(written > 0);
    RUDA_EXPECT(written > 0 && (size_t)written < sizeof(source));
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker-f64",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 23);
        RUDA_EXPECT(checked.constants.length == 1);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        for (function_index = 0;
            function_index < sizeof(expected_f64) / sizeof(expected_f64[0]);
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                function_index,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_F64);
            RUDA_EXPECT(result.as.f64_bits == expected_f64[function_index]);
            if (function_index < 8 && function != NULL) {
                const RudaMirInstruction *constant =
                    ruda_test_mir_instruction_kind_at(
                        &function->mir,
                        RUDA_MIR_CONSTANT_F64,
                        0
                    );
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &function->mir,
                    RUDA_MIR_CONSTANT_F64
                ) == 1);
                RUDA_EXPECT(constant != NULL);
                if (constant != NULL) {
                    RUDA_EXPECT(constant->f64_bits
                        == expected_f64[function_index]);
                }
            }
            if (function_index >= 8 && function_index < 12
                && function != NULL) {
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &function->mir,
                    arithmetic_kinds[function_index - 8]
                ) == 1);
            }
        }
        for (function_index = 12; function_index < 18;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                function_index,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_BOOL);
            RUDA_EXPECT(result.as.boolean);
            if (function != NULL) {
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &function->mir,
                    comparison_kinds[function_index - 12]
                ) == 1);
                if (function_index == 13) {
                    RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                        &function->mir,
                        RUDA_MIR_NOT_BOOL
                    ) == 1);
                }
            }
        }
        argument.type = RUDA_TYPE_F64;
        argument.as.f64_bits = UINT64_C(0x7ff8000000000042);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            18,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_F64);
        RUDA_EXPECT(result.as.f64_bits == argument.as.f64_bits);

        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            19,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_F64);
        RUDA_EXPECT(result.as.f64_bits == UINT64_C(0x8000000000000000));
        {
            const RudaCheckedFunction *direct = ruda_checked_function(
                &checked,
                19
            );
            RUDA_EXPECT(direct != NULL);
            if (direct != NULL) {
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &direct->mir,
                    RUDA_MIR_CALL
                ) == 1);
            }
        }
        argument.as.f64_bits = UINT64_C(0x4008000000000000);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            21,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_F64);
        RUDA_EXPECT(result.as.f64_bits == UINT64_C(0x4010000000000000));
        {
            const RudaCheckedFunction *compound = ruda_checked_function(
                &checked,
                21
            );
            RUDA_EXPECT(compound != NULL);
            if (compound != NULL) {
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &compound->mir, RUDA_MIR_ADD_F64
                ) == 1);
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &compound->mir, RUDA_MIR_SUBTRACT_F64
                ) == 1);
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &compound->mir, RUDA_MIR_MULTIPLY_F64
                ) == 1);
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &compound->mir, RUDA_MIR_DIVIDE_F64
                ) == 1);
            }
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            22,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_F64);
        RUDA_EXPECT(result.as.f64_bits == UINT64_C(0x8000000000000000));

        argument.as.f64_bits = UINT64_C(0x3ff8000000000000);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            20,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_F64);
        RUDA_EXPECT(result.as.f64_bits == UINT64_C(0xbff8000000000000));
        {
            const RudaCheckedFunction *negate = ruda_checked_function(
                &checked,
                20
            );
            RUDA_EXPECT(negate != NULL);
            if (negate != NULL) {
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &negate->mir,
                    RUDA_MIR_NEGATE_F64
                ) == 1);
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "fn bad(): int { return 1.0; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1.0",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(left: f64, right: int): f64 { return left + right; }\n",
        RUDA_DIAGNOSTIC_INVALID_OPERATOR,
        "+",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(left: f64, right: f64): f64 { return left % right; }\n",
        RUDA_DIAGNOSTIC_INVALID_OPERATOR,
        "%",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(left: f64, right: f64): f64 { return left & right; }\n",
        RUDA_DIAGNOSTIC_INVALID_OPERATOR,
        "&",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): f64 { let value: f64 = 1.0; value %= 1.0; return value; }\n",
        RUDA_DIAGNOSTIC_INVALID_OPERATOR,
        "%=",
        0
    );
}

static void ruda_test_checker_float_literal_digit_limit(void) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckOptions options = ruda_check_default_options();
    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn limited(): f64 { return 1.234; }\n"
    ));
    options.max_float_literal_digits = 3;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker-f64-limit",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_locals_and_literal_boundary(void) {
    static const char source[] =
        "fn update(): int {\n"
        "  let value: int;\n"
        "  value = 41;\n"
        "  value += 1;\n"
        "  return value;\n"
        "}\n"
        "fn minimum(): int {\n"
        "  return -9223372036854775808;\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *update = NULL;
    const RudaCheckedFunction *minimum = NULL;
    RudaMirValue result;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 2);
        update = ruda_checked_function(&checked, 0);
        minimum = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(update != NULL);
        RUDA_EXPECT(minimum != NULL);
        if (update != NULL) {
            RUDA_EXPECT(ruda_mir_interpret(
                &update->mir,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 42);
        }
        if (minimum != NULL) {
            RUDA_EXPECT(ruda_mir_interpret(
                &minimum->mir,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == INT64_MIN);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_primitive_signatures(void) {
    static const char source[] =
        "fn primitives(\n"
        "  p0: bool, p1: i8, p2: i16, p3: i32, p4: i64, p5: int,\n"
        "  p6: u8, p7: byte, p8: u16, p9: u32, p10: u64, p11: uint,\n"
        "  p12: f32, p13: f64, p14: float, p15: rune, p16: string,\n"
        "  p17: never\n"
        "): void {}\n";
    static const RudaTypeId expected[] = {
        RUDA_TYPE_BOOL,
        RUDA_TYPE_I8,
        RUDA_TYPE_I16,
        RUDA_TYPE_I32,
        RUDA_TYPE_I64,
        RUDA_TYPE_I64,
        RUDA_TYPE_U8,
        RUDA_TYPE_U8,
        RUDA_TYPE_U16,
        RUDA_TYPE_U32,
        RUDA_TYPE_U64,
        RUDA_TYPE_U64,
        RUDA_TYPE_F32,
        RUDA_TYPE_F64,
        RUDA_TYPE_F64,
        RUDA_TYPE_RUNE,
        RUDA_TYPE_STRING,
        RUDA_TYPE_NEVER
    };
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *function = NULL;
    size_t index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 1);
        function = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(function != NULL);
        if (function != NULL) {
            RUDA_EXPECT(function->mir.parameter_count
                == sizeof(expected) / sizeof(expected[0]));
            for (index = 0; index < sizeof(expected) / sizeof(expected[0]);
                index += 1) {
                const RudaTypeId *actual = ruda_vector_at_const(
                    &function->mir.register_types,
                    index
                );
                RUDA_EXPECT(actual != NULL);
                if (actual != NULL) {
                    RUDA_EXPECT(*actual == expected[index]);
                }
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_control_flow(void) {
    static const char source[] =
        "fn choose(ready: bool): int {\n"
        "  if ready { return 1; }\n"
        "  return 0;\n"
        "}\n"
        "fn branch(ready: bool): int {\n"
        "  let value: int;\n"
        "  if ready { value = 10; } else { value = 20; }\n"
        "  return value;\n"
        "}\n"
        "fn count(limit: int): int {\n"
        "  let value: int = 0;\n"
        "  while value < limit { value = value + 1; }\n"
        "  value |= 0;\n"
        "  value <<= 0;\n"
        "  return value;\n"
        "}\n"
        "fn direct(ready: bool): int {\n"
        "  if ready { return 30; } else { return 40; }\n"
        "}\n"
        "fn nestedBlock(): int {\n"
        "  let value: int = 1;\n"
        "  { const increment: int = 2; value = value + increment; }\n"
        "  return value;\n"
        "}\n"
        "fn classify(first: bool, second: bool): int {\n"
        "  if first { return 1; } else if second { return 2; } else { return 3; }\n"
        "}\n"
        "fn loopControl(limit: int): int {\n"
        "  let value: int = 0;\n"
        "  while value < limit {\n"
        "    value = value + 1;\n"
        "    if value == 2 { continue; }\n"
        "    if value >= 4 { break; }\n"
        "  }\n"
        "  return value;\n"
        "}\n"
        "fn cFor(limit: int): int {\n"
        "  let total: int = 0;\n"
        "  for let index = 0; index < limit; index += 1 {\n"
        "    if index == 2 { continue; }\n"
        "    total += index;\n"
        "    if total >= 10 { break; }\n"
        "  }\n"
        "  return total;\n"
        "}\n"
        "fn logicalLoops(ready: bool): int {\n"
        "  let total: int = 0;\n"
        "  while total < 2 && ready { total += 1; }\n"
        "  for let keep = ready && true; keep && total < 3; keep = false || false {\n"
        "    total += 1;\n"
        "  }\n"
        "  return total;\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *choose = NULL;
    const RudaCheckedFunction *branch = NULL;
    const RudaCheckedFunction *count = NULL;
    const RudaCheckedFunction *direct = NULL;
    const RudaCheckedFunction *nested_block = NULL;
    const RudaCheckedFunction *classify = NULL;
    const RudaCheckedFunction *loop_control = NULL;
    const RudaCheckedFunction *c_for = NULL;
    const RudaCheckedFunction *logical_loops = NULL;
    RudaMirValue arguments[2];
    RudaMirValue result;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 9);
        choose = ruda_checked_function(&checked, 0);
        branch = ruda_checked_function(&checked, 1);
        count = ruda_checked_function(&checked, 2);
        direct = ruda_checked_function(&checked, 3);
        nested_block = ruda_checked_function(&checked, 4);
        classify = ruda_checked_function(&checked, 5);
        loop_control = ruda_checked_function(&checked, 6);
        c_for = ruda_checked_function(&checked, 7);
        logical_loops = ruda_checked_function(&checked, 8);
        RUDA_EXPECT(choose != NULL);
        RUDA_EXPECT(branch != NULL);
        RUDA_EXPECT(count != NULL);
        RUDA_EXPECT(direct != NULL);
        RUDA_EXPECT(nested_block != NULL);
        RUDA_EXPECT(classify != NULL);
        RUDA_EXPECT(loop_control != NULL);
        RUDA_EXPECT(c_for != NULL);
        RUDA_EXPECT(logical_loops != NULL);
        if (choose != NULL) {
            arguments[0].type = RUDA_TYPE_BOOL;
            arguments[0].as.boolean = true;
            RUDA_EXPECT(ruda_mir_interpret(
                &choose->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 1);
            arguments[0].as.boolean = false;
            RUDA_EXPECT(ruda_mir_interpret(
                &choose->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 0);
        }
        if (branch != NULL) {
            arguments[0].type = RUDA_TYPE_BOOL;
            arguments[0].as.boolean = true;
            RUDA_EXPECT(ruda_mir_interpret(
                &branch->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 10);
            arguments[0].as.boolean = false;
            RUDA_EXPECT(ruda_mir_interpret(
                &branch->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 20);
        }
        if (count != NULL) {
            arguments[0].type = RUDA_TYPE_I64;
            arguments[0].as.i64 = 5;
            RUDA_EXPECT(ruda_mir_interpret(
                &count->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 5);
        }
        if (direct != NULL) {
            arguments[0].type = RUDA_TYPE_BOOL;
            arguments[0].as.boolean = false;
            RUDA_EXPECT(ruda_mir_interpret(
                &direct->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 40);
        }
        if (nested_block != NULL) {
            RUDA_EXPECT(ruda_mir_interpret(
                &nested_block->mir,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 3);
        }
        if (classify != NULL) {
            arguments[0].type = RUDA_TYPE_BOOL;
            arguments[0].as.boolean = false;
            arguments[1].type = RUDA_TYPE_BOOL;
            arguments[1].as.boolean = true;
            RUDA_EXPECT(ruda_mir_interpret(
                &classify->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 2);
        }
        if (loop_control != NULL) {
            arguments[0].type = RUDA_TYPE_I64;
            arguments[0].as.i64 = 10;
            RUDA_EXPECT(ruda_mir_interpret(
                &loop_control->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 4);
        }
        if (c_for != NULL) {
            arguments[0].type = RUDA_TYPE_I64;
            arguments[0].as.i64 = 10;
            RUDA_EXPECT(ruda_mir_interpret(
                &c_for->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 13);
        }
        if (logical_loops != NULL) {
            arguments[0].type = RUDA_TYPE_BOOL;
            arguments[0].as.boolean = true;
            RUDA_EXPECT(ruda_mir_interpret(
                &logical_loops->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 3);
            arguments[0].as.boolean = false;
            RUDA_EXPECT(ruda_mir_interpret(
                &logical_loops->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 0);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_control_flow_diagnostics(void) {
    static const char condition[] =
        "fn bad(): int { if 1 { return 1; } return 0; }\n";
    static const char branch_initialization[] =
        "fn bad(ready: bool): int { let value: int; if ready { value = 1; } return value; }\n";
    static const char loop_initialization[] =
        "fn bad(ready: bool): int { let value: int; while ready { value = 1; } return value; }\n";
    static const char scoped_name[] =
        "fn bad(): int { { const inner: int = 1; } return inner; }\n";
    static const char outside_break[] =
        "fn bad(): void { break; }\n";
    static const char outside_continue[] =
        "fn bad(): void { continue; }\n";
    static const char for_scope[] =
        "fn bad(): int { for let index = 0; index < 1; index += 1 {} return index; }\n";
    ruda_checker_expect_one_diagnostic(
        condition,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        branch_initialization,
        RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
        "value",
        2
    );
    ruda_checker_expect_one_diagnostic(
        loop_initialization,
        RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
        "value",
        2
    );
    ruda_checker_expect_one_diagnostic(
        scoped_name,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "inner",
        1
    );
    ruda_checker_expect_one_diagnostic(
        outside_break,
        RUDA_DIAGNOSTIC_LOOP_CONTROL_OUTSIDE_LOOP,
        "break",
        0
    );
    ruda_checker_expect_one_diagnostic(
        outside_continue,
        RUDA_DIAGNOSTIC_LOOP_CONTROL_OUTSIDE_LOOP,
        "continue",
        0
    );
    ruda_checker_expect_one_diagnostic(
        for_scope,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "index",
        3
    );
}

static void ruda_test_checker_unsupported_is_explicit(void) {
    RudaCheckerFixture fixture;
    RudaCheckResult checked;

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn sum(a: i32, b: i32): i32 { return a + b; }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn iterate(items: int): void { for const item of items {} }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "const message: string = \"not implemented\";\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_limits_are_failure_atomic(void) {
    static const char source[] =
        "fn calc(a: int, b: int, c: int): int { return a + b * c; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckOptions options = ruda_check_default_options();
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    options.mir_options.max_registers = 3;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "type Pair { value: int; }\n"
        "fn same(left: Pair, right: Pair): bool {\n"
        "  return identity.same(left, right);\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_registers = 2;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "type Pair { first: int; second: int; }\n"
        "type Other { value: int; }\n"
        "fn build(): int {\n"
        "  const pair = Pair { second: 2, first: 1 };\n"
        "  const other = Other { value: 3 };\n"
        "  return pair.first + other.value;\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_record_fields = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_record_constructions = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_record_arguments = 2;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_record_layouts = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "type Pair { first: int; second: int; }\n"
        "fn project(pair: Pair): int {\n"
        "  return match pair { case Pair { first, second } => first + second; };\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.max_locals_per_function = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_record_fields = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_registers = 4;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "enum First { Unit, Pair(int, int), Named { x: int; y: int; }, }\n"
        "enum Second { Unit, }\n"
        "fn build(value: int): Second {\n"
        "  const unit = First.Unit;\n"
        "  const pair = First.Pair(value, value);\n"
        "  return Second.Unit;\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_enum_layouts = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_enum_variants = 3;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_enum_payloads = 3;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_enum_constructions = 2;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_enum_arguments = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "enum Choice { A, B(int, int), C, }\n"
        "fn choose(value: Choice): int {\n"
        "  return match value {\n"
        "    case A => 0;\n"
        "    case B(left, right) => left + right;\n"
        "    case C => 3;\n"
        "  };\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_blocks = 5;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.max_locals_per_function = 2;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn limited(value: bool): int {\n"
        "  return match value {\n"
        "    case true => panic(\"a\");\n"
        "    case false => panic(\"b\");\n"
        "  };\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_string_constants = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn limited(value: string): int {\n"
        "  return match value { case \"a\" => 1; case \"b\" => 2; case _ => 0; };\n"
        "}\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_string_constants = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.mir_options.max_string_bytes = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn limited(): int { panic(\"ab\"); }\n"
    ));
    options = ruda_check_default_options();
    options.mir_options.max_string_bytes = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_allocation_failures(void) {
    static const char source[] =
        "type Count = int;\n"
        "type Box { public count: Count; mutable next: Box; }\n"
        "type Pair { left: Count; mutable right: Count; }\n"
        "enum Signal { Idle, Paused, Counted(Count), }\n"
        "const one: Count = 1;\n"
        "fn seed(ready: bool): Count {\n"
        "  if ready && one == 1 { return one; } else { return 2; }\n"
        "}\n"
        "fn truth(value: bool): Count {\n"
        "  return match value { case true => 1; case false => 0; };\n"
        "}\n"
        "fn number(value: Count): Count {\n"
        "  return match value { case 0 => 0; case other => other; };\n"
        "}\n"
        "fn text(value: string): Count {\n"
        "  return match value { case \"ready\" | r\"done\" => 1; case _ => 0; };\n"
        "}\n"
        "fn allStop(value: bool): Count {\n"
        "  while match value {\n"
        "    case true => { return 1; } case false => { return 0; }\n"
        "  } {}\n"
        "}\n"
        "fn guardStop(value: Count, ready: bool): Count {\n"
        "  return match value {\n"
        "    case 0 if match ready {\n"
        "      case true => { return 1; } case false => { return 2; }\n"
        "    } => 9;\n"
        "    case _ => 0;\n"
        "  };\n"
        "}\n"
        "fn stepStop(value: bool, ready: bool): Count {\n"
        "  for let index = 0; ready; index = match value {\n"
        "    case true => { return 1; } case false => { return 2; }\n"
        "  } {}\n"
        "  return 3;\n"
        "}\n"
        "fn panicStop(): Count { panic(\"allocation\"); }\n"
        "fn declaredStop(): never { panic(\"declared allocation\"); }\n"
        "fn callStop(): Count { declaredStop(); }\n"
        "fn flow(ready: bool, limit: Count): Count {\n"
        "  let value: Count = seed(ready);\n"
        "  while value < limit { value = value + 1; }\n"
        "  return value;\n"
        "}\n"
        "fn pair(value: Count): Count {\n"
        "  const item = Pair { right: value, left: value };\n"
        "  item.right += 1;\n"
        "  return item.right;\n"
        "}\n"
        "fn pairMatch(item: Pair): Count {\n"
        "  return match item { case Pair { left, right } => left + right; };\n"
        "}\n"
        "fn pairSame(left: Pair, right: Pair): bool {\n"
        "  return identity.same(left, right);\n"
        "}\n"
        "fn signal(value: Count): Signal {\n"
        "  return Signal.Counted(value);\n"
        "}\n"
        "fn signalValue(signal: Signal): Count {\n"
        "  return match signal {\n"
        "    case Idle | Paused => { return 0; }\n"
        "    case Counted(value) if value > 0 => { yield value; }\n"
        "    case Counted(_) => { yield 0; }\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCheckResult checked;
        RudaCheckStatus status;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        status = ruda_check_ast(
            &fixture.ast,
            "test/checker",
            allocator,
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            reached_success = true;
            RUDA_EXPECT(checked.functions.length == 16);
            RUDA_EXPECT(checked.constants.length == 1);
            RUDA_EXPECT(checked.type_aliases.length == 1);
            RUDA_EXPECT(checked.records.length == 2);
            RUDA_EXPECT(checked.enums.length == 1);
            ruda_check_result_dispose(&checked);
        } else {
            RUDA_EXPECT(status == RUDA_CHECK_ALLOCATION_FAILED);
            RUDA_EXPECT(!checked.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_checker_fixture_dispose(&fixture);
}

static void ruda_test_checker_package_allocation_failures(void) {
    static const char entry_source[] =
        "fn entry(value: Count): Count { return helper(value) + offset; }\n"
        "fn truth(value: bool): Count {\n"
        "  return match value { case true => 1; case false => 0; };\n"
        "}\n"
        "fn number(value: Count): Count {\n"
        "  return match value { case 0 => 0; case other => other; };\n"
        "}\n"
        "fn project(value: Count): Count {\n"
        "  const marker = Marker { count: value };\n"
        "  return marker.count;\n"
        "}\n"
        "fn status(value: Count): Status {\n"
        "  return Status.Counted(value);\n"
        "}\n"
        "fn statusValue(status: Status): Count {\n"
        "  return match status {\n"
        "    case Ready | Paused => 0;\n"
        "    case Counted(value) if value > 0 => value;\n"
        "    case Counted(_) => 0;\n"
        "  };\n"
        "}\n";
    static const char helper_source[] =
        "type Count = int;\n"
        "type Marker { public count: Count; }\n"
        "enum Status { Ready, Paused, Counted(Count), }\n"
        "const offset: Count = 1;\n"
        "fn helper(value: Count): Count { return value; }\n";
    RudaCheckerFixture entry;
    RudaCheckerFixture helper;
    const RudaAstTree *asts[2];
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &entry,
        "a_entry.ruda",
        entry_source
    ));
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &helper,
        "z_helper.ruda",
        helper_source
    ));
    asts[0] = &helper.ast;
    asts[1] = &entry.ast;
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCheckResult checked;
        RudaCheckStatus status;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        status = ruda_check_package(
            asts,
            2,
            "test/package",
            allocator,
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            reached_success = true;
            RUDA_EXPECT(checked.functions.length == 7);
            RUDA_EXPECT(checked.constants.length == 1);
            RUDA_EXPECT(checked.type_aliases.length == 1);
            RUDA_EXPECT(checked.records.length == 1);
            RUDA_EXPECT(checked.enums.length == 1);
            ruda_check_result_dispose(&checked);
        } else {
            RUDA_EXPECT(status == RUDA_CHECK_ALLOCATION_FAILED);
            RUDA_EXPECT(!checked.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_checker_fixture_dispose(&helper);
    ruda_checker_fixture_dispose(&entry);
}

void ruda_test_checker(void) {
    ruda_test_checker_vertical_slice();
    ruda_test_checker_diagnostics();
    ruda_test_checker_signature_phase();
    ruda_test_checker_function_attributes_and_panic_shadow();
    ruda_test_checker_contextual_word_symbols();
    ruda_test_checker_package_sources();
    ruda_test_checker_declaration_index_growth();
    ruda_test_checker_type_aliases();
    ruda_test_checker_record_declarations();
    ruda_test_checker_record_package_order();
    ruda_test_checker_record_execution();
    ruda_test_checker_record_identity();
    ruda_test_checker_record_match();
    ruda_test_checker_public_type_visibility();
    ruda_test_checker_enum_declarations();
    ruda_test_checker_enum_package_order();
    ruda_test_checker_enum_execution();
    ruda_test_checker_enum_match();
    ruda_test_checker_enum_or_patterns();
    ruda_test_checker_boolean_match();
    ruda_test_checker_match_witnesses();
    ruda_test_checker_i64_match();
    ruda_test_checker_match_block_arms();
    ruda_test_checker_match_return_arms();
    ruda_test_checker_all_never_match();
    ruda_test_checker_never_conditions_and_guards();
    ruda_test_checker_strings_and_panic();
    ruda_test_checker_string_match();
    ruda_test_checker_declared_never_calls();
    ruda_test_checker_calls();
    ruda_test_checker_top_level_constants();
    ruda_test_checker_constant_diagnostics();
    ruda_test_checker_short_circuit();
    ruda_test_checker_bitwise();
    ruda_test_checker_shifts();
    ruda_test_checker_division_and_remainder();
    ruda_test_checker_f64_literals_and_operators();
    ruda_test_checker_float_literal_digit_limit();
    ruda_test_checker_locals_and_literal_boundary();
    ruda_test_checker_primitive_signatures();
    ruda_test_checker_control_flow();
    ruda_test_checker_control_flow_diagnostics();
    ruda_test_checker_unsupported_is_explicit();
    ruda_test_checker_limits_are_failure_atomic();
    ruda_test_checker_allocation_failures();
    ruda_test_checker_package_allocation_failures();
    ruda_test_checker_witness_allocation_failures();
}
