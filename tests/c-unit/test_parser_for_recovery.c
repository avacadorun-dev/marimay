#include "test_parser_support.h"

#include "mmay/format/formatter.h"

static void mmay_for_recovery_observe(
    const MarimayParseResult *parsed,
    size_t offset,
    size_t function_count
) {
    const MarimayParseDiagnostic *diagnostic = mmay_parse_diagnostic(parsed, 0);
    MMAY_EXPECT(parsed->initialized);
    MMAY_EXPECT(parsed->diagnostics.length == 1);
    MMAY_EXPECT(parsed->suppressed_diagnostics == 0);
    MMAY_EXPECT(diagnostic != NULL);
    if (diagnostic != NULL) {
        MMAY_EXPECT(strcmp(diagnostic->code, MMAY_DIAGNOSTIC_EXPECTED_TOKEN) == 0);
        MMAY_EXPECT(strcmp(diagnostic->message,
            "loop binding must be a single identifier") == 0);
        MMAY_EXPECT(diagnostic->start == offset && diagnostic->end == offset);
    }
    MMAY_EXPECT(mmay_syntax_tree_is_lossless(&parsed->tree));
    MMAY_EXPECT(mmay_syntax_count_kind(parsed->tree.root,
        MMAY_SYNTAX_FUNCTION_DECL) == function_count);
    MMAY_EXPECT(mmay_syntax_count_kind(parsed->tree.root,
        MMAY_SYNTAX_FOR_STATEMENT) == 1);
    MMAY_EXPECT(mmay_syntax_count_kind(parsed->tree.root,
        MMAY_SYNTAX_BREAK_STATEMENT) == 1);
}

static void mmay_for_recovery_failures(
    const MarimayLexResult *lexed,
    MarimayParseOptions options,
    MarimayParseStatus expected,
    size_t offset,
    size_t function_count
) {
    size_t failure = 0;
    bool complete = false;
    for (failure = 0; failure < 4096; failure += 1) {
        MarimayFailAllocator state;
        MarimayParseResult parsed;
        MarimayParseStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        status = mmay_parse_file(lexed, mmay_fail_allocator(&state), options, &parsed);
        if (status == expected) {
            complete = true;
            if (status == MMAY_PARSE_OK) {
                mmay_for_recovery_observe(&parsed, offset, function_count);
            } else {
                MMAY_EXPECT(!parsed.initialized);
            }
        } else {
            MMAY_EXPECT(status == MMAY_PARSE_ALLOCATION_FAILED);
            MMAY_EXPECT(!parsed.initialized);
        }
        mmay_parse_result_dispose(&parsed);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (complete) {
            break;
        }
    }
    MMAY_EXPECT(complete);
}

void mmay_test_parser_for_recovery(void) {
    static const char *const bindings[] = {
        "(key, value)", "(key, (value, other))", "()", "(key,)",
        "[key, value]", "[(key, value)]", "(key, value", "[key, value",
        "(key, value]", "(ключ, /* in } */ значение)"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(bindings) / sizeof(bindings[0]); index += 1) {
        char source[2048];
        size_t function_index = 0;
        size_t length = 0;
        int written = snprintf(source, sizeof(source),
            "fn main(): void { for %s in values { break; } }\n", bindings[index]);
        MarimayParseFixture fixture;
        MarimayParseOptions options = mmay_parse_default_options();
        MarimayFormatResult formatted;
        MarimayFailAllocator format_state;
        bool initialized = false;
        MMAY_EXPECT(written > 0 && (size_t)written < sizeof(source));
        if (written <= 0 || (size_t)written >= sizeof(source)) {
            continue;
        }
        length = (size_t)written;
        /* A single invalid header used to swallow these declarations and
           exhaust the diagnostic budget, hiding the original P0001. */
        for (function_index = 0; function_index < 30; function_index += 1) {
            written = snprintf(source + length, sizeof(source) - length,
                "fn after%zu(): void {}\n", function_index);
            MMAY_EXPECT(written > 0 && (size_t)written < sizeof(source) - length);
            if (written <= 0 || (size_t)written >= sizeof(source) - length) {
                break;
            }
            length += (size_t)written;
        }
        initialized = mmay_parse_fixture_init(&fixture, source);
        MMAY_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        mmay_for_recovery_observe(&fixture.parsed, 22, 31);
        mmay_fail_allocator_init(&format_state, mmay_system_allocator(), 0);
        MMAY_EXPECT(mmay_format_source(&fixture.lexed, &fixture.parsed,
            mmay_fail_allocator(&format_state), mmay_format_default_options(),
            &formatted) == MMAY_FORMAT_SOURCE_INVALID);
        MMAY_EXPECT(!formatted.initialized);
        mmay_format_result_dispose(&formatted);
        MMAY_EXPECT(format_state.attempts == 0);
        MMAY_EXPECT(format_state.live_allocations == 0);
        MMAY_EXPECT(format_state.live_bytes == 0);
        options.max_diagnostics = 1;
        mmay_for_recovery_failures(&fixture.lexed, options, MMAY_PARSE_OK, 22, 31);
        options.max_events = 16;
        mmay_for_recovery_failures(&fixture.lexed, options,
            MMAY_PARSE_LIMIT_REACHED, 22, 31);
        options = mmay_parse_default_options();
        options.max_nesting = 4;
        mmay_for_recovery_failures(&fixture.lexed, options,
            MMAY_PARSE_LIMIT_REACHED, 22, 31);
        mmay_parse_fixture_dispose(&fixture);
    }
}
