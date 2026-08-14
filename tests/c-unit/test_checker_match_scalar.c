#include "test_checker_support.h"

void ruda_test_checker_boolean_match(void) {
    static const char source[] =
        "fn choose(value: bool): int {\n"
        "  return match value { case true => 1; case false => 0; };\n"
        "}\n"
        "fn reverse(value: bool): int {\n"
        "  return match value { case false => 0; case true => 1; };\n"
        "}\n"
        "fn bind(value: bool): bool {\n"
        "  return match value { case observed => observed; };\n"
        "}\n"
        "fn guarded(value: bool): int {\n"
        "  return match value {\n"
        "    case true if false => 9;\n"
        "    case true => 1;\n"
        "    case false => 0;\n"
        "  };\n"
        "}\n"
        "fn boundGuard(value: bool): int {\n"
        "  return match value {\n"
        "    case observed if observed => 1;\n"
        "    case _ => 0;\n"
        "  };\n"
        "}\n"
        "fn guardBomb(): bool { return 1 / 0 == 0; }\n"
        "fn guardOrder(value: bool): int {\n"
        "  return match value {\n"
        "    case true if guardBomb() => 1;\n"
        "    case _ => 2;\n"
        "  };\n"
        "}\n"
        "fn union(value: bool): int {\n"
        "  return match value { case true | false => 3; };\n"
        "}\n"
        "fn identity(value: bool): bool { return value; }\n"
        "fn viaCall(value: bool): int {\n"
        "  return match identity(value) {\n"
        "    case true => 1;\n"
        "    case false => 0;\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue argument;
    RudaMirValue result;
    size_t function_index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/boolean-match",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 10);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        {
            const RudaCheckedFunction *guard_order = ruda_checked_function(
                &checked,
                6
            );
            const RudaCheckedFunction *via_call = ruda_checked_function(
                &checked,
                9
            );
            RUDA_EXPECT(guard_order != NULL && via_call != NULL);
            if (guard_order != NULL) {
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &guard_order->mir,
                    RUDA_MIR_CALL
                ) == 1);
            }
            if (via_call != NULL) {
                RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                    &via_call->mir,
                    RUDA_MIR_CALL
                ) == 1);
            }
        }
        argument.type = RUDA_TYPE_BOOL;
        argument.as.boolean = false;
        for (function_index = 0; function_index < 2;
            function_index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                function_index,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 0);
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_BOOL && !result.as.boolean);
        for (function_index = 3; function_index <= 4;
            function_index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                function_index,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 0);
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            6,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 3);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            9,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);

        argument.as.boolean = true;
        for (function_index = 0; function_index < 2;
            function_index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                function_index,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 1);
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_BOOL && result.as.boolean);
        for (function_index = 3; function_index <= 4;
            function_index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                function_index,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == 1);
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            6,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 3);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            9,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case true => 1; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true if true => 1; case false => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case true if true => 1; case false => 0; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true => 1; case true => 2; case false => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "true",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case _ => 0; case false => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "false",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true | true => 1; case false => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "true",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true | _ => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true | false if true => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case true | false if true => 1; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true if 1 => 1; case true => 2; case false => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: bool): int {\n"
        "  return match value { case true => 1; case false => false; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "false",
        1
    );

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn bad(value: bool): int {\n"
        "  return match value { case 1 => 1; case _ => 0; };\n"
        "}\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/boolean-match-boundary",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_match_witnesses(void) {
    ruda_checker_expect_diagnostic_message(
        "enum Message {\n"
        "  Quit, Write(int, int), Move { x: int; y: int; },\n"
        "  Pause, Stop, Wait,\n"
        "}\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; };\n"
        "}\n",
        "enum match is missing witnesses: Write(_, _), "
        "Move { x: _, y: _ }, Pause, Stop; +1 more"
    );
    ruda_checker_expect_diagnostic_message(
        "enum Message { Quit, Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value {\n"
        "    case Quit if true => 0;\n"
        "    case Write(_) => 1;\n"
        "  };\n"
        "}\n",
        "enum match is missing witness: Quit; guarded patterns do not count "
        "toward exhaustiveness"
    );
    ruda_checker_expect_diagnostic_message(
        "enum Message { Quit, Write(int), Move, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value {\n"
        "    case Quit if true => 0; case Quit => 1; case Write(_) => 2;\n"
        "  };\n"
        "}\n",
        "enum match is missing witness: Move"
    );
    ruda_checker_expect_diagnostic_message(
        "enum Message { Quit, Many(int, int, int, int, int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; };\n"
        "}\n",
        "enum match is missing witness: Many(_, _, _, _, ...)"
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: bool): int {\n"
        "  return match value { case true => 1; };\n"
        "}\n",
        "boolean match is missing witness: false"
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: bool): int {\n"
        "  return match value { case true if true => 1; case true => 2; };\n"
        "}\n",
        "boolean match is missing witness: false"
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: bool): int {\n"
        "  return match value { case observed if observed => 1; };\n"
        "}\n",
        "boolean match is missing witnesses: false, true; guarded patterns "
        "do not count toward exhaustiveness"
    );
    ruda_checker_expect_diagnostic_message(
        "enum Message { Quit, Write(int), }\n"
        "fn bad(value: Message): int { return match value {}; }\n",
        "enum match is missing witnesses: Quit, Write(_)"
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: bool): int { return match value {}; }\n",
        "boolean match is missing witnesses: false, true"
    );
}

void ruda_test_checker_i64_match(void) {
    static const char source[] =
        "fn classify(value: int): int {\n"
        "  return match value {\n"
        "    case 0 => 10; case 1 | 2 => 20; case other => other;\n"
        "  };\n"
        "}\n"
        "fn guarded(value: int): int {\n"
        "  return match value {\n"
        "    case 1 | 2 if value > 1 => 9;\n"
        "    case 1 | 2 => 8; case _ => 0;\n"
        "  };\n"
        "}\n"
        "fn boundGuard(value: int): int {\n"
        "  return match value {\n"
        "    case observed if observed > 0 => observed; case _ => 0;\n"
        "  };\n"
        "}\n"
        "fn guardBomb(): bool { return 1 / 0 == 0; }\n"
        "fn guardOrder(value: int): int {\n"
        "  return match value { case 1 if guardBomb() => 1; case _ => 2; };\n"
        "}\n"
        "fn maximum(value: int): int {\n"
        "  return match value {\n"
        "    case 9223372036854775807 => 1; case _ => 0;\n"
        "  };\n"
        "}\n"
        "fn identity(value: int): int { return value; }\n"
        "fn viaCall(value: int): int {\n"
        "  return match identity(value) { case 0 => 1; case other => other; };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue argument;
    RudaMirValue result;
    size_t function_index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/i64-match",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedFunction *via_call = NULL;
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 8);
        for (function_index = 0; function_index < checked.functions.length;
            function_index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                function_index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        via_call = ruda_checked_function(&checked, 7);
        RUDA_EXPECT(via_call != NULL);
        if (via_call != NULL) {
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &via_call->mir,
                RUDA_MIR_CALL
            ) == 1);
        }
        argument.type = RUDA_TYPE_I64;
        argument.as.i64 = 0;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 10);
        argument.as.i64 = 1;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 20);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 8);
        argument.as.i64 = 2;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 9);
        argument.as.i64 = 7;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 7);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 7);
        argument.as.i64 = -7;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);
        argument.as.i64 = 0;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);
        argument.as.i64 = 1;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        argument.as.i64 = INT64_MAX;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);
        argument.as.i64 = 0;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);
        argument.as.i64 = 9;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, &argument, 1, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 9);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_diagnostic_message(
        "fn bad(value: int): int {\n"
        "  return match value { case 0 => 1; case 1 => 2; case 2 => 3; };\n"
        "}\n",
        "integer match is missing witness: 3"
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: int): int {\n"
        "  return match value { case observed if observed > 0 => 1; };\n"
        "}\n",
        "integer match is missing witness: 0; guarded patterns do not count "
        "toward exhaustiveness"
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: int): int { return match value {}; }\n",
        "integer match is missing witness: 0"
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case 1 => 1; case 1 => 2; case _ => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "1",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case 1 | 1 => 1; case _ => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "1",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case _ => 0; case 1 => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case 0 | _ => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "0",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case true => 1; case _ => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case 9223372036854775808 => 1; case _ => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_INTEGER_LITERAL_RANGE,
        "9223372036854775808",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case 1 if 1 => 1; case _ => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: int): int {\n"
        "  return match value { case 1 => 1; case _ => true; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
}
