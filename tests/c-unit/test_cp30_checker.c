#include "test_checker_imports_support.h"

static const char mmay_test_cp30_checker_source[] =
    "fn render(a: i8, b: i64, c: u64, ready: bool, number: f64, text: string): string {\n"
    "  return $\"{a}|{b}|{c}|{ready}|{number}|{text}|{$\"nested {ready}\"}\";\n"
    "}\n"
    "fn empty(): string { return $\"\"; }\n";

static const char mmay_test_cp30_convert_source[] =
    "public enum ParseError {\n"
    "  Empty, InvalidDigit { offset: int; }, OutOfRange, InvalidRadix,\n"
    "}\n"
    "public extern \"c\" fn parseInt(text: string): Result<int, ParseError>;\n"
    "public extern \"c\" fn parseIntRadix(text: string, radix: int): Result<int, ParseError>;\n"
    "public extern \"c\" fn formatI8(value: i8): string;\n";

static const char mmay_test_cp30_package_source[] =
    "import { formatI8 as tiny, parseInt as decimalInt } from \"std/convert\";\n"
    "import * as convert from \"std/convert\";\n"
    "fn render(value: i8): string { return $\"{tiny(value)}|{value}\"; }\n"
    "fn decimal(text: string): Result<int, convert.ParseError> {\n"
    "  return decimalInt(text);\n"
    "}\n"
    "fn radix(text: string): Result<int, convert.ParseError> {\n"
    "  return convert.parseIntRadix(text, 16);\n"
    "}\n";

static bool mmay_test_cp30_package_fixture(
    MarimayCheckerImportFixture *fixture
) {
    const MarimayTestImportSource sources[] = {
        {"cp30/app", "app/main.my", mmay_test_cp30_package_source},
        {"std/convert", "std/convert/convert.my", mmay_test_cp30_convert_source}
    };
    return mmay_checker_import_fixture_init(
        fixture, sources, sizeof(sources) / sizeof(sources[0])
    );
}

static void mmay_test_cp30_checker_lowering(void) {
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *render;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture, mmay_test_cp30_checker_source
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/cp30-checker", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized && checked.diagnostics.length == 0);
    render = mmay_checked_function(&checked, 0);
    MMAY_EXPECT(render != NULL);
    if (render != NULL) {
        MMAY_EXPECT(mmay_mir_verify(&render->mir) == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &render->mir, MMAY_MIR_FORMAT_INTEGER
        ) == 3);
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &render->mir, MMAY_MIR_FORMAT_BOOL
        ) == 2);
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &render->mir, MMAY_MIR_FORMAT_F64
        ) == 1);
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &render->mir, MMAY_MIR_STRING_CONCAT
        ) >= 6);
    }
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_cp30_checker_package_lowering(void) {
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *render;
    const MarimayCheckedFunction *decimal;
    const MarimayCheckedFunction *radix;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_test_cp30_package_fixture(&fixture));
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "cp30/app", mmay_system_allocator(),
        mmay_check_graph_default_options(), &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized && checked.diagnostics.length == 0);
    render = mmay_checker_import_function(&checked, "render", NULL);
    decimal = mmay_checker_import_function(&checked, "decimal", NULL);
    radix = mmay_checker_import_function(&checked, "radix", NULL);
    MMAY_EXPECT(render != NULL && decimal != NULL && radix != NULL);
    if (render != NULL) {
        MMAY_EXPECT(mmay_mir_verify(&render->mir) == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &render->mir, MMAY_MIR_FORMAT_INTEGER
        ) == 2);
    }
    if (decimal != NULL) {
        MMAY_EXPECT(mmay_mir_verify(&decimal->mir) == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &decimal->mir, MMAY_MIR_PARSE_INTEGER
        ) == 1);
    }
    if (radix != NULL) {
        MMAY_EXPECT(mmay_mir_verify(&radix->mir) == MMAY_MIR_VERIFY_OK);
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &radix->mir, MMAY_MIR_PARSE_INTEGER
        ) == 1);
    }
    if (checked.initialized) mmay_check_result_dispose(&checked);
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_test_cp30_checker_import_rejection(
    const char *source,
    const char *code
) {
    const MarimayTestImportSource sources[] = {
        {"cp30/app", "app/main.my", source},
        {"std/convert", "std/convert/convert.my", mmay_test_cp30_convert_source}
    };
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckDiagnostic *diagnostic;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_import_fixture_init(
        &fixture, sources, sizeof(sources) / sizeof(sources[0])
    ));
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "cp30/app", mmay_system_allocator(),
        mmay_check_graph_default_options(), &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized && checked.diagnostics.length == 1);
    diagnostic = mmay_check_diagnostic(&checked, 0);
    MMAY_EXPECT(diagnostic != NULL);
    if (diagnostic != NULL) {
        MMAY_EXPECT(strcmp(diagnostic->code, code) == 0);
    }
    if (checked.initialized) mmay_check_result_dispose(&checked);
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_test_cp30_checker_rejections(void) {
    mmay_checker_expect_one_diagnostic(
        "fn bad(values: []int): string { return $\"{values}\"; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "values",
        1
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: f32): string { return $\"{value}\"; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "value",
        1
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: int): int { return $\"{value}\"; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "$\"{value}\"",
        0
    );
    mmay_test_cp30_checker_import_rejection(
        "import { formatI8 as tiny } from \"std/convert\";\n"
        "fn bad(): string { return tiny(true); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH
    );
    mmay_test_cp30_checker_import_rejection(
        "import { formatI8 as tiny } from \"std/convert\";\n"
        "fn bad(): string { return tiny(); }\n",
        MMAY_DIAGNOSTIC_CALL_ARITY
    );
    mmay_test_cp30_checker_import_rejection(
        "import { ParseError, parseInt } from \"std/convert\";\n"
        "fn bad(): Result<int, ParseError> { return parseInt(); }\n",
        MMAY_DIAGNOSTIC_CALL_ARITY
    );
    mmay_test_cp30_checker_import_rejection(
        "import { ParseError, parseInt } from \"std/convert\";\n"
        "fn bad(): Result<int, ParseError> { return parseInt(1); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH
    );
    mmay_test_cp30_checker_import_rejection(
        "import { ParseError } from \"std/convert\";\n"
        "import * as convert from \"std/convert\";\n"
        "fn bad(): Result<int, ParseError> {\n"
        "  return convert.parseIntRadix(\"1\", true);\n"
        "}\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH
    );
    mmay_test_cp30_checker_import_rejection(
        "import { parseInt } from \"std/convert\";\n"
        "fn bad(): int { return parseInt(\"1\"); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH
    );
}

static void mmay_test_cp30_checker_allocation_failures(void) {
    MarimayCheckerImportFixture fixture;
    size_t failure_index;
    bool reached_success = false;
    memset(&fixture, 0, sizeof(fixture));
    MMAY_EXPECT(mmay_test_cp30_package_fixture(&fixture));
    for (failure_index = 0; failure_index < 4096; failure_index += 1U) {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        bool success_clean;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_checker_import_fixture_check(
            &fixture, "cp30/app", mmay_fail_allocator(&state),
            mmay_check_graph_default_options(), &checked
        );
        MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED
            || status == MMAY_CHECK_OK);
        success_clean = status == MMAY_CHECK_OK
            && checked.initialized && checked.diagnostics.length == 0;
        if (status == MMAY_CHECK_OK) {
            MMAY_EXPECT(checked.initialized);
            MMAY_EXPECT(checked.diagnostics.length == 0);
        }
        if (checked.initialized) mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (success_clean) {
            reached_success = true;
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_checker_import_fixture_dispose(&fixture);
}

void mmay_test_cp30_checker(void) {
    mmay_test_cp30_checker_lowering();
    mmay_test_cp30_checker_package_lowering();
    mmay_test_cp30_checker_rejections();
    mmay_test_cp30_checker_allocation_failures();
}
