#include "test_parser_support.h"

static void mmay_type_separator_allocations(const MarimayLexResult *lexed) {
    MarimayFailAllocator state;
    MarimayParseResult parsed;
    size_t index = 0;
    size_t attempts = 0;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_parse_file(lexed, mmay_fail_allocator(&state),
        mmay_parse_default_options(), &parsed) == MMAY_PARSE_OK);
    MMAY_EXPECT(parsed.initialized && parsed.diagnostics.length == 0);
    if (parsed.initialized) MMAY_EXPECT(mmay_syntax_tree_is_lossless(&parsed.tree));
    attempts = state.attempts;
    mmay_parse_result_dispose(&parsed);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    for (index = 0; index < attempts; index += 1) {
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        MMAY_EXPECT(mmay_parse_file(lexed, mmay_fail_allocator(&state),
            mmay_parse_default_options(), &parsed) == MMAY_PARSE_ALLOCATION_FAILED);
        MMAY_EXPECT(!parsed.initialized);
        mmay_parse_result_dispose(&parsed);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
}

void mmay_test_parser_type_list_separators(void) {
    static const char *const valid[] = {
        "type X = (int,);", "type X = (int, string,);",
        "type X = ((int,), (int, string,),);",
        "type X = () => int;", "type X = (int,) => int;",
        "type X = (int, string,) => (int,);",
        "enum E { A(int,), B(int, string,), C((int,),), }"
    };
    static const char *const invalid[] = {
        "type X = ();", "type X = (int);", "type X = (int,,);",
        "type X = (int,,) => int;", "enum E { A(,int), }",
        "enum E { A(int,,), }", "enum E { A(), }"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(valid) / sizeof(valid[0]); index += 1) {
        MarimayParseFixture fixture;
        bool ready = mmay_parse_fixture_init(&fixture, valid[index]);
        MMAY_EXPECT(ready);
        if (!ready) continue;
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
        MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
        mmay_type_separator_allocations(&fixture.lexed);
        mmay_parse_fixture_dispose(&fixture);
    }
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        MarimayParseFixture fixture;
        bool ready = mmay_parse_fixture_init(&fixture, invalid[index]);
        MMAY_EXPECT(ready);
        if (!ready) continue;
        MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
        MMAY_EXPECT(fixture.parsed.diagnostics.length != 0);
        MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
        mmay_parse_fixture_dispose(&fixture);
    }
}
