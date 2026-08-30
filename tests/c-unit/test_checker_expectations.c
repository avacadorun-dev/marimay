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

void mmay_test_checker_expectations(void) {
    mmay_test_checker_expectations_execute();

    mmay_checker_expect_one_diagnostic_path(
        "outside.my",
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
