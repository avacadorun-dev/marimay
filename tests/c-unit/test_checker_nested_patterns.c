#include "test_checker_support.h"

void ruda_test_checker_nested_patterns(void) {
    static const char source[] =
        "type Leaf { value: int; }\n"
        "type Wrap { leaf: Leaf; }\n"
        "enum Inner { Some(int), None, }\n"
        "enum Outer { Box(Inner), Empty, }\n"
        "fn nested(value: Wrap): int {\n"
        "  return match value { case Wrap { leaf: Leaf { value } } => value; };\n"
        "}\n";
    static const char enum_source[] =
        "enum Inner { Some(int), None, }\n"
        "enum Outer { Box(Inner), Empty, }\n"
        "fn nested(value: Outer): int {\n"
        "  return match value {\n"
        "    case Box(Some(item)) if item > 0 => item;\n"
        "    case Box(Some(_)) => 0;\n"
        "    case Box(_) => 0;\n"
        "    case Empty => 0;\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *nested = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/nested-patterns", ruda_system_allocator(),
        ruda_check_default_options(), &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        nested = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(nested != NULL && ruda_test_mir_instruction_kind_count(
            &nested->mir, RUDA_MIR_RECORD_GET
        ) == 2);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, enum_source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/nested-enum-patterns", ruda_system_allocator(),
        ruda_check_default_options(), &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        nested = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(nested != NULL && ruda_test_mir_instruction_kind_count(
            &nested->mir, RUDA_MIR_ENUM_IS_VARIANT
        ) == 5);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}
