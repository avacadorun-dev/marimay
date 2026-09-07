#include "test_checker_support.h"

/* D092 owns implicit void arms and their lexical cleanup boundary. The
 * existing D074 owner continues to cover Result discard across expressions. */
static const char mmay_void_arm_program[] =
    "enum Choice { Empty, Value(int), }\n"
    "type Item { value: int, }\n"
    "fn append(log: []int, value: int): void { const _ = log.push(value); }\n"
    "fn kinds(): int {\n"
    " const log: []int = [];\n"
    " match true { true => { append(log, 1); }, false => {}, };\n"
    " match 2 { 2 => { append(log, 2); }, _ => {}, };\n"
    " match \"x\" { \"x\" => { append(log, 3); }, _ => {}, };\n"
    " match Choice.Value(4) { Value(value) => { append(log, value); }, Empty => {}, };\n"
    " match (5, true) { (value, flag) => { if flag { append(log, value); } }, };\n"
    " match (Item { value: 6 }) { Item { value } => { append(log, value); }, };\n"
    " let code = 0; for value in log { code = code * 10 + value; } return code;\n"
    "}\n"
    "fn cleanup(flag: bool): int {\n"
    " const log: []int = [];\n"
    " match flag {\n"
    "  true => { defer append(log, 1); append(log, 2) },\n"
    "  false => { defer append(log, 3); append(log, 4); },\n"
    " };\n"
    " let code = 0; for value in log { code = code * 10 + value; } return code;\n"
    "}\n"
    "fn valueTail(): int {\n"
    " const log: []int = [];\n"
    " const result = match true {\n"
    "  true => { defer append(log, 1); defer append(log, 2); [7, 8] },\n"
    "  false => { defer append(log, 3); [9] },\n"
    " };\n"
    " return result[0] * 100 + log[0] * 10 + log[1];\n"
    "}\n";

static void mmay_void_arm_oracle(const MarimayCheckResult *checked,
    MarimayAllocator allocator) {
    MarimayMirExecution execution;
    MarimayCheckInterpretOptions options = mmay_check_interpret_default_options();
    MarimayMirValue result, argument;
    size_t index;
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(checked, index);
        MMAY_EXPECT(function != NULL);
        if (function != NULL) MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
    }
    MMAY_EXPECT(mmay_mir_execution_init(&execution, allocator,
        mmay_mir_execution_default_options()));
    options.execution = &execution;
    MMAY_EXPECT(mmay_check_interpret_function(checked, 1, NULL, 0, allocator,
        options, &result) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 123456);
    argument.type = MMAY_TYPE_BOOL;
    argument.as.boolean = true;
    MMAY_EXPECT(mmay_check_interpret_function(checked, 2, &argument, 1, allocator,
        options, &result) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 21);
    argument.as.boolean = false;
    MMAY_EXPECT(mmay_check_interpret_function(checked, 2, &argument, 1, allocator,
        options, &result) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 43);
    MMAY_EXPECT(mmay_check_interpret_function(checked, 3, NULL, 0, allocator,
        options, &result) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 721);
    mmay_mir_execution_dispose(&execution);
}

static void mmay_void_arm_allocations(const char *source, bool valid) {
    MarimayCheckerFixture fixture;
    size_t failure;
    bool complete = false;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    for (failure = 0; failure < 8192; failure += 1) {
        MarimayFailAllocator tracked;
        MarimayCheckResult checked = {0};
        MarimayCheckStatus status;
        size_t attempts;
        mmay_fail_allocator_init(&tracked, mmay_system_allocator(), failure);
        status = mmay_check_ast(&fixture.ast, "test/void-arms",
            mmay_fail_allocator(&tracked), mmay_check_default_options(), &checked);
        attempts = tracked.attempts;
        if (status == MMAY_CHECK_OK) {
            MMAY_EXPECT(checked.initialized);
            MMAY_EXPECT(checked.diagnostics.length == (valid ? 0U : 1U));
            complete = failure >= attempts;
            if (complete) {
                MMAY_EXPECT(failure == attempts);
                tracked.failure_index = SIZE_MAX;
                if (valid && checked.diagnostics.length == 0) {
                    mmay_void_arm_oracle(&checked, mmay_fail_allocator(&tracked));
                }
            }
            mmay_check_result_dispose(&checked);
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED && !checked.initialized);
        }
        MMAY_EXPECT(tracked.live_allocations == 0 && tracked.live_bytes == 0);
        if (complete) break;
    }
    MMAY_EXPECT(complete);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_void_match(void) {
    static const struct {
        const char *source;
        const char *code;
        const char *span;
    } negative[] = {
        {"fn bad(): int { return match true { true => {}, false => 1, }; }",
            MMAY_DIAGNOSTIC_MISSING_MATCH_VALUE, "{}"},
        {"fn bad(): void { match true { true => {}, false => 1, }; }",
            MMAY_DIAGNOSTIC_TYPE_MISMATCH, "1"},
        {"fn call(): Result<int, string> { return Ok(1); }\n"
         "fn bad(): void { match true { true => { call(); }, false => {}, }; }",
            MMAY_DIAGNOSTIC_IGNORED_RESULT, "call()"},
        {"fn bad(): void { const value = match true { true => {}, false => {}, }; }",
            MMAY_DIAGNOSTIC_TYPE_MISMATCH, "match true { true => {}, false => {}, }"},
        {"fn action(): void {}\nfn bad(): void { const value = action(); }",
            MMAY_DIAGNOSTIC_TYPE_MISMATCH, "action()"},
        {"fn bad(): void { let value: void; }",
            MMAY_DIAGNOSTIC_TYPE_MISMATCH, "void"}
    };
    size_t index;
    mmay_void_arm_allocations(mmay_void_arm_program, true);
    for (index = 0; index < sizeof(negative) / sizeof(negative[0]); index += 1) {
        mmay_checker_expect_one_diagnostic(negative[index].source,
            negative[index].code, negative[index].span,
            index == 2 || index == 4 || index == 5 ? 1U : 0U);
        mmay_void_arm_allocations(negative[index].source, false);
    }
}
