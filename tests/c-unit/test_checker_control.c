#include "test_checker_support.h"

void mmay_test_checker_primitive_signatures(void) {
    static const char source[] =
        "fn primitives(\n"
        "  p0: bool, p1: i8, p2: i16, p3: i32, p4: i64, p5: int,\n"
        "  p6: u8, p7: byte, p8: u16, p9: u32, p10: u64, p11: uint,\n"
        "  p12: f32, p13: f64, p14: float, p15: rune, p16: string,\n"
        "  p17: never\n"
        "): void {}\n";
    static const MarimayTypeId expected[] = {
        MMAY_TYPE_BOOL,
        MMAY_TYPE_I8,
        MMAY_TYPE_I16,
        MMAY_TYPE_I32,
        MMAY_TYPE_I64,
        MMAY_TYPE_I64,
        MMAY_TYPE_U8,
        MMAY_TYPE_U8,
        MMAY_TYPE_U16,
        MMAY_TYPE_U32,
        MMAY_TYPE_U64,
        MMAY_TYPE_U64,
        MMAY_TYPE_F32,
        MMAY_TYPE_F64,
        MMAY_TYPE_F64,
        MMAY_TYPE_RUNE,
        MMAY_TYPE_STRING,
        MMAY_TYPE_NEVER
    };
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *function = NULL;
    size_t index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 1);
        function = mmay_checked_function(&checked, 0);
        MMAY_EXPECT(function != NULL);
        if (function != NULL) {
            MMAY_EXPECT(function->mir.parameter_count
                == sizeof(expected) / sizeof(expected[0]));
            for (index = 0; index < sizeof(expected) / sizeof(expected[0]);
                index += 1) {
                const MarimayTypeId *actual = mmay_vector_at_const(
                    &function->mir.register_types,
                    index
                );
                MMAY_EXPECT(actual != NULL);
                if (actual != NULL) {
                    MMAY_EXPECT(*actual == expected[index]);
                }
            }
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_control_flow(void) {
    static const char source[] =
        "fn choose(ready: bool): int {\n"
        "  if ready { return 1; }\n"
        "  return 0;\n"
        "}\n"
        "fn branch(ready: bool): int {\n"
        "  let value: int;\n"
        "  if ready { value = 10; } else { value = 20; }\n"
        "  return value;\n"
        "}\n"
        "fn count(limit: int): int {\n"
        "  let value: int = 0;\n"
        "  while value < limit { value = value + 1; }\n"
        "  value |= 0;\n"
        "  value <<= 0;\n"
        "  return value;\n"
        "}\n"
        "fn direct(ready: bool): int {\n"
        "  if ready { return 30; } else { return 40; }\n"
        "}\n"
        "fn nestedBlock(): int {\n"
        "  let value: int = 1;\n"
        "  { const increment: int = 2; value = value + increment; }\n"
        "  return value;\n"
        "}\n"
        "fn classify(first: bool, second: bool): int {\n"
        "  if first { return 1; } else if second { return 2; } else { return 3; }\n"
        "}\n"
        "fn loopControl(limit: int): int {\n"
        "  let value: int = 0;\n"
        "  while value < limit {\n"
        "    value = value + 1;\n"
        "    if value == 2 { continue; }\n"
        "    if value >= 4 { break; }\n"
        "  }\n"
        "  return value;\n"
        "}\n"
        "fn cFor(limit: int): int {\n"
        "  let total: int = 0;\n"
        "  for index in 0..limit {\n"
        "    if index == 2 { continue; }\n"
        "    total += index;\n"
        "    if total >= 10 { break; }\n"
        "  }\n"
        "  return total;\n"
        "}\n"
        "fn logicalLoops(ready: bool): int {\n"
        "  let total: int = 0;\n"
        "  while total < 2 && ready { total += 1; }\n"
        "  let keep = ready && true;\n"
        "  while keep && total < 3 {\n"
        "    total += 1;\n"
        "    keep = false || false;\n"
        "  }\n"
        "  return total;\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *choose = NULL;
    const MarimayCheckedFunction *branch = NULL;
    const MarimayCheckedFunction *count = NULL;
    const MarimayCheckedFunction *direct = NULL;
    const MarimayCheckedFunction *nested_block = NULL;
    const MarimayCheckedFunction *classify = NULL;
    const MarimayCheckedFunction *loop_control = NULL;
    const MarimayCheckedFunction *c_for = NULL;
    const MarimayCheckedFunction *logical_loops = NULL;
    MarimayMirValue arguments[2];
    MarimayMirValue result;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 9);
        choose = mmay_checked_function(&checked, 0);
        branch = mmay_checked_function(&checked, 1);
        count = mmay_checked_function(&checked, 2);
        direct = mmay_checked_function(&checked, 3);
        nested_block = mmay_checked_function(&checked, 4);
        classify = mmay_checked_function(&checked, 5);
        loop_control = mmay_checked_function(&checked, 6);
        c_for = mmay_checked_function(&checked, 7);
        logical_loops = mmay_checked_function(&checked, 8);
        MMAY_EXPECT(choose != NULL);
        MMAY_EXPECT(branch != NULL);
        MMAY_EXPECT(count != NULL);
        MMAY_EXPECT(direct != NULL);
        MMAY_EXPECT(nested_block != NULL);
        MMAY_EXPECT(classify != NULL);
        MMAY_EXPECT(loop_control != NULL);
        MMAY_EXPECT(c_for != NULL);
        MMAY_EXPECT(logical_loops != NULL);
        if (choose != NULL) {
            arguments[0].type = MMAY_TYPE_BOOL;
            arguments[0].as.boolean = true;
            MMAY_EXPECT(mmay_mir_interpret(
                &choose->mir,
                arguments,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 1);
            arguments[0].as.boolean = false;
            MMAY_EXPECT(mmay_mir_interpret(
                &choose->mir,
                arguments,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 0);
        }
        if (branch != NULL) {
            arguments[0].type = MMAY_TYPE_BOOL;
            arguments[0].as.boolean = true;
            MMAY_EXPECT(mmay_mir_interpret(
                &branch->mir,
                arguments,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 10);
            arguments[0].as.boolean = false;
            MMAY_EXPECT(mmay_mir_interpret(
                &branch->mir,
                arguments,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 20);
        }
        if (count != NULL) {
            arguments[0].type = MMAY_TYPE_I64;
            arguments[0].as.i64 = 5;
            MMAY_EXPECT(mmay_mir_interpret(
                &count->mir,
                arguments,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 5);
        }
        if (direct != NULL) {
            arguments[0].type = MMAY_TYPE_BOOL;
            arguments[0].as.boolean = false;
            MMAY_EXPECT(mmay_mir_interpret(
                &direct->mir,
                arguments,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 40);
        }
        if (nested_block != NULL) {
            MMAY_EXPECT(mmay_mir_interpret(
                &nested_block->mir,
                NULL,
                0,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 3);
        }
        if (classify != NULL) {
            arguments[0].type = MMAY_TYPE_BOOL;
            arguments[0].as.boolean = false;
            arguments[1].type = MMAY_TYPE_BOOL;
            arguments[1].as.boolean = true;
            MMAY_EXPECT(mmay_mir_interpret(
                &classify->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 2);
        }
        if (loop_control != NULL) {
            arguments[0].type = MMAY_TYPE_I64;
            arguments[0].as.i64 = 10;
            MMAY_EXPECT(mmay_mir_interpret(
                &loop_control->mir,
                arguments,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 4);
        }
        if (c_for != NULL) {
            arguments[0].type = MMAY_TYPE_I64;
            arguments[0].as.i64 = 10;
            MMAY_EXPECT(mmay_mir_interpret(
                &c_for->mir,
                arguments,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 13);
        }
        if (logical_loops != NULL) {
            arguments[0].type = MMAY_TYPE_BOOL;
            arguments[0].as.boolean = true;
            MMAY_EXPECT(mmay_mir_interpret(
                &logical_loops->mir,
                arguments,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 3);
            arguments[0].as.boolean = false;
            MMAY_EXPECT(mmay_mir_interpret(
                &logical_loops->mir,
                arguments,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 0);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_control_flow_diagnostics(void) {
    static const char condition[] =
        "fn bad(): int { if 1 { return 1; } return 0; }\n";
    static const char branch_initialization[] =
        "fn bad(ready: bool): int { let value: int; if ready { value = 1; } return value; }\n";
    static const char loop_initialization[] =
        "fn bad(ready: bool): int { let value: int; while ready { value = 1; } return value; }\n";
    static const char scoped_name[] =
        "fn bad(): int { { const inner: int = 1; } return inner; }\n";
    static const char outside_break[] =
        "fn bad(): void { break; }\n";
    static const char outside_continue[] =
        "fn bad(): void { continue; }\n";
    static const char for_scope[] =
        "fn bad(): int { for index in 0..1 {} return index; }\n";
    mmay_checker_expect_one_diagnostic(
        condition,
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    mmay_checker_expect_one_diagnostic(
        branch_initialization,
        MMAY_DIAGNOSTIC_UNINITIALIZED_LOCAL,
        "value",
        2
    );
    mmay_checker_expect_one_diagnostic(
        loop_initialization,
        MMAY_DIAGNOSTIC_UNINITIALIZED_LOCAL,
        "value",
        2
    );
    mmay_checker_expect_one_diagnostic(
        scoped_name,
        MMAY_DIAGNOSTIC_UNKNOWN_NAME,
        "inner",
        1
    );
    mmay_checker_expect_one_diagnostic(
        outside_break,
        MMAY_DIAGNOSTIC_LOOP_CONTROL_OUTSIDE_LOOP,
        "break",
        0
    );
    mmay_checker_expect_one_diagnostic(
        outside_continue,
        MMAY_DIAGNOSTIC_LOOP_CONTROL_OUTSIDE_LOOP,
        "continue",
        0
    );
    mmay_checker_expect_one_diagnostic(
        for_scope,
        MMAY_DIAGNOSTIC_UNKNOWN_NAME,
        "index",
        1
    );
}
