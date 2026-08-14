#include "test_checker_support.h"

void ruda_test_checker_strings_and_panic(void) {
    static const char source[] =
        "fn direct(): int { panic(\"broken\\n\\u{26cf}\"); }\n"
        "fn selected(value: bool): int {\n"
        "  return match value {\n"
        "    case true => panic(r\"path\\\\raw\"); case false => 7;\n"
        "  };\n"
        "}\n"
        "fn conditional(value: bool): int {\n"
        "  if value { panic(\"nested\"); }\n"
        "  return 9;\n"
        "}\n"
        "fn text(): string { return \"line\\n\\u{26cf}\\0end\"; }\n"
        "fn callee(): int { panic(\"callee\"); }\n"
        "fn caller(): int { return callee(); }\n"
        "fn accept(value: int): int { return value; }\n"
        "fn argument(): int { return accept(panic(\"argument\")); }\n"
        "fn allPanic(value: bool): int {\n"
        "  return match value {\n"
        "    case true => panic(\"yes\"); case false => panic(\"no\");\n"
        "  };\n"
        "}\n"
        "fn message(): string { return \"dynamic\"; }\n"
        "fn indirect(): int { panic(message()); }\n";
    static const unsigned char text_value[] = "line\n\xe2\x9b\x8f\0end";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirExecution execution;
    RudaMirPanic panic;
    RudaMirValue argument;
    RudaMirValue result;
    const unsigned char *bytes = NULL;
    size_t length = 0;
    size_t function_index = 0;
    const char *direct_panic = strstr(source, "panic(\"broken");
    memset(&panic, 0, sizeof(panic));
    RUDA_EXPECT(direct_panic != NULL);
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/strings-panic",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 11);
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
        options.panic = &panic;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, NULL, 0, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4000") == 0);
        RUDA_EXPECT(ruda_symbol_id_equal(
            panic.symbol,
            ruda_checked_function(&checked, 0)->mir.symbol
        ));
        RUDA_EXPECT(panic.message.length == 10);
        RUDA_EXPECT(panic.message.data != NULL && memcmp(
            panic.message.data,
            "broken\n\xe2\x9b\x8f",
            10
        ) == 0);
        if (direct_panic != NULL) {
            size_t start = (size_t)(direct_panic - source);
            RUDA_EXPECT(panic.span.start == start);
            RUDA_EXPECT(panic.span.end == start + strlen(
                "panic(\"broken\\n\\u{26cf}\")"
            ));
        }

        argument.type = RUDA_TYPE_BOOL;
        argument.as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, &argument, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 7);
        RUDA_EXPECT(panic.message.data == NULL);
        argument.as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, &argument, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 9 && memcmp(
            panic.message.data,
            "path\\\\raw",
            9
        ) == 0);
        argument.as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, &argument, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 9);
        argument.as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, &argument, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 6 && memcmp(
            panic.message.data,
            "nested",
            6
        ) == 0);

        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 3, NULL, 0, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_STRING);
        RUDA_EXPECT(ruda_mir_execution_string_bytes(
            &execution,
            result,
            &bytes,
            &length
        ));
        RUDA_EXPECT(length == sizeof(text_value) - 1);
        RUDA_EXPECT(bytes != NULL && memcmp(
            bytes,
            text_value,
            sizeof(text_value) - 1
        ) == 0);
        options.execution = NULL;
        ruda_mir_execution_dispose(&execution);

        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, NULL, 0, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 6 && memcmp(
            panic.message.data,
            "callee",
            6
        ) == 0);
        RUDA_EXPECT(ruda_symbol_id_equal(
            panic.symbol,
            ruda_checked_function(&checked, 4)->mir.symbol
        ));
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, NULL, 0, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 8 && memcmp(
            panic.message.data,
            "argument",
            8
        ) == 0);
        argument.as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 8, &argument, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 3
            && memcmp(panic.message.data, "yes", 3) == 0);
        argument.as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 8, &argument, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 2
            && memcmp(panic.message.data, "no", 2) == 0);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 10, NULL, 0, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 7
            && memcmp(panic.message.data, "dynamic", 7) == 0);
        RUDA_EXPECT(ruda_symbol_id_equal(
            panic.symbol,
            ruda_checked_function(&checked, 10)->mir.symbol
        ));
        ruda_mir_panic_dispose(&panic);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "fn bad(): int { panic( ); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "( )",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): int { panic(\"a\", \"b\"); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "(\"a\", \"b\")",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): int { panic(1); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): int { panic; }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "panic",
        0
    );
}

void ruda_test_checker_string_match(void) {
    static const char source[] =
        "fn empty(): string { return \"\"; }\n"
        "fn ready(): string { return \"ready\"; }\n"
        "fn embedded(): string { return \"a\\0b\"; }\n"
        "fn other(): string { return \"other\"; }\n"
        "fn classify(value: string): int {\n"
        "  return match value {\n"
        "    case \"\" => 0; case \"ready\" => 1;\n"
        "    case \"a\\0b\" => 2; case remaining => 3;\n"
        "  };\n"
        "}\n"
        "fn rawClassify(value: string): int {\n"
        "  return match value { case r\"ready\" => 4; case _ => 5; };\n"
        "}\n"
        "fn guarded(value: string, flag: bool): int {\n"
        "  return match value {\n"
        "    case \"ready\" if flag => 6; case _ => 7;\n"
        "  };\n"
        "}\n"
        "fn block(value: string): int {\n"
        "  return match value {\n"
        "    case \"ready\" => { yield 8; } case _ => 9;\n"
        "  };\n"
        "}\n"
        "fn composed(): string { return \"\\u{e9}\"; }\n"
        "fn decomposed(): string { return \"e\\u{301}\"; }\n"
        "fn accents(value: string): int {\n"
        "  return match value {\n"
        "    case \"\\u{e9}\" => 10; case \"e\\u{301}\" => 11; case _ => 12;\n"
        "  };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirValue arguments[2];
    RudaMirValue strings[4];
    RudaMirValue result;
    size_t index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/string-match",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedFunction *classify = ruda_checked_function(
            &checked,
            4
        );
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 11);
        for (index = 0; index < checked.functions.length; index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                &checked,
                index
            );
            RUDA_EXPECT(function != NULL);
            if (function != NULL) {
                RUDA_EXPECT(ruda_mir_verify(&function->mir)
                    == RUDA_MIR_VERIFY_OK);
            }
        }
        RUDA_EXPECT(classify != NULL);
        if (classify != NULL) {
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &classify->mir,
                RUDA_MIR_EQUAL_STRING
            ) == 3);
        }
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        for (index = 0; index < 4; index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                index,
                NULL,
                0,
                ruda_system_allocator(),
                options,
                &strings[index]
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(strings[index].type == RUDA_TYPE_STRING);
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                4,
                &strings[index],
                1,
                ruda_system_allocator(),
                options,
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == (int64_t)index);
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, &strings[1], 1, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 4);
        arguments[0] = strings[1];
        arguments[1].type = RUDA_TYPE_BOOL;
        arguments[1].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, arguments, 2, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 6);
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, arguments, 2, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 7);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, &strings[1], 1, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 8);
        for (index = 0; index < 2; index += 1) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                8 + index,
                NULL,
                0,
                ruda_system_allocator(),
                options,
                &arguments[0]
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                10,
                arguments,
                1,
                ruda_system_allocator(),
                options,
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.as.i64 == (int64_t)(10 + index));
        }
        arguments[0] = strings[1];
        arguments[0].as.reference = RUDA_MIR_INVALID_REFERENCE;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, arguments, 1, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "fn bad(value: string): int {\n"
        "  return match value { case \"ready\" => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case \"ready\" => 1; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: string): int {\n"
        "  return match value { case \"a\" => 1; case r\"a\" => 2; case _ => 3; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "r\"a\"",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: string): int {\n"
        "  return match value { case \"a\" | \"\\u{61}\" => 1; case _ => 2; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "\"\\u{61}\"",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: string): int {\n"
        "  return match value { case 1 => 1; case _ => 2; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(value: string): int {\n"
        "  return match value { case \"a\" if true => 1; };\n"
        "}\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match value { case \"a\" if true => 1; }",
        0
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: string): int {\n"
        "  return match value { case \"ruda#0\" => 1; };\n"
        "}\n",
        "string match is missing witness: \"ruda#1\""
    );
    ruda_checker_expect_diagnostic_message(
        "fn bad(value: string): int {\n"
        "  return match value { case \"ruda#0\" if true => 1; };\n"
        "}\n",
        "string match is missing witness: \"ruda#0\"; guarded patterns do not count toward exhaustiveness"
    );
}

void ruda_test_checker_declared_never_calls(void) {
    static const char source[] =
        "fn stop(message: string): never { panic(message); }\n"
        "fn direct(): int { stop(\"direct\"); }\n"
        "fn selected(value: bool): int {\n"
        "  return match value {\n"
        "    case true => stop(\"selected\"); case false => 7;\n"
        "  };\n"
        "}\n"
        "fn nested(): never { return stop(\"nested\"); }\n"
        "fn caller(): int { nested(); }\n"
        "fn accept(value: int): int { return value; }\n"
        "fn argument(): int { return accept(stop(\"argument\")); }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirPanic panic;
    RudaMirValue argument;
    RudaMirValue result;
    size_t function_index = 0;
    memset(&panic, 0, sizeof(panic));
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/declared-never",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 7);
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
        RUDA_EXPECT(ruda_checked_function(&checked, 0)->mir.result_type
            == RUDA_TYPE_NEVER);
        RUDA_EXPECT(ruda_checked_function(&checked, 3)->mir.result_type
            == RUDA_TYPE_NEVER);
        options.panic = &panic;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            NULL,
            0,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 6
            && memcmp(panic.message.data, "direct", 6) == 0);
        RUDA_EXPECT(ruda_symbol_id_equal(
            panic.symbol,
            ruda_checked_function(&checked, 0)->mir.symbol
        ));

        argument.type = RUDA_TYPE_BOOL;
        argument.as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 7);
        argument.as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 8
            && memcmp(panic.message.data, "selected", 8) == 0);

        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            NULL,
            0,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 6
            && memcmp(panic.message.data, "nested", 6) == 0);
        RUDA_EXPECT(ruda_symbol_id_equal(
            panic.symbol,
            ruda_checked_function(&checked, 0)->mir.symbol
        ));

        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            6,
            NULL,
            0,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.message.length == 8
            && memcmp(panic.message.data, "argument", 8) == 0);
        ruda_mir_panic_dispose(&panic);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn bad(): never {}\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/declared-never-missing",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckDiagnostic *diagnostic = ruda_check_diagnostic(
            &checked,
            0
        );
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_MISSING_RETURN
            ) == 0);
            RUDA_EXPECT(diagnostic->start == diagnostic->end);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
    ruda_checker_expect_one_diagnostic(
        "fn bad(): never { return; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "return;",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): never { return 1; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
}

static void ruda_test_checker_one_witness_allocation_failure(
    const char *source,
    const char *expected_message
) {
    RudaCheckerFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(source != NULL && expected_message != NULL);
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCheckResult checked;
        RudaCheckStatus status;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        status = ruda_check_ast(
            &fixture.ast,
            "test/witness-allocation",
            allocator,
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            const RudaCheckDiagnostic *diagnostic = NULL;
            reached_success = true;
            RUDA_EXPECT(checked.initialized);
            RUDA_EXPECT(checked.diagnostics.length == 1);
            diagnostic = ruda_check_diagnostic(&checked, 0);
            RUDA_EXPECT(diagnostic != NULL);
            if (diagnostic != NULL) {
                RUDA_EXPECT(strcmp(
                    diagnostic->message,
                    expected_message
                ) == 0);
            }
            ruda_check_result_dispose(&checked);
        } else {
            RUDA_EXPECT(status == RUDA_CHECK_ALLOCATION_FAILED);
            RUDA_EXPECT(!checked.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_witness_allocation_failures(void) {
    static const char enum_source[] =
        "enum Message { Quit, Write(int), Move { x: int; y: int; }, }\n"
        "fn bad(value: Message): int {\n"
        "  return match value { case Quit => 0; };\n"
        "}\n";
    static const char string_source[] =
        "fn bad(value: string): int {\n"
        "  return match value { case \"ruda#0\" => 0; };\n"
        "}\n";
    static const char record_source[] =
        "type Point { x: int; y: int; }\n"
        "fn bad(value: Point): int {\n"
        "  return match value { case Point { x, y } if true => x + y; };\n"
        "}\n";
    ruda_test_checker_one_witness_allocation_failure(
        enum_source,
        "enum match is missing witnesses: Write(_), Move { x: _, y: _ }"
    );
    ruda_test_checker_one_witness_allocation_failure(
        string_source,
        "string match is missing witness: \"ruda#1\""
    );
    ruda_test_checker_one_witness_allocation_failure(
        record_source,
        "record match is missing witness: Point { x: _, y: _ }; guarded patterns do not count toward exhaustiveness"
    );
}

void ruda_test_checker_calls(void) {
    static const char source[] =
        "fn caller(value: int): int { return double(value) + one(); }\n"
        "fn one(): int { return 1; }\n"
        "fn double(value: int): int { return value + value; }\n"
        "fn factorial(value: int): int {\n"
        "  if value <= 1 { return 1; }\n"
        "  return value * factorial(value - 1);\n"
        "}\n"
        "fn noop(): void {}\n"
        "fn entry(): void { noop(); }\n";
    static const char wrong_arity[] =
        "fn target(value: int): int { return value; }\n"
        "fn bad(): int { return target(); }\n";
    static const char wrong_type[] =
        "fn target(value: int): int { return value; }\n"
        "fn bad(): int { return target(true); }\n";
    static const char unknown[] =
        "fn bad(): int { return absent(1); }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue argument;
    RudaMirValue result;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    const RudaCheckedFunction *caller = NULL;
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
        RUDA_EXPECT(checked.functions.length == 6);
        RUDA_EXPECT(checked.function_index.length == 6);
        caller = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(caller != NULL);
        if (caller != NULL) {
            RUDA_EXPECT(caller->mir.calls.length == 2);
            RUDA_EXPECT(caller->mir.call_arguments.length == 1);
        }
        argument.type = RUDA_TYPE_I64;
        argument.as.i64 = 5;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64);
        RUDA_EXPECT(result.as.i64 == 11);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 120);
        options.max_total_steps = 2;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_STEP_LIMIT);
        options = ruda_check_interpret_default_options();
        options.max_call_depth = 2;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_STEP_LIMIT);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            5,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_VOID);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
    ruda_checker_expect_one_diagnostic(
        wrong_arity,
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "()",
        1
    );
    ruda_checker_expect_one_diagnostic(
        wrong_type,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        unknown,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "absent",
        0
    );
}

void ruda_test_checker_top_level_constants(void) {
    static const char source[] =
        "const answer: int = later + 1;\n"
        "const later = 41;\n"
        "const ready = answer == 42;\n"
        "fn entry(): int {\n"
        "  if ready { return answer; }\n"
        "  return 0;\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedConstant *answer = NULL;
    const RudaCheckedConstant *later = NULL;
    const RudaCheckedConstant *ready = NULL;
    RudaMirValue result;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/constants",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.constants.length == 3);
        RUDA_EXPECT(checked.functions.length == 1);
        answer = ruda_checked_constant(&checked, 0);
        later = ruda_checked_constant(&checked, 1);
        ready = ruda_checked_constant(&checked, 2);
        RUDA_EXPECT(answer != NULL);
        RUDA_EXPECT(later != NULL);
        RUDA_EXPECT(ready != NULL);
        if (answer != NULL && later != NULL && ready != NULL) {
            RUDA_EXPECT(answer->name.length == strlen("answer"));
            RUDA_EXPECT(answer->value.type == RUDA_TYPE_I64);
            RUDA_EXPECT(answer->value.as.i64 == 42);
            RUDA_EXPECT(later->value.type == RUDA_TYPE_I64);
            RUDA_EXPECT(later->value.as.i64 == 41);
            RUDA_EXPECT(ready->value.type == RUDA_TYPE_BOOL);
            RUDA_EXPECT(ready->value.as.boolean);
            RUDA_EXPECT(!ruda_symbol_id_equal(
                answer->symbol,
                later->symbol
            ));
        }
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64);
        RUDA_EXPECT(result.as.i64 == 42);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_constant_diagnostics(void) {
    static const char cycle[] =
        "const first: int = second;\n"
        "const second: int = first;\n"
        "fn entry(): int { return first; }\n";
    static const char overflow[] =
        "const bad: int = 9223372036854775807 + 1;\n"
        "fn entry(): int { return bad; }\n";
    static const char division_by_zero[] =
        "const bad: int = 1 / 0;\n"
        "fn entry(): int { return bad; }\n";
    static const char call[] =
        "fn one(): int { return 1; }\n"
        "const bad: int = one();\n";
    static const char mismatch[] =
        "const bad: bool = 1;\n"
        "fn entry(): bool { return bad; }\n";
    static const char unknown[] =
        "const bad: int = missing;\n"
        "fn entry(): int { return bad; }\n";
    static const char duplicate[] =
        "const repeated: int = 1;\n"
        "fn repeated(): int { return 2; }\n";
    static const char called_constant[] =
        "const value: int = 1;\n"
        "fn bad(): int { return value(); }\n";
    static const char body_error_after_constant[] =
        "const value: int = 1;\n"
        "fn bad(): int { return missing; }\n";
    static const char invalid_logical_constant[] =
        "const bad = true || 1;\n";
    static const char limits[] =
        "const first: int = second;\n"
        "const second: int = 1;\n"
        "fn entry(): int { return first; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckOptions options;
    const RudaCheckDiagnostic *diagnostic = NULL;

    ruda_checker_expect_one_diagnostic(
        cycle,
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "first",
        1
    );
    ruda_checker_expect_one_diagnostic(
        overflow,
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "9223372036854775807 + 1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        division_by_zero,
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "1 / 0",
        0
    );
    ruda_checker_expect_one_diagnostic(
        call,
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "one()",
        1
    );
    ruda_checker_expect_one_diagnostic(
        mismatch,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        unknown,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        duplicate,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "repeated",
        1
    );
    ruda_checker_expect_one_diagnostic(
        called_constant,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "value",
        1
    );
    ruda_checker_expect_one_diagnostic(
        body_error_after_constant,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        invalid_logical_constant,
        RUDA_DIAGNOSTIC_INVALID_OPERATOR,
        "||",
        0
    );

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, limits));
    options = ruda_check_default_options();
    options.max_constant_depth = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/constants",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.constants.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_CONSTANT_EVALUATION
            ) == 0);
        }
        ruda_check_result_dispose(&checked);
    }
    options = ruda_check_default_options();
    options.max_constant_steps = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/constants",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.constants.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_CONSTANT_EVALUATION
            ) == 0);
        }
        ruda_check_result_dispose(&checked);
    }
    options = ruda_check_default_options();
    options.max_constants = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/constants",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

}
