#include "test_formatter_support.h"

void mmay_test_formatter_array_type_spelling(void) {
    static const char source[] =
        "type Scalar = [ ] int;\n"
        "type Nested = [ ] [ ] int;\n"
        "type Qualified = [ ] pkg.Edge;\n"
        "type Generic = Option < [ ] int >;\n"
        "type TupleElement = [ ] ( int,string );\n"
        "type FunctionElement = [ ] ( int,string ) => [ ] int;\n"
        "type Commented = [ ] /* element */ [ ] int;\n";
    static const char expected[] =
        "type Scalar = []int;\n\n"
        "type Nested = [][]int;\n\n"
        "type Qualified = []pkg.Edge;\n\n"
        "type Generic = Option<[]int>;\n\n"
        "type TupleElement = [](int, string);\n\n"
        "type FunctionElement = [](int, string) => []int;\n\n"
        "type Commented = [] /* element */[]int;\n";
    MarimayFormatterFixture fixture;
    MarimayFormatterFixture canonical;
    MarimayFormatResult first;
    MarimayFormatResult second;
    MMAY_EXPECT(mmay_formatter_fixture_init(
        &fixture,
        "array-types.my",
        (const unsigned char *)source,
        strlen(source)
    ));
    if (!fixture.parsed.initialized) {
        return;
    }
    MMAY_EXPECT(fixture.lexed.diagnostics.length == 0);
    MMAY_EXPECT(fixture.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_format_source(
        &fixture.lexed,
        &fixture.parsed,
        mmay_system_allocator(),
        mmay_format_default_options(),
        &first
    ) == MMAY_FORMAT_OK);
    if (first.initialized) {
        MMAY_EXPECT(first.output.length == sizeof(expected) - 1);
        MMAY_EXPECT(first.output.length != sizeof(expected) - 1
            || memcmp(first.output.data, expected, sizeof(expected) - 1) == 0);
        MMAY_EXPECT(mmay_formatter_fixture_init(
            &canonical,
            "array-types-canonical.my",
            first.output.data,
            first.output.length
        ));
        if (canonical.parsed.initialized) {
            MMAY_EXPECT(mmay_format_source(
                &canonical.lexed,
                &canonical.parsed,
                mmay_system_allocator(),
                mmay_format_default_options(),
                &second
            ) == MMAY_FORMAT_OK);
            if (second.initialized) {
                MMAY_EXPECT(!second.changed);
                MMAY_EXPECT(second.output.length == first.output.length);
                MMAY_EXPECT(second.output.length != first.output.length
                    || memcmp(
                        second.output.data,
                        first.output.data,
                        first.output.length
                    ) == 0);
                mmay_format_result_dispose(&second);
            }
            mmay_formatter_fixture_dispose(&canonical);
        }
        mmay_format_result_dispose(&first);
    }
    mmay_formatter_fixture_dispose(&fixture);
}
