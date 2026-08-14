#include "test_checker_support.h"

void ruda_test_checker_enum_match(void) {
    static const char source[] =
        "enum Message {\n"
        "  Quit,\n"
        "  Write(int, int),\n"
        "  Move { x: int; y: int; },\n"
        "}\n"
        "fn quit(): Message { return Message.Quit; }\n"
        "fn write(x: int, y: int): Message {\n"
        "  return Message.Write(x, y);\n"
        "}\n"
        "fn move(x: int, y: int): Message {\n"
        "  return Message.Move { y, x };\n"
        "}\n"
        "fn describe(value: Message): int {\n"
        "  return match value {\n"
        "    case Quit => 0;\n"
        "    case Write(number, _) => number;\n"
        "    case Move { x, y: vertical } => x + vertical;\n"
        "  };\n"
        "}\n"
        "fn fallback(value: Message): int {\n"
        "  return match value {\n"
        "    case Quit => 1;\n"
        "    case other => 2;\n"
        "  };\n"
        "}\n"
        "fn divideZero(): int { return 1 / 0; }\n"
        "fn overflow(): int { return 9223372036854775807 + 1; }\n"
        "fn lazy(value: Message): int {\n"
        "  return match value {\n"
        "    case Quit => 1;\n"
        "    case Write(_, _) => divideZero();\n"
        "    case Move { x: _, y: _ } => overflow();\n"
        "  };\n"
        "}\n"
        "fn guarded(value: Message): int {\n"
        "  return match value {\n"
        "    case Write(number, _) if number > 0 => number;\n"
        "    case Write(_, _) => 0;\n"
        "    case Quit => -1;\n"
        "    case Move { x, y } if x > y => x;\n"
        "    case Move { x, y } => y;\n"
        "  };\n"
        "}\n"
        "fn guardedCatch(value: Message): int {\n"
        "  return match value {\n"
        "    case other if false => 9;\n"
        "    case _ => 4;\n"
        "  };\n"
        "}\n"
        "fn guardBomb(): bool { return 1 / 0 == 0; }\n"
        "fn guardOrder(value: Message): int {\n"
        "  return match value {\n"
        "    case Write(_, _) if guardBomb() => 1;\n"
        "    case _ => 2;\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *describe = NULL;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirValue constructor_arguments[2];
    RudaMirValue enum_value;
    RudaMirValue result;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enum-match",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.enums.length == 1);
        RUDA_EXPECT(checked.functions.length == 12);
        describe = ruda_checked_function(&checked, 3);
        RUDA_EXPECT(describe != NULL);
        if (describe != NULL) {
            RUDA_EXPECT(describe->mir.enums.length == 1);
            RUDA_EXPECT(describe->mir.enum_constructions.length == 0);
            RUDA_EXPECT(describe->mir.blocks.length >= 6);
            RUDA_EXPECT(ruda_mir_verify(&describe->mir) == RUDA_MIR_VERIFY_OK);
        }
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            NULL,
            0,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 0);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == -1);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            9,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 4);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            11,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 1);

        constructor_arguments[0].type = RUDA_TYPE_I64;
        constructor_arguments[0].as.i64 = 41;
        constructor_arguments[1].type = RUDA_TYPE_I64;
        constructor_arguments[1].as.i64 = 99;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            constructor_arguments,
            2,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 41);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 41);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            11,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        constructor_arguments[0].as.i64 = -1;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            constructor_arguments,
            2,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);

        constructor_arguments[0].as.i64 = 10;
        constructor_arguments[1].as.i64 = 20;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            constructor_arguments,
            2,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 30);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OVERFLOW);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 20);
        constructor_arguments[0].as.i64 = 30;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            constructor_arguments,
            2,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 30);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &enum_value,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
        enum_value.as.reference = RUDA_MIR_INVALID_REFERENCE;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case Quit => 0; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; case Quit => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Quit",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case _ => 0; case Quit => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Quit",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Write => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Write",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Missing(x) => x; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_UNKNOWN_VARIANT,
        "Missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Write(int, int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Write(x) => x; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "(x)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Write(int, int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Write(x, x) => x; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "x",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, Write(int), }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; case Write(x) => true; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; y: int; }, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Move { z } => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
        "z",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; y: int; }, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Move { x, x } => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
        "x",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; y: int; }, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Move { x } => x; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_MISSING_FIELD,
        "{ x }",
        0
    );

    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn guarded(value: Message): int {\n"
        "  return match value { case Quit if true => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case Quit if true => 0; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit if 1 => 0; case _ => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; case Quit if false => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Quit",
        2
    );
}


void ruda_test_checker_enum_or_patterns(void) {
    static const char source[] =
        "enum Choice {\n"
        "  Idle, Stop, Pause,\n"
        "  Left(int), Right(int),\n"
        "  Point { x: int; y: int; },\n"
        "  Pixel { x: int; y: int; },\n"
        "}\n"
        "fn idle(): Choice { return Choice.Idle; }\n"
        "fn stop(): Choice { return Choice.Stop; }\n"
        "fn pause(): Choice { return Choice.Pause; }\n"
        "fn left(value: int): Choice { return Choice.Left(value); }\n"
        "fn right(value: int): Choice { return Choice.Right(value); }\n"
        "fn point(x: int, y: int): Choice {\n"
        "  return Choice.Point { x, y };\n"
        "}\n"
        "fn pixel(x: int, y: int): Choice {\n"
        "  return Choice.Pixel { y, x };\n"
        "}\n"
        "fn classify(value: Choice): int {\n"
        "  return match value {\n"
        "    case Idle | Stop | Pause => 1;\n"
        "    case Left(_) | Right(_) => 2;\n"
        "    case Point { x: _, y: _ } | Pixel { x: _, y: _ } => 3;\n"
        "  };\n"
        "}\n"
        "fn payload(value: Choice): int {\n"
        "  return match value {\n"
        "    case Left(number) | Right(number) => number;\n"
        "    case _ => -1;\n"
        "  };\n"
        "}\n"
        "fn named(value: Choice): int {\n"
        "  return match value {\n"
        "    case Point { x: horizontal, y: vertical }\n"
        "      | Pixel { y: vertical, x: horizontal }\n"
        "      => horizontal * 10 + vertical;\n"
        "    case _ => -1;\n"
        "  };\n"
        "}\n"
        "fn guarded(value: Choice): int {\n"
        "  return match value {\n"
        "    case Left(number) | Right(number) if number > 0 => number;\n"
        "    case Left(_) | Right(_) => 0;\n"
        "    case _ => -1;\n"
        "  };\n"
        "}\n"
        "fn guardBomb(): bool { return 1 / 0 == 0; }\n"
        "fn guardOrder(value: Choice): int {\n"
        "  return match value {\n"
        "    case Left(_) | Right(_) if guardBomb() => 1;\n"
        "    case _ => 2;\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirValue arguments[2];
    RudaMirValue enum_value;
    RudaMirValue result;
    size_t constructor_index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enum-or-patterns",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedFunction *guard_order = NULL;
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.enums.length == 1);
        RUDA_EXPECT(checked.functions.length == 13);
        guard_order = ruda_checked_function(&checked, 12);
        RUDA_EXPECT(guard_order != NULL);
        if (guard_order != NULL) {
            RUDA_EXPECT(ruda_mir_verify(&guard_order->mir)
                == RUDA_MIR_VERIFY_OK);
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &guard_order->mir,
                RUDA_MIR_CALL
            ) == 1);
        }
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        for (constructor_index = 0; constructor_index < 3;
            constructor_index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                constructor_index,
                NULL,
                0,
                ruda_system_allocator(),
                options,
                &enum_value
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                7,
                &enum_value,
                1,
                ruda_system_allocator(),
                options,
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 1);
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            12,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);

        arguments[0].type = RUDA_TYPE_I64;
        arguments[0].as.i64 = 41;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            arguments,
            1,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 2);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 41);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            10,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 41);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            12,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);

        arguments[0].as.i64 = -7;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            arguments,
            1,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == -7);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            10,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 0);

        arguments[0].as.i64 = 3;
        arguments[1].type = RUDA_TYPE_I64;
        arguments[1].as.i64 = 4;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            5,
            arguments,
            2,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            9,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 34);
        arguments[0].as.i64 = 5;
        arguments[1].as.i64 = 6;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            6,
            arguments,
            2,
            ruda_system_allocator(),
            options,
            &enum_value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            9,
            &enum_value,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 56);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "enum Choice { Left(int), Right(int), }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Left(x) | Right(y) => x; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Right(y)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Number(int), Flag(bool), }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Number(item) | Flag(item) => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Flag(item)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Idle, Left(int), }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Left(item) | Idle => item; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Idle",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Idle, Stop, }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Idle | Idle => 0; case Stop => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Idle",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Idle, Stop, }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Idle => 0; case Idle | Stop => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Idle",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Idle, Stop, }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case _ | Idle => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Idle",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Idle, Stop, }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Idle | _ => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "Idle",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { Idle, Stop, }\n"
        "fn bad(value: Choice): int {\n"
        "  return match value { case Idle | Stop if true => 0; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case Idle | Stop if true => 0; }",
        0
    );
}
