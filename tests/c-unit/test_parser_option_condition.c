#include "test_parser_support.h"

static void mmay_option_parse_failures(const char *source, bool valid) {
    MarimayParseFixture fixture;
    bool complete = false;
    MMAY_EXPECT(mmay_parse_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
    MMAY_EXPECT((fixture.parsed.diagnostics.length == 0) == valid);
    if (mmay_syntax_count_kind(fixture.parsed.tree.root, MMAY_SYNTAX_FUNCTION_DECL) != 2) {
        fprintf(stderr, "Option condition recovery lost a declaration: %s", source);
    }
    MMAY_EXPECT(mmay_syntax_count_kind(fixture.parsed.tree.root, MMAY_SYNTAX_FUNCTION_DECL) == 2);
    for (size_t failure = 0; failure < 4096; failure += 1) {
        MarimayFailAllocator state;
        MarimayParseResult parsed;
        MarimayParseStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        status = mmay_parse_file(&fixture.lexed, mmay_fail_allocator(&state),
            mmay_parse_default_options(), &parsed);
        if (status == MMAY_PARSE_OK) {
            complete = true;
            MMAY_EXPECT(mmay_syntax_tree_is_lossless(&parsed.tree));
            MMAY_EXPECT((parsed.diagnostics.length == 0) == valid);
            MMAY_EXPECT(mmay_syntax_count_kind(parsed.tree.root, MMAY_SYNTAX_FUNCTION_DECL) == 2);
        } else {
            MMAY_EXPECT(status == MMAY_PARSE_ALLOCATION_FAILED);
            MMAY_EXPECT(!parsed.initialized);
        }
        mmay_parse_result_dispose(&parsed);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (complete) break;
    }
    MMAY_EXPECT(complete);
    {
        MarimayParseOptions options = mmay_parse_default_options();
        MarimayFailAllocator state;
        MarimayParseResult parsed;
        options.max_events = 8;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_parse_file(&fixture.lexed, mmay_fail_allocator(&state),
            options, &parsed) == MMAY_PARSE_LIMIT_REACHED);
        mmay_parse_result_dispose(&parsed);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    mmay_parse_fixture_dispose(&fixture);
}

void mmay_test_parser_option_condition(void) {
    mmay_option_parse_failures(
        "fn main(): void { if const item = Some(1) {} else if let other = value {}\n"
        " while const item = items.pop() { continue; } }\nfn after(): void {}\n", true);
    mmay_option_parse_failures(
        "fn main(): void { if const item = {} }\nfn after(): void {}\n", false);
    mmay_option_parse_failures(
        "fn main(): void { while let item {} }\nfn after(): void {}\n", false);
    mmay_option_parse_failures(
        "fn main(): void { if const _ = value {} }\nfn after(): void {}\n", false);
    mmay_option_parse_failures(
        "fn main(): void { if const item: int = value {} }\nfn after(): void {}\n", false);
    mmay_option_parse_failures(
        "fn main(): void { while let (first, second) = value {} }\nfn after(): void {}\n", false);
    mmay_option_parse_failures(
        "fn main(): void { if const item = value && const other = value {} }\nfn after(): void {}\n", false);
}
