#include "test_checker_support.h"

static void mmay_test_checker_interface_diagnostics(void) {
    mmay_checker_expect_one_diagnostic(
        "type Counter { value: int, }\n"
        "interface Show { show(): int; }\n"
        "fn consume(value: Show): int { return value.show(); }\n"
        "fn main(): int { return consume(Counter { value: 4 }); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Counter { value: 4 }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Counter { value: int, }\n"
        "interface Show { show(): int; }\n"
        "fn show(value: Counter): bool { return true; }\n"
        "fn consume(value: Show): int { return value.show(); }\n"
        "fn main(): int { return consume(Counter { value: 4 }); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Counter { value: 4 }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Counter { value: int, }\n"
        "interface Show { show(flag: int): int; }\n"
        "fn show(value: Counter, flag: bool): int { return 1; }\n"
        "fn consume(value: Show): int { return value.show(1); }\n"
        "fn main(): int { return consume(Counter { value: 4 }); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Counter { value: 4 }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "interface Show { show(): int; }\n"
        "fn consume(value: Show): int { return value.missing(); }\n",
        MMAY_DIAGNOSTIC_UNKNOWN_NAME,
        "missing",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "interface Show { show(value: int): int; }\n"
        "fn consume(value: Show): int { return value.show(); }\n",
        MMAY_DIAGNOSTIC_CALL_ARITY,
        "()",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Counter { value: int, }\n"
        "interface Show { show(): int; }\n"
        "fn show(value: int): int { return value; }\n"
        "fn consume(value: Show): int { return value.show(); }\n"
        "fn main(): int { return consume(Counter { value: 4 }); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Counter { value: 4 }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Counter { value: int, }\n"
        "type Other { value: int, }\n"
        "interface Show { show(): int; }\n"
        "fn show(value: Other): int { return value.value; }\n"
        "fn consume(value: Show): int { return value.show(); }\n"
        "fn main(): int { return consume(Counter { value: 4 }); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Counter { value: 4 }",
        0
    );
}

void mmay_test_checker_interfaces(void) {
    static const char source[] =
        "public type Counter { mutable value: int, }\n"
        "public interface Show { show(): int; }\n"
        "public fn show(value: Counter): int { return value.value; }\n"
        "public fn consume(value: Show): int { return value.show(); }\n"
        "fn main(): int { return consume(Counter { value: 4 }); }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedInterface *show = NULL;
    const MarimayCheckedFunction *consume = NULL;
    const MarimayCheckedFunction *main = NULL;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/interfaces", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.interfaces.length == 1);
        show = mmay_checked_interface(&checked, 0);
        consume = mmay_checked_function(&checked, 1);
        main = mmay_checked_function(&checked, 2);
        MMAY_EXPECT(show != NULL && show->methods.length == 1);
        MMAY_EXPECT(consume != NULL && mmay_test_mir_instruction_kind_count(
            &consume->mir, MMAY_MIR_CALL_INTERFACE
        ) == 1);
        MMAY_EXPECT(main != NULL && mmay_test_mir_instruction_kind_count(
            &main->mir, MMAY_MIR_INTERFACE_NEW
        ) == 1);
        if (show != NULL) {
            MMAY_EXPECT(mmay_type_kind(&checked.types, show->type)
                == MMAY_TYPE_KIND_INTERFACE);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
    mmay_test_checker_interface_diagnostics();
}
