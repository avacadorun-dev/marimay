#include "test_checker_support.h"

static void ruda_test_checker_interface_diagnostics(void) {
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "interface Show { show(): int; }\n"
        "fn consume(value: Show): int { return value.show(); }\n"
        "fn main(): int { return consume(Counter { value: 4 }); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Counter { value: 4 }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "interface Show { show(): int; }\n"
        "fn show(value: Counter): bool { return true; }\n"
        "fn consume(value: Show): int { return value.show(); }\n"
        "fn main(): int { return consume(Counter { value: 4 }); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Counter { value: 4 }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "interface Show { show(flag: int): int; }\n"
        "fn show(value: Counter, flag: bool): int { return 1; }\n"
        "fn consume(value: Show): int { return value.show(1); }\n"
        "fn main(): int { return consume(Counter { value: 4 }); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Counter { value: 4 }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "interface Show { show(): int; }\n"
        "fn consume(value: Show): int { return value.missing(); }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "interface Show { show(value: int): int; }\n"
        "fn consume(value: Show): int { return value.show(); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "()",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "interface Show { show(): int; }\n"
        "fn show(value: int): int { return value; }\n"
        "fn consume(value: Show): int { return value.show(); }\n"
        "fn main(): int { return consume(Counter { value: 4 }); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Counter { value: 4 }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "type Other { value: int; }\n"
        "interface Show { show(): int; }\n"
        "fn show(value: Other): int { return value.value; }\n"
        "fn consume(value: Show): int { return value.show(); }\n"
        "fn main(): int { return consume(Counter { value: 4 }); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Counter { value: 4 }",
        0
    );
}

void ruda_test_checker_interfaces(void) {
    static const char source[] =
        "public type Counter { mutable value: int; }\n"
        "public interface Show { show(): int; }\n"
        "public fn show(value: Counter): int { return value.value; }\n"
        "public fn consume(value: Show): int { return value.show(); }\n"
        "fn main(): int { return consume(Counter { value: 4 }); }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedInterface *show = NULL;
    const RudaCheckedFunction *consume = NULL;
    const RudaCheckedFunction *main = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/interfaces", ruda_system_allocator(),
        ruda_check_default_options(), &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.interfaces.length == 1);
        show = ruda_checked_interface(&checked, 0);
        consume = ruda_checked_function(&checked, 1);
        main = ruda_checked_function(&checked, 2);
        RUDA_EXPECT(show != NULL && show->methods.length == 1);
        RUDA_EXPECT(consume != NULL && ruda_test_mir_instruction_kind_count(
            &consume->mir, RUDA_MIR_CALL_INTERFACE
        ) == 1);
        RUDA_EXPECT(main != NULL && ruda_test_mir_instruction_kind_count(
            &main->mir, RUDA_MIR_INTERFACE_NEW
        ) == 1);
        if (show != NULL) {
            RUDA_EXPECT(ruda_type_kind(&checked.types, show->type)
                == RUDA_TYPE_KIND_INTERFACE);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
    ruda_test_checker_interface_diagnostics();
}
