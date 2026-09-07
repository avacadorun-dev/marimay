#include "test_checker_support.h"

static const char mmay_mapper_context_source[] =
    "enum Input { Bad(int), }\n"
    "enum Other { Bad(bool), }\n"
    "enum Wrapped<T> { Value(T), }\n"
    "fn message(error: Input): string { return \"mapped\"; }\n"
    "fn message(error: Other): string { return \"other\"; }\n"
    "fn read(bad: bool): Result<int, Input> {\n"
    " if bad { return Err(Input.Bad(9)); } return Ok(7);\n"
    "}\n"
    "fn named(bad: bool): Result<bool, string> {\n"
    " const value = read(bad).mapError(message)?; return Ok(value == 7);\n"
    "}\n"
    "fn closure(bad: bool): Result<bool, string> {\n"
    " const value = ((read(bad).mapError((error) => message(error))))?; return Ok(value == 7);\n"
    "}\n"
    "fn constructed(bad: bool): Result<bool, Wrapped<Input>> {\n"
    " const value = read(bad).mapError(Wrapped.Value)?; return Ok(value == 7);\n"
    "}\n"
    "fn wrappedRead(bad: bool): Result<Result<int, Input>, string> { return Ok(read(bad)); }\n"
    "fn nested(bad: bool): Result<bool, string> {\n"
    " const value = (wrappedRead(bad)?).mapError((error) => message(error))?; return Ok(value == 7);\n"
    "}\n"
    "@test fn paths(): void {\n"
    " expect.equal(named(false), Ok(true)); expect.equal(named(true), Err(\"mapped\"));\n"
    " expect.equal(closure(false), Ok(true)); expect.equal(closure(true), Err(\"mapped\"));\n"
    " expect.equal(constructed(false), Ok(true));\n"
    " expect.equal(constructed(true), Err(Wrapped.Value(Input.Bad(9))));\n"
    " expect.equal(nested(false), Ok(true)); expect.equal(nested(true), Err(\"mapped\"));\n"
    "}\n";

static void mmay_mapper_context_oracle(const MarimayCheckResult *checked,
    MarimayAllocator allocator) {
    MarimayMirExecution execution = {0};
    MarimayCheckInterpretOptions options = mmay_check_interpret_default_options();
    MarimayMirValue result = {0};
    size_t index, tests = 0;
    MMAY_EXPECT(mmay_mir_execution_init(&execution, allocator,
        mmay_mir_execution_default_options()));
    options.execution = &execution;
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(checked, index);
        MMAY_EXPECT(function != NULL);
        if (function == NULL) continue;
        MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
        if (function->is_test) {
            tests += 1;
            MMAY_EXPECT(mmay_check_interpret_function(checked, index, NULL, 0,
                allocator, options, &result) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_VOID);
        }
    }
    MMAY_EXPECT(tests == 1);
    mmay_mir_execution_dispose(&execution);
}

static void mmay_mapper_context_allocations(const char *source, bool valid) {
    MarimayCheckerFixture fixture;
    size_t failure;
    bool complete = false;
    MMAY_EXPECT(mmay_checker_fixture_init_path(&fixture, "mapper-context_test.my", source));
    for (failure = 0; failure < 16384; failure += 1) {
        MarimayFailAllocator tracked;
        MarimayCheckResult checked = {0};
        MarimayCheckStatus status;
        mmay_fail_allocator_init(&tracked, mmay_system_allocator(), failure);
        status = mmay_check_ast(&fixture.ast, "test/mapper-context",
            mmay_fail_allocator(&tracked), mmay_check_default_options(), &checked);
        if (status == MMAY_CHECK_OK) {
            MMAY_EXPECT(checked.initialized);
            MMAY_EXPECT(checked.diagnostics.length == (valid ? 0U : 1U));
            complete = failure >= tracked.attempts;
            if (complete) {
                MMAY_EXPECT(failure == tracked.attempts);
                tracked.failure_index = SIZE_MAX;
                if (valid && checked.diagnostics.length == 0) {
                    mmay_mapper_context_oracle(&checked, mmay_fail_allocator(&tracked));
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

void mmay_test_checker_map_error_context(void) {
    static const struct { const char *source; const char *span; } negative[] = {
        {"enum Pretend<T,E> { Good(T), Bad(E), }\n"
         "fn describe(value: int): string { return \"mapped\"; }\n"
         "fn bad(value: Pretend<int,int>): Pretend<int,string> { return value.mapError(describe); }",
         "value"},
        {"enum Pretend<T,E> { Good(T), Bad(E), }\n"
         "fn bad(value: Pretend<int,string>): Pretend<int,string> { const count = value?; return value; }",
         "value?"},
        {"fn describe(value: int): string { return \"mapped\"; }\n"
         "fn bad(value: Result<int,int>): Result<int,string> { const describe = 0; const count = value.mapError(describe)?; return Ok(count); }",
         "describe"},
        {"fn bad(value: Result<int,int>): void { const mapped = value.mapError((error) => \"mapped\"); }",
         "(error) => \"mapped\""},
        {"fn bad(value: Result<int,int>): Result<int,string> {\n"
         " const count = value.mapError((error) => \"mapped\").mapError((error) => error)?; return Ok(count); }",
         "(error) => \"mapped\""},
        {"fn bad(value: Result<int,int>): void { value.mapError((error) => \"mapped\")?; }",
         "(error) => \"mapped\""},
        {"fn bad(value: Result<int,int>): Result<int,string> {\n"
         " const count = value.mapError((error): int => error)?; return Ok(count); }",
         "value.mapError((error): int => error)?"}
    };
    size_t index;
    mmay_mapper_context_allocations(mmay_mapper_context_source, true);
    for (index = 0; index < sizeof(negative) / sizeof(negative[0]); index += 1) {
        mmay_checker_expect_one_diagnostic(negative[index].source,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH, negative[index].span,
            index == 0 || index == 2 ? 2U : 0U);
        mmay_mapper_context_allocations(negative[index].source, false);
    }
}
