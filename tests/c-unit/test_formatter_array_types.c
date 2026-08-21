#include "test_formatter_support.h"

void ruda_test_formatter_array_type_spelling(void) {
    static const char source[] =
        "type Scalar = [ ] int;\n"
        "type Nested = [ ] [ ] int;\n"
        "type Qualified = [ ] pkg.Edge;\n"
        "type Generic = Option < [ ] int >;\n"
        "type TupleElement = [ ] [ int,string ];\n"
        "type FunctionElement = [ ] ( int,string ) => [ ] int;\n"
        "type Commented = [ ] /* element */ [ ] int;\n";
    static const char expected[] =
        "type Scalar = []int;\n\n"
        "type Nested = [][]int;\n\n"
        "type Qualified = []pkg.Edge;\n\n"
        "type Generic = Option<[]int>;\n\n"
        "type TupleElement = [][int, string];\n\n"
        "type FunctionElement = [](int, string) => []int;\n\n"
        "type Commented = [] /* element */[]int;\n";
    RudaFormatterFixture fixture;
    RudaFormatterFixture canonical;
    RudaFormatResult first;
    RudaFormatResult second;
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &fixture,
        "array-types.ruda",
        (const unsigned char *)source,
        strlen(source)
    ));
    if (!fixture.parsed.initialized) {
        return;
    }
    RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
    RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_format_source(
        &fixture.lexed,
        &fixture.parsed,
        ruda_system_allocator(),
        ruda_format_default_options(),
        &first
    ) == RUDA_FORMAT_OK);
    if (first.initialized) {
        RUDA_EXPECT(first.output.length == sizeof(expected) - 1);
        RUDA_EXPECT(first.output.length != sizeof(expected) - 1
            || memcmp(first.output.data, expected, sizeof(expected) - 1) == 0);
        RUDA_EXPECT(ruda_formatter_fixture_init(
            &canonical,
            "array-types-canonical.ruda",
            first.output.data,
            first.output.length
        ));
        if (canonical.parsed.initialized) {
            RUDA_EXPECT(ruda_format_source(
                &canonical.lexed,
                &canonical.parsed,
                ruda_system_allocator(),
                ruda_format_default_options(),
                &second
            ) == RUDA_FORMAT_OK);
            if (second.initialized) {
                RUDA_EXPECT(!second.changed);
                RUDA_EXPECT(second.output.length == first.output.length);
                RUDA_EXPECT(second.output.length != first.output.length
                    || memcmp(
                        second.output.data,
                        first.output.data,
                        first.output.length
                    ) == 0);
                ruda_format_result_dispose(&second);
            }
            ruda_formatter_fixture_dispose(&canonical);
        }
        ruda_format_result_dispose(&first);
    }
    ruda_formatter_fixture_dispose(&fixture);
}
