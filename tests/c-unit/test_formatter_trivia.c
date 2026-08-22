#include "test_formatter_support.h"

static uint32_t mmay_formatter_random_next(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

void mmay_test_formatter_trivia_property(void) {
    static const char *const trivia[] = {
        " ",
        "\t",
        "\n",
        "\r\n",
        " \n\t"
    };
    unsigned char source[131072];
    unsigned char expected[131072];
    unsigned char variant[262144];
    size_t source_length = 0;
    size_t expected_length = 0;
    size_t seed = 0;
    MarimayFormatterFixture canonical;
    bool canonical_ready;
    MMAY_EXPECT(mmay_formatter_read_file(
        "format/grammar.input.my",
        source,
        sizeof(source),
        &source_length
    ));
    MMAY_EXPECT(mmay_formatter_read_file(
        "format/grammar.expected.my",
        expected,
        sizeof(expected),
        &expected_length
    ));
    canonical_ready = mmay_formatter_fixture_init(
        &canonical,
        "grammar-property-source.my",
        source,
        source_length
    );
    MMAY_EXPECT(canonical_ready);
    if (!canonical_ready) {
        return;
    }
    for (seed = 1; seed <= 128; seed += 1) {
        MarimayFormatterFixture fixture;
        MarimayFormatResult formatted;
        size_t token_index = 0;
        size_t variant_length = 0;
        bool first = true;
        bool variant_valid = true;
        uint32_t random_state = (uint32_t)seed * UINT32_C(0x9e3779b9);
        for (token_index = 0;
            token_index < canonical.lexed.tokens.length;
            token_index += 1) {
            const MarimayToken *token = mmay_lex_token(&canonical.lexed, token_index);
            size_t token_length = 0;
            if (token == NULL) {
                variant_valid = false;
                break;
            }
            if (token->kind == MMAY_TOKEN_EOF) {
                break;
            }
            if (token->kind == MMAY_TOKEN_BOM
                || token->kind == MMAY_TOKEN_WHITESPACE) {
                continue;
            }
            if (!first) {
                const char *selected = trivia[
                    mmay_formatter_random_next(&random_state)
                        % (sizeof(trivia) / sizeof(trivia[0]))
                ];
                size_t trivia_length = strlen(selected);
                if (trivia_length > sizeof(variant) - variant_length) {
                    variant_valid = false;
                    break;
                }
                memcpy(variant + variant_length, selected, trivia_length);
                variant_length += trivia_length;
            }
            token_length = token->end - token->start;
            if (token_length > sizeof(variant) - variant_length) {
                variant_valid = false;
                break;
            }
            memcpy(
                variant + variant_length,
                source + token->start,
                token_length
            );
            variant_length += token_length;
            first = false;
        }
        MMAY_EXPECT(variant_valid);
        if (!variant_valid) {
            break;
        }
        MMAY_EXPECT(mmay_formatter_fixture_init(
            &fixture,
            "grammar-property.my",
            variant,
            variant_length
        ));
        if (!fixture.parsed.initialized) {
            continue;
        }
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
        MMAY_EXPECT(mmay_format_source(
            &fixture.lexed,
            &fixture.parsed,
            mmay_system_allocator(),
            mmay_format_default_options(),
            &formatted
        ) == MMAY_FORMAT_OK);
        if (formatted.initialized) {
            MMAY_EXPECT(formatted.output.length == expected_length);
            MMAY_EXPECT(formatted.output.length != expected_length
                || memcmp(
                    formatted.output.data,
                    expected,
                    expected_length
                ) == 0);
            mmay_format_result_dispose(&formatted);
        }
        mmay_formatter_fixture_dispose(&fixture);
    }
    mmay_formatter_fixture_dispose(&canonical);
}
