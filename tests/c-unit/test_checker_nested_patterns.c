#include "test_checker_support.h"

void mmay_test_checker_nested_patterns(void) {
    static const char source[] =
        "type Leaf { value: int, }\n"
        "type Wrap { leaf: Leaf, }\n"
        "enum Inner { Some(int), None, }\n"
        "enum Outer { Box(Inner), Empty, }\n"
        "fn nested(value: Wrap): int {\n"
        "  return match value { Wrap { leaf: Leaf { value } } => value, };\n"
        "}\n";
    static const char enum_source[] =
        "enum Inner { Some(int), None, }\n"
        "enum Outer { Box(Inner), Empty, }\n"
        "fn nested(value: Outer): int {\n"
        "  return match value {\n"
        "    Box(Some(item)) if item > 0 => item,\n"
        "    Box(Some(_)) => 0,\n"
        "    Box(_) => 0,\n"
        "    Empty => 0,\n"
        "  };\n"
        "}\n";
    static const char void_payload_source[] =
        "enum Failure { Bad, }\n"
        "fn classify(value: Result<void, Failure>): int {\n"
        "  return match value { Ok(_) => 1, Err(_) => 2, };\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *nested = NULL;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/nested-patterns", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        nested = mmay_checked_function(&checked, 0);
        MMAY_EXPECT(nested != NULL && mmay_test_mir_instruction_kind_count(
            &nested->mir, MMAY_MIR_RECORD_GET
        ) == 2);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, enum_source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/nested-enum-patterns", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        nested = mmay_checked_function(&checked, 0);
        MMAY_EXPECT(nested != NULL && mmay_test_mir_instruction_kind_count(
            &nested->mir, MMAY_MIR_ENUM_IS_VARIANT
        ) == 5);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, void_payload_source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/void-payload-pattern", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        nested = mmay_checked_function(&checked, 0);
        MMAY_EXPECT(nested != NULL && mmay_mir_verify(&nested->mir)
            == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(nested != NULL && mmay_test_mir_instruction_kind_count(
            &nested->mir, MMAY_MIR_ENUM_GET_PAYLOAD
        ) == 1);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}
