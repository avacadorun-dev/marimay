#include "test_checker_support.h"

void ruda_test_checker_numeric_policy(void) {
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
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    size_t index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/numeric-policy",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 26);
        for (index = 0; index < checked.functions.length; index += 1) {
            const RudaCheckedFunction *checked_function = ruda_checked_function(
                &checked, index
            );
            RUDA_EXPECT(checked_function != NULL);
            if (checked_function != NULL) {
                RUDA_EXPECT(
                    ruda_mir_verify(&checked_function->mir) == RUDA_MIR_VERIFY_OK
                );
            }
        }
        if (checked.functions.length == 26) {
            for (index = 0; index < 24; index += 1) {
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &ruda_checked_function(&checked, index)->mir,
                    RUDA_MIR_INTEGER_POLICY
                ) == 1);
            }
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &ruda_checked_function(&checked, 24)->mir,
                RUDA_MIR_INTEGER_CONVERT
            ) == 1);
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &ruda_checked_function(&checked, 25)->mir,
                RUDA_MIR_INTEGER_CONVERT
            ) == 1);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    {
        static const char shadow_source[] =
            "fn wrappingAdd(left: i8, right: i8): i8 { return left; }\n"
            "fn useShadow(): i8 { return wrappingAdd(1, 2); }\n";
        RudaCheckerFixture shadow_fixture;
        RudaCheckResult shadow_checked;
        memset(&shadow_fixture, 0, sizeof(shadow_fixture));
        memset(&shadow_checked, 0, sizeof(shadow_checked));
        RUDA_EXPECT(ruda_checker_fixture_init(
            &shadow_fixture, shadow_source
        ));
        RUDA_EXPECT(ruda_check_ast(
            &shadow_fixture.ast, "test/numeric-policy-shadow",
            ruda_system_allocator(), ruda_check_default_options(),
            &shadow_checked
        ) == RUDA_CHECK_OK);
        RUDA_EXPECT(shadow_checked.diagnostics.length == 0);
        RUDA_EXPECT(shadow_checked.functions.length == 2);
        if (shadow_checked.functions.length == 2) {
            const RudaMirFunction *use =
                &ruda_checked_function(&shadow_checked, 1)->mir;
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                use, RUDA_MIR_INTEGER_POLICY
            ) == 0);
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                use, RUDA_MIR_CALL
            ) == 1);
        }
        if (shadow_checked.initialized) {
            ruda_check_result_dispose(&shadow_checked);
        }
        ruda_checker_fixture_dispose(&shadow_fixture);
    }

    ruda_checker_expect_one_diagnostic(
        "fn bad(value: f64): f64 { return wrappingAdd(value, value); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "value",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: u16): u8 { return value as u8; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "value as u8",
        0
    );
}
