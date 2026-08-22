#include "test_parser_support.h"

void mmay_test_parser_nested_generic_boundaries(void) {
    static const char source[] =
        "type First = Result<Choice<[]int>, Problem>;\n"
        "type Second = Result<Option<[]int>, Problem>;\n"
        "type Third = Outer<Middle<Inner<int>>, Sibling<string>>;\n"
        "fn nested(value: Result<Choice<[]int>, Problem>): "
        "Result<Option<[]int>, Problem> { return value; }\n";
    MarimayParseFixture fixture;
    bool initialized = mmay_parse_fixture_init(&fixture, source);
    MMAY_EXPECT(initialized);
    if (!initialized) {
        return;
    }
    MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
    MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_syntax_tree_is_lossless(&fixture.parsed.tree));
    MMAY_EXPECT(mmay_syntax_count_kind(
        fixture.parsed.tree.root,
        MMAY_SYNTAX_TYPE_ARGUMENTS
    ) == 12);
    mmay_parse_fixture_dispose(&fixture);
}
