#include "test_checker_support.h"

void mmay_test_checker_numeric_policy(void) {
    static const char source[] =
        "fn ca(value: i8): Option<i8> { return checkedAdd(value, 1); }\n"
        "fn cs(value: i8): Option<i8> { return checkedSubtract(value, 1); }\n"
        "fn cm(value: i8): Option<i8> { return checkedMultiply(value, 2); }\n"
        "fn cd(value: i8): Option<i8> { return checkedDivide(value, 2); }\n"
        "fn cr(value: i8): Option<i8> { return checkedRemainder(value, 2); }\n"
        "fn cn(value: i8): Option<i8> { return checkedNegate(value); }\n"
        "fn csl(value: i8): Option<i8> { return checkedShiftLeft(value, 1); }\n"
        "fn csr(value: i8): Option<i8> { return checkedShiftRight(value, 1); }\n"
        "fn wa(value: i8): i8 { return wrappingAdd(value, 1); }\n"
        "fn ws(value: i8): i8 { return wrappingSubtract(value, 1); }\n"
        "fn wm(value: i8): i8 { return wrappingMultiply(value, 2); }\n"
        "fn wd(value: i8): i8 { return wrappingDivide(value, 2); }\n"
        "fn wr(value: i8): i8 { return wrappingRemainder(value, 2); }\n"
        "fn wn(value: i8): i8 { return wrappingNegate(value); }\n"
        "fn wsl(value: i8): i8 { return wrappingShiftLeft(value, 1); }\n"
        "fn wsr(value: i8): i8 { return wrappingShiftRight(value, 1); }\n"
        "fn sa(value: i8): i8 { return saturatingAdd(value, 1); }\n"
        "fn ss(value: i8): i8 { return saturatingSubtract(value, 1); }\n"
        "fn sm(value: i8): i8 { return saturatingMultiply(value, 2); }\n"
        "fn sd(value: i8): i8 { return saturatingDivide(value, 2); }\n"
        "fn sr(value: i8): i8 { return saturatingRemainder(value, 2); }\n"
        "fn sn(value: i8): i8 { return saturatingNegate(value); }\n"
        "fn ssl(value: i8): i8 { return saturatingShiftLeft(value, 1); }\n"
        "fn ssr(value: i8): i8 { return saturatingShiftRight(value, 1); }\n"
        "fn widened(value: u8): u16 { return value as u16; }\n"
        "fn narrowed(value: u16): Option<u8> { return value as u8; }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    size_t index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/numeric-policy",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 26);
        for (index = 0; index < checked.functions.length; index += 1) {
            const MarimayCheckedFunction *checked_function = mmay_checked_function(
                &checked, index
            );
            MMAY_EXPECT(checked_function != NULL);
            if (checked_function != NULL) {
                MMAY_EXPECT(
                    mmay_mir_verify(&checked_function->mir) == MMAY_MIR_VERIFY_OK
                );
            }
        }
        if (checked.functions.length == 26) {
            for (index = 0; index < 24; index += 1) {
                MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                    &mmay_checked_function(&checked, index)->mir,
                    MMAY_MIR_INTEGER_POLICY
                ) == 1);
            }
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                &mmay_checked_function(&checked, 24)->mir,
                MMAY_MIR_INTEGER_CONVERT
            ) == 1);
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                &mmay_checked_function(&checked, 25)->mir,
                MMAY_MIR_INTEGER_CONVERT
            ) == 1);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    {
        static const char shadow_source[] =
            "fn wrappingAdd(left: i8, right: i8): i8 { return left; }\n"
            "fn useShadow(): i8 { return wrappingAdd(1, 2); }\n";
        MarimayCheckerFixture shadow_fixture;
        MarimayCheckResult shadow_checked;
        memset(&shadow_fixture, 0, sizeof(shadow_fixture));
        memset(&shadow_checked, 0, sizeof(shadow_checked));
        MMAY_EXPECT(mmay_checker_fixture_init(
            &shadow_fixture, shadow_source
        ));
        MMAY_EXPECT(mmay_check_ast(
            &shadow_fixture.ast, "test/numeric-policy-shadow",
            mmay_system_allocator(), mmay_check_default_options(),
            &shadow_checked
        ) == MMAY_CHECK_OK);
        MMAY_EXPECT(shadow_checked.diagnostics.length == 0);
        MMAY_EXPECT(shadow_checked.functions.length == 2);
        if (shadow_checked.functions.length == 2) {
            const MarimayMirFunction *use =
                &mmay_checked_function(&shadow_checked, 1)->mir;
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                use, MMAY_MIR_INTEGER_POLICY
            ) == 0);
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                use, MMAY_MIR_CALL
            ) == 1);
        }
        if (shadow_checked.initialized) {
            mmay_check_result_dispose(&shadow_checked);
        }
        mmay_checker_fixture_dispose(&shadow_fixture);
    }

    mmay_checker_expect_one_diagnostic(
        "fn bad(value: f64): f64 { return wrappingAdd(value, value); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "value",
        1
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: u16): u8 { return value as u8; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "value as u8",
        0
    );
}
