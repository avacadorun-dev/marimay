#include "test_checker_support.h"

void ruda_test_checker_vertical_slice(void) {
    static const char source[] =
        "fn calc(a: int, b: int, c: int): int {\n"
        "  return a + b * c;\n"
        "}\n"
        "fn atLeast(a: int, b: int): bool {\n"
        "  return a >= b;\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *calc = NULL;
    const RudaCheckedFunction *at_least = NULL;
    const RudaTypeDescriptor *signature = NULL;
    RudaMirValue arguments[3];
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
        RUDA_EXPECT(checked.functions.length == 2);
        calc = ruda_checked_function(&checked, 0);
        at_least = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(calc != NULL);
        RUDA_EXPECT(at_least != NULL);
        if (calc != NULL) {
            RUDA_EXPECT(calc->name.length == strlen("calc"));
            RUDA_EXPECT(memcmp(calc->name.data, "calc", strlen("calc")) == 0);
            signature = ruda_type_descriptor(
                &checked.types,
                calc->signature_type
            );
            RUDA_EXPECT(signature != NULL);
            if (signature != NULL) {
                RUDA_EXPECT(signature->kind == RUDA_TYPE_KIND_FUNCTION);
                RUDA_EXPECT(signature->parameter_count == 3);
                RUDA_EXPECT(signature->argument_count == 4);
                RUDA_EXPECT(signature->arguments[0] == RUDA_TYPE_I64);
                RUDA_EXPECT(signature->arguments[1] == RUDA_TYPE_I64);
                RUDA_EXPECT(signature->arguments[2] == RUDA_TYPE_I64);
                RUDA_EXPECT(signature->arguments[3] == RUDA_TYPE_I64);
            }
            arguments[0].type = RUDA_TYPE_I64;
            arguments[0].as.i64 = 2;
            arguments[1].type = RUDA_TYPE_I64;
            arguments[1].as.i64 = 3;
            arguments[2].type = RUDA_TYPE_I64;
            arguments[2].as.i64 = 4;
            RUDA_EXPECT(ruda_mir_interpret(
                &calc->mir,
                arguments,
                3,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 14);
        }
        if (at_least != NULL) {
            arguments[0].type = RUDA_TYPE_I64;
            arguments[0].as.i64 = 4;
            arguments[1].type = RUDA_TYPE_I64;
            arguments[1].as.i64 = 3;
            RUDA_EXPECT(ruda_mir_interpret(
                &at_least->mir,
                arguments,
                2,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_BOOL);
            RUDA_EXPECT(result.as.boolean);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_diagnostics(void) {
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
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckDiagnostic *diagnostic = NULL;

    ruda_checker_expect_one_diagnostic(
        unknown,
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        duplicate,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "value",
        1
    );
    ruda_checker_expect_one_diagnostic(
        mismatch,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        immutable,
        RUDA_DIAGNOSTIC_ASSIGN_IMMUTABLE,
        "value",
        1
    );
    ruda_checker_expect_one_diagnostic(
        uninitialized,
        RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
        "value",
        1
    );
    ruda_checker_expect_one_diagnostic(
        out_of_range,
        RUDA_DIAGNOSTIC_INTEGER_LITERAL_RANGE,
        "9223372036854775808",
        0
    );
    ruda_checker_expect_one_diagnostic(
        unknown_type,
        RUDA_DIAGNOSTIC_UNKNOWN_TYPE,
        "Mystery",
        0
    );
    ruda_checker_expect_one_diagnostic(
        invalid_operator,
        RUDA_DIAGNOSTIC_INVALID_OPERATOR,
        "+",
        0
    );
    ruda_checker_expect_one_diagnostic(
        duplicate_parameter,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "value",
        1
    );

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "fn bad(): int { const value: int = 1; }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
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
}

void ruda_test_checker_signature_phase(void) {
    static const char duplicate_before_body[] =
        "fn repeated(): int { return unavailable(); }\n"
        "fn repeated(): int { return 1; }\n";
    static const char canonical[] =
        "fn first(value: int): int { return value; }\n"
        "fn second(value: int): int { return value; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *first = NULL;
    const RudaCheckedFunction *second = NULL;
    ruda_checker_expect_one_diagnostic(
        duplicate_before_body,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "repeated",
        1
    );
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, canonical));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/checker",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 2);
        first = ruda_checked_function(&checked, 0);
        second = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(first != NULL);
        RUDA_EXPECT(second != NULL);
        if (first != NULL && second != NULL) {
            RUDA_EXPECT(first->signature_type == second->signature_type);
            RUDA_EXPECT(!ruda_symbol_id_equal(
                first->mir.symbol,
                second->mir.symbol
            ));
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_function_attributes_and_panic_shadow(void) {
    static const char attributed[] =
        "@test\n"
        "fn verifies(): void { return; }\n"
        "@fuzz\n"
        "fn fuzzInput(input: int): void { const copied = input; }\n";
    static const char shadow[] =
        "fn panic(message: string): string { return message; }\n"
        "fn callShadow(): string { return panic(\"ordinary\"); }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *test_function = NULL;
    const RudaCheckedFunction *fuzz_function = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &fixture,
        "attributes_test.ruda",
        attributed
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/attributes",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 2);
        test_function = ruda_checked_function(&checked, 0);
        fuzz_function = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(test_function != NULL && test_function->is_test);
        RUDA_EXPECT(test_function != NULL && !test_function->is_fuzz);
        RUDA_EXPECT(fuzz_function != NULL && !fuzz_function->is_test);
        RUDA_EXPECT(fuzz_function != NULL && fuzz_function->is_fuzz);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, shadow));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/panic-shadow",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 2);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic_path(
        "invalid_test.ruda",
        "@test(1) fn invalid(): void {}\n",
        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "@test(1)"
    );
    ruda_checker_expect_one_diagnostic_path(
        "invalid_test.ruda",
        "@fuzz fn invalid(): void {}\n",
        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "invalid"
    );
    ruda_checker_expect_one_diagnostic_path(
        "invalid_test.ruda",
        "@test public fn invalid(): void {}\n",
        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "invalid"
    );
    ruda_checker_expect_one_diagnostic_path(
        "invalid_test.ruda",
        "@mystery fn invalid(): void {}\n",
        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "@mystery"
    );
    ruda_checker_expect_one_diagnostic_path(
        "invalid.ruda",
        "@test fn invalid(): void {}\n",
        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
        "invalid"
    );
}

void ruda_test_checker_contextual_word_symbols(void) {
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
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue result;
    const RudaCheckedFunction *total = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/contextual-word-symbols",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.records.length == 2);
        RUDA_EXPECT(checked.functions.length == 22);
        total = ruda_checked_function(&checked, 21);
        RUDA_EXPECT(total != NULL);
        if (total != NULL) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                21,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 231);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_package_sources(void) {
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
    RudaCheckerFixture entry;
    RudaCheckerFixture helper;
    const RudaAstTree *asts[2];
    RudaCheckResult checked;
    const RudaCheckedFunction *entry_function = NULL;
    const RudaCheckedFunction *helper_function = NULL;
    const RudaCheckedConstant *offset = NULL;
    const RudaCheckedTypeAlias *base = NULL;
    const RudaCheckedTypeAlias *counter = NULL;
    const RudaCheckDiagnostic *diagnostic = NULL;
    RudaSymbolId entry_symbol;
    RudaSymbolId helper_symbol;
    RudaSymbolId offset_symbol;
    RudaSymbolId base_symbol;
    RudaSymbolId counter_symbol;
    RudaMirValue argument;
    RudaMirValue value;
    RudaCheckOptions options;

    memset(&entry_symbol, 0, sizeof(entry_symbol));
    memset(&helper_symbol, 0, sizeof(helper_symbol));
    memset(&offset_symbol, 0, sizeof(offset_symbol));
    memset(&base_symbol, 0, sizeof(base_symbol));
    memset(&counter_symbol, 0, sizeof(counter_symbol));
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &entry,
        "a_entry.ruda",
        entry_source
    ));
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &helper,
        "z_helper.ruda",
        helper_source
    ));
    asts[0] = &helper.ast;
    asts[1] = &entry.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.snapshot == entry.ast.snapshot);
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 2);
        RUDA_EXPECT(checked.constants.length == 1);
        RUDA_EXPECT(checked.type_aliases.length == 2);
        RUDA_EXPECT(checked.function_index.length == 2);
        entry_function = ruda_checked_function(&checked, 0);
        helper_function = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(entry_function != NULL);
        RUDA_EXPECT(helper_function != NULL);
        offset = ruda_checked_constant(&checked, 0);
        base = ruda_checked_type_alias(&checked, 0);
        counter = ruda_checked_type_alias(&checked, 1);
        RUDA_EXPECT(offset != NULL);
        RUDA_EXPECT(base != NULL);
        RUDA_EXPECT(counter != NULL);
        if (entry_function != NULL && helper_function != NULL) {
            RUDA_EXPECT(entry_function->name.length == strlen("entry"));
            RUDA_EXPECT(memcmp(
                entry_function->name.data,
                "entry",
                strlen("entry")
            ) == 0);
            RUDA_EXPECT(helper_function->name.length == strlen("helper"));
            entry_symbol = entry_function->mir.symbol;
            helper_symbol = helper_function->mir.symbol;
            if (offset != NULL) {
                RUDA_EXPECT(offset->name.length == strlen("offset"));
                RUDA_EXPECT(offset->value.type == RUDA_TYPE_I64);
                RUDA_EXPECT(offset->value.as.i64 == 1);
                offset_symbol = offset->symbol;
            }
            if (base != NULL && counter != NULL) {
                RUDA_EXPECT(base->target_type == RUDA_TYPE_I64);
                RUDA_EXPECT(counter->target_type == RUDA_TYPE_I64);
                base_symbol = base->symbol;
                counter_symbol = counter->symbol;
            }
            argument.type = RUDA_TYPE_I64;
            argument.as.i64 = 41;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                0,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &value
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(value.type == RUDA_TYPE_I64);
            RUDA_EXPECT(value.as.i64 == 42);
        }
        ruda_check_result_dispose(&checked);
    }

    options = ruda_check_default_options();
    options.max_source_files = 1;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/package",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);

    asts[0] = &entry.ast;
    asts[1] = &helper.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        entry_function = ruda_checked_function(&checked, 0);
        helper_function = ruda_checked_function(&checked, 1);
        offset = ruda_checked_constant(&checked, 0);
        base = ruda_checked_type_alias(&checked, 0);
        counter = ruda_checked_type_alias(&checked, 1);
        RUDA_EXPECT(entry_function != NULL);
        RUDA_EXPECT(helper_function != NULL);
        RUDA_EXPECT(offset != NULL);
        RUDA_EXPECT(base != NULL);
        RUDA_EXPECT(counter != NULL);
        if (entry_function != NULL && helper_function != NULL) {
            RUDA_EXPECT(ruda_symbol_id_equal(
                entry_function->mir.symbol,
                entry_symbol
            ));
            RUDA_EXPECT(ruda_symbol_id_equal(
                helper_function->mir.symbol,
                helper_symbol
            ));
            if (offset != NULL) {
                RUDA_EXPECT(ruda_symbol_id_equal(
                    offset->symbol,
                    offset_symbol
                ));
            }
            if (base != NULL && counter != NULL) {
                RUDA_EXPECT(ruda_symbol_id_equal(base->symbol, base_symbol));
                RUDA_EXPECT(ruda_symbol_id_equal(
                    counter->symbol,
                    counter_symbol
                ));
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&helper);
    ruda_checker_fixture_dispose(&entry);

    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &entry,
        "a_first.ruda",
        first_duplicate
    ));
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &helper,
        "z_second.ruda",
        second_duplicate
    ));
    asts[0] = &helper.ast;
    asts[1] = &entry.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        RUDA_EXPECT(checked.function_index.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME
            ) == 0);
            RUDA_EXPECT(diagnostic->snapshot == helper.ast.snapshot);
            RUDA_EXPECT(strcmp(
                ruda_source_path(diagnostic->snapshot),
                "z_second.ruda"
            ) == 0);
        }
        ruda_check_result_dispose(&checked);
    }

    asts[0] = &entry.ast;
    asts[1] = &entry.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INVALID_ARGUMENT);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&helper);
    ruda_checker_fixture_dispose(&entry);
}

void ruda_test_checker_declaration_index_growth(void) {
    char source[32768];
    size_t length = 0;
    size_t index = 0;
    int written = 0;
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue value;
    const RudaCheckDiagnostic *diagnostic = NULL;

    written = snprintf(
        source,
        sizeof(source),
        "fn entry(): int { return function_255(); }\n"
    );
    RUDA_EXPECT(written > 0 && (size_t)written < sizeof(source));
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
        RUDA_EXPECT(written > 0
            && (size_t)written < sizeof(source) - length);
        if (written <= 0 || (size_t)written >= sizeof(source) - length) {
            return;
        }
        length += (size_t)written;
    }
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/index-growth",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 257);
        RUDA_EXPECT(checked.function_index.length == 257);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(value.type == RUDA_TYPE_I64);
        RUDA_EXPECT(value.as.i64 == 255);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    written = snprintf(
        source + length,
        sizeof(source) - length,
        "fn function_127(): int { return 999; }\n"
    );
    RUDA_EXPECT(written > 0 && (size_t)written < sizeof(source) - length);
    if (written <= 0 || (size_t)written >= sizeof(source) - length) {
        return;
    }
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/index-growth",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        diagnostic = ruda_check_diagnostic(&checked, 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->code,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME
            ) == 0);
            RUDA_EXPECT(diagnostic->snapshot == fixture.ast.snapshot);
            RUDA_EXPECT(diagnostic->end - diagnostic->start
                == strlen("function_127"));
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_type_aliases(void) {
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
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckOptions options;
    const RudaCheckedTypeAlias *user_id = NULL;
    const RudaCheckedTypeAlias *later = NULL;
    const RudaCheckedConstant *answer = NULL;
    const RudaCheckedFunction *identity = NULL;
    const RudaTypeDescriptor *signature = NULL;
    RudaMirValue argument;
    RudaMirValue result;

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/type-aliases",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.type_aliases.length == 2);
        RUDA_EXPECT(checked.constants.length == 1);
        RUDA_EXPECT(checked.functions.length == 1);
        user_id = ruda_checked_type_alias(&checked, 0);
        later = ruda_checked_type_alias(&checked, 1);
        answer = ruda_checked_constant(&checked, 0);
        identity = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(user_id != NULL);
        RUDA_EXPECT(later != NULL);
        RUDA_EXPECT(answer != NULL);
        RUDA_EXPECT(identity != NULL);
        if (user_id != NULL && later != NULL) {
            RUDA_EXPECT(user_id->target_type == RUDA_TYPE_I64);
            RUDA_EXPECT(later->target_type == RUDA_TYPE_I64);
            RUDA_EXPECT(!ruda_symbol_id_equal(
                user_id->symbol,
                later->symbol
            ));
        }
        if (answer != NULL) {
            RUDA_EXPECT(answer->value.type == RUDA_TYPE_I64);
            RUDA_EXPECT(answer->value.as.i64 == 42);
        }
        if (identity != NULL) {
            signature = ruda_type_descriptor(
                &checked.types,
                identity->signature_type
            );
            RUDA_EXPECT(signature != NULL);
            if (signature != NULL) {
                RUDA_EXPECT(signature->argument_count == 2);
                RUDA_EXPECT(signature->arguments[0] == RUDA_TYPE_I64);
                RUDA_EXPECT(signature->arguments[1] == RUDA_TYPE_I64);
            }
            argument.type = RUDA_TYPE_I64;
            argument.as.i64 = 17;
            RUDA_EXPECT(ruda_mir_interpret(
                &identity->mir,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_mir_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 17);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        cycle,
        RUDA_DIAGNOSTIC_TYPE_ALIAS_CYCLE,
        "First",
        1
    );
    ruda_checker_expect_one_diagnostic(
        unknown,
        RUDA_DIAGNOSTIC_UNKNOWN_TYPE,
        "Missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        primitive_redefinition,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "int",
        0
    );
    ruda_checker_expect_one_diagnostic(
        duplicate_value,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "Shared",
        0
    );

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    options = ruda_check_default_options();
    options.max_type_aliases = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/type-aliases",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.max_type_alias_depth = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/type-aliases",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "type Generic<T> = T;\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/type-aliases",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

}
