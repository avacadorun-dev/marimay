#include "test_checker_support.h"

static void mmay_void_match_allocation_case(const char *body, bool valid) {
    MarimayCheckerFixture fixture;
    bool reached_success = false;
    size_t failure_index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, body));
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        status = mmay_check_ast(&fixture.ast, "test/void-match",
            mmay_fail_allocator(&state), mmay_check_default_options(), &checked);
        if (status == MMAY_CHECK_OK) {
            reached_success = true;
            MMAY_EXPECT(checked.initialized);
            MMAY_EXPECT(checked.diagnostics.length == (valid ? 0U : 1U));
            if (valid) {
                state.failure_index = SIZE_MAX;
                for (size_t index = 0; index < checked.functions.length; index += 1) {
                    const MarimayCheckedFunction *function = mmay_checked_function(&checked, index);
                    MMAY_EXPECT(function != NULL);
                    if (function != NULL) MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
                }
            }
            mmay_check_result_dispose(&checked);
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
    mmay_checker_fixture_dispose(&fixture);
}

/* Owner: D074 expected-error handling at standalone match boundaries. */
void mmay_test_checker_expression_statements(void) {
    mmay_void_match_allocation_case(
        "fn action(): void {}\n"
        "type Item { value: int, }\n"
        "fn main(): void {\n"
        " match true { true => action(), false => panic(\"no\"), };\n"
        " match 1 { 1 => action(), _ => action(), };\n"
        " match \"x\" { \"x\" => action(), _ => action(), };\n"
        " const r: Result<int, string> = Ok(1);\n"
        " match r { Ok(_) => action(), Err(_) => action(), };\n"
        " match (1, true) { (_, _) => action(), };\n"
        " match (Item { value: 1 }) { Item { value } => action(), };\n"
        " match false { true => { return; }, false => { action() }, };\n"
        " action();\n"
        "}\n", true);
    mmay_void_match_allocation_case(
        "fn action(): void {}\n"
        "fn main(): void { match true { true => action(), false => 1, }; }\n", false);
    mmay_void_match_allocation_case(
        "fn action(): void {}\n"
        "fn main(): void { match true { true => 1, false => action(), }; }\n", false);
    mmay_void_match_allocation_case(
        "fn action(): void {}\n"
        "fn main(): void { match true { true => { action(); }, false => action(), }; }\n", false);
    mmay_checker_expect_one_diagnostic_path(
        "discard.my",
        "fn value(): Result<int, string> { return Ok(1); }\n"
        "fn main(): void { match true { true => value(), false => value(), }; }\n",
        MMAY_DIAGNOSTIC_IGNORED_RESULT,
        "match true { true => value(), false => value(), }"
    );
    mmay_checker_expect_one_diagnostic_path(
        "mixed.my",
        "fn action(): void {}\n"
        "fn main(): void { match true { true => action(), false => 1, }; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH, "1"
    );
    mmay_checker_expect_one_diagnostic_path(
        "mixed.my",
        "fn action(value: int): void {}\n"
        "fn main(): void { match true { true => 1, false => action(0), }; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH, "action(0)"
    );
    mmay_checker_expect_one_diagnostic_path(
        "tail.my",
        "fn action(): void {}\n"
        "fn main(): void { match true { true => { action(); }, false => action(), }; }\n",
        MMAY_DIAGNOSTIC_MISSING_MATCH_VALUE, "{ action(); }"
    );
}
