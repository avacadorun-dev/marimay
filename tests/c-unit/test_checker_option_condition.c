#include "test_checker_support.h"

static void mmay_option_check_failures(const char *source, bool valid) {
    MarimayCheckerFixture fixture;
    size_t failure = 0;
    bool completed = false;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    for (failure = 0; failure < 4096; failure += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        status = mmay_check_ast(&fixture.ast, "test/option-condition",
            mmay_fail_allocator(&state), mmay_check_default_options(), &checked);
        if (status == MMAY_CHECK_OK) {
            completed = true;
            MMAY_EXPECT(checked.initialized);
            MMAY_EXPECT(checked.diagnostics.length == (valid ? 0U : 1U));
            state.failure_index = SIZE_MAX;
            for (size_t index = 0; valid && index < checked.functions.length; index += 1) {
                const MarimayCheckedFunction *function = mmay_checked_function(&checked, index);
                MMAY_EXPECT(function != NULL);
                if (function != NULL) MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
            }
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (completed) break;
    }
    MMAY_EXPECT(completed);
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_option_binding_limit(void) {
    MarimayCheckerFixture fixture;
    MarimayCheckOptions options = mmay_check_default_options();
    bool completed = false;
    options.max_locals_per_function = 1;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture,
        "fn main(): void { const occupied = 0; if const item = Some(1) {} }\n"));
    for (size_t failure = 0; failure < 4096; failure += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        status = mmay_check_ast(&fixture.ast, "test/option-binding-limit",
            mmay_fail_allocator(&state), options, &checked);
        completed = status == MMAY_CHECK_LIMIT_REACHED;
        MMAY_EXPECT(completed || status == MMAY_CHECK_ALLOCATION_FAILED);
        MMAY_EXPECT(!checked.initialized);
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (completed) break;
    }
    MMAY_EXPECT(completed);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_option_condition(void) {
    mmay_option_binding_limit();
    mmay_option_check_failures(
        "fn cleanup(value: int): void {}\n"
        "fn value(input: Option<int>): int {\n"
        " let result = 0; if const item = input { defer cleanup(item); result = item; }\n"
        " else if let item = Some(3) { item += 1; result = item; }\n"
        " const values = [1, 2, 3];\n"
        " while const item = values.pop() { defer cleanup(item);\n"
        "   if item == 3 { continue; } if item == 1 { break; } result += item;\n"
        " } return result;\n"
        "}\n", true);
    mmay_option_check_failures(
        "fn invalid(): void { if const item = true {} }\n", false);
    mmay_option_check_failures(
        "fn invalid(): void { if const item = Some(1) { const item = 2; } }\n", false);
    mmay_option_check_failures(
        "fn invalid(): void { if let item = Some(1) { item = false; } }\n", false);
    mmay_checker_expect_one_diagnostic(
        "fn invalid(value: Result<int, string>): void { if const item = value {} }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH, "value", 1);
    mmay_checker_expect_one_diagnostic(
        "enum Maybe<T> { Some(T), None, }\n"
        "fn invalid(value: Maybe<int>): void { while const item = value {} }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH, "value", 1);
    mmay_option_check_failures(
        "fn invalid(): void { if const item = Some(1) { item = 2; } }\n",
        false);
    mmay_option_check_failures(
        "fn invalid(): int { if const item = Some(1) {} return item; }\n", false);
    mmay_option_check_failures(
        "fn invalid(): void { if const item = Some(1) {} else { const x = item; } }\n", false);
    mmay_option_check_failures(
        "fn invalid(): int { let result: int; if const item = Some(1) { result = item; } return result; }\n", false);
    mmay_option_check_failures(
        "fn invalid(input: Option<int>): int { let result: int; while const item = input { result = item; break; } return result; }\n", false);
    mmay_option_check_failures(
        "fn invalid(): void { if const item = item {} }\n", false);
    mmay_option_check_failures(
        "fn invalid(input: Option<void>): void { if const item = input {} }\n", false);
    mmay_option_check_failures(
        "fn stops(): never { if const item = panic(\"stop\") {} }\n", true);
}
