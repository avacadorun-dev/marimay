#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/lex/lexer.h"
#include "mmay/lex/token.h"
#include "mmay/source/source_manager.h"

typedef struct MarimayLexFixture {
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *snapshot;
    MarimayLexResult result;
} MarimayLexFixture;

static bool mmay_lex_fixture_init_bytes(
    MarimayLexFixture *fixture,
    const unsigned char *bytes,
    size_t length,
    MarimayLexOptions options
) {
    memset(fixture, 0, sizeof(*fixture));
    if (!mmay_source_manager_init(&fixture->sources, mmay_system_allocator())) {
        return false;
    }
    if (mmay_source_manager_add(
            &fixture->sources,
            "fixture.my",
            strlen("fixture.my"),
            bytes,
            length,
            &fixture->snapshot
        ) != MMAY_SOURCE_OK) {
        mmay_source_manager_dispose(&fixture->sources);
        return false;
    }
    if (mmay_lex_snapshot(
            fixture->snapshot,
            mmay_system_allocator(),
            options,
            &fixture->result
        ) != MMAY_LEX_OK) {
        mmay_lex_result_dispose(&fixture->result);
        mmay_source_manager_dispose(&fixture->sources);
        return false;
    }
    return true;
}

static bool mmay_lex_fixture_init(MarimayLexFixture *fixture, const char *text) {
    return mmay_lex_fixture_init_bytes(
        fixture,
        (const unsigned char *)text,
        strlen(text),
        mmay_lex_default_options()
    );
}

static void mmay_lex_fixture_dispose(MarimayLexFixture *fixture) {
    mmay_lex_result_dispose(&fixture->result);
    mmay_source_manager_dispose(&fixture->sources);
}

static void mmay_expect_lossless(const MarimayLexFixture *fixture) {
    size_t index = 0;
    size_t cursor = 0;
    for (index = 0; index < fixture->result.tokens.length; index += 1) {
        const MarimayToken *token = mmay_lex_token(&fixture->result, index);
        MMAY_EXPECT(token != NULL);
        if (token == NULL) {
            return;
        }
        MMAY_EXPECT(token->start == cursor);
        MMAY_EXPECT(token->end >= token->start);
        if (token->kind == MMAY_TOKEN_EOF) {
            MMAY_EXPECT(index + 1 == fixture->result.tokens.length);
            MMAY_EXPECT(token->end == token->start);
        } else {
            cursor = token->end;
        }
    }
    MMAY_EXPECT(cursor == mmay_source_length(fixture->snapshot));
}

static bool mmay_result_has_diagnostic(
    const MarimayLexResult *result,
    const char *code
) {
    size_t index = 0;
    for (index = 0; index < result->diagnostics.length; index += 1) {
        const MarimayLexDiagnostic *diagnostic = mmay_lex_diagnostic(result, index);
        if (diagnostic != NULL && strcmp(diagnostic->code, code) == 0) {
            return true;
        }
    }
    return false;
}

static void mmay_dump_tokens(const MarimayLexFixture *fixture, char *output, size_t capacity) {
    size_t index = 0;
    size_t length = 0;
    for (index = 0; index < fixture->result.tokens.length; index += 1) {
        const MarimayToken *token = mmay_lex_token(&fixture->result, index);
        int written = 0;
        if (token == NULL || length >= capacity) {
            break;
        }
        written = snprintf(
            output + length,
            capacity - length,
            "%s %zu..%zu\n",
            mmay_token_kind_name(token->kind),
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

static bool mmay_read_lexer_fixture(
    const char *path,
    unsigned char *bytes,
    size_t capacity,
    size_t *length
) {
    FILE *file = mmay_test_open_file(path, "rb");
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

static void mmay_dump_lex_diagnostics(
    const MarimayLexResult *result,
    char *output,
    size_t capacity
) {
    size_t index = 0;
    size_t length = 0;
    for (index = 0; index < result->diagnostics.length; index += 1) {
        const MarimayLexDiagnostic *diagnostic = mmay_lex_diagnostic(result, index);
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

static void mmay_test_lexer_external_fixtures(void) {
    unsigned char source[4096];
    size_t source_length = 0;
    MarimayLexFixture fixture;
    char dump[16384] = {0};

    MMAY_EXPECT(mmay_read_lexer_fixture(
        "syntax/lexer/lossless.my",
        source,
        sizeof(source),
        &source_length
    ));
    MMAY_EXPECT(mmay_lex_fixture_init_bytes(
        &fixture,
        source,
        source_length,
        mmay_lex_default_options()
    ));
    if (fixture.result.initialized) {
        MMAY_EXPECT(fixture.result.diagnostics.length == 0);
        mmay_expect_lossless(&fixture);
        mmay_dump_tokens(&fixture, dump, sizeof(dump));
        MMAY_EXPECT_FILE("syntax/lexer/lossless.tokens", dump);
        mmay_lex_fixture_dispose(&fixture);
    }

    memset(dump, 0, sizeof(dump));
    MMAY_EXPECT(mmay_read_lexer_fixture(
        "syntax/lexer/invalid.my",
        source,
        sizeof(source),
        &source_length
    ));
    MMAY_EXPECT(mmay_lex_fixture_init_bytes(
        &fixture,
        source,
        source_length,
        mmay_lex_default_options()
    ));
    if (fixture.result.initialized) {
        mmay_expect_lossless(&fixture);
        mmay_dump_lex_diagnostics(&fixture.result, dump, sizeof(dump));
        MMAY_EXPECT_FILE("syntax/lexer/invalid.diagnostics", dump);
        mmay_lex_fixture_dispose(&fixture);
    }
}

static void mmay_test_lexer_complete_snapshot(void) {
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
    MarimayLexFixture fixture;
    char dump[16384] = {0};
    MMAY_EXPECT(mmay_lex_fixture_init_bytes(
        &fixture,
        source,
        sizeof(source) - 1,
        mmay_lex_default_options()
    ));
    if (!fixture.result.initialized) {
        return;
    }
    MMAY_EXPECT(fixture.result.diagnostics.length == 0);
    mmay_expect_lossless(&fixture);
    mmay_dump_tokens(&fixture, dump, sizeof(dump));
    MMAY_EXPECT_SNAPSHOT("lexer-complete.tokens", dump);
    mmay_lex_fixture_dispose(&fixture);
}

static void mmay_test_lexer_keywords_and_reserved(void) {
    static const char source[] =
        "as break case const continue defer else enum extern false fn for from "
        "fuzz if import in interface let match mutable of panic public return "
        "scope spawn test true type unsafe while yield "
        "any async await class extends impl null throws undefined";
    static const MarimayTokenKind expected[] = {
        MMAY_TOKEN_KW_AS, MMAY_TOKEN_KW_BREAK, MMAY_TOKEN_IDENTIFIER, MMAY_TOKEN_KW_CONST,
        MMAY_TOKEN_KW_CONTINUE, MMAY_TOKEN_KW_DEFER, MMAY_TOKEN_KW_ELSE,
        MMAY_TOKEN_KW_ENUM, MMAY_TOKEN_KW_EXTERN, MMAY_TOKEN_KW_FALSE, MMAY_TOKEN_KW_FN,
        MMAY_TOKEN_KW_FOR, MMAY_TOKEN_KW_FROM, MMAY_TOKEN_IDENTIFIER, MMAY_TOKEN_KW_IF,
        MMAY_TOKEN_KW_IMPORT, MMAY_TOKEN_KW_IN, MMAY_TOKEN_KW_INTERFACE,
        MMAY_TOKEN_KW_LET, MMAY_TOKEN_KW_MATCH, MMAY_TOKEN_KW_MUTABLE, MMAY_TOKEN_IDENTIFIER,
        MMAY_TOKEN_IDENTIFIER, MMAY_TOKEN_KW_PUBLIC, MMAY_TOKEN_KW_RETURN,
        MMAY_TOKEN_KW_SCOPE, MMAY_TOKEN_KW_SPAWN, MMAY_TOKEN_IDENTIFIER, MMAY_TOKEN_KW_TRUE,
        MMAY_TOKEN_KW_TYPE, MMAY_TOKEN_KW_UNSAFE, MMAY_TOKEN_KW_WHILE,
        MMAY_TOKEN_IDENTIFIER, MMAY_TOKEN_IDENTIFIER, MMAY_TOKEN_IDENTIFIER,
        MMAY_TOKEN_IDENTIFIER, MMAY_TOKEN_IDENTIFIER, MMAY_TOKEN_IDENTIFIER,
        MMAY_TOKEN_IDENTIFIER, MMAY_TOKEN_RESERVED_WORD, MMAY_TOKEN_IDENTIFIER,
        MMAY_TOKEN_RESERVED_WORD
    };
    MarimayLexFixture fixture;
    size_t token_index = 0;
    size_t expected_index = 0;
    MMAY_EXPECT(mmay_lex_fixture_init(&fixture, source));
    if (!fixture.result.initialized) {
        return;
    }
    for (token_index = 0; token_index < fixture.result.tokens.length; token_index += 1) {
        const MarimayToken *token = mmay_lex_token(&fixture.result, token_index);
        if (token != NULL && token->kind != MMAY_TOKEN_WHITESPACE
            && token->kind != MMAY_TOKEN_EOF) {
            MMAY_EXPECT(expected_index < sizeof(expected) / sizeof(expected[0]));
            if (expected_index < sizeof(expected) / sizeof(expected[0])) {
                MMAY_EXPECT(token->kind == expected[expected_index]);
            }
            expected_index += 1;
        }
    }
    MMAY_EXPECT(expected_index == sizeof(expected) / sizeof(expected[0]));
    MMAY_EXPECT(fixture.result.diagnostics.length == 2);
    for (token_index = 0; token_index < fixture.result.diagnostics.length; token_index += 1) {
        const MarimayLexDiagnostic *diagnostic = mmay_lex_diagnostic(
            &fixture.result,
            token_index
        );
        MMAY_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            MMAY_EXPECT(strcmp(diagnostic->code, MMAY_DIAGNOSTIC_RESERVED_WORD) == 0);
        }
    }
    mmay_expect_lossless(&fixture);
    mmay_lex_fixture_dispose(&fixture);
}

static void mmay_test_lexer_invalid_forms(void) {
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
    MarimayLexFixture fixture;
    MMAY_EXPECT(mmay_lex_fixture_init_bytes(
        &fixture,
        source,
        sizeof(source),
        mmay_lex_default_options()
    ));
    if (!fixture.result.initialized) {
        return;
    }
    MMAY_EXPECT(mmay_result_has_diagnostic(
        &fixture.result,
        MMAY_DIAGNOSTIC_INVALID_UTF8
    ));
    MMAY_EXPECT(mmay_result_has_diagnostic(
        &fixture.result,
        MMAY_DIAGNOSTIC_UNEXPECTED_CHARACTER
    ));
    MMAY_EXPECT(mmay_result_has_diagnostic(
        &fixture.result,
        MMAY_DIAGNOSTIC_INVALID_NUMBER
    ));
    MMAY_EXPECT(mmay_result_has_diagnostic(
        &fixture.result,
        MMAY_DIAGNOSTIC_INVALID_ESCAPE
    ));
    MMAY_EXPECT(mmay_result_has_diagnostic(
        &fixture.result,
        MMAY_DIAGNOSTIC_UNTERMINATED_LITERAL
    ));
    MMAY_EXPECT(mmay_result_has_diagnostic(
        &fixture.result,
        MMAY_DIAGNOSTIC_INVALID_BYTE_STRING
    ));
    MMAY_EXPECT(mmay_result_has_diagnostic(
        &fixture.result,
        MMAY_DIAGNOSTIC_INVALID_RUNE
    ));
    MMAY_EXPECT(mmay_result_has_diagnostic(
        &fixture.result,
        MMAY_DIAGNOSTIC_UNTERMINATED_BLOCK_COMMENT
    ));
    mmay_expect_lossless(&fixture);
    mmay_lex_fixture_dispose(&fixture);
}

static void mmay_test_lexer_interpolation(void) {
    MarimayLexFixture fixture;
    MMAY_EXPECT(mmay_lex_fixture_init(
        &fixture,
        "$\"a {call($\"b {x}\")} c\""
    ));
    if (fixture.result.initialized) {
        MMAY_EXPECT(fixture.result.diagnostics.length == 0);
        mmay_expect_lossless(&fixture);
        mmay_lex_fixture_dispose(&fixture);
    }

    MMAY_EXPECT(mmay_lex_fixture_init(&fixture, "$\"x }\""));
    if (fixture.result.initialized) {
        MMAY_EXPECT(mmay_result_has_diagnostic(
            &fixture.result,
            MMAY_DIAGNOSTIC_UNMATCHED_INTERPOLATION_BRACE
        ));
        mmay_expect_lossless(&fixture);
        mmay_lex_fixture_dispose(&fixture);
    }
}

static void mmay_test_lexer_fixed_tokens(void) {
    static const char source[] =
        "@ , : ; . ( ) { } [ ] + - * / % ! ~ & | ^ ? = < > "
        "=> -> == != <= >= && || << >> += -= *= /= %= &= |= ^= <<= >>=";
    static const MarimayTokenKind expected[] = {
        MMAY_TOKEN_AT, MMAY_TOKEN_COMMA, MMAY_TOKEN_COLON, MMAY_TOKEN_SEMICOLON,
        MMAY_TOKEN_DOT, MMAY_TOKEN_LEFT_PAREN, MMAY_TOKEN_RIGHT_PAREN,
        MMAY_TOKEN_LEFT_BRACE, MMAY_TOKEN_RIGHT_BRACE, MMAY_TOKEN_LEFT_BRACKET,
        MMAY_TOKEN_RIGHT_BRACKET, MMAY_TOKEN_PLUS, MMAY_TOKEN_MINUS, MMAY_TOKEN_STAR,
        MMAY_TOKEN_SLASH, MMAY_TOKEN_PERCENT, MMAY_TOKEN_BANG, MMAY_TOKEN_TILDE,
        MMAY_TOKEN_AMPERSAND, MMAY_TOKEN_PIPE, MMAY_TOKEN_CARET, MMAY_TOKEN_QUESTION,
        MMAY_TOKEN_EQUAL, MMAY_TOKEN_LESS, MMAY_TOKEN_GREATER, MMAY_TOKEN_FAT_ARROW,
        MMAY_TOKEN_THIN_ARROW, MMAY_TOKEN_EQUAL_EQUAL, MMAY_TOKEN_BANG_EQUAL,
        MMAY_TOKEN_LESS_EQUAL, MMAY_TOKEN_GREATER_EQUAL,
        MMAY_TOKEN_AMPERSAND_AMPERSAND, MMAY_TOKEN_PIPE_PIPE, MMAY_TOKEN_SHIFT_LEFT,
        MMAY_TOKEN_SHIFT_RIGHT, MMAY_TOKEN_PLUS_EQUAL, MMAY_TOKEN_MINUS_EQUAL,
        MMAY_TOKEN_STAR_EQUAL, MMAY_TOKEN_SLASH_EQUAL, MMAY_TOKEN_PERCENT_EQUAL,
        MMAY_TOKEN_AMPERSAND_EQUAL, MMAY_TOKEN_PIPE_EQUAL, MMAY_TOKEN_CARET_EQUAL,
        MMAY_TOKEN_SHIFT_LEFT_EQUAL, MMAY_TOKEN_SHIFT_RIGHT_EQUAL
    };
    MarimayLexFixture fixture;
    size_t token_index = 0;
    size_t expected_index = 0;
    MMAY_EXPECT(mmay_lex_fixture_init(&fixture, source));
    if (!fixture.result.initialized) {
        return;
    }
    MMAY_EXPECT(fixture.result.diagnostics.length == 0);
    for (token_index = 0; token_index < fixture.result.tokens.length; token_index += 1) {
        const MarimayToken *token = mmay_lex_token(&fixture.result, token_index);
        if (token != NULL && token->kind != MMAY_TOKEN_WHITESPACE
            && token->kind != MMAY_TOKEN_EOF) {
            MMAY_EXPECT(expected_index < sizeof(expected) / sizeof(expected[0]));
            if (expected_index < sizeof(expected) / sizeof(expected[0])) {
                MMAY_EXPECT(token->kind == expected[expected_index]);
            }
            expected_index += 1;
        }
    }
    MMAY_EXPECT(expected_index == sizeof(expected) / sizeof(expected[0]));
    mmay_expect_lossless(&fixture);
    mmay_lex_fixture_dispose(&fixture);
}

static void mmay_test_lexer_literal_boundaries(void) {
    static const char valid[] =
        "0 0B10 0O77 0XCA_FE 1_2 1.25 1e3 1.0E-2 "
        "\"\\\\\\\"\\'\\n\\r\\t\\0\\u{10FFFF}\" "
        "b\"A\\\\\\\"\\'\\n\\r\\t\\0\\xFF\" "
        "r\"raw\\n\" '\\u{3BB}'";
    static const char invalid[] =
        "0b 0o8 0xG 1_ 1e 12name "
        "\"\\xFF\" b\"λ\" b\"\\u{41}\" '\\u{D800}' '' 'ab'";
    MarimayLexFixture fixture;
    MMAY_EXPECT(mmay_lex_fixture_init(&fixture, valid));
    if (fixture.result.initialized) {
        MMAY_EXPECT(fixture.result.diagnostics.length == 0);
        mmay_expect_lossless(&fixture);
        mmay_lex_fixture_dispose(&fixture);
    }
    MMAY_EXPECT(mmay_lex_fixture_init(&fixture, invalid));
    if (fixture.result.initialized) {
        MMAY_EXPECT(mmay_result_has_diagnostic(
            &fixture.result,
            MMAY_DIAGNOSTIC_INVALID_NUMBER
        ));
        MMAY_EXPECT(mmay_result_has_diagnostic(
            &fixture.result,
            MMAY_DIAGNOSTIC_INVALID_ESCAPE
        ));
        MMAY_EXPECT(mmay_result_has_diagnostic(
            &fixture.result,
            MMAY_DIAGNOSTIC_INVALID_BYTE_STRING
        ));
        MMAY_EXPECT(mmay_result_has_diagnostic(
            &fixture.result,
            MMAY_DIAGNOSTIC_INVALID_RUNE
        ));
        mmay_expect_lossless(&fixture);
        mmay_lex_fixture_dispose(&fixture);
    }
}

static void mmay_test_lexer_float_grammar(void) {
    static const char accepted[] =
        "0.0 1_2.5_0 6.02e23 1e-9 1.0E+2 5e-324 9_0e3_0";
    static const char rejected[] =
        "1_.0 1.0_ 1e_2 1e+_2 1e- 1.0f64";
    static const char not_float[] = ".5 1. 0x1.0";
    MarimayLexFixture fixture;
    size_t token_index = 0;
    size_t float_count = 0;

    MMAY_EXPECT(mmay_lex_fixture_init(&fixture, accepted));
    if (fixture.result.initialized) {
        MMAY_EXPECT(fixture.result.diagnostics.length == 0);
        for (token_index = 0; token_index < fixture.result.tokens.length;
            token_index += 1) {
            const MarimayToken *token = mmay_lex_token(
                &fixture.result,
                token_index
            );
            if (token != NULL && token->kind == MMAY_TOKEN_FLOAT) {
                float_count += 1;
            }
        }
        MMAY_EXPECT(float_count == 7);
        mmay_expect_lossless(&fixture);
        mmay_lex_fixture_dispose(&fixture);
    }

    MMAY_EXPECT(mmay_lex_fixture_init(&fixture, rejected));
    if (fixture.result.initialized) {
        MMAY_EXPECT(fixture.result.diagnostics.length == 6);
        for (token_index = 0; token_index < fixture.result.diagnostics.length;
            token_index += 1) {
            const MarimayLexDiagnostic *diagnostic = mmay_lex_diagnostic(
                &fixture.result,
                token_index
            );
            MMAY_EXPECT(diagnostic != NULL);
            if (diagnostic != NULL) {
                MMAY_EXPECT(strcmp(
                    diagnostic->code,
                    MMAY_DIAGNOSTIC_INVALID_NUMBER
                ) == 0);
            }
        }
        mmay_expect_lossless(&fixture);
        mmay_lex_fixture_dispose(&fixture);
    }

    MMAY_EXPECT(mmay_lex_fixture_init(&fixture, not_float));
    if (fixture.result.initialized) {
        float_count = 0;
        MMAY_EXPECT(fixture.result.diagnostics.length == 0);
        for (token_index = 0; token_index < fixture.result.tokens.length;
            token_index += 1) {
            const MarimayToken *token = mmay_lex_token(
                &fixture.result,
                token_index
            );
            if (token != NULL && token->kind == MMAY_TOKEN_FLOAT) {
                float_count += 1;
            }
        }
        MMAY_EXPECT(float_count == 0);
        mmay_expect_lossless(&fixture);
        mmay_lex_fixture_dispose(&fixture);
    }
}

static void mmay_test_lexer_limits(void) {
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *snapshot = NULL;
    MarimayLexResult result;
    MarimayLexOptions options = mmay_lex_default_options();
    MMAY_EXPECT(mmay_source_manager_init(&sources, mmay_system_allocator()));
    MMAY_EXPECT(mmay_source_manager_add(
        &sources,
        "limits.my",
        strlen("limits.my"),
        "a b c",
        strlen("a b c"),
        &snapshot
    ) == MMAY_SOURCE_OK);
    options.max_tokens = 3;
    MMAY_EXPECT(mmay_lex_snapshot(
        snapshot,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_LEX_LIMIT_REACHED);
    MMAY_EXPECT(result.tokens.length == 3);
    MMAY_EXPECT(mmay_lex_token(&result, 2)->kind == MMAY_TOKEN_EOF);
    MMAY_EXPECT(mmay_result_has_diagnostic(&result, MMAY_DIAGNOSTIC_LEXICAL_WORK_LIMIT));
    mmay_lex_result_dispose(&result);
    mmay_source_manager_dispose(&sources);

    MMAY_EXPECT(mmay_source_manager_init(&sources, mmay_system_allocator()));
    MMAY_EXPECT(mmay_source_manager_add(
        &sources,
        "nest.my",
        strlen("nest.my"),
        "/* /* /* */ */ */",
        strlen("/* /* /* */ */ */"),
        &snapshot
    ) == MMAY_SOURCE_OK);
    options = mmay_lex_default_options();
    options.max_nesting = 2;
    MMAY_EXPECT(mmay_lex_snapshot(
        snapshot,
        mmay_system_allocator(),
        options,
        &result
    ) == MMAY_LEX_LIMIT_REACHED);
    MMAY_EXPECT(mmay_result_has_diagnostic(
        &result,
        MMAY_DIAGNOSTIC_LEXICAL_NESTING_LIMIT
    ));
    mmay_lex_result_dispose(&result);
    mmay_source_manager_dispose(&sources);
}

static void mmay_test_lexer_allocation_failure(void) {
    static const char source[] =
        "public fn main(): void { const value_2 = $\"{1 + 2}\"; }";
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *snapshot = NULL;
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_source_manager_init(&sources, mmay_system_allocator()));
    MMAY_EXPECT(mmay_source_manager_add(
        &sources,
        "allocation.my",
        strlen("allocation.my"),
        source,
        strlen(source),
        &snapshot
    ) == MMAY_SOURCE_OK);

    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayLexResult result;
        MarimayLexStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        status = mmay_lex_snapshot(
            snapshot,
            allocator,
            mmay_lex_default_options(),
            &result
        );
        if (status == MMAY_LEX_OK) {
            reached_success = true;
            MMAY_EXPECT(result.diagnostics.length == 0);
            mmay_lex_result_dispose(&result);
        } else {
            MMAY_EXPECT(status == MMAY_LEX_ALLOCATION_FAILED);
            MMAY_EXPECT(!result.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_source_manager_dispose(&sources);
}

static uint32_t mmay_lexer_test_random(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static void mmay_test_lexer_arbitrary_bytes(void) {
    unsigned int byte = 0;
    uint32_t random_state = 0x6D614C65U;
    size_t case_index = 0;
    MarimayLexOptions options = mmay_lex_default_options();
    options.max_tokens = 2048;
    options.max_diagnostics = 512;

    for (byte = 0; byte <= 0xFFU; byte += 1) {
        unsigned char source = (unsigned char)byte;
        MarimayLexFixture fixture;
        MMAY_EXPECT(mmay_lex_fixture_init_bytes(
            &fixture,
            &source,
            1,
            options
        ));
        if (fixture.result.initialized) {
            mmay_expect_lossless(&fixture);
            mmay_lex_fixture_dispose(&fixture);
        }
    }

    for (case_index = 0; case_index < 100; case_index += 1) {
        unsigned char source[256];
        size_t index = 0;
        MarimayLexFixture fixture;
        for (index = 0; index < sizeof(source); index += 1) {
            source[index] = (unsigned char)mmay_lexer_test_random(&random_state);
        }
        MMAY_EXPECT(mmay_lex_fixture_init_bytes(
            &fixture,
            source,
            sizeof(source),
            options
        ));
        if (fixture.result.initialized) {
            mmay_expect_lossless(&fixture);
            mmay_lex_fixture_dispose(&fixture);
        }
    }
}

void mmay_test_lexer(void) {
    mmay_test_lexer_external_fixtures();
    mmay_test_lexer_complete_snapshot();
    mmay_test_lexer_keywords_and_reserved();
    mmay_test_lexer_invalid_forms();
    mmay_test_lexer_interpolation();
    mmay_test_lexer_fixed_tokens();
    mmay_test_lexer_literal_boundaries();
    mmay_test_lexer_float_grammar();
    mmay_test_lexer_limits();
    mmay_test_lexer_allocation_failure();
    mmay_test_lexer_arbitrary_bytes();
}
