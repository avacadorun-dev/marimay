#include "test_checker_support.h"

void mmay_test_checker_boolean_match(void) {
    static const char source[] =
        "fn choose(value: bool): int {\n"
        "  return match value { true => 1, false => 0, };\n"
        "}\n"
        "fn reverse(value: bool): int {\n"
        "  return match value { false => 0, true => 1, };\n"
        "}\n"
        "fn bind(value: bool): bool {\n"
        "  return match value { _ as observed => observed, };\n"
        "}\n"
        "fn guarded(value: bool): int {\n"
        "  return match value {\n"
        "    true if false => 9,\n"
        "    true => 1,\n"
        "    false => 0,\n"
        "  };\n"
        "}\n"
        "fn boundGuard(value: bool): int {\n"
        "  return match value {\n"
        "    _ as observed if observed => 1,\n"
        "    _ => 0,\n"
        "  };\n"
        "}\n"
        "fn guardBomb(): bool { return 1 / 0 == 0; }\n"
        "fn guardOrder(value: bool): int {\n"
        "  return match value {\n"
        "    true if guardBomb() => 1,\n"
        "    _ => 2,\n"
        "  };\n"
        "}\n"
        "fn union(value: bool): int {\n"
        "  return match value { true | false => 3, };\n"
        "}\n"
        "fn identity(value: bool): bool { return value; }\n"
        "fn viaCall(value: bool): int {\n"
        "  return match identity(value) {\n"
        "    true => 1,\n"
        "    false => 0,\n"
        "  };\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirValue argument;
    MarimayMirValue result;
    size_t function_index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/boolean-match",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
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
        {
            const MarimayCheckedFunction *guard_order = mmay_checked_function(
                &checked,
                6
            );
            const MarimayCheckedFunction *via_call = mmay_checked_function(
                &checked,
                9
            );
            MMAY_EXPECT(guard_order != NULL && via_call != NULL);
            if (guard_order != NULL) {
                MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                    &guard_order->mir,
                    MMAY_MIR_CALL
                ) == 1);
            }
            if (via_call != NULL) {
                MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                    &via_call->mir,
                    MMAY_MIR_CALL
                ) == 1);
            }
        }
        argument.type = MMAY_TYPE_BOOL;
        argument.as.boolean = false;
        for (function_index = 0; function_index < 2;
            function_index += 1) {
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                function_index,
                &argument,
                1,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 0);
        }
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_BOOL && !result.as.boolean);
        for (function_index = 3; function_index <= 4;
            function_index += 1) {
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                function_index,
                &argument,
                1,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 0);
        }
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            6,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 2);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            7,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 3);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            9,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 0);

        argument.as.boolean = true;
        for (function_index = 0; function_index < 2;
            function_index += 1) {
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                function_index,
                &argument,
                1,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 1);
        }
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_BOOL && result.as.boolean);
        for (function_index = 3; function_index <= 4;
            function_index += 1) {
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                function_index,
                &argument,
                1,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.as.i64 == 1);
        }
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            6,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_DIVISION_BY_ZERO);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            7,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 3);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            9,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 1);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { true => 1, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { true => 1, }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { true if true => 1, false => 0, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { true if true => 1, false => 0, }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { true => 1, true => 2, false => 0, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
        "true",
        1
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { _ => 0, false => 1, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
        "false",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { true | true => 1, false => 0, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
        "true",
        1
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { true | _ => 1, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
        "true",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { true | false if true => 1, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { true | false if true => 1, }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { true if 1 => 1, true => 2, false => 0, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { true => 1, false => false, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "false",
        1
    );

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "fn bad(value: bool): int {\n"
        "  return match value { 1 => 1, _ => 0, };\n"
        "}\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/boolean-match-boundary",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_INCOMPLETE_LANGUAGE);
    MMAY_EXPECT(checked.initialized);
    if (checked.initialized) {
        const MarimayCheckDiagnostic *diagnostic =
            mmay_check_diagnostic(&checked, 0);
        MMAY_EXPECT(checked.diagnostics.length == 1);
        MMAY_EXPECT(diagnostic != NULL && strcmp(
            diagnostic->code, MMAY_DIAGNOSTIC_COMPILER_INCOMPLETE
        ) == 0);
        MMAY_EXPECT(diagnostic != NULL && diagnostic->snapshot
            == fixture.ast.snapshot);
        MMAY_EXPECT(diagnostic != NULL
            && diagnostic->start < diagnostic->end);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_match_witnesses(void) {
    mmay_checker_expect_diagnostic_message(
        "enum Message {\n"
        "  Quit, Write(int, int), Move { x: int, y: int, },\n"
        "  Pause, Stop, Wait,\n"
        "}\n"
        "fn bad(value: Message): int {\n"
        "  return match value { Quit => 0, };\n"
        "}\n",
        "enum match is missing witnesses: Write(_, _), "
        "Move { x: _, y: _ }, Pause, Stop; +1 more"
    );
    mmay_checker_expect_diagnostic_message(
        "enum Message { Quit, Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value {\n"
        "    Quit if true => 0,\n"
        "    Write(_) => 1,\n"
        "  };\n"
        "}\n",
        "enum match is missing witness: Quit; guarded patterns do not count "
        "toward exhaustiveness"
    );
    mmay_checker_expect_diagnostic_message(
        "enum Message { Quit, Write(int), Move, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value {\n"
        "    Quit if true => 0, Quit => 1, Write(_) => 2,\n"
        "  };\n"
        "}\n",
        "enum match is missing witness: Move"
    );
    mmay_checker_expect_diagnostic_message(
        "enum Message { Quit, Many(int, int, int, int, int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { Quit => 0, };\n"
        "}\n",
        "enum match is missing witness: Many(_, _, _, _, ...)"
    );
    mmay_checker_expect_diagnostic_message(
        "fn bad(value: bool): int {\n"
        "  return match value { true => 1, };\n"
        "}\n",
        "boolean match is missing witness: false"
    );
    mmay_checker_expect_diagnostic_message(
        "fn bad(value: bool): int {\n"
        "  return match value { true if true => 1, true => 2, };\n"
        "}\n",
        "boolean match is missing witness: false"
    );
    mmay_checker_expect_diagnostic_message(
        "fn bad(value: bool): int {\n"
        "  return match value { _ as observed if observed => 1, };\n"
        "}\n",
        "boolean match is missing witnesses: false, true; guarded patterns "
        "do not count toward exhaustiveness"
    );
    mmay_checker_expect_diagnostic_message(
        "enum Message { Quit, Write(int), }\n"
        "fn bad(value: Message): int { return match value {}; }\n",
        "enum match is missing witnesses: Quit, Write(_)"
    );
    mmay_checker_expect_diagnostic_message(
        "fn bad(value: bool): int { return match value {}; }\n",
        "boolean match is missing witnesses: false, true"
    );
}

void mmay_test_checker_i64_match(void) {
    static const char source[] =
        "fn classify(value: int): int {\n"
        "  return match value {\n"
        "    0 => 10, 1 | 2 => 20, _ as other => other,\n"
        "  };\n"
        "}\n"
        "fn guarded(value: int): int {\n"
        "  return match value {\n"
        "    1 | 2 if value > 1 => 9,\n"
        "    1 | 2 => 8, _ => 0,\n"
        "  };\n"
        "}\n"
        "fn boundGuard(value: int): int {\n"
        "  return match value {\n"
        "    _ as observed if observed > 0 => observed, _ => 0,\n"
        "  };\n"
        "}\n"
        "fn guardBomb(): bool { return 1 / 0 == 0; }\n"
        "fn guardOrder(value: int): int {\n"
        "  return match value { 1 if guardBomb() => 1, _ => 2, };\n"
        "}\n"
        "fn maximum(value: int): int {\n"
        "  return match value {\n"
        "    9223372036854775807 => 1, _ => 0,\n"
        "  };\n"
        "}\n"
        "fn identity(value: int): int { return value; }\n"
        "fn viaCall(value: int): int {\n"
        "  return match identity(value) { 0 => 1, _ as other => other, };\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirValue argument;
    MarimayMirValue result;
    size_t function_index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/i64-match",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        const MarimayCheckedFunction *via_call = NULL;
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 8);
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
        via_call = mmay_checked_function(&checked, 7);
        MMAY_EXPECT(via_call != NULL);
        if (via_call != NULL) {
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                &via_call->mir,
                MMAY_MIR_CALL
            ) == 1);
        }
        argument.type = MMAY_TYPE_I64;
        argument.as.i64 = 0;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 0, &argument, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 10);
        argument.as.i64 = 1;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 0, &argument, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 20);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 1, &argument, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 8);
        argument.as.i64 = 2;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 1, &argument, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 9);
        argument.as.i64 = 7;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 0, &argument, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 7);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 2, &argument, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 7);
        argument.as.i64 = -7;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 2, &argument, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 0);
        argument.as.i64 = 0;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 4, &argument, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 2);
        argument.as.i64 = 1;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 4, &argument, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_DIVISION_BY_ZERO);
        argument.as.i64 = INT64_MAX;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 5, &argument, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 1);
        argument.as.i64 = 0;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 7, &argument, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 1);
        argument.as.i64 = 9;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 7, &argument, 1, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 9);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_diagnostic_message(
        "fn bad(value: int): int {\n"
        "  return match value { 0 => 1, 1 => 2, 2 => 3, };\n"
        "}\n",
        "integer match is missing witness: 3"
    );
    mmay_checker_expect_diagnostic_message(
        "fn bad(value: int): int {\n"
        "  return match value { _ as observed if observed > 0 => 1, };\n"
        "}\n",
        "integer match is missing witness: 0; guarded patterns do not count "
        "toward exhaustiveness"
    );
    mmay_checker_expect_diagnostic_message(
        "fn bad(value: int): int { return match value {}; }\n",
        "integer match is missing witness: 0"
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { 1 => 1, 1 => 2, _ => 0, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
        "1",
        2
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { 1 | 1 => 1, _ => 0, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
        "1",
        1
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { _ => 0, 1 => 1, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
        "1",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { 0 | _ => 1, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
        "0",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { true => 1, _ => 0, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { 9223372036854775808 => 1, _ => 0, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_INTEGER_LITERAL_RANGE,
        "9223372036854775808",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { 1 if 1 => 1, _ => 0, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        1
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { 1 => 1, _ => true, };\n"
        "}\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
}
