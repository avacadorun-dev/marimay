#include "test_checker_support.h"

void mmay_test_checker_vertical_slice(void) {
    static const char source[] =
        "fn calc(a: int, b: int, c: int): int {\n"
        "  return a + b * c;\n"
        "}\n"
        "fn atLeast(a: int, b: int): bool {\n"
        "  return a >= b;\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *calc = NULL;
    const MarimayCheckedFunction *at_least = NULL;
    const MarimayTypeDescriptor *signature = NULL;
    MarimayMirValue arguments[3];
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
        MMAY_EXPECT(checked.functions.length == 2);
        calc = mmay_checked_function(&checked, 0);
        at_least = mmay_checked_function(&checked, 1);
        MMAY_EXPECT(calc != NULL);
        MMAY_EXPECT(at_least != NULL);
        if (calc != NULL) {
            MMAY_EXPECT(calc->name.length == strlen("calc"));
            MMAY_EXPECT(memcmp(calc->name.data, "calc", strlen("calc")) == 0);
            signature = mmay_type_descriptor(
                &checked.types,
                calc->signature_type
            );
            MMAY_EXPECT(signature != NULL);
            if (signature != NULL) {
                MMAY_EXPECT(signature->kind == MMAY_TYPE_KIND_FUNCTION);
                MMAY_EXPECT(signature->parameter_count == 3);
                MMAY_EXPECT(signature->argument_count == 4);
                MMAY_EXPECT(signature->arguments[0] == MMAY_TYPE_I64);
                MMAY_EXPECT(signature->arguments[1] == MMAY_TYPE_I64);
                MMAY_EXPECT(signature->arguments[2] == MMAY_TYPE_I64);
                MMAY_EXPECT(signature->arguments[3] == MMAY_TYPE_I64);
            }
            arguments[0].type = MMAY_TYPE_I64;
            arguments[0].as.i64 = 2;
            arguments[1].type = MMAY_TYPE_I64;
            arguments[1].as.i64 = 3;
            arguments[2].type = MMAY_TYPE_I64;
            arguments[2].as.i64 = 4;
            MMAY_EXPECT(mmay_mir_interpret(
                &calc->mir,
                arguments,
                3,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 14);
        }
        if (at_least != NULL) {
            arguments[0].type = MMAY_TYPE_I64;
            arguments[0].as.i64 = 4;
            arguments[1].type = MMAY_TYPE_I64;
            arguments[1].as.i64 = 3;
            MMAY_EXPECT(mmay_mir_interpret(
                &at_least->mir,
                arguments,
                2,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_BOOL);
            MMAY_EXPECT(result.as.boolean);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_diagnostics(void) {
    static const char unknown[] =
        "fn bad(): int { return missing; }\n";
    static const char duplicate[] =
        "fn bad(): int { const value: int = 1; let value: int = 2; return value; }\n";
    static const char mismatch[] =
        "fn bad(): int { return true; }\n";
    static const char immutable[] =
        "fn bad(): int { const value: int = 1; value = 2; return value; }\n";
    static const char uninitialized[] =
        "fn bad(): int { let value: int; return value; }\n";
    static const char out_of_range[] =
        "fn bad(): int { return 9223372036854775808; }\n";
    static const char unknown_type[] =
        "fn bad(value: Mystery): int { return 0; }\n";
    static const char invalid_operator[] =
        "fn bad(): int { return true + false; }\n";
    static const char duplicate_parameter[] =
        "fn bad(value: int, value: int): int { return value; }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckDiagnostic *diagnostic = NULL;

    mmay_checker_expect_one_diagnostic(
        unknown,
        MMAY_DIAGNOSTIC_UNKNOWN_NAME,
        "missing",
        0
    );
    mmay_checker_expect_one_diagnostic(
        duplicate,
        MMAY_DIAGNOSTIC_DUPLICATE_NAME,
        "value",
        1
    );
    mmay_checker_expect_one_diagnostic(
        mismatch,
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    mmay_checker_expect_one_diagnostic(
        immutable,
        MMAY_DIAGNOSTIC_ASSIGN_IMMUTABLE,
        "value",
        1
    );
    mmay_checker_expect_one_diagnostic(
        uninitialized,
        MMAY_DIAGNOSTIC_UNINITIALIZED_LOCAL,
        "value",
        1
    );
    mmay_checker_expect_one_diagnostic(
        out_of_range,
        MMAY_DIAGNOSTIC_INTEGER_LITERAL_RANGE,
        "9223372036854775808",
        0
    );
    mmay_checker_expect_one_diagnostic(
        unknown_type,
        MMAY_DIAGNOSTIC_UNKNOWN_TYPE,
        "Mystery",
        0
    );
    mmay_checker_expect_one_diagnostic(
        invalid_operator,
        MMAY_DIAGNOSTIC_INVALID_OPERATOR,
        "+",
        0
    );
    mmay_checker_expect_one_diagnostic(
        duplicate_parameter,
        MMAY_DIAGNOSTIC_DUPLICATE_NAME,
        "value",
        1
    );

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "fn bad(): int { const value: int = 1; }\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 1);
        MMAY_EXPECT(checked.functions.length == 0);
        diagnostic = mmay_check_diagnostic(&checked, 0);
        MMAY_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            MMAY_EXPECT(strcmp(
                diagnostic->code,
                MMAY_DIAGNOSTIC_MISSING_RETURN
            ) == 0);
            MMAY_EXPECT(diagnostic->start == diagnostic->end);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_signature_phase(void) {
    static const char duplicate_before_body[] =
        "fn repeated(): int { return unavailable(); }\n"
        "fn repeated(): int { return 1; }\n";
    static const char canonical[] =
        "fn first(value: int): int { return value; }\n"
        "fn second(value: int): int { return value; }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *first = NULL;
    const MarimayCheckedFunction *second = NULL;
    mmay_checker_expect_one_diagnostic(
        duplicate_before_body,
        MMAY_DIAGNOSTIC_DUPLICATE_NAME,
        "repeated",
        1
    );
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, canonical));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/checker",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 2);
        first = mmay_checked_function(&checked, 0);
        second = mmay_checked_function(&checked, 1);
        MMAY_EXPECT(first != NULL);
        MMAY_EXPECT(second != NULL);
        if (first != NULL && second != NULL) {
            MMAY_EXPECT(first->signature_type == second->signature_type);
            MMAY_EXPECT(!mmay_symbol_id_equal(
                first->mir.symbol,
                second->mir.symbol
            ));
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_function_attributes_and_panic_shadow(void) {
    static const char attributed[] =
        "@test\n"
        "fn verifies(): void { return; }\n"
        "@test\n"
        "fn verifiesArguments(arguments: []string): void {\n"
        "  const count = arguments.length();\n"
        "}\n"
        "@fuzz\n"
        "fn fuzzInput(input: int): void { const copied = input; }\n";
    static const char shadow[] =
        "fn panic(message: string): string { return message; }\n"
        "fn callShadow(): string { return panic(\"ordinary\"); }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *test_function = NULL;
    const MarimayCheckedFunction *argument_test_function = NULL;
    const MarimayCheckedFunction *fuzz_function = NULL;
    MMAY_EXPECT(mmay_checker_fixture_init_path(
        &fixture,
        "attributes_test.my",
        attributed
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/attributes",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 3);
        test_function = mmay_checked_function(&checked, 0);
        argument_test_function = mmay_checked_function(&checked, 1);
        fuzz_function = mmay_checked_function(&checked, 2);
        MMAY_EXPECT(test_function != NULL && test_function->is_test);
        MMAY_EXPECT(test_function != NULL && !test_function->is_fuzz);
        MMAY_EXPECT(argument_test_function != NULL
            && argument_test_function->is_test
            && argument_test_function->mir.parameter_count == 1);
        MMAY_EXPECT(fuzz_function != NULL && !fuzz_function->is_test);
        MMAY_EXPECT(fuzz_function != NULL && fuzz_function->is_fuzz);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, shadow));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/panic-shadow",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 2);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic_path(
        "invalid_test.my",
        "@test(1) fn invalid(): void {}\n",
        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "@test(1)"
    );
    mmay_checker_expect_one_diagnostic_path(
        "invalid_test.my",
        "@fuzz fn invalid(): void {}\n",
        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "invalid"
    );
    mmay_checker_expect_one_diagnostic_path(
        "invalid_test.my",
        "@test public fn invalid(): void {}\n",
        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "invalid"
    );
    mmay_checker_expect_one_diagnostic_path(
        "invalid_test.my",
        "@test fn invalid(values: []int): void {}\n",
        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "invalid"
    );
    mmay_checker_expect_one_diagnostic_path(
        "invalid_test.my",
        "@mystery fn invalid(): void {}\n",
        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "@mystery"
    );
    mmay_checker_expect_one_diagnostic_path(
        "invalid.my",
        "@test fn invalid(): void {}\n",
        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "invalid"
    );
}

void mmay_test_checker_contextual_word_symbols(void) {
    static const char source[] =
        "type Context { as: int; break: int; case: int; const: int; "
        "continue: int; else: int; enum: int; extern: int; fn: int; "
        "for: int; from: int; import: int; interface: int; let: int; "
        "mutable: int; of: int; public: int; scope: int; type: int; "
        "unsafe: int; yield: int; }\n"
        "type C17Words { auto: int; break: int; case: int; char: int; "
        "const: int; continue: int; default: int; do: int; double: int; "
        "else: int; enum: int; extern: int; float: int; for: int; "
        "goto: int; inline: int; int: int; long: int; register: int; "
        "restrict: int; short: int; signed: int; sizeof: int; static: int; "
        "struct: int; switch: int; typedef: int; union: int; unsigned: int; "
        "void: int; volatile: int; }\n"
        "fn as(): int { return 1; }\n"
        "fn break(): int { return 2; }\n"
        "fn case(): int { return 3; }\n"
        "fn const(): int { return 4; }\n"
        "fn continue(): int { return 5; }\n"
        "fn else(): int { return 6; }\n"
        "fn enum(): int { return 7; }\n"
        "fn extern(): int { return 8; }\n"
        "fn fn(): int { return 9; }\n"
        "fn for(): int { return 10; }\n"
        "fn from(): int { return 11; }\n"
        "fn import(): int { return 12; }\n"
        "fn interface(): int { return 13; }\n"
        "fn let(): int { return 14; }\n"
        "fn mutable(): int { return 15; }\n"
        "fn of(): int { return 16; }\n"
        "fn public(): int { return 17; }\n"
        "fn scope(): int { return 18; }\n"
        "fn type(): int { return 19; }\n"
        "fn unsafe(): int { return 20; }\n"
        "fn yield(): int { return 21; }\n"
        "fn total(): int {\n"
        "  return as() + break() + case() + const() + continue() + else() "
        "+ enum() + extern() + fn() + for() + from() + import() "
        "+ interface() + let() + mutable() + of() + public() + scope() "
        "+ type() + unsafe() + yield();\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirValue result;
    const MarimayCheckedFunction *total = NULL;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/contextual-word-symbols",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.records.length == 2);
        MMAY_EXPECT(checked.functions.length == 22);
        total = mmay_checked_function(&checked, 21);
        MMAY_EXPECT(total != NULL);
        if (total != NULL) {
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                21,
                NULL,
                0,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 231);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_package_sources(void) {
    static const char entry_source[] =
        "fn entry(value: Counter): Counter { return helper(value) + offset; }\n";
    static const char helper_source[] =
        "type Base = int;\n"
        "type Counter = Base;\n"
        "const offset: Counter = 1;\n"
        "fn helper(value: Counter): Base { return value; }\n";
    static const char first_duplicate[] =
        "fn repeated(): int { return 1; }\n";
    static const char second_duplicate[] =
        "fn repeated(): int { return 2; }\n";
    MarimayCheckerFixture entry;
    MarimayCheckerFixture helper;
    const MarimayAstTree *asts[2];
    MarimayCheckResult checked;
    const MarimayCheckedFunction *entry_function = NULL;
    const MarimayCheckedFunction *helper_function = NULL;
    const MarimayCheckedConstant *offset = NULL;
    const MarimayCheckedTypeAlias *base = NULL;
    const MarimayCheckedTypeAlias *counter = NULL;
    const MarimayCheckDiagnostic *diagnostic = NULL;
    MarimaySymbolId entry_symbol;
    MarimaySymbolId helper_symbol;
    MarimaySymbolId offset_symbol;
    MarimaySymbolId base_symbol;
    MarimaySymbolId counter_symbol;
    MarimayMirValue argument;
    MarimayMirValue value;
    MarimayCheckOptions options;

    memset(&entry_symbol, 0, sizeof(entry_symbol));
    memset(&helper_symbol, 0, sizeof(helper_symbol));
    memset(&offset_symbol, 0, sizeof(offset_symbol));
    memset(&base_symbol, 0, sizeof(base_symbol));
    memset(&counter_symbol, 0, sizeof(counter_symbol));
    MMAY_EXPECT(mmay_checker_fixture_init_path(
        &entry,
        "a_entry.my",
        entry_source
    ));
    MMAY_EXPECT(mmay_checker_fixture_init_path(
        &helper,
        "z_helper.my",
        helper_source
    ));
    asts[0] = &helper.ast;
    asts[1] = &entry.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        2,
        "test/package",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.snapshot == entry.ast.snapshot);
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 2);
        MMAY_EXPECT(checked.constants.length == 1);
        MMAY_EXPECT(checked.type_aliases.length == 2);
        MMAY_EXPECT(checked.function_index.length == 2);
        entry_function = mmay_checked_function(&checked, 0);
        helper_function = mmay_checked_function(&checked, 1);
        MMAY_EXPECT(entry_function != NULL);
        MMAY_EXPECT(helper_function != NULL);
        offset = mmay_checked_constant(&checked, 0);
        base = mmay_checked_type_alias(&checked, 0);
        counter = mmay_checked_type_alias(&checked, 1);
        MMAY_EXPECT(offset != NULL);
        MMAY_EXPECT(base != NULL);
        MMAY_EXPECT(counter != NULL);
        if (entry_function != NULL && helper_function != NULL) {
            MMAY_EXPECT(entry_function->name.length == strlen("entry"));
            MMAY_EXPECT(memcmp(
                entry_function->name.data,
                "entry",
                strlen("entry")
            ) == 0);
            MMAY_EXPECT(helper_function->name.length == strlen("helper"));
            entry_symbol = entry_function->mir.symbol;
            helper_symbol = helper_function->mir.symbol;
            if (offset != NULL) {
                MMAY_EXPECT(offset->name.length == strlen("offset"));
                MMAY_EXPECT(offset->value.type == MMAY_TYPE_I64);
                MMAY_EXPECT(offset->value.as.i64 == 1);
                offset_symbol = offset->symbol;
            }
            if (base != NULL && counter != NULL) {
                MMAY_EXPECT(base->target_type == MMAY_TYPE_I64);
                MMAY_EXPECT(counter->target_type == MMAY_TYPE_I64);
                base_symbol = base->symbol;
                counter_symbol = counter->symbol;
            }
            argument.type = MMAY_TYPE_I64;
            argument.as.i64 = 41;
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                0,
                &argument,
                1,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &value
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(value.type == MMAY_TYPE_I64);
            MMAY_EXPECT(value.as.i64 == 42);
        }
        mmay_check_result_dispose(&checked);
    }

    options = mmay_check_default_options();
    options.max_source_files = 1;
    MMAY_EXPECT(mmay_check_package(
        asts,
        2,
        "test/package",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);

    asts[0] = &entry.ast;
    asts[1] = &helper.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        2,
        "test/package",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        entry_function = mmay_checked_function(&checked, 0);
        helper_function = mmay_checked_function(&checked, 1);
        offset = mmay_checked_constant(&checked, 0);
        base = mmay_checked_type_alias(&checked, 0);
        counter = mmay_checked_type_alias(&checked, 1);
        MMAY_EXPECT(entry_function != NULL);
        MMAY_EXPECT(helper_function != NULL);
        MMAY_EXPECT(offset != NULL);
        MMAY_EXPECT(base != NULL);
        MMAY_EXPECT(counter != NULL);
        if (entry_function != NULL && helper_function != NULL) {
            MMAY_EXPECT(mmay_symbol_id_equal(
                entry_function->mir.symbol,
                entry_symbol
            ));
            MMAY_EXPECT(mmay_symbol_id_equal(
                helper_function->mir.symbol,
                helper_symbol
            ));
            if (offset != NULL) {
                MMAY_EXPECT(mmay_symbol_id_equal(
                    offset->symbol,
                    offset_symbol
                ));
            }
            if (base != NULL && counter != NULL) {
                MMAY_EXPECT(mmay_symbol_id_equal(base->symbol, base_symbol));
                MMAY_EXPECT(mmay_symbol_id_equal(
                    counter->symbol,
                    counter_symbol
                ));
            }
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&helper);
    mmay_checker_fixture_dispose(&entry);

    MMAY_EXPECT(mmay_checker_fixture_init_path(
        &entry,
        "a_first.my",
        first_duplicate
    ));
    MMAY_EXPECT(mmay_checker_fixture_init_path(
        &helper,
        "z_second.my",
        second_duplicate
    ));
    asts[0] = &helper.ast;
    asts[1] = &entry.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        2,
        "test/package",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 1);
        MMAY_EXPECT(checked.functions.length == 0);
        MMAY_EXPECT(checked.function_index.length == 0);
        diagnostic = mmay_check_diagnostic(&checked, 0);
        MMAY_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            MMAY_EXPECT(strcmp(
                diagnostic->code,
                MMAY_DIAGNOSTIC_DUPLICATE_NAME
            ) == 0);
            MMAY_EXPECT(diagnostic->snapshot == helper.ast.snapshot);
            MMAY_EXPECT(strcmp(
                mmay_source_path(diagnostic->snapshot),
                "z_second.my"
            ) == 0);
        }
        mmay_check_result_dispose(&checked);
    }

    asts[0] = &entry.ast;
    asts[1] = &entry.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        2,
        "test/package",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_INVALID_ARGUMENT);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&helper);
    mmay_checker_fixture_dispose(&entry);
}

void mmay_test_checker_declaration_index_growth(void) {
    char source[32768];
    size_t length = 0;
    size_t index = 0;
    int written = 0;
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirValue value;
    const MarimayCheckDiagnostic *diagnostic = NULL;

    written = snprintf(
        source,
        sizeof(source),
        "fn entry(): int { return function_255(); }\n"
    );
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(source));
    if (written <= 0 || (size_t)written >= sizeof(source)) {
        return;
    }
    length = (size_t)written;
    for (index = 0; index < 256; index += 1) {
        written = snprintf(
            source + length,
            sizeof(source) - length,
            "fn function_%03zu(): int { return %zu; }\n",
            index,
            index
        );
        MMAY_EXPECT(written > 0
            && (size_t)written < sizeof(source) - length);
        if (written <= 0 || (size_t)written >= sizeof(source) - length) {
            return;
        }
        length += (size_t)written;
    }
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/index-growth",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 257);
        MMAY_EXPECT(checked.function_index.length == 257);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            0,
            NULL,
            0,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &value
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(value.type == MMAY_TYPE_I64);
        MMAY_EXPECT(value.as.i64 == 255);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    written = snprintf(
        source + length,
        sizeof(source) - length,
        "fn function_127(): int { return 999; }\n"
    );
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(source) - length);
    if (written <= 0 || (size_t)written >= sizeof(source) - length) {
        return;
    }
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/index-growth",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 1);
        MMAY_EXPECT(checked.functions.length == 0);
        diagnostic = mmay_check_diagnostic(&checked, 0);
        MMAY_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            MMAY_EXPECT(strcmp(
                diagnostic->code,
                MMAY_DIAGNOSTIC_DUPLICATE_NAME
            ) == 0);
            MMAY_EXPECT(diagnostic->snapshot == fixture.ast.snapshot);
            MMAY_EXPECT(diagnostic->end - diagnostic->start
                == strlen("function_127"));
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_type_aliases(void) {
    static const char source[] =
        "type UserId = Later;\n"
        "type Later = int;\n"
        "const answer: UserId = 42;\n"
        "fn identity(value: UserId): Later { return value; }\n";
    static const char cycle[] =
        "type First = Second;\n"
        "type Second = First;\n";
    static const char unknown[] =
        "type Known = Missing;\n";
    static const char primitive_redefinition[] =
        "type int = bool;\n";
    static const char duplicate_value[] =
        "fn Shared(): int { return 1; }\n"
        "type Shared = int;\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckOptions options;
    const MarimayCheckedTypeAlias *user_id = NULL;
    const MarimayCheckedTypeAlias *later = NULL;
    const MarimayCheckedConstant *answer = NULL;
    const MarimayCheckedFunction *identity = NULL;
    const MarimayTypeDescriptor *signature = NULL;
    MarimayMirValue argument;
    MarimayMirValue result;

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/type-aliases",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.type_aliases.length == 2);
        MMAY_EXPECT(checked.constants.length == 1);
        MMAY_EXPECT(checked.functions.length == 1);
        user_id = mmay_checked_type_alias(&checked, 0);
        later = mmay_checked_type_alias(&checked, 1);
        answer = mmay_checked_constant(&checked, 0);
        identity = mmay_checked_function(&checked, 0);
        MMAY_EXPECT(user_id != NULL);
        MMAY_EXPECT(later != NULL);
        MMAY_EXPECT(answer != NULL);
        MMAY_EXPECT(identity != NULL);
        if (user_id != NULL && later != NULL) {
            MMAY_EXPECT(user_id->target_type == MMAY_TYPE_I64);
            MMAY_EXPECT(later->target_type == MMAY_TYPE_I64);
            MMAY_EXPECT(!mmay_symbol_id_equal(
                user_id->symbol,
                later->symbol
            ));
        }
        if (answer != NULL) {
            MMAY_EXPECT(answer->value.type == MMAY_TYPE_I64);
            MMAY_EXPECT(answer->value.as.i64 == 42);
        }
        if (identity != NULL) {
            signature = mmay_type_descriptor(
                &checked.types,
                identity->signature_type
            );
            MMAY_EXPECT(signature != NULL);
            if (signature != NULL) {
                MMAY_EXPECT(signature->argument_count == 2);
                MMAY_EXPECT(signature->arguments[0] == MMAY_TYPE_I64);
                MMAY_EXPECT(signature->arguments[1] == MMAY_TYPE_I64);
            }
            argument.type = MMAY_TYPE_I64;
            argument.as.i64 = 17;
            MMAY_EXPECT(mmay_mir_interpret(
                &identity->mir,
                &argument,
                1,
                mmay_system_allocator(),
                mmay_mir_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 17);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        cycle,
        MMAY_DIAGNOSTIC_TYPE_ALIAS_CYCLE,
        "First",
        1
    );
    mmay_checker_expect_one_diagnostic(
        unknown,
        MMAY_DIAGNOSTIC_UNKNOWN_TYPE,
        "Missing",
        0
    );
    mmay_checker_expect_one_diagnostic(
        primitive_redefinition,
        MMAY_DIAGNOSTIC_DUPLICATE_NAME,
        "int",
        0
    );
    mmay_checker_expect_one_diagnostic(
        duplicate_value,
        MMAY_DIAGNOSTIC_DUPLICATE_NAME,
        "Shared",
        0
    );

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    options = mmay_check_default_options();
    options.max_type_aliases = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/type-aliases",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.max_type_alias_depth = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/type-aliases",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "type Generic<T> = T;\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/type-aliases",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_INCOMPLETE_LANGUAGE);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

}
