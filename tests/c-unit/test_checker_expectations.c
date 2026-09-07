#include "test_checker_support.h"

static void mmay_test_checker_expectations_execute(void) {
    static const char source[] =
        "@test fn passes(): void {\n"
        "  expect.equal(1, 1);\n"
        "  expect.notEqual(1, 2);\n"
        "  expect.equal(\"a\\0b\", \"a\\0b\");\n"
        "  expect.true(true);\n"
        "  expect.false(false);\n"
        "  const some: Option<int> = Some(1);\n"
        "  const none: Option<int> = None;\n"
        "  expect.some(some);\n"
        "  expect.none(none);\n"
        "  const ok: Result<int, string> = Ok(1);\n"
        "  const err: Result<int, string> = Err(\"bad\");\n"
        "  expect.ok(ok);\n"
        "  expect.err(err);\n"
        "}\n"
        "@test fn fails(): void { expect.true(false); }\n";
    static const unsigned char failure_message[] =
        "expect.true failed: value is false";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckInterpretOptions options =
        mmay_check_interpret_default_options();
    MarimayMirPanic panic;
    MarimayMirValue result;
    const MarimayCheckedFunction *passes = NULL;
    const MarimayCheckedFunction *fails = NULL;
    const char *failure_call = strstr(source, "expect.true(false)");
    memset(&checked, 0, sizeof(checked));
    memset(&panic, 0, sizeof(panic));
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_checker_fixture_init_path(
        &fixture, "expectations_test.my", source
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/expectations", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 2);
        passes = mmay_checked_function(&checked, 0);
        fails = mmay_checked_function(&checked, 1);
        MMAY_EXPECT(passes != NULL && passes->is_test);
        MMAY_EXPECT(fails != NULL && fails->is_test);
        if (passes != NULL) {
            MMAY_EXPECT(mmay_mir_verify(&passes->mir) == MMAY_MIR_VERIFY_OK);
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                &passes->mir, MMAY_MIR_TEST_ASSERT
            ) == 9);
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, 0, NULL, 0, mmay_system_allocator(),
                options, &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_VOID);
        }
        options.panic = &panic;
        if (fails != NULL) {
            MMAY_EXPECT(mmay_mir_verify(&fails->mir) == MMAY_MIR_VERIFY_OK);
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, 1, NULL, 0, mmay_system_allocator(),
                options, &result
            ) == MMAY_MIR_INTERPRET_PANIC);
            MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4004") == 0);
            MMAY_EXPECT(panic.message.length == sizeof(failure_message) - 1);
            MMAY_EXPECT(panic.message.data != NULL && memcmp(
                panic.message.data, failure_message,
                sizeof(failure_message) - 1
            ) == 0);
            if (failure_call != NULL) {
                MMAY_EXPECT(mmay_symbol_id_equal(
                    panic.symbol, fails->mir.symbol
                ));
                MMAY_EXPECT(panic.span.start
                    == (size_t)(failure_call - source));
                MMAY_EXPECT(panic.span.end
                    == (size_t)(failure_call - source)
                        + strlen("expect.true(false)"));
            }
        }
        mmay_mir_panic_dispose(&panic);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

static void mmay_test_checker_expectation_helpers(void) {
    static const char source[] =
        "fn require(value: bool): void { expect.true(value); }\n"
        "fn nested(value: bool): void { require(value); }\n"
        "@test fn accepts(): void { nested(true); }\n"
        "@test fn rejects(): void { nested(false); }\n";
    const char *call = strstr(source, "expect.true(value)");
    MarimayCheckerFixture fixture;
    size_t failure;
    bool complete = false;
    MMAY_EXPECT(mmay_checker_fixture_init_path(&fixture, "helpers_test.my", source));
    for (failure = 0; failure < 8192; failure += 1) {
        MarimayFailAllocator tracked;
        MarimayCheckResult checked = {0};
        MarimayCheckStatus status;
        mmay_fail_allocator_init(&tracked, mmay_system_allocator(), failure);
        status = mmay_check_ast(&fixture.ast, "test/expectation-helpers",
            mmay_fail_allocator(&tracked), mmay_check_default_options(), &checked);
        if (status == MMAY_CHECK_OK) {
            MMAY_EXPECT(checked.initialized && checked.diagnostics.length == 0);
            complete = failure >= tracked.attempts;
            if (complete && checked.diagnostics.length == 0) {
                MarimayCheckInterpretOptions options = mmay_check_interpret_default_options();
                MarimayMirPanic panic = {0};
                MarimayMirValue result = {0};
                size_t index;
                MMAY_EXPECT(failure == tracked.attempts);
                tracked.failure_index = SIZE_MAX;
                MMAY_EXPECT(checked.functions.length == 4);
                for (index = 0; index < checked.functions.length; index += 1) {
                    const MarimayCheckedFunction *function = mmay_checked_function(&checked, index);
                    MMAY_EXPECT(function != NULL && function->is_test == (index >= 2));
                    if (function != NULL) {
                        MMAY_EXPECT(!function->is_fuzz);
                        MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
                    }
                }
                MMAY_EXPECT(mmay_check_interpret_function(&checked, 2, NULL, 0,
                    mmay_fail_allocator(&tracked), options, &result) == MMAY_MIR_INTERPRET_OK);
                MMAY_EXPECT(result.type == MMAY_TYPE_VOID);
                options.panic = &panic;
                MMAY_EXPECT(mmay_check_interpret_function(&checked, 3, NULL, 0,
                    mmay_fail_allocator(&tracked), options, &result) == MMAY_MIR_INTERPRET_PANIC);
                MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4004") == 0);
                MMAY_EXPECT(panic.message.length == strlen("expect.true failed: value is false")
                    && panic.message.data != NULL && memcmp(panic.message.data,
                    "expect.true failed: value is false",
                    strlen("expect.true failed: value is false")) == 0);
                MMAY_EXPECT(mmay_checked_function(&checked, 0) != NULL && mmay_symbol_id_equal(panic.symbol,
                    mmay_checked_function(&checked, 0)->mir.symbol));
                MMAY_EXPECT(call != NULL && panic.span.start == (size_t)(call - source));
                MMAY_EXPECT(panic.span.end == panic.span.start + strlen("expect.true(value)"));
                mmay_mir_panic_dispose(&panic);
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

void mmay_test_checker_expectations(void) {
    mmay_test_checker_expectations_execute();
    mmay_test_checker_expectation_helpers();

    mmay_checker_expect_one_diagnostic_path(
        "outside.my",
        "fn bad(): void { expect.true(true); }\n",
        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "expect.true(true)"
    );
    mmay_checker_expect_one_diagnostic_path(
        "almost_test.my.bak",
        "fn bad(): void { expect.true(true); }\n",
        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "expect.true(true)"
    );
    mmay_checker_expect_one_diagnostic_path(
        "wrong_test.my",
        "@test fn bad(): void { expect.true(1); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "1"
    );
    mmay_checker_expect_one_diagnostic_path(
        "arity_test.my",
        "@test fn bad(): void { expect.equal(1); }\n",
        MMAY_DIAGNOSTIC_CALL_ARITY,
        "expect.equal(1)"
    );
    mmay_checker_expect_one_diagnostic_path(
        "option_test.my",
        "@test fn bad(): void { expect.some(1); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "1"
    );
}
