#include "test_parser_support.h"

/* The diagnostics recovery/limit contract must survive a pattern's delayed
   parent marker, including when completing its name exhausted the budget. */
static size_t mmay_pattern_limit_parse(
    const MarimayLexResult *lexed,
    MarimayParseOptions options,
    size_t failure_index,
    MarimayParseStatus expected
) {
    MarimayFailAllocator state;
    MarimayParseResult parsed;
    MarimayParseStatus status;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
    status = mmay_parse_file(lexed, mmay_fail_allocator(&state), options, &parsed);
    MMAY_EXPECT(status == expected);
    if (status == MMAY_PARSE_OK) {
        MMAY_EXPECT(parsed.initialized);
        MMAY_EXPECT(mmay_syntax_tree_is_lossless(&parsed.tree));
    } else {
        MMAY_EXPECT(!parsed.initialized);
    }
    mmay_parse_result_dispose(&parsed);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
    return state.attempts;
}

static void mmay_pattern_limit_allocation_sweep(
    const MarimayLexResult *lexed,
    MarimayParseOptions options,
    MarimayParseStatus expected
) {
    size_t failure_index = 0;
    size_t attempts = mmay_pattern_limit_parse(lexed, options, SIZE_MAX, expected);
    for (failure_index = 0; failure_index < attempts; failure_index += 1) {
        (void)mmay_pattern_limit_parse(
            lexed, options, failure_index, MMAY_PARSE_ALLOCATION_FAILED
        );
    }
}

void mmay_test_parser_pattern_limits(void) {
    static const char *const sources[] = {
        "fn main(): void { match value { Some(x) => {}, }; }",
        "fn main(): void { match value { Some { x } => {}, }; }",
        "fn main(): void { match value { Some. .(x) => {}, }; }",
        "fn main(): void { match value { Some. . { x } => {}, }; }"
    };
    size_t source_index = 0;
    for (source_index = 0;
        source_index < sizeof(sources) / sizeof(sources[0]); source_index += 1) {
        MarimayParseFixture fixture;
        MarimayParseOptions options = mmay_parse_default_options();
        size_t limit = 0;
        bool initialized = mmay_parse_fixture_init(&fixture, sources[source_index]);
        MMAY_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT((fixture.parsed.diagnostics.length == 0) == (source_index < 2));
        mmay_pattern_limit_allocation_sweep(&fixture.lexed, options, MMAY_PARSE_OK);

        /* This boundary stops precisely while completing the pattern name;
           the following '(' or '{' used to turn the limit into allocator failure. */
        options.max_events = 35;
        mmay_pattern_limit_allocation_sweep(
            &fixture.lexed, options, MMAY_PARSE_LIMIT_REACHED
        );
        options = mmay_parse_default_options();
        if (source_index >= 2) {
            options.max_diagnostics = 1;
            mmay_pattern_limit_allocation_sweep(
                &fixture.lexed, options, MMAY_PARSE_LIMIT_REACHED
            );
        }

        /* Exercise neighbouring event and nesting boundaries as well as the
           exact regression, always checking teardown counters. */
        for (limit = 2; limit <= 80; limit += 1) {
            MarimayFailAllocator state;
            MarimayParseResult parsed;
            MarimayParseStatus status;
            options = mmay_parse_default_options();
            options.max_events = limit;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
            status = mmay_parse_file(
                &fixture.lexed, mmay_fail_allocator(&state), options, &parsed
            );
            MMAY_EXPECT(status == MMAY_PARSE_OK || status == MMAY_PARSE_LIMIT_REACHED);
            MMAY_EXPECT(parsed.initialized == (status == MMAY_PARSE_OK));
            mmay_parse_result_dispose(&parsed);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        for (limit = 2; limit <= 16; limit += 1) {
            MarimayFailAllocator state;
            MarimayParseResult parsed;
            MarimayParseStatus status;
            options = mmay_parse_default_options();
            options.max_nesting = limit;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
            status = mmay_parse_file(
                &fixture.lexed, mmay_fail_allocator(&state), options, &parsed
            );
            MMAY_EXPECT(status == MMAY_PARSE_OK || status == MMAY_PARSE_LIMIT_REACHED);
            MMAY_EXPECT(parsed.initialized == (status == MMAY_PARSE_OK));
            mmay_parse_result_dispose(&parsed);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        mmay_parse_fixture_dispose(&fixture);
    }
}
