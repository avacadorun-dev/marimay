#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/lex/lexer.h"
#include "ruda/lex/token.h"
#include "ruda/source/source_manager.h"

typedef struct RudaLexFixture {
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot;
    RudaLexResult result;
} RudaLexFixture;

static bool ruda_lex_fixture_init_bytes(
    RudaLexFixture *fixture,
    const unsigned char *bytes,
    size_t length,
    RudaLexOptions options
) {
    memset(fixture, 0, sizeof(*fixture));
    if (!ruda_source_manager_init(&fixture->sources, ruda_system_allocator())) {
        return false;
    }
    if (ruda_source_manager_add(
            &fixture->sources,
            "fixture.ruda",
            strlen("fixture.ruda"),
            bytes,
            length,
            &fixture->snapshot
        ) != RUDA_SOURCE_OK) {
        ruda_source_manager_dispose(&fixture->sources);
        return false;
    }
    if (ruda_lex_snapshot(
            fixture->snapshot,
            ruda_system_allocator(),
            options,
            &fixture->result
        ) != RUDA_LEX_OK) {
        ruda_lex_result_dispose(&fixture->result);
        ruda_source_manager_dispose(&fixture->sources);
        return false;
    }
    return true;
}

static bool ruda_lex_fixture_init(RudaLexFixture *fixture, const char *text) {
    return ruda_lex_fixture_init_bytes(
        fixture,
        (const unsigned char *)text,
        strlen(text),
        ruda_lex_default_options()
    );
}

static void ruda_lex_fixture_dispose(RudaLexFixture *fixture) {
    ruda_lex_result_dispose(&fixture->result);
    ruda_source_manager_dispose(&fixture->sources);
}

static void ruda_expect_lossless(const RudaLexFixture *fixture) {
    size_t index = 0;
    size_t cursor = 0;
    for (index = 0; index < fixture->result.tokens.length; index += 1) {
        const RudaToken *token = ruda_lex_token(&fixture->result, index);
        RUDA_EXPECT(token != NULL);
        if (token == NULL) {
            return;
        }
        RUDA_EXPECT(token->start == cursor);
        RUDA_EXPECT(token->end >= token->start);
        if (token->kind == RUDA_TOKEN_EOF) {
            RUDA_EXPECT(index + 1 == fixture->result.tokens.length);
            RUDA_EXPECT(token->end == token->start);
        } else {
            cursor = token->end;
        }
    }
    RUDA_EXPECT(cursor == ruda_source_length(fixture->snapshot));
}

static bool ruda_result_has_diagnostic(
    const RudaLexResult *result,
    const char *code
) {
    size_t index = 0;
    for (index = 0; index < result->diagnostics.length; index += 1) {
        const RudaLexDiagnostic *diagnostic = ruda_lex_diagnostic(result, index);
        if (diagnostic != NULL && strcmp(diagnostic->code, code) == 0) {
            return true;
        }
    }
    return false;
}

static void ruda_dump_tokens(const RudaLexFixture *fixture, char *output, size_t capacity) {
    size_t index = 0;
    size_t length = 0;
    for (index = 0; index < fixture->result.tokens.length; index += 1) {
        const RudaToken *token = ruda_lex_token(&fixture->result, index);
        int written = 0;
        if (token == NULL || length >= capacity) {
            break;
        }
        written = snprintf(
            output + length,
            capacity - length,
            "%s %zu..%zu\n",
            ruda_token_kind_name(token->kind),
            token->start,
            token->end
        );
        if (written < 0 || (size_t)written >= capacity - length) {
            output[capacity - 1] = '\0';
            return;
        }
        length += (size_t)written;
    }
}

static bool ruda_read_lexer_fixture(
    const char *path,
    unsigned char *bytes,
    size_t capacity,
    size_t *length
) {
    FILE *file = fopen(path, "rb");
    size_t count = 0;
    bool read_ok = false;
    bool close_ok = false;
    if (file == NULL || capacity == 0) {
        return false;
    }
    count = fread(bytes, 1, capacity, file);
    read_ok = ferror(file) == 0 && feof(file) != 0;
    close_ok = fclose(file) == 0;
    if (!read_ok || !close_ok) {
        return false;
    }
    *length = count;
    return true;
}

static void ruda_dump_lex_diagnostics(
    const RudaLexResult *result,
    char *output,
    size_t capacity
) {
    size_t index = 0;
    size_t length = 0;
    for (index = 0; index < result->diagnostics.length; index += 1) {
        const RudaLexDiagnostic *diagnostic = ruda_lex_diagnostic(result, index);
        int written = 0;
        if (diagnostic == NULL || length >= capacity) {
            break;
        }
        written = snprintf(
            output + length,
            capacity - length,
            "%s %zu..%zu\n",
            diagnostic->code,
            diagnostic->start,
            diagnostic->end
        );
        if (written < 0 || (size_t)written >= capacity - length) {
            output[capacity - 1] = '\0';
            return;
        }
        length += (size_t)written;
    }
}

static void ruda_test_lexer_external_fixtures(void) {
    unsigned char source[4096];
    size_t source_length = 0;
    RudaLexFixture fixture;
    char dump[16384] = {0};

    RUDA_EXPECT(ruda_read_lexer_fixture(
        "syntax/lexer/lossless.ruda",
        source,
        sizeof(source),
        &source_length
    ));
    RUDA_EXPECT(ruda_lex_fixture_init_bytes(
        &fixture,
        source,
        source_length,
        ruda_lex_default_options()
    ));
    if (fixture.result.initialized) {
        RUDA_EXPECT(fixture.result.diagnostics.length == 0);
        ruda_expect_lossless(&fixture);
        ruda_dump_tokens(&fixture, dump, sizeof(dump));
        RUDA_EXPECT_FILE("syntax/lexer/lossless.tokens", dump);
        ruda_lex_fixture_dispose(&fixture);
    }

    memset(dump, 0, sizeof(dump));
    RUDA_EXPECT(ruda_read_lexer_fixture(
        "syntax/lexer/invalid.ruda",
        source,
        sizeof(source),
        &source_length
    ));
    RUDA_EXPECT(ruda_lex_fixture_init_bytes(
        &fixture,
        source,
        source_length,
        ruda_lex_default_options()
    ));
    if (fixture.result.initialized) {
        ruda_expect_lossless(&fixture);
        ruda_dump_lex_diagnostics(&fixture.result, dump, sizeof(dump));
        RUDA_EXPECT_FILE("syntax/lexer/invalid.diagnostics", dump);
        ruda_lex_fixture_dispose(&fixture);
    }
}

static void ruda_test_lexer_complete_snapshot(void) {
    static const unsigned char source[] =
        "\xEF\xBB\xBF/// doc\r\n"
        "public fn λ2(value_2: int): Result<int, E> {\n"
        "  /** outer /* nested */ doc */\n"
        "  const n = 0b1010 + 0o755 + 0xCA_FE + 1_000 + 6.02e+23;\n"
        "  const s = \"a\\n\\u{3bb}\";\n"
        "  const r = r\"raw\\n\";\n"
        "  const b = b\"A\\x00\";\n"
        "  const c = 'λ';\n"
        "  return $\"{{{value_2 + map[0]}}}\";\n"
        "}\n";
    RudaLexFixture fixture;
    char dump[16384] = {0};
    RUDA_EXPECT(ruda_lex_fixture_init_bytes(
        &fixture,
        source,
        sizeof(source) - 1,
        ruda_lex_default_options()
    ));
    if (!fixture.result.initialized) {
        return;
    }
    RUDA_EXPECT(fixture.result.diagnostics.length == 0);
    ruda_expect_lossless(&fixture);
    ruda_dump_tokens(&fixture, dump, sizeof(dump));
    RUDA_EXPECT_SNAPSHOT("lexer-complete.tokens", dump);
    ruda_lex_fixture_dispose(&fixture);
}

static void ruda_test_lexer_keywords_and_reserved(void) {
    static const char source[] =
        "as break case const continue defer else enum extern false fn for from "
        "fuzz if import in interface let match mutable of panic public return "
        "scope spawn test true type unsafe while yield "
        "any async await class extends impl null throws undefined";
    static const RudaTokenKind expected[] = {
        RUDA_TOKEN_KW_AS, RUDA_TOKEN_KW_BREAK, RUDA_TOKEN_KW_CASE, RUDA_TOKEN_KW_CONST,
        RUDA_TOKEN_KW_CONTINUE, RUDA_TOKEN_KW_DEFER, RUDA_TOKEN_KW_ELSE,
        RUDA_TOKEN_KW_ENUM, RUDA_TOKEN_KW_EXTERN, RUDA_TOKEN_KW_FALSE, RUDA_TOKEN_KW_FN,
        RUDA_TOKEN_KW_FOR, RUDA_TOKEN_KW_FROM, RUDA_TOKEN_IDENTIFIER, RUDA_TOKEN_KW_IF,
        RUDA_TOKEN_KW_IMPORT, RUDA_TOKEN_IDENTIFIER, RUDA_TOKEN_KW_INTERFACE,
        RUDA_TOKEN_KW_LET, RUDA_TOKEN_KW_MATCH, RUDA_TOKEN_KW_MUTABLE, RUDA_TOKEN_KW_OF,
        RUDA_TOKEN_IDENTIFIER, RUDA_TOKEN_KW_PUBLIC, RUDA_TOKEN_KW_RETURN,
        RUDA_TOKEN_KW_SCOPE, RUDA_TOKEN_KW_SPAWN, RUDA_TOKEN_IDENTIFIER, RUDA_TOKEN_KW_TRUE,
        RUDA_TOKEN_KW_TYPE, RUDA_TOKEN_KW_UNSAFE, RUDA_TOKEN_KW_WHILE,
        RUDA_TOKEN_KW_YIELD, RUDA_TOKEN_IDENTIFIER, RUDA_TOKEN_IDENTIFIER,
        RUDA_TOKEN_IDENTIFIER, RUDA_TOKEN_IDENTIFIER, RUDA_TOKEN_IDENTIFIER,
        RUDA_TOKEN_IDENTIFIER, RUDA_TOKEN_RESERVED_WORD, RUDA_TOKEN_IDENTIFIER,
        RUDA_TOKEN_RESERVED_WORD
    };
    RudaLexFixture fixture;
    size_t token_index = 0;
    size_t expected_index = 0;
    RUDA_EXPECT(ruda_lex_fixture_init(&fixture, source));
    if (!fixture.result.initialized) {
        return;
    }
    for (token_index = 0; token_index < fixture.result.tokens.length; token_index += 1) {
        const RudaToken *token = ruda_lex_token(&fixture.result, token_index);
        if (token != NULL && token->kind != RUDA_TOKEN_WHITESPACE
            && token->kind != RUDA_TOKEN_EOF) {
            RUDA_EXPECT(expected_index < sizeof(expected) / sizeof(expected[0]));
            if (expected_index < sizeof(expected) / sizeof(expected[0])) {
                RUDA_EXPECT(token->kind == expected[expected_index]);
            }
            expected_index += 1;
        }
    }
    RUDA_EXPECT(expected_index == sizeof(expected) / sizeof(expected[0]));
    RUDA_EXPECT(fixture.result.diagnostics.length == 2);
    for (token_index = 0; token_index < fixture.result.diagnostics.length; token_index += 1) {
        const RudaLexDiagnostic *diagnostic = ruda_lex_diagnostic(
            &fixture.result,
            token_index
        );
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(diagnostic->code, RUDA_DIAGNOSTIC_RESERVED_WORD) == 0);
        }
    }
    ruda_expect_lossless(&fixture);
    ruda_lex_fixture_dispose(&fixture);
}

static void ruda_test_lexer_invalid_forms(void) {
    static const unsigned char source[] = {
        '_', 'n', 'a', 'm', 'e', ' ',
        '0', 'b', '1', '0', '2', ' ',
        '1', '_', '_', '2', ' ',
        '1', 'e', '+', ' ',
        '"', 'b', 'a', 'd', '\\', 'q', '\n',
        'b', '"', 0xCE, 0xBB, '"', ' ',
        '\'', '\'', ' ', '\'', 'a', 'b', '\'', ' ',
        '/', '*', ' ', 0xFF
    };
    RudaLexFixture fixture;
    RUDA_EXPECT(ruda_lex_fixture_init_bytes(
        &fixture,
        source,
        sizeof(source),
        ruda_lex_default_options()
    ));
    if (!fixture.result.initialized) {
        return;
    }
    RUDA_EXPECT(ruda_result_has_diagnostic(
        &fixture.result,
        RUDA_DIAGNOSTIC_INVALID_UTF8
    ));
    RUDA_EXPECT(ruda_result_has_diagnostic(
        &fixture.result,
        RUDA_DIAGNOSTIC_UNEXPECTED_CHARACTER
    ));
    RUDA_EXPECT(ruda_result_has_diagnostic(
        &fixture.result,
        RUDA_DIAGNOSTIC_INVALID_NUMBER
    ));
    RUDA_EXPECT(ruda_result_has_diagnostic(
        &fixture.result,
        RUDA_DIAGNOSTIC_INVALID_ESCAPE
    ));
    RUDA_EXPECT(ruda_result_has_diagnostic(
        &fixture.result,
        RUDA_DIAGNOSTIC_UNTERMINATED_LITERAL
    ));
    RUDA_EXPECT(ruda_result_has_diagnostic(
        &fixture.result,
        RUDA_DIAGNOSTIC_INVALID_BYTE_STRING
    ));
    RUDA_EXPECT(ruda_result_has_diagnostic(
        &fixture.result,
        RUDA_DIAGNOSTIC_INVALID_RUNE
    ));
    RUDA_EXPECT(ruda_result_has_diagnostic(
        &fixture.result,
        RUDA_DIAGNOSTIC_UNTERMINATED_BLOCK_COMMENT
    ));
    ruda_expect_lossless(&fixture);
    ruda_lex_fixture_dispose(&fixture);
}

static void ruda_test_lexer_interpolation(void) {
    RudaLexFixture fixture;
    RUDA_EXPECT(ruda_lex_fixture_init(
        &fixture,
        "$\"a {call($\"b {x}\")} c\""
    ));
    if (fixture.result.initialized) {
        RUDA_EXPECT(fixture.result.diagnostics.length == 0);
        ruda_expect_lossless(&fixture);
        ruda_lex_fixture_dispose(&fixture);
    }

    RUDA_EXPECT(ruda_lex_fixture_init(&fixture, "$\"x }\""));
    if (fixture.result.initialized) {
        RUDA_EXPECT(ruda_result_has_diagnostic(
            &fixture.result,
            RUDA_DIAGNOSTIC_UNMATCHED_INTERPOLATION_BRACE
        ));
        ruda_expect_lossless(&fixture);
        ruda_lex_fixture_dispose(&fixture);
    }
}

static void ruda_test_lexer_fixed_tokens(void) {
    static const char source[] =
        "@ , : ; . ( ) { } [ ] + - * / % ! ~ & | ^ ? = < > "
        "=> -> == != <= >= && || << >> += -= *= /= %= &= |= ^= <<= >>=";
    static const RudaTokenKind expected[] = {
        RUDA_TOKEN_AT, RUDA_TOKEN_COMMA, RUDA_TOKEN_COLON, RUDA_TOKEN_SEMICOLON,
        RUDA_TOKEN_DOT, RUDA_TOKEN_LEFT_PAREN, RUDA_TOKEN_RIGHT_PAREN,
        RUDA_TOKEN_LEFT_BRACE, RUDA_TOKEN_RIGHT_BRACE, RUDA_TOKEN_LEFT_BRACKET,
        RUDA_TOKEN_RIGHT_BRACKET, RUDA_TOKEN_PLUS, RUDA_TOKEN_MINUS, RUDA_TOKEN_STAR,
        RUDA_TOKEN_SLASH, RUDA_TOKEN_PERCENT, RUDA_TOKEN_BANG, RUDA_TOKEN_TILDE,
        RUDA_TOKEN_AMPERSAND, RUDA_TOKEN_PIPE, RUDA_TOKEN_CARET, RUDA_TOKEN_QUESTION,
        RUDA_TOKEN_EQUAL, RUDA_TOKEN_LESS, RUDA_TOKEN_GREATER, RUDA_TOKEN_FAT_ARROW,
        RUDA_TOKEN_THIN_ARROW, RUDA_TOKEN_EQUAL_EQUAL, RUDA_TOKEN_BANG_EQUAL,
        RUDA_TOKEN_LESS_EQUAL, RUDA_TOKEN_GREATER_EQUAL,
        RUDA_TOKEN_AMPERSAND_AMPERSAND, RUDA_TOKEN_PIPE_PIPE, RUDA_TOKEN_SHIFT_LEFT,
        RUDA_TOKEN_SHIFT_RIGHT, RUDA_TOKEN_PLUS_EQUAL, RUDA_TOKEN_MINUS_EQUAL,
        RUDA_TOKEN_STAR_EQUAL, RUDA_TOKEN_SLASH_EQUAL, RUDA_TOKEN_PERCENT_EQUAL,
        RUDA_TOKEN_AMPERSAND_EQUAL, RUDA_TOKEN_PIPE_EQUAL, RUDA_TOKEN_CARET_EQUAL,
        RUDA_TOKEN_SHIFT_LEFT_EQUAL, RUDA_TOKEN_SHIFT_RIGHT_EQUAL
    };
    RudaLexFixture fixture;
    size_t token_index = 0;
    size_t expected_index = 0;
    RUDA_EXPECT(ruda_lex_fixture_init(&fixture, source));
    if (!fixture.result.initialized) {
        return;
    }
    RUDA_EXPECT(fixture.result.diagnostics.length == 0);
    for (token_index = 0; token_index < fixture.result.tokens.length; token_index += 1) {
        const RudaToken *token = ruda_lex_token(&fixture.result, token_index);
        if (token != NULL && token->kind != RUDA_TOKEN_WHITESPACE
            && token->kind != RUDA_TOKEN_EOF) {
            RUDA_EXPECT(expected_index < sizeof(expected) / sizeof(expected[0]));
            if (expected_index < sizeof(expected) / sizeof(expected[0])) {
                RUDA_EXPECT(token->kind == expected[expected_index]);
            }
            expected_index += 1;
        }
    }
    RUDA_EXPECT(expected_index == sizeof(expected) / sizeof(expected[0]));
    ruda_expect_lossless(&fixture);
    ruda_lex_fixture_dispose(&fixture);
}

static void ruda_test_lexer_literal_boundaries(void) {
    static const char valid[] =
        "0 0B10 0O77 0XCA_FE 1_2 1.25 1e3 1.0E-2 "
        "\"\\\\\\\"\\'\\n\\r\\t\\0\\u{10FFFF}\" "
        "b\"A\\\\\\\"\\'\\n\\r\\t\\0\\xFF\" "
        "r\"raw\\n\" '\\u{3BB}'";
    static const char invalid[] =
        "0b 0o8 0xG 1_ 1e 12name "
        "\"\\xFF\" b\"λ\" b\"\\u{41}\" '\\u{D800}' '' 'ab'";
    RudaLexFixture fixture;
    RUDA_EXPECT(ruda_lex_fixture_init(&fixture, valid));
    if (fixture.result.initialized) {
        RUDA_EXPECT(fixture.result.diagnostics.length == 0);
        ruda_expect_lossless(&fixture);
        ruda_lex_fixture_dispose(&fixture);
    }
    RUDA_EXPECT(ruda_lex_fixture_init(&fixture, invalid));
    if (fixture.result.initialized) {
        RUDA_EXPECT(ruda_result_has_diagnostic(
            &fixture.result,
            RUDA_DIAGNOSTIC_INVALID_NUMBER
        ));
        RUDA_EXPECT(ruda_result_has_diagnostic(
            &fixture.result,
            RUDA_DIAGNOSTIC_INVALID_ESCAPE
        ));
        RUDA_EXPECT(ruda_result_has_diagnostic(
            &fixture.result,
            RUDA_DIAGNOSTIC_INVALID_BYTE_STRING
        ));
        RUDA_EXPECT(ruda_result_has_diagnostic(
            &fixture.result,
            RUDA_DIAGNOSTIC_INVALID_RUNE
        ));
        ruda_expect_lossless(&fixture);
        ruda_lex_fixture_dispose(&fixture);
    }
}

static void ruda_test_lexer_float_grammar(void) {
    static const char accepted[] =
        "0.0 1_2.5_0 6.02e23 1e-9 1.0E+2 5e-324 9_0e3_0";
    static const char rejected[] =
        "1_.0 1.0_ 1e_2 1e+_2 1e- 1.0f64";
    static const char not_float[] = ".5 1. 0x1.0";
    RudaLexFixture fixture;
    size_t token_index = 0;
    size_t float_count = 0;

    RUDA_EXPECT(ruda_lex_fixture_init(&fixture, accepted));
    if (fixture.result.initialized) {
        RUDA_EXPECT(fixture.result.diagnostics.length == 0);
        for (token_index = 0; token_index < fixture.result.tokens.length;
            token_index += 1) {
            const RudaToken *token = ruda_lex_token(
                &fixture.result,
                token_index
            );
            if (token != NULL && token->kind == RUDA_TOKEN_FLOAT) {
                float_count += 1;
            }
        }
        RUDA_EXPECT(float_count == 7);
        ruda_expect_lossless(&fixture);
        ruda_lex_fixture_dispose(&fixture);
    }

    RUDA_EXPECT(ruda_lex_fixture_init(&fixture, rejected));
    if (fixture.result.initialized) {
        RUDA_EXPECT(fixture.result.diagnostics.length == 6);
        for (token_index = 0; token_index < fixture.result.diagnostics.length;
            token_index += 1) {
            const RudaLexDiagnostic *diagnostic = ruda_lex_diagnostic(
                &fixture.result,
                token_index
            );
            RUDA_EXPECT(diagnostic != NULL);
            if (diagnostic != NULL) {
                RUDA_EXPECT(strcmp(
                    diagnostic->code,
                    RUDA_DIAGNOSTIC_INVALID_NUMBER
                ) == 0);
            }
        }
        ruda_expect_lossless(&fixture);
        ruda_lex_fixture_dispose(&fixture);
    }

    RUDA_EXPECT(ruda_lex_fixture_init(&fixture, not_float));
    if (fixture.result.initialized) {
        float_count = 0;
        RUDA_EXPECT(fixture.result.diagnostics.length == 0);
        for (token_index = 0; token_index < fixture.result.tokens.length;
            token_index += 1) {
            const RudaToken *token = ruda_lex_token(
                &fixture.result,
                token_index
            );
            if (token != NULL && token->kind == RUDA_TOKEN_FLOAT) {
                float_count += 1;
            }
        }
        RUDA_EXPECT(float_count == 0);
        ruda_expect_lossless(&fixture);
        ruda_lex_fixture_dispose(&fixture);
    }
}

static void ruda_test_lexer_limits(void) {
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot = NULL;
    RudaLexResult result;
    RudaLexOptions options = ruda_lex_default_options();
    RUDA_EXPECT(ruda_source_manager_init(&sources, ruda_system_allocator()));
    RUDA_EXPECT(ruda_source_manager_add(
        &sources,
        "limits.ruda",
        strlen("limits.ruda"),
        "a b c",
        strlen("a b c"),
        &snapshot
    ) == RUDA_SOURCE_OK);
    options.max_tokens = 3;
    RUDA_EXPECT(ruda_lex_snapshot(
        snapshot,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_LEX_LIMIT_REACHED);
    RUDA_EXPECT(result.tokens.length == 3);
    RUDA_EXPECT(ruda_lex_token(&result, 2)->kind == RUDA_TOKEN_EOF);
    RUDA_EXPECT(ruda_result_has_diagnostic(&result, RUDA_DIAGNOSTIC_LEXICAL_WORK_LIMIT));
    ruda_lex_result_dispose(&result);
    ruda_source_manager_dispose(&sources);

    RUDA_EXPECT(ruda_source_manager_init(&sources, ruda_system_allocator()));
    RUDA_EXPECT(ruda_source_manager_add(
        &sources,
        "nest.ruda",
        strlen("nest.ruda"),
        "/* /* /* */ */ */",
        strlen("/* /* /* */ */ */"),
        &snapshot
    ) == RUDA_SOURCE_OK);
    options = ruda_lex_default_options();
    options.max_nesting = 2;
    RUDA_EXPECT(ruda_lex_snapshot(
        snapshot,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_LEX_LIMIT_REACHED);
    RUDA_EXPECT(ruda_result_has_diagnostic(
        &result,
        RUDA_DIAGNOSTIC_LEXICAL_NESTING_LIMIT
    ));
    ruda_lex_result_dispose(&result);
    ruda_source_manager_dispose(&sources);
}

static void ruda_test_lexer_allocation_failure(void) {
    static const char source[] =
        "public fn main(): void { const value_2 = $\"{1 + 2}\"; }";
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot = NULL;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_source_manager_init(&sources, ruda_system_allocator()));
    RUDA_EXPECT(ruda_source_manager_add(
        &sources,
        "allocation.ruda",
        strlen("allocation.ruda"),
        source,
        strlen(source),
        &snapshot
    ) == RUDA_SOURCE_OK);

    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaLexResult result;
        RudaLexStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_lex_snapshot(
            snapshot,
            allocator,
            ruda_lex_default_options(),
            &result
        );
        if (status == RUDA_LEX_OK) {
            reached_success = true;
            RUDA_EXPECT(result.diagnostics.length == 0);
            ruda_lex_result_dispose(&result);
        } else {
            RUDA_EXPECT(status == RUDA_LEX_ALLOCATION_FAILED);
            RUDA_EXPECT(!result.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_source_manager_dispose(&sources);
}

static uint32_t ruda_lexer_test_random(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static void ruda_test_lexer_arbitrary_bytes(void) {
    unsigned int byte = 0;
    uint32_t random_state = 0x6D614C65U;
    size_t case_index = 0;
    RudaLexOptions options = ruda_lex_default_options();
    options.max_tokens = 2048;
    options.max_diagnostics = 512;

    for (byte = 0; byte <= 0xFFU; byte += 1) {
        unsigned char source = (unsigned char)byte;
        RudaLexFixture fixture;
        RUDA_EXPECT(ruda_lex_fixture_init_bytes(
            &fixture,
            &source,
            1,
            options
        ));
        if (fixture.result.initialized) {
            ruda_expect_lossless(&fixture);
            ruda_lex_fixture_dispose(&fixture);
        }
    }

    for (case_index = 0; case_index < 100; case_index += 1) {
        unsigned char source[256];
        size_t index = 0;
        RudaLexFixture fixture;
        for (index = 0; index < sizeof(source); index += 1) {
            source[index] = (unsigned char)ruda_lexer_test_random(&random_state);
        }
        RUDA_EXPECT(ruda_lex_fixture_init_bytes(
            &fixture,
            source,
            sizeof(source),
            options
        ));
        if (fixture.result.initialized) {
            ruda_expect_lossless(&fixture);
            ruda_lex_fixture_dispose(&fixture);
        }
    }
}

void ruda_test_lexer(void) {
    ruda_test_lexer_external_fixtures();
    ruda_test_lexer_complete_snapshot();
    ruda_test_lexer_keywords_and_reserved();
    ruda_test_lexer_invalid_forms();
    ruda_test_lexer_interpolation();
    ruda_test_lexer_fixed_tokens();
    ruda_test_lexer_literal_boundaries();
    ruda_test_lexer_float_grammar();
    ruda_test_lexer_limits();
    ruda_test_lexer_allocation_failure();
    ruda_test_lexer_arbitrary_bytes();
}
