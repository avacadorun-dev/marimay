#include "test_parser_support.h"

void ruda_test_parser_nested_generic_boundaries(void) {
    static const char source[] =
        "type First = Result<Choice<[]int>, Problem>;\n"
        "type Second = Result<Option<[]int>, Problem>;\n"
        "type Third = Outer<Middle<Inner<int>>, Sibling<string>>;\n"
        "fn nested(value: Result<Choice<[]int>, Problem>): "
        "Result<Option<[]int>, Problem> { return value; }\n";
    RudaParseFixture fixture;
    bool initialized = ruda_parse_fixture_init(&fixture, source);
    RUDA_EXPECT(initialized);
    if (!initialized) {
        return;
    }
    RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
    RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
    RUDA_EXPECT(ruda_syntax_count_kind(
        fixture.parsed.tree.root,
        RUDA_SYNTAX_TYPE_ARGUMENTS
    ) == 12);
    ruda_parse_fixture_dispose(&fixture);
}
