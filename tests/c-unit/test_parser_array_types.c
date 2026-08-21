#include "test_parser_support.h"

void ruda_test_parser_array_type_spelling(void) {
    static const char valid[] =
        "type Scalar = []int;\n"
        "type Nested = [][]int;\n"
        "type Qualified = []pkg.Edge;\n"
        "type Generic = Option<[]int>;\n"
        "type ResultRows = Result<[][]pkg.Edge, Problem>;\n"
        "type TupleElement = [][int, string];\n"
        "type FunctionElement = [](int, string) => bool;\n"
        "type GenericElement = []Map<string, int>;\n";
    static const char *const invalid[] = {
        "type Missing = []; fn after(): void {}",
        "type MissingNested = [][]; fn after(): void {}",
        "type MissingTuple = [][; fn after(): void {}",
        "type MissingGeneric = []Map<string,; fn after(): void {}"
    };
    RudaParseFixture fixture;
    size_t index = 0;
    RUDA_EXPECT(ruda_parse_fixture_init(&fixture, valid));
    if (fixture.parsed.initialized) {
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_TYPE
        ) >= 28);
        ruda_parse_fixture_dispose(&fixture);
    }
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        RudaParseFixture repeated;
        size_t diagnostic_index = 0;
        RUDA_EXPECT(ruda_parse_fixture_init(&fixture, invalid[index]));
        if (!fixture.parsed.initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length != 0);
        RUDA_EXPECT(ruda_syntax_tree_is_lossless(&fixture.parsed.tree));
        RUDA_EXPECT(ruda_syntax_count_kind(
            fixture.parsed.tree.root,
            RUDA_SYNTAX_FUNCTION_DECL
        ) == 1);
        RUDA_EXPECT(ruda_parse_fixture_init(&repeated, invalid[index]));
        if (repeated.parsed.initialized) {
            RUDA_EXPECT(repeated.parsed.diagnostics.length
                == fixture.parsed.diagnostics.length);
            for (diagnostic_index = 0;
                diagnostic_index < fixture.parsed.diagnostics.length;
                diagnostic_index += 1) {
                const RudaParseDiagnostic *first = ruda_parse_diagnostic(
                    &fixture.parsed, diagnostic_index
                );
                const RudaParseDiagnostic *second = ruda_parse_diagnostic(
                    &repeated.parsed, diagnostic_index
                );
                RUDA_EXPECT(first != NULL && second != NULL);
                if (first != NULL && second != NULL) {
                    RUDA_EXPECT(strcmp(first->code, second->code) == 0);
                    RUDA_EXPECT(strcmp(first->message, second->message) == 0);
                    RUDA_EXPECT(first->start == second->start);
                    RUDA_EXPECT(first->end == second->end);
                }
            }
            ruda_parse_fixture_dispose(&repeated);
        }
        ruda_parse_fixture_dispose(&fixture);
    }
}
