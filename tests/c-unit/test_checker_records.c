#include "test_checker_support.h"

void mmay_test_checker_record_declarations(void) {
    static const char source[] =
        "public type UserId = int;\n"
        "public type User {\n"
        "  public id: UserId,\n"
        "  mutable parent: User,\n"
        "}\n"
        "public type Shadow { public id: UserId, mutable parent: User, }\n"
        "fn identity(value: User): User { return value; }\n";
    static const char duplicate_field[] =
        "type User { id: int, id: string, }\n";
    static const char unknown_field_type[] =
        "type User { value: Missing, }\n";
    static const char primitive_redefinition[] =
        "type int { value: int, }\n";
    static const char duplicate_top_level[] =
        "type Shared = int;\n"
        "type Shared { value: int, }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckOptions options;
    const MarimayCheckedRecord *user = NULL;
    const MarimayCheckedRecord *shadow = NULL;
    const MarimayCheckedRecordField *id = NULL;
    const MarimayCheckedRecordField *parent = NULL;
    const MarimayCheckedFunction *identity = NULL;
    const MarimayTypeDescriptor *nominal = NULL;
    const MarimayTypeDescriptor *signature = NULL;

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/records",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.records.length == 2);
        MMAY_EXPECT(checked.type_aliases.length == 1);
        MMAY_EXPECT(checked.functions.length == 1);
        user = mmay_checked_record(&checked, 0);
        shadow = mmay_checked_record(&checked, 1);
        identity = mmay_checked_function(&checked, 0);
        MMAY_EXPECT(user != NULL);
        MMAY_EXPECT(shadow != NULL);
        MMAY_EXPECT(identity != NULL);
        if (user != NULL) {
            MMAY_EXPECT(user->name.length == strlen("User"));
            MMAY_EXPECT(memcmp(user->name.data, "User", strlen("User")) == 0);
            MMAY_EXPECT(user->is_public);
            MMAY_EXPECT(user->fields.length == 2);
            MMAY_EXPECT(mmay_type_kind(&checked.types, user->type)
                == MMAY_TYPE_KIND_NOMINAL);
            nominal = mmay_type_descriptor(&checked.types, user->type);
            MMAY_EXPECT(nominal != NULL);
            if (nominal != NULL) {
                MMAY_EXPECT(mmay_symbol_id_equal(nominal->symbol, user->symbol));
                MMAY_EXPECT(nominal->argument_count == 0);
            }
            id = mmay_vector_at_const(&user->fields, 0);
            parent = mmay_vector_at_const(&user->fields, 1);
            MMAY_EXPECT(id != NULL);
            MMAY_EXPECT(parent != NULL);
            if (id != NULL) {
                MMAY_EXPECT(id->name.length == strlen("id"));
                MMAY_EXPECT(id->type == MMAY_TYPE_I64);
                MMAY_EXPECT(id->is_public);
                MMAY_EXPECT(!id->is_mutable);
            }
            if (parent != NULL) {
                MMAY_EXPECT(parent->name.length == strlen("parent"));
                MMAY_EXPECT(parent->type == user->type);
                MMAY_EXPECT(!parent->is_public);
                MMAY_EXPECT(parent->is_mutable);
            }
        }
        if (user != NULL && shadow != NULL) {
            MMAY_EXPECT(shadow->is_public);
            MMAY_EXPECT(shadow->fields.length == user->fields.length);
            MMAY_EXPECT(shadow->type != user->type);
            MMAY_EXPECT(!mmay_symbol_id_equal(shadow->symbol, user->symbol));
        }
        if (user != NULL && identity != NULL) {
            signature = mmay_type_descriptor(
                &checked.types,
                identity->signature_type
            );
            MMAY_EXPECT(signature != NULL);
            if (signature != NULL) {
                MMAY_EXPECT(signature->argument_count == 2);
                MMAY_EXPECT(signature->arguments[0] == user->type);
                MMAY_EXPECT(signature->arguments[1] == user->type);
            }
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "type First { second: Second, }\n"
        "type Second { first: First, }\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/records",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        const MarimayCheckedRecord *first = mmay_checked_record(&checked, 0);
        const MarimayCheckedRecord *second = mmay_checked_record(&checked, 1);
        const MarimayCheckedRecordField *first_field = first != NULL
            ? mmay_vector_at_const(&first->fields, 0)
            : NULL;
        const MarimayCheckedRecordField *second_field = second != NULL
            ? mmay_vector_at_const(&second->fields, 0)
            : NULL;
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(first != NULL);
        MMAY_EXPECT(second != NULL);
        MMAY_EXPECT(first_field != NULL);
        MMAY_EXPECT(second_field != NULL);
        if (first != NULL && second != NULL && first_field != NULL
            && second_field != NULL) {
            MMAY_EXPECT(first_field->type == second->type);
            MMAY_EXPECT(second_field->type == first->type);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        duplicate_field,
        MMAY_DIAGNOSTIC_DUPLICATE_NAME,
        "id",
        1
    );
    mmay_checker_expect_one_diagnostic(
        unknown_field_type,
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
        duplicate_top_level,
        MMAY_DIAGNOSTIC_DUPLICATE_NAME,
        "Shared",
        1
    );

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    options = mmay_check_default_options();
    options.max_records = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/records",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.max_fields_per_record = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/records",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "type Generic<T> { value: T, }\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/records",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "@derive(Debug) type User { id: int, }\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/records",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_INCOMPLETE_LANGUAGE);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

}

void mmay_test_checker_record_package_order(void) {
    static const char entry_source[] =
        "fn keep(value: Node): Node { return value; }\n"
        "fn build(value: int): int {\n"
        "  const leaf = Leaf { value };\n"
        "  return leaf.value;\n"
        "}\n";
    static const char types_source[] =
        "type Alias = Node;\n"
        "type Node { public id: int, next: Alias, }\n"
        "type Leaf { value: int, }\n";
    MarimayCheckerFixture entry;
    MarimayCheckerFixture types;
    const MarimayAstTree *asts[2];
    MarimayCheckResult checked;
    const MarimayCheckedRecord *node = NULL;
    const MarimayCheckedTypeAlias *alias = NULL;
    const MarimayCheckedFunction *keep = NULL;
    MarimayMirValue argument;
    MarimayMirValue result;
    const MarimayTypeDescriptor *signature = NULL;
    MarimaySymbolId symbol;
    MarimayTypeId type = MMAY_TYPE_INVALID;
    memset(&symbol, 0, sizeof(symbol));

    MMAY_EXPECT(mmay_checker_fixture_init_path(
        &entry,
        "a_entry.my",
        entry_source
    ));
    MMAY_EXPECT(mmay_checker_fixture_init_path(
        &types,
        "z_types.my",
        types_source
    ));
    asts[0] = &types.ast;
    asts[1] = &entry.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        2,
        "test/record-package",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.records.length == 2);
        MMAY_EXPECT(checked.type_aliases.length == 1);
        MMAY_EXPECT(checked.functions.length == 2);
        node = mmay_checked_record(&checked, 0);
        alias = mmay_checked_type_alias(&checked, 0);
        keep = mmay_checked_function(&checked, 0);
        MMAY_EXPECT(node != NULL);
        MMAY_EXPECT(alias != NULL);
        MMAY_EXPECT(keep != NULL);
        if (node != NULL) {
            MMAY_EXPECT(node->fields.length == 2);
            symbol = node->symbol;
            type = node->type;
        }
        if (node != NULL && alias != NULL) {
            MMAY_EXPECT(alias->target_type == node->type);
        }
        if (node != NULL && keep != NULL) {
            signature = mmay_type_descriptor(&checked.types, keep->signature_type);
            MMAY_EXPECT(signature != NULL);
            if (signature != NULL) {
                MMAY_EXPECT(signature->arguments[0] == node->type);
                MMAY_EXPECT(signature->arguments[1] == node->type);
            }
        }
        argument.type = MMAY_TYPE_I64;
        argument.as.i64 = 42;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            1,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_I64);
        MMAY_EXPECT(result.as.i64 == 42);
        mmay_check_result_dispose(&checked);
    }

    asts[0] = &entry.ast;
    asts[1] = &types.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        2,
        "test/record-package",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        node = mmay_checked_record(&checked, 0);
        MMAY_EXPECT(node != NULL);
        if (node != NULL) {
            MMAY_EXPECT(mmay_symbol_id_equal(node->symbol, symbol));
            MMAY_EXPECT(node->type == type);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&types);
    mmay_checker_fixture_dispose(&entry);
}

void mmay_test_checker_record_execution(void) {
    static const char source[] =
        "type Counter {\n"
        "  seed: int,\n"
        "  mutable value: int,\n"
        "}\n"
        "type CounterAlias = Counter;\n"
        "fn make(seed: int): Counter {\n"
        "  return CounterAlias { value: seed, seed };\n"
        "}\n"
        "fn read(counter: Counter): int { return counter.value; }\n"
        "fn aliasing(seed: int): int {\n"
        "  const counter = Counter { seed, value: 1 };\n"
        "  const alias = counter;\n"
        "  counter.value += seed;\n"
        "  return alias.value;\n"
        "}\n"
        "fn adjust(counter: Counter, delta: int): int {\n"
        "  counter.value = counter.value + delta;\n"
        "  return counter.value;\n"
        "}\n"
        "fn acrossCall(seed: int): int {\n"
        "  const counter = make(seed);\n"
        "  const alias = counter;\n"
        "  adjust(counter, 1);\n"
        "  return alias.value;\n"
        "}\n"
        "fn overflow(): int { return 9223372036854775807 + 1; }\n"
        "fn divideZero(): int { return 1 / 0; }\n"
        "fn order(): int {\n"
        "  const counter = Counter { value: divideZero(), seed: overflow() };\n"
        "  return counter.value;\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *make = NULL;
    const MarimayCheckedFunction *read = NULL;
    const MarimayCheckedFunction *aliasing = NULL;
    const MarimayCheckedFunction *across_call = NULL;
    const MarimayCheckedFunction *order = NULL;
    MarimayMirValue argument;
    MarimayMirValue result;
    MarimayMirValue record_argument;
    MarimayCheckInterpretOptions interpret_options =
        mmay_check_interpret_default_options();
    MarimayMirExecution execution;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/record-execution",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.records.length == 1);
        MMAY_EXPECT(checked.type_aliases.length == 1);
        MMAY_EXPECT(checked.functions.length == 8);
        make = mmay_checked_function(&checked, 0);
        read = mmay_checked_function(&checked, 1);
        aliasing = mmay_checked_function(&checked, 2);
        across_call = mmay_checked_function(&checked, 4);
        order = mmay_checked_function(&checked, 7);
        MMAY_EXPECT(make != NULL);
        MMAY_EXPECT(read != NULL);
        MMAY_EXPECT(aliasing != NULL);
        MMAY_EXPECT(across_call != NULL);
        MMAY_EXPECT(order != NULL);
        if (make != NULL) {
            MMAY_EXPECT(make->mir.records.length == 1);
            MMAY_EXPECT(make->mir.record_fields.length == 2);
            MMAY_EXPECT(make->mir.record_constructions.length == 1);
            MMAY_EXPECT(make->mir.record_arguments.length == 2);
        }
        if (read != NULL) {
            MMAY_EXPECT(read->mir.records.length == 1);
            MMAY_EXPECT(read->mir.record_constructions.length == 0);
        }
        argument.type = MMAY_TYPE_I64;
        argument.as.i64 = 4;
        if (aliasing != NULL) {
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                2,
                &argument,
                1,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 5);
            {
                size_t failure_index = 0;
                bool reached_success = false;
                for (failure_index = 0; failure_index < 32;
                    failure_index += 1) {
                    MarimayFailAllocator state;
                    MarimayAllocator allocator;
                    MarimayMirInterpretStatus status;
                    mmay_fail_allocator_init(
                        &state,
                        mmay_system_allocator(),
                        failure_index
                    );
                    allocator = mmay_fail_allocator(&state);
                    status = mmay_check_interpret_function(
                        &checked,
                        2,
                        &argument,
                        1,
                        allocator,
                        mmay_check_interpret_default_options(),
                        &result
                    );
                    if (status == MMAY_MIR_INTERPRET_OK) {
                        reached_success = true;
                        MMAY_EXPECT(result.as.i64 == 5);
                    } else {
                        MMAY_EXPECT(
                            status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED
                        );
                    }
                    MMAY_EXPECT(state.live_allocations == 0);
                    MMAY_EXPECT(state.live_bytes == 0);
                    if (reached_success) {
                        break;
                    }
                }
                MMAY_EXPECT(reached_success);
            }
        }
        if (across_call != NULL) {
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                4,
                &argument,
                1,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 5);
        }
        if (order != NULL) {
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                7,
                NULL,
                0,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_DIVISION_BY_ZERO);
        }
        if (make != NULL && read != NULL) {
            argument.as.i64 = 7;
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                0,
                &argument,
                1,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_INVALID_ARGUMENT);
            MMAY_EXPECT(mmay_mir_execution_init(
                &execution,
                mmay_system_allocator(),
                mmay_mir_execution_default_options()
            ));
            interpret_options.execution = &execution;
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                0,
                &argument,
                1,
                mmay_system_allocator(),
                interpret_options,
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == make->mir.result_type);
            MMAY_EXPECT(mmay_mir_execution_owns_reference(
                &execution,
                result
            ));
            MMAY_EXPECT(execution.objects.length == 1);
            record_argument = result;
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                1,
                &record_argument,
                1,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_INVALID_ARGUMENT);
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                1,
                &record_argument,
                1,
                mmay_system_allocator(),
                interpret_options,
                &result
            ) == MMAY_MIR_INTERPRET_OK);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 7);
            record_argument.as.reference = MMAY_MIR_INVALID_REFERENCE;
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                1,
                &record_argument,
                1,
                mmay_system_allocator(),
                interpret_options,
                &result
            ) == MMAY_MIR_INTERPRET_INVALID_REFERENCE);
            mmay_mir_execution_dispose(&execution);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        "type Counter { value: int, }\n"
        "fn bad(): Counter { return Counter { other: 1 }; }\n",
        MMAY_DIAGNOSTIC_UNKNOWN_FIELD,
        "other",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Counter { value: int, }\n"
        "fn bad(): Counter { return Counter { value: 1, value: 2 }; }\n",
        MMAY_DIAGNOSTIC_DUPLICATE_FIELD,
        "value",
        2
    );
    mmay_checker_expect_one_diagnostic(
        "type Counter { seed: int, value: int, }\n"
        "fn bad(): Counter { return Counter { value: 1 }; }\n",
        MMAY_DIAGNOSTIC_MISSING_FIELD,
        "Counter { value: 1 }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Counter { value: int, }\n"
        "fn bad(): Counter { return Counter { value: true }; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Counter { seed: int, }\n"
        "fn bad(counter: Counter): int { counter.seed = 1; return 0; }\n",
        MMAY_DIAGNOSTIC_ASSIGN_IMMUTABLE,
        "seed",
        1
    );
    mmay_checker_expect_one_diagnostic(
        "type Counter { value: int, }\n"
        "fn bad(counter: Counter): int { return counter.other; }\n",
        MMAY_DIAGNOSTIC_UNKNOWN_FIELD,
        "other",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Counter { value: int, }\n"
        "fn bad(): Counter { return Counter { value }; }\n",
        MMAY_DIAGNOSTIC_UNKNOWN_NAME,
        "value",
        1
    );
}

void mmay_test_checker_record_identity(void) {
    static const char source[] =
        "type Point { mutable x: int, }\n"
        "type Other { x: int, }\n"
        "type PointAlias = Point;\n"
        "fn make(x: int): Point { return Point { x }; }\n"
        "fn same(left: PointAlias, right: Point): bool {\n"
        "  return identity.same(left, right);\n"
        "}\n"
        "fn self(x: int): bool {\n"
        "  const point = Point { x };\n"
        "  return identity.same(point, point);\n"
        "}\n"
        "fn distinct(x: int): bool {\n"
        "  return identity.same(Point { x }, Point { x });\n"
        "}\n"
        "fn aliasing(x: int): bool {\n"
        "  const point = Point { x };\n"
        "  const alias = point;\n"
        "  point.x += 1;\n"
        "  return identity.same(point, alias);\n"
        "}\n"
        "fn overflow(): int { return 9223372036854775807 + 1; }\n"
        "fn divideZero(): int { return 1 / 0; }\n"
        "fn order(): bool {\n"
        "  return identity.same(\n"
        "    Point { x: divideZero() },\n"
        "    Point { x: overflow() }\n"
        "  );\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirValue argument;
    MarimayMirValue first;
    MarimayMirValue second;
    MarimayMirValue comparison_arguments[2];
    MarimayMirValue result;
    MarimayMirExecution execution;
    MarimayCheckInterpretOptions options =
        mmay_check_interpret_default_options();
    const MarimayCheckedFunction *same = NULL;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/record-identity",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.records.length == 2);
        MMAY_EXPECT(checked.type_aliases.length == 1);
        MMAY_EXPECT(checked.functions.length == 8);
        same = mmay_checked_function(&checked, 1);
        MMAY_EXPECT(same != NULL);
        if (same != NULL) {
            MMAY_EXPECT(same->mir.records.length == 1);
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                &same->mir,
                MMAY_MIR_RECORD_IDENTITY_SAME
            ) == 1);
        }
        argument.type = MMAY_TYPE_I64;
        argument.as.i64 = 7;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_BOOL);
        MMAY_EXPECT(result.as.boolean);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            3,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(!result.as.boolean);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            4,
            &argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.boolean);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            7,
            NULL,
            0,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_DIVISION_BY_ZERO);

        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            mmay_mir_execution_default_options()
        ));
        options.execution = &execution;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            0,
            &argument,
            1,
            mmay_system_allocator(),
            options,
            &first
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            0,
            &argument,
            1,
            mmay_system_allocator(),
            options,
            &second
        ) == MMAY_MIR_INTERPRET_OK);
        comparison_arguments[0] = first;
        comparison_arguments[1] = first;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            1,
            comparison_arguments,
            2,
            mmay_system_allocator(),
            options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.boolean);
        comparison_arguments[1] = second;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            1,
            comparison_arguments,
            2,
            mmay_system_allocator(),
            options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(!result.as.boolean);
        comparison_arguments[1].as.reference = MMAY_MIR_INVALID_REFERENCE;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            1,
            comparison_arguments,
            2,
            mmay_system_allocator(),
            options,
            &result
        ) == MMAY_MIR_INTERPRET_INVALID_REFERENCE);
        mmay_mir_execution_dispose(&execution);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "type Other { x: int, }\n"
        "fn bad(): bool { return identity.same(Point { x: 1 }, Other { x: 1 }); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Other { x: 1 }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(): bool { return identity.same(1, 1); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "fn bad(): bool { return identity.same(\"a\", \"a\"); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "\"a\"",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "enum Choice { A, }\n"
        "fn bad(): bool { return identity.same(Choice.A, Choice.A); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Choice.A",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "fn bad(value: Point): bool { return identity.same(value); }\n",
        MMAY_DIAGNOSTIC_CALL_ARITY,
        "(value)",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "fn bad(value: Point): bool { return identity.same(value, value, value); }\n",
        MMAY_DIAGNOSTIC_CALL_ARITY,
        "(value, value, value)",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "fn bad(value: Point): bool { return identity.same<Point>(value, value); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "identity.same<Point>(value, value)",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "fn bad(value: Point): bool { return identity.same(value, 1); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "const same: bool = identity.same(Point { x: 1 }, Point { x: 1 });\n",
        MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
        "identity.same(Point { x: 1 }, Point { x: 1 })",
        0
    );
}

void mmay_test_checker_record_match(void) {
    static const char source[] =
        "type Point { x: int, y: int, }\n"
        "type Empty {}\n"
        "type PointAlias = Point;\n"
        "fn make(x: int, y: int): Point { return Point { x, y }; }\n"
        "fn sum(point: Point): int {\n"
        "  return match point { Point { x, y } => x + y, };\n"
        "}\n"
        "fn renamed(point: Point): int {\n"
        "  return match point { PointAlias { x: left, y: right } => left - right, };\n"
        "}\n"
        "fn ignored(point: Point): int {\n"
        "  return match point { Point { x: _, y } => y, };\n"
        "}\n"
        "fn guarded(point: Point, ready: bool): int {\n"
        "  return match point {\n"
        "    Point { x, y } if ready => x,\n"
        "    Point { x: _, y } => y,\n"
        "  };\n"
        "}\n"
        "fn whole(point: Point): Point {\n"
        "  return match point { _ as value => value, };\n"
        "}\n"
        "fn block(point: Point): int {\n"
        "  return match point { Point { x, y } => { x + y }, };\n"
        "}\n"
        "fn makeEmpty(): Empty { return Empty {}; }\n"
        "fn emptyValue(value: Empty): int {\n"
        "  return match value { Empty {} => 14, };\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayMirExecution execution;
    MarimayCheckInterpretOptions options = mmay_check_interpret_default_options();
    MarimayMirValue arguments[2];
    MarimayMirValue point;
    MarimayMirValue result;
    size_t index = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/record-match",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        const MarimayCheckedFunction *sum = mmay_checked_function(&checked, 1);
        const MarimayCheckedFunction *ignored = mmay_checked_function(&checked, 3);
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 9);
        MMAY_EXPECT(checked.records.length == 2);
        for (index = 0; index < checked.functions.length; index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                &checked,
                index
            );
            MMAY_EXPECT(function != NULL);
            if (function != NULL) {
                MMAY_EXPECT(mmay_mir_verify(&function->mir)
                    == MMAY_MIR_VERIFY_OK);
            }
        }
        MMAY_EXPECT(sum != NULL && ignored != NULL);
        if (sum != NULL) {
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                &sum->mir,
                MMAY_MIR_RECORD_GET
            ) == 2);
        }
        if (ignored != NULL) {
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                &ignored->mir,
                MMAY_MIR_RECORD_GET
            ) == 1);
        }
        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            mmay_mir_execution_default_options()
        ));
        options.execution = &execution;
        arguments[0].type = MMAY_TYPE_I64;
        arguments[0].as.i64 = 9;
        arguments[1].type = MMAY_TYPE_I64;
        arguments[1].as.i64 = 4;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 0, arguments, 2, mmay_system_allocator(), options,
            &point
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 1, &point, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 13);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 2, &point, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 5);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 3, &point, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 4);
        arguments[0] = point;
        arguments[1].type = MMAY_TYPE_BOOL;
        arguments[1].as.boolean = true;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 4, arguments, 2, mmay_system_allocator(), options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 9);
        arguments[1].as.boolean = false;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 4, arguments, 2, mmay_system_allocator(), options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 4);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 5, &point, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == point.type
            && mmay_mir_reference_equal(
                result.as.reference,
                point.as.reference
            ));
        arguments[0] = point;
        arguments[0].as.reference = MMAY_MIR_INVALID_REFERENCE;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 5, arguments, 1, mmay_system_allocator(), options,
            &result
        ) == MMAY_MIR_INTERPRET_INVALID_REFERENCE);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 6, &point, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 13);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 7, NULL, 0, mmay_system_allocator(), options, &point
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, 8, &point, 1, mmay_system_allocator(), options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.as.i64 == 14);
        mmay_mir_execution_dispose(&execution);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, y: int, }\n"
        "fn bad(point: Point): int { return match point { Point { x } => x, }; }\n",
        MMAY_DIAGNOSTIC_MISSING_FIELD,
        "{ x }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "fn bad(point: Point): int { return match point { Point { z } => z, }; }\n",
        MMAY_DIAGNOSTIC_UNKNOWN_FIELD,
        "z",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "fn bad(point: Point): int { return match point { Point { x, x } => x, }; }\n",
        MMAY_DIAGNOSTIC_DUPLICATE_FIELD,
        "x",
        2
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, y: int, }\n"
        "fn bad(point: Point): int { return match point { Point { x: value, y: value } => value, }; }\n",
        MMAY_DIAGNOSTIC_DUPLICATE_NAME,
        "value",
        1
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "type Other { x: int, }\n"
        "fn bad(point: Point): int { return match point { Other { x } => x, }; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Other",
        1
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "fn bad(point: Point): int { return match point { Point { x } | _ => x, }; }\n",
        MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
        "_",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "fn bad(point: Point): int { return match point { Point { x } if true => x, }; }\n",
        MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match point { Point { x } if true => x, }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "fn bad(point: Point): int { return match point { Point(x) => x, }; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Point(x)",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "type Point { x: int, }\n"
        "type Alias = int;\n"
        "fn bad(point: Point): int { return match point { Alias { x } => x, }; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Alias",
        1
    );
    mmay_checker_expect_diagnostic_message(
        "type Empty {}\n"
        "fn bad(value: Empty): int { return match value {}; }\n",
        "record match is missing witness: Empty {}"
    );
    mmay_checker_expect_diagnostic_message(
        "type Point { x: int, }\n"
        "fn bad(point: Point): int { return match point { Point { x } if true => x, }; }\n",
        "record match is missing witness: Point { x: _ }; guarded patterns do not count toward exhaustiveness"
    );
}

void mmay_test_checker_public_type_visibility(void) {
    static const char source[] =
        "public type PublicId = int;\n"
        "type PrivateId = int;\n"
        "public type PublicUser {\n"
        "  public id: PublicId,\n"
        "  internal: PrivateId,\n"
        "}\n"
        "type PrivateUser { public id: int, }\n"
        "public const answer: PublicId = 42;\n"
        "const hidden: PrivateId = 1;\n"
        "public fn identity(value: PublicUser): PublicUser { return value; }\n"
        "fn local(value: PrivateUser): PrivateUser { return value; }\n";
    static const char public_alias_private[] =
        "type Hidden = int;\n"
        "public type Exposed = Hidden;\n";
    static const char public_parameter_private[] =
        "type Hidden {}\n"
        "public fn expose(value: Hidden): void {}\n";
    static const char public_return_private[] =
        "type Hidden {}\n"
        "public fn expose(): Hidden {}\n";
    static const char public_field_private[] =
        "type Hidden {}\n"
        "public type Visible { public value: Hidden, internal: Hidden, }\n";
    static const char public_constant_private[] =
        "type Hidden = int;\n"
        "public const answer: Hidden = 1;\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedTypeAlias *public_id = NULL;
    const MarimayCheckedTypeAlias *private_id = NULL;
    const MarimayCheckedRecord *public_user = NULL;
    const MarimayCheckedRecord *private_user = NULL;
    const MarimayCheckedConstant *answer = NULL;
    const MarimayCheckedConstant *hidden = NULL;
    const MarimayCheckedFunction *identity = NULL;
    const MarimayCheckedFunction *local = NULL;

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/visibility",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.type_aliases.length == 2);
        MMAY_EXPECT(checked.records.length == 2);
        MMAY_EXPECT(checked.constants.length == 2);
        MMAY_EXPECT(checked.functions.length == 2);
        public_id = mmay_checked_type_alias(&checked, 0);
        private_id = mmay_checked_type_alias(&checked, 1);
        public_user = mmay_checked_record(&checked, 0);
        private_user = mmay_checked_record(&checked, 1);
        answer = mmay_checked_constant(&checked, 0);
        hidden = mmay_checked_constant(&checked, 1);
        identity = mmay_checked_function(&checked, 0);
        local = mmay_checked_function(&checked, 1);
        MMAY_EXPECT(public_id != NULL && public_id->is_public);
        MMAY_EXPECT(private_id != NULL && !private_id->is_public);
        MMAY_EXPECT(public_user != NULL && public_user->is_public);
        MMAY_EXPECT(private_user != NULL && !private_user->is_public);
        MMAY_EXPECT(answer != NULL && answer->is_public);
        MMAY_EXPECT(hidden != NULL && !hidden->is_public);
        MMAY_EXPECT(identity != NULL && identity->is_public);
        MMAY_EXPECT(local != NULL && !local->is_public);
        if (public_user != NULL) {
            const MarimayCheckedRecordField *id = mmay_vector_at_const(
                &public_user->fields,
                0
            );
            const MarimayCheckedRecordField *internal = mmay_vector_at_const(
                &public_user->fields,
                1
            );
            MMAY_EXPECT(id != NULL && id->is_public);
            MMAY_EXPECT(internal != NULL && !internal->is_public);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        public_alias_private,
        MMAY_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
    mmay_checker_expect_one_diagnostic(
        public_parameter_private,
        MMAY_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
    mmay_checker_expect_one_diagnostic(
        public_return_private,
        MMAY_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
    mmay_checker_expect_one_diagnostic(
        public_field_private,
        MMAY_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
    mmay_checker_expect_one_diagnostic(
        public_constant_private,
        MMAY_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
}
