#include "test_checker_support.h"

void mmay_test_checker_match_block_arms(void) {
    static const char source[] =
        "enum Choice { None, Value(int), }\n"
        "fn none(): Choice { return Choice.None; }\n"
        "fn value(item: int): Choice { return Choice.Value(item); }\n"
        "fn enumBlock(choice: Choice, flag: bool): int {\n"
        "  return match choice {\n"
        "    None => { const base = 10; base },\n"
        "    Value(item) if item > 0 => {\n"
        "      let selected = item + 1;\n"
        "      if flag { selected = item; }\n"
        "      selected\n"
        "    },\n"
        "    Value(item) => { item },\n"
        "  };\n"
        "}\n"
        "fn boolBlock(value: bool): int {\n"
        "  return match value {\n"
        "    true | false => {\n"
        "      let selected = 0;\n"
        "      if value { selected = 1; }\n"
        "      selected\n"
        "    },\n"
        "  };\n"
        "}\n"
        "fn intBlock(value: int): int {\n"
        "  return match value {\n"
        "    0 | 1 => { const next = value + 10; next },\n"
        "    _ as other => { other },\n"
        "  };\n"
        "}\n"
        "fn nested(value: bool, other: bool): int {\n"
        "  return match value {\n"
        "    true => {\n"
        "      const inner = match other {\n"
        "        true => { 1 },\n"
        "        false => { 2 },\n"
        "      };\n"
        "      inner + 10\n"
        "    },\n"
        "    false => { 0 },\n"
        "  };\n"
        "}\n"
        "fn mixed(value: bool): int {\n"
        "  return match value { true => 1, false => { 0 }, };\n"
        "}\n"
        "fn mixedReturn(value: bool): int {\n"
        "  return match value {\n"
        "    true => { if value { return 9; } 1 },\n"
        "    false => { 0 },\n"
        "  };\n"
        "}\n"
        "fn blockBomb(): int { return 1 / 0; }\n"
        "fn lazyBlock(value: bool): int {\n"
        "  return match value {\n"
        "    true => { 1 },\n"
        "    false => { blockBomb() },\n"
        "  };\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirExecution execution;
    MarimayCheckInterpretOptions options = mmay_check_interpret_default_options();
    MarimayMirValue arguments[2];
    MarimayMirValue enum_value;
    MarimayMirValue result;
    size_t function_index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/match-block-arms",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.enums.length == 1);
        MMAY_EXPECT(checked.functions.length == 10);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked,
                function_index
            );
            MMAY_EXPECT(function != NULL);
            if (function != NULL) {
                MMAY_EXPECT(mmay_mir_verify(&function->mir)
                    == MMAY_MIR_VERIFY_OK);
            }
        }
        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            mmay_mir_execution_default_options()
        ));
        options.execution = &execution;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 0, NULL, 0, mmay_system_allocator(), options, &enum_value
        ) == MMAY_MIR_INTERPRET_OK);
        arguments[0] = enum_value;
        arguments[1].type = MMAY_TYPE_BOOL;
        arguments[1].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 2, arguments, 2, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 10);

        arguments[0].type = MMAY_TYPE_I64;
        arguments[0].as.i64 = 5;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 1, arguments, 1, mmay_system_allocator(), options,
            &enum_value
        ) == MMAY_MIR_INTERPRET_OK);
        arguments[0] = enum_value;
        arguments[1].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 2, arguments, 2, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 5);
        arguments[1].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 2, arguments, 2, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 6);

        arguments[0].type = MMAY_TYPE_BOOL;
        arguments[0].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 3, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 0);
        arguments[0].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 3, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 1);

        arguments[0].type = MMAY_TYPE_I64;
        arguments[0].as.i64 = 0;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 4, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 10);
        arguments[0].as.i64 = 1;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 4, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 11);
        arguments[0].as.i64 = 7;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 4, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 7);

        arguments[0].type = MMAY_TYPE_BOOL;
        arguments[0].as.boolean = true;
        arguments[1].type = MMAY_TYPE_BOOL;
        arguments[1].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 5, arguments, 2, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 11);
        arguments[1].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 5, arguments, 2, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 12);
        arguments[0].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 5, arguments, 2, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 0);

        arguments[0].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 6, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 1);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 7, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 9);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 9, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 1);
        arguments[0].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 6, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 0);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 7, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 0);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 9, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_DIVISION_BY_ZERO);
        mmay_mir_execution_dispose(&execution);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value {\n"
        "    true => { const item = 1; }, false => { 0 },\n"
        "  };\n"
        "}\n",
        MMAY_DIAGNOSTIC_MISSING_MATCH_VALUE,
        "{ const item = 1; }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value {\n"
        "    true => { if value { return 1; } },\n"
        "    false => { 0 },\n"
        "  };\n"
        "}\n",
        MMAY_DIAGNOSTIC_MISSING_MATCH_VALUE,
        "{ if value { return 1; } }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value {\n"
        "    true => { 1 }, false => { false },\n"
        "  };\n"
        "}\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "false",
        1
    );

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "fn pending(value: bool): int {\n"
        "  return match value {\n"
        "    true => { return 1; }, false => { return 0; },\n"
        "  };\n"
        "}\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/match-block-never",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        const MarimayCheckedFunction *pending = mmay_checked_function(&checked, 0);
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 1);
        MMAY_EXPECT(pending != NULL);
        if (pending != NULL) {
            MMAY_EXPECT(mmay_mir_verify(&pending->mir) == MMAY_MIR_VERIFY_OK);
        }
        arguments[0].type = MMAY_TYPE_BOOL;
        arguments[0].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 0, arguments, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 1);
        arguments[0].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 0, arguments, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 0);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_match_return_arms(void) {
    static const char source[] =
        "enum Decision { Stop, Value(int), }\n"
        "fn stop(): Decision { return Decision.Stop; }\n"
        "fn decisionValue(item: int): Decision { return Decision.Value(item); }\n"
        "fn enumReturn(value: Decision): int {\n"
        "  return match value {\n"
        "    Stop => { return 9; }, Value(item) => { item },\n"
        "  };\n"
        "}\n"
        "fn firstReturn(value: bool): int {\n"
        "  return match value {\n"
        "    true => { return 7; }, false => { 0 },\n"
        "  };\n"
        "}\n"
        "fn lastReturn(value: bool): int {\n"
        "  return match value {\n"
        "    true => { 1 }, false => { return 8; },\n"
        "  };\n"
        "}\n"
        "fn branching(value: bool, condition: bool): int {\n"
        "  return match value {\n"
        "    true => {\n"
        "      if condition { return 11; } else { return 12; }\n"
        "    },\n"
        "    false => { 0 },\n"
        "  };\n"
        "}\n"
        "fn binding(value: bool): int {\n"
        "  const selected = match value {\n"
        "    true => { return 9; }, false => { 2 },\n"
        "  };\n"
        "  return selected + 1;\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirExecution execution;
    MarimayCheckInterpretOptions options = mmay_check_interpret_default_options();
    MarimayMirValue arguments[2];
    MarimayMirValue enum_value;
    MarimayMirValue result;
    size_t function_index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/match-return-arms",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 7);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked,
                function_index
            );
            MMAY_EXPECT(function != NULL);
            if (function != NULL) {
                MMAY_EXPECT(mmay_mir_verify(&function->mir)
                    == MMAY_MIR_VERIFY_OK);
            }
        }
        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            mmay_mir_execution_default_options()
        ));
        options.execution = &execution;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 0, NULL, 0, mmay_system_allocator(), options, &enum_value
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 2, &enum_value, 1, mmay_system_allocator(), options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 9);

        arguments[0].type = MMAY_TYPE_I64;
        arguments[0].as.i64 = 4;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 1, arguments, 1, mmay_system_allocator(), options,
            &enum_value
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 2, &enum_value, 1, mmay_system_allocator(), options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 4);

        arguments[0].type = MMAY_TYPE_BOOL;
        arguments[0].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 3, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 7);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 4, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 1);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 6, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 9);

        arguments[1].type = MMAY_TYPE_BOOL;
        arguments[1].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 5, arguments, 2, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 11);
        arguments[1].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 5, arguments, 2, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 12);

        arguments[0].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 3, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 0);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 4, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 8);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 5, arguments, 2, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 0);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 6, arguments, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 3);
        mmay_mir_execution_dispose(&execution);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_all_never_match(void) {
    static const char source[] =
        "fn allReturn(value: bool): int {\n"
        "  return match value {\n"
        "    true => { return 1; }, false => { return 2; },\n"
        "  };\n"
        "}\n"
        "fn bindingNever(value: bool): int {\n"
        "  const unreachable = match value {\n"
        "    true => { return 11; }, false => { return 12; },\n"
        "  };\n"
        "}\n"
        "fn binaryNever(value: bool): int {\n"
        "  return (match value {\n"
        "    true => { return 21; }, false => { return 22; },\n"
        "  }) + 100;\n"
        "}\n"
        "fn consume(value: int): int { return value + 1000; }\n"
        "fn callNever(value: bool): int {\n"
        "  return consume(match value {\n"
        "    true => { return 31; }, false => { return 32; },\n"
        "  });\n"
        "}\n"
        "fn unaryNever(value: bool): int {\n"
        "  return -(match value {\n"
        "    true => { return 41; }, false => { return 42; },\n"
        "  });\n"
        "}\n"
        "fn shortNever(flag: bool, value: bool): bool {\n"
        "  return flag || match value {\n"
        "    true => { return true; }, false => { return false; },\n"
        "  };\n"
        "}\n"
        "fn leftNever(flag: bool, value: bool): bool {\n"
        "  return (match value {\n"
        "    true => { return true; }, false => { return false; },\n"
        "  }) || flag;\n"
        "}\n"
        "fn voidNever(value: bool): void {\n"
        "  return match value {\n"
        "    true => { return; }, false => { return; },\n"
        "  };\n"
        "}\n"
        "type Box { item: int, }\n"
        "fn recordNever(value: bool): int {\n"
        "  const box = Box { item: match value {\n"
        "    true => { return 51; }, false => { return 52; },\n"
        "  } };\n"
        "}\n"
        "enum Wrap { Value(int), }\n"
        "fn enumNever(value: bool): int {\n"
        "  const wrapped = Wrap.Value(match value {\n"
        "    true => { return 61; }, false => { return 62; },\n"
        "  });\n"
        "}\n"
        "fn assignmentNever(value: bool): int {\n"
        "  let selected: int = 0;\n"
        "  selected = match value {\n"
        "    true => { return 71; }, false => { return 72; },\n"
        "  };\n"
        "}\n"
        "fn nestedNever(value: bool, other: bool): int {\n"
        "  return match value {\n"
        "    true => match other {\n"
        "      true => { return 81; }, false => { return 82; },\n"
        "    },\n"
        "    false => { return 80; },\n"
        "  };\n"
        "}\n"
        "fn intAllNever(value: int): int {\n"
        "  return match value { _ => { return 91; }, };\n"
        "}\n"
        "fn enumAllNever(value: Wrap): int {\n"
        "  return match value { Value(_) => { return 101; }, };\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirValue arguments[2];
    MarimayMirValue result;
    size_t function_index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/all-never-match",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        const MarimayCheckedFunction *all_return = mmay_checked_function(
            &checked,
            0
        );
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 15);
        MMAY_EXPECT(checked.records.length == 1);
        MMAY_EXPECT(checked.enums.length == 1);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked,
                function_index
            );
            MMAY_EXPECT(function != NULL);
            if (function != NULL) {
                MMAY_EXPECT(mmay_mir_verify(&function->mir)
                    == MMAY_MIR_VERIFY_OK);
            }
        }
        MMAY_EXPECT(all_return != NULL);
        if (all_return != NULL) {
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                &all_return->mir,
                MMAY_MIR_COPY
            ) == 0);
        }
        arguments[0].type = MMAY_TYPE_BOOL;
        arguments[1].type = MMAY_TYPE_BOOL;
        for (function_index = 0; function_index < 2; function_index += 1) {
            arguments[0].as.boolean = function_index != 0;
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, 0, arguments, 1, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == (function_index != 0 ? 1 : 2));
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, 1, arguments, 1, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == (function_index != 0 ? 11 : 12));
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, 2, arguments, 1, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == (function_index != 0 ? 21 : 22));
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, 4, arguments, 1, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == (function_index != 0 ? 31 : 32));
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, 5, arguments, 1, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == (function_index != 0 ? 41 : 42));
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, 8, arguments, 1, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_VOID);
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, 9, arguments, 1, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == (function_index != 0 ? 51 : 52));
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, 10, arguments, 1, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == (function_index != 0 ? 61 : 62));
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, 11, arguments, 1, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == (function_index != 0 ? 71 : 72));
        }
        arguments[0].as.boolean = true;
        arguments[1].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 6, arguments, 2, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_BOOL && result.as.boolean);
        arguments[0].as.boolean = false;
        arguments[1].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 6, arguments, 2, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_BOOL && !result.as.boolean);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 7, arguments, 2, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_BOOL && !result.as.boolean);
        arguments[0].as.boolean = true;
        arguments[1].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 12, arguments, 2, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 82);
        arguments[0].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 12, arguments, 2, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 80);
        arguments[0].type = MMAY_TYPE_I64;
        arguments[0].as.i64 = 123;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 13, arguments, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 91);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_never_conditions_and_guards(void) {
    static const char source[] =
        "fn ifNever(value: bool): int {\n"
        "  if match value {\n"
        "    true => { return 1; }, false => { return 2; },\n"
        "  } { return 99; }\n"
        "}\n"
        "fn i64Guard(value: int, decision: bool): int {\n"
        "  return match value {\n"
        "    0 if match decision {\n"
        "      true => { return 11; }, false => { return 12; },\n"
        "    } => 99,\n"
        "    _ => 1,\n"
        "  };\n"
        "}\n"
        "fn boolGuard(value: bool, decision: bool): int {\n"
        "  return match value {\n"
        "    true if match decision {\n"
        "      true => { return 21; }, false => { return 22; },\n"
        "    } => 99,\n"
        "    true => 1, false => 0,\n"
        "  };\n"
        "}\n"
        "enum Signal { A, B, }\n"
        "fn makeA(): Signal { return Signal.A; }\n"
        "fn makeB(): Signal { return Signal.B; }\n"
        "fn enumGuard(value: Signal, decision: bool): int {\n"
        "  return match value {\n"
        "    A if match decision {\n"
        "      true => { return 31; }, false => { return 32; },\n"
        "    } => 99,\n"
        "    A => 1, B => 2,\n"
        "  };\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirExecution execution;
    MarimayCheckInterpretOptions options = mmay_check_interpret_default_options();
    MarimayMirValue arguments[2];
    MarimayMirValue enum_value;
    MarimayMirValue result;
    size_t function_index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/never-conditions-guards",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 6);
        MMAY_EXPECT(checked.enums.length == 1);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked,
                function_index
            );
            MMAY_EXPECT(function != NULL);
            if (function != NULL) {
                MMAY_EXPECT(mmay_mir_verify(&function->mir)
                    == MMAY_MIR_VERIFY_OK);
            }
        }
        arguments[0].type = MMAY_TYPE_BOOL;
        arguments[0].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 0, arguments, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 1);
        arguments[0].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 0, arguments, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 2);

        arguments[0].type = MMAY_TYPE_I64;
        arguments[0].as.i64 = 1;
        arguments[1].type = MMAY_TYPE_BOOL;
        arguments[1].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 1, arguments, 2, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 1);
        arguments[0].as.i64 = 0;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 1, arguments, 2, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 11);
        arguments[1].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 1, arguments, 2, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 12);

        arguments[0].type = MMAY_TYPE_BOOL;
        arguments[0].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 2, arguments, 2, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 0);
        arguments[0].as.boolean = true;
        arguments[1].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 2, arguments, 2, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 21);
        arguments[1].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 2, arguments, 2, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 22);

        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            mmay_mir_execution_default_options()
        ));
        options.execution = &execution;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 3, NULL, 0, mmay_system_allocator(), options, &enum_value
        ) == MMAY_MIR_INTERPRET_OK);
        arguments[0] = enum_value;
        arguments[1].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 5, arguments, 2, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 31);
        arguments[1].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 5, arguments, 2, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 32);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 4, NULL, 0, mmay_system_allocator(), options, &enum_value
        ) == MMAY_MIR_INTERPRET_OK);
        arguments[0] = enum_value;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 5, arguments, 2, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 2);
        mmay_mir_execution_dispose(&execution);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "fn whileNever(value: bool): int {\n"
        "  while match value {\n"
        "    true => { return 1; }, false => { return 2; },\n"
        "  } {}\n"
        "}\n"
        "fn whileMixed(value: bool): int {\n"
        "  while match value {\n"
        "    true => { return 3; },\n"
        "    false => { false },\n"
        "  } {}\n"
        "  return 4;\n"
        "}\n"
        "fn rangeBodyNever(value: bool): int {\n"
        "  for index in 0..1 {\n"
        "    const _ = index;\n"
        "    return match value { true => 5, false => 6, };\n"
        "  }\n"
        "  return 0;\n"
        "}\n"
        "fn rangeBodyMixed(value: bool): int {\n"
        "  let selected = 8;\n"
        "  for index in 0..1 {\n"
        "    const _ = index;\n"
        "    selected = match value { true => 7, false => selected, };\n"
        "  }\n"
        "  return selected;\n"
        "}\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/never-loop-conditions",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 4);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked,
                function_index
            );
            MMAY_EXPECT(function != NULL);
            if (function != NULL) {
                MMAY_EXPECT(mmay_mir_verify(&function->mir)
                    == MMAY_MIR_VERIFY_OK);
            }
        }
        arguments[0].type = MMAY_TYPE_BOOL;
        for (function_index = 0; function_index < 4; function_index += 1) {
            arguments[0].as.boolean = true;
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, function_index, arguments, 1, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == (int64_t)(function_index * 2 + 1));
            arguments[0].as.boolean = false;
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked, function_index, arguments, 1, mmay_system_allocator(),
                mmay_check_interpret_default_options(), &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == (int64_t)(function_index * 2 + 2));
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}
