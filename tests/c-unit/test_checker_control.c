#include "test_checker_support.h"

void ruda_test_checker_primitive_signatures(void) {
    static const char source[] =
        "fn primitives(\n"
        "  p0: bool, p1: i8, p2: i16, p3: i32, p4: i64, p5: int,\n"
        "  p6: u8, p7: byte, p8: u16, p9: u32, p10: u64, p11: uint,\n"
        "  p12: f32, p13: f64, p14: float, p15: rune, p16: string,\n"
        "  p17: never\n"
        "): void {}\n";
    static const RudaTypeId expected[] = {
        RUDA_TYPE_BOOL,
        RUDA_TYPE_I8,
        RUDA_TYPE_I16,
        RUDA_TYPE_I32,
        RUDA_TYPE_I64,
        RUDA_TYPE_I64,
        RUDA_TYPE_U8,
        RUDA_TYPE_U8,
        RUDA_TYPE_U16,
        RUDA_TYPE_U32,
        RUDA_TYPE_U64,
        RUDA_TYPE_U64,
        RUDA_TYPE_F32,
        RUDA_TYPE_F64,
        RUDA_TYPE_F64,
        RUDA_TYPE_RUNE,
        RUDA_TYPE_STRING,
        RUDA_TYPE_NEVER
    };
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *function = NULL;
    size_t index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 1);
        function = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(function != NULL);
        if (function != NULL) {
            RUDA_EXPECT(function->mir.parameter_count
                == sizeof(expected) / sizeof(expected[0]));
            for (index = 0; index < sizeof(expected) / sizeof(expected[0]);
                index += 1) {
                const RudaTypeId *actual = ruda_vector_at_const(
                    &function->mir.register_types,
                    index
                );
                RUDA_EXPECT(actual != NULL);
                if (actual != NULL) {
                    RUDA_EXPECT(*actual == expected[index]);
                }
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_control_flow(void) {
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
        "  for let index = 0; index < limit; index += 1 {\n"
        "    if index == 2 { continue; }\n"
        "    total += index;\n"
        "    if total >= 10 { break; }\n"
        "  }\n"
        "  return total;\n"
        "}\n"
        "fn logicalLoops(ready: bool): int {\n"
        "  let total: int = 0;\n"
        "  while total < 2 && ready { total += 1; }\n"
        "  for let keep = ready && true; keep && total < 3; keep = false || false {\n"
        "    total += 1;\n"
        "  }\n"
        "  return total;\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *choose = NULL;
    const RudaCheckedFunction *branch = NULL;
    const RudaCheckedFunction *count = NULL;
    const RudaCheckedFunction *direct = NULL;
    const RudaCheckedFunction *nested_block = NULL;
    const RudaCheckedFunction *classify = NULL;
    const RudaCheckedFunction *loop_control = NULL;
    const RudaCheckedFunction *c_for = NULL;
    const RudaCheckedFunction *logical_loops = NULL;
    RudaMirValue arguments[2];
    RudaMirValue result;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 9);
        choose = ruda_checked_function(&checked, 0);
        branch = ruda_checked_function(&checked, 1);
        count = ruda_checked_function(&checked, 2);
        direct = ruda_checked_function(&checked, 3);
        nested_block = ruda_checked_function(&checked, 4);
        classify = ruda_checked_function(&checked, 5);
        loop_control = ruda_checked_function(&checked, 6);
        c_for = ruda_checked_function(&checked, 7);
        logical_loops = ruda_checked_function(&checked, 8);
        RUDA_EXPECT(choose != NULL);
        RUDA_EXPECT(branch != NULL);
        RUDA_EXPECT(count != NULL);
        RUDA_EXPECT(direct != NULL);
        RUDA_EXPECT(nested_block != NULL);
        RUDA_EXPECT(classify != NULL);
        RUDA_EXPECT(loop_control != NULL);
        RUDA_EXPECT(c_for != NULL);
        RUDA_EXPECT(logical_loops != NULL);
        if (choose != NULL) {
            arguments[0].type = RUDA_TYPE_BOOL;
            arguments[0].as.boolean = true;
            RUDA_EXPECT(ruda_mir_interpret(
                &choose->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 1);
            arguments[0].as.boolean = false;
            RUDA_EXPECT(ruda_mir_interpret(
                &choose->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 0);
        }
        if (branch != NULL) {
            arguments[0].type = RUDA_TYPE_BOOL;
            arguments[0].as.boolean = true;
            RUDA_EXPECT(ruda_mir_interpret(
                &branch->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 10);
            arguments[0].as.boolean = false;
            RUDA_EXPECT(ruda_mir_interpret(
                &branch->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 20);
        }
        if (count != NULL) {
            arguments[0].type = RUDA_TYPE_I64;
            arguments[0].as.i64 = 5;
            RUDA_EXPECT(ruda_mir_interpret(
                &count->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 5);
        }
        if (direct != NULL) {
            arguments[0].type = RUDA_TYPE_BOOL;
            arguments[0].as.boolean = false;
            RUDA_EXPECT(ruda_mir_interpret(
                &direct->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 40);
        }
        if (nested_block != NULL) {
            RUDA_EXPECT(ruda_mir_interpret(
                &nested_block->mir,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 3);
        }
        if (classify != NULL) {
            arguments[0].type = RUDA_TYPE_BOOL;
            arguments[0].as.boolean = false;
            arguments[1].type = RUDA_TYPE_BOOL;
            arguments[1].as.boolean = true;
            RUDA_EXPECT(ruda_mir_interpret(
                &classify->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 2);
        }
        if (loop_control != NULL) {
            arguments[0].type = RUDA_TYPE_I64;
            arguments[0].as.i64 = 10;
            RUDA_EXPECT(ruda_mir_interpret(
                &loop_control->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 4);
        }
        if (c_for != NULL) {
            arguments[0].type = RUDA_TYPE_I64;
            arguments[0].as.i64 = 10;
            RUDA_EXPECT(ruda_mir_interpret(
                &c_for->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 13);
        }
        if (logical_loops != NULL) {
            arguments[0].type = RUDA_TYPE_BOOL;
            arguments[0].as.boolean = true;
            RUDA_EXPECT(ruda_mir_interpret(
                &logical_loops->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 3);
            arguments[0].as.boolean = false;
            RUDA_EXPECT(ruda_mir_interpret(
                &logical_loops->mir,
                arguments,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 0);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_control_flow_diagnostics(void) {
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
        "fn bad(): int { for let index = 0; index < 1; index += 1 {} return index; }\n";
    ruda_checker_expect_one_diagnostic(
        condition,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        branch_initialization,
        RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
        "value",
        2
    );
    ruda_checker_expect_one_diagnostic(
        loop_initialization,
        RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
        "value",
        2
    );
    ruda_checker_expect_one_diagnostic(
        scoped_name,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "inner",
        1
    );
    ruda_checker_expect_one_diagnostic(
        outside_break,
        RUDA_DIAGNOSTIC_LOOP_CONTROL_OUTSIDE_LOOP,
        "break",
        0
    );
    ruda_checker_expect_one_diagnostic(
        outside_continue,
        RUDA_DIAGNOSTIC_LOOP_CONTROL_OUTSIDE_LOOP,
        "continue",
        0
    );
    ruda_checker_expect_one_diagnostic(
        for_scope,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "index",
        3
    );
}
