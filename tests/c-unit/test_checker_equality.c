#include "test_checker_support.h"

void mmay_test_checker_equality(void) {
    static const char source[] =
        "enum Theme { Work, Play, Named(string), }\n"
        "@derive(Eq) type Pair { left: int, right: string, }\n"
        "fn noValue(): Option<int> { return None; }\n"
        "fn strings(): bool {\n"
        "  return \"a\\0b\" == \"a\\0b\" && \"é\" != \"e\";\n"
        "}\n"
        "fn enums(): bool {\n"
        "  return Theme.Work == Theme.Work\n"
        "    && Theme.Work != Theme.Play\n"
        "    && Theme.Named(\"x\") == Theme.Named(\"x\")\n"
        "    && Theme.Named(\"x\") != Theme.Named(\"y\");\n"
        "}\n"
        "fn options(): bool {\n"
        "  return Some(1) == Some(1) && Some(1) != Some(2)\n"
        "    && noValue() == noValue() && Some(1) != noValue();\n"
        "}\n"
        "fn records(): bool {\n"
        "  return Pair { left: 1, right: \"x\" }\n"
        "      == Pair { left: 1, right: \"x\" }\n"
        "    && Pair { left: 1, right: \"x\" }\n"
        "      != Pair { left: 2, right: \"x\" };\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    size_t index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/equality", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 5);
        for (index = 0; index < checked.functions.length; index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked, index
            );
            MarimayMirValue result;
            if (function == NULL || function->mir.result_type != MMAY_TYPE_BOOL) {
                continue;
            }
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, index, NULL, 0, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_BOOL);
            MMAY_EXPECT(result.as.boolean);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        "type Pair { value: int, }\n"
        "fn bad(left: Pair, right: Pair): bool { return left == right; }\n",
        MMAY_DIAGNOSTIC_INVALID_OPERATOR, "left == right", 0
    );
    mmay_checker_expect_one_diagnostic(
        "@derive(Hash) type Pair { value: int, }\n",
        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "@derive(Hash) type Pair { value: int, }", 0
    );
}
