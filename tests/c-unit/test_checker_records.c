#include "test_checker_support.h"

void ruda_test_checker_record_declarations(void) {
    static const char source[] =
        "public type UserId = int;\n"
        "public type User {\n"
        "  public id: UserId;\n"
        "  mutable parent: User;\n"
        "}\n"
        "public type Shadow { public id: UserId; mutable parent: User; }\n"
        "fn identity(value: User): User { return value; }\n";
    static const char duplicate_field[] =
        "type User { id: int; id: string; }\n";
    static const char unknown_field_type[] =
        "type User { value: Missing; }\n";
    static const char primitive_redefinition[] =
        "type int { value: int; }\n";
    static const char duplicate_top_level[] =
        "type Shared = int;\n"
        "type Shared { value: int; }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckOptions options;
    const RudaCheckedRecord *user = NULL;
    const RudaCheckedRecord *shadow = NULL;
    const RudaCheckedRecordField *id = NULL;
    const RudaCheckedRecordField *parent = NULL;
    const RudaCheckedFunction *identity = NULL;
    const RudaTypeDescriptor *nominal = NULL;
    const RudaTypeDescriptor *signature = NULL;

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/records",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.records.length == 2);
        RUDA_EXPECT(checked.type_aliases.length == 1);
        RUDA_EXPECT(checked.functions.length == 1);
        user = ruda_checked_record(&checked, 0);
        shadow = ruda_checked_record(&checked, 1);
        identity = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(user != NULL);
        RUDA_EXPECT(shadow != NULL);
        RUDA_EXPECT(identity != NULL);
        if (user != NULL) {
            RUDA_EXPECT(user->name.length == strlen("User"));
            RUDA_EXPECT(memcmp(user->name.data, "User", strlen("User")) == 0);
            RUDA_EXPECT(user->is_public);
            RUDA_EXPECT(user->fields.length == 2);
            RUDA_EXPECT(ruda_type_kind(&checked.types, user->type)
                == RUDA_TYPE_KIND_NOMINAL);
            nominal = ruda_type_descriptor(&checked.types, user->type);
            RUDA_EXPECT(nominal != NULL);
            if (nominal != NULL) {
                RUDA_EXPECT(ruda_symbol_id_equal(nominal->symbol, user->symbol));
                RUDA_EXPECT(nominal->argument_count == 0);
            }
            id = ruda_vector_at_const(&user->fields, 0);
            parent = ruda_vector_at_const(&user->fields, 1);
            RUDA_EXPECT(id != NULL);
            RUDA_EXPECT(parent != NULL);
            if (id != NULL) {
                RUDA_EXPECT(id->name.length == strlen("id"));
                RUDA_EXPECT(id->type == RUDA_TYPE_I64);
                RUDA_EXPECT(id->is_public);
                RUDA_EXPECT(!id->is_mutable);
            }
            if (parent != NULL) {
                RUDA_EXPECT(parent->name.length == strlen("parent"));
                RUDA_EXPECT(parent->type == user->type);
                RUDA_EXPECT(!parent->is_public);
                RUDA_EXPECT(parent->is_mutable);
            }
        }
        if (user != NULL && shadow != NULL) {
            RUDA_EXPECT(shadow->is_public);
            RUDA_EXPECT(shadow->fields.length == user->fields.length);
            RUDA_EXPECT(shadow->type != user->type);
            RUDA_EXPECT(!ruda_symbol_id_equal(shadow->symbol, user->symbol));
        }
        if (user != NULL && identity != NULL) {
            signature = ruda_type_descriptor(
                &checked.types,
                identity->signature_type
            );
            RUDA_EXPECT(signature != NULL);
            if (signature != NULL) {
                RUDA_EXPECT(signature->argument_count == 2);
                RUDA_EXPECT(signature->arguments[0] == user->type);
                RUDA_EXPECT(signature->arguments[1] == user->type);
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "type First { second: Second; }\n"
        "type Second { first: First; }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/records",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedRecord *first = ruda_checked_record(&checked, 0);
        const RudaCheckedRecord *second = ruda_checked_record(&checked, 1);
        const RudaCheckedRecordField *first_field = first != NULL
            ? ruda_vector_at_const(&first->fields, 0)
            : NULL;
        const RudaCheckedRecordField *second_field = second != NULL
            ? ruda_vector_at_const(&second->fields, 0)
            : NULL;
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(first != NULL);
        RUDA_EXPECT(second != NULL);
        RUDA_EXPECT(first_field != NULL);
        RUDA_EXPECT(second_field != NULL);
        if (first != NULL && second != NULL && first_field != NULL
            && second_field != NULL) {
            RUDA_EXPECT(first_field->type == second->type);
            RUDA_EXPECT(second_field->type == first->type);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        duplicate_field,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "id",
        1
    );
    ruda_checker_expect_one_diagnostic(
        unknown_field_type,
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
        duplicate_top_level,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "Shared",
        1
    );

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    options = ruda_check_default_options();
    options.max_records = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/records",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.max_fields_per_record = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/records",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "type Generic<T> { value: T; }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/records",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized);
    ruda_check_result_dispose(&checked);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "@derive(Debug) type User { id: int; }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/records",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

}

void ruda_test_checker_record_package_order(void) {
    static const char entry_source[] =
        "fn keep(value: Node): Node { return value; }\n"
        "fn build(value: int): int {\n"
        "  const leaf = Leaf { value };\n"
        "  return leaf.value;\n"
        "}\n";
    static const char types_source[] =
        "type Alias = Node;\n"
        "type Node { public id: int; next: Alias; }\n"
        "type Leaf { value: int; }\n";
    RudaCheckerFixture entry;
    RudaCheckerFixture types;
    const RudaAstTree *asts[2];
    RudaCheckResult checked;
    const RudaCheckedRecord *node = NULL;
    const RudaCheckedTypeAlias *alias = NULL;
    const RudaCheckedFunction *keep = NULL;
    RudaMirValue argument;
    RudaMirValue result;
    const RudaTypeDescriptor *signature = NULL;
    RudaSymbolId symbol;
    RudaTypeId type = RUDA_TYPE_INVALID;
    memset(&symbol, 0, sizeof(symbol));

    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &entry,
        "a_entry.ruda",
        entry_source
    ));
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &types,
        "z_types.ruda",
        types_source
    ));
    asts[0] = &types.ast;
    asts[1] = &entry.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/record-package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.records.length == 2);
        RUDA_EXPECT(checked.type_aliases.length == 1);
        RUDA_EXPECT(checked.functions.length == 2);
        node = ruda_checked_record(&checked, 0);
        alias = ruda_checked_type_alias(&checked, 0);
        keep = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(node != NULL);
        RUDA_EXPECT(alias != NULL);
        RUDA_EXPECT(keep != NULL);
        if (node != NULL) {
            RUDA_EXPECT(node->fields.length == 2);
            symbol = node->symbol;
            type = node->type;
        }
        if (node != NULL && alias != NULL) {
            RUDA_EXPECT(alias->target_type == node->type);
        }
        if (node != NULL && keep != NULL) {
            signature = ruda_type_descriptor(&checked.types, keep->signature_type);
            RUDA_EXPECT(signature != NULL);
            if (signature != NULL) {
                RUDA_EXPECT(signature->arguments[0] == node->type);
                RUDA_EXPECT(signature->arguments[1] == node->type);
            }
        }
        argument.type = RUDA_TYPE_I64;
        argument.as.i64 = 42;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64);
        RUDA_EXPECT(result.as.i64 == 42);
        ruda_check_result_dispose(&checked);
    }

    asts[0] = &entry.ast;
    asts[1] = &types.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/record-package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        node = ruda_checked_record(&checked, 0);
        RUDA_EXPECT(node != NULL);
        if (node != NULL) {
            RUDA_EXPECT(ruda_symbol_id_equal(node->symbol, symbol));
            RUDA_EXPECT(node->type == type);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&types);
    ruda_checker_fixture_dispose(&entry);
}

void ruda_test_checker_record_execution(void) {
    static const char source[] =
        "type Counter {\n"
        "  seed: int;\n"
        "  mutable value: int;\n"
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
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *make = NULL;
    const RudaCheckedFunction *read = NULL;
    const RudaCheckedFunction *aliasing = NULL;
    const RudaCheckedFunction *across_call = NULL;
    const RudaCheckedFunction *order = NULL;
    RudaMirValue argument;
    RudaMirValue result;
    RudaMirValue record_argument;
    RudaCheckInterpretOptions interpret_options =
        ruda_check_interpret_default_options();
    RudaMirExecution execution;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/record-execution",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.records.length == 1);
        RUDA_EXPECT(checked.type_aliases.length == 1);
        RUDA_EXPECT(checked.functions.length == 8);
        make = ruda_checked_function(&checked, 0);
        read = ruda_checked_function(&checked, 1);
        aliasing = ruda_checked_function(&checked, 2);
        across_call = ruda_checked_function(&checked, 4);
        order = ruda_checked_function(&checked, 7);
        RUDA_EXPECT(make != NULL);
        RUDA_EXPECT(read != NULL);
        RUDA_EXPECT(aliasing != NULL);
        RUDA_EXPECT(across_call != NULL);
        RUDA_EXPECT(order != NULL);
        if (make != NULL) {
            RUDA_EXPECT(make->mir.records.length == 1);
            RUDA_EXPECT(make->mir.record_fields.length == 2);
            RUDA_EXPECT(make->mir.record_constructions.length == 1);
            RUDA_EXPECT(make->mir.record_arguments.length == 2);
        }
        if (read != NULL) {
            RUDA_EXPECT(read->mir.records.length == 1);
            RUDA_EXPECT(read->mir.record_constructions.length == 0);
        }
        argument.type = RUDA_TYPE_I64;
        argument.as.i64 = 4;
        if (aliasing != NULL) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                2,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 5);
            {
                size_t failure_index = 0;
                bool reached_success = false;
                for (failure_index = 0; failure_index < 32;
                    failure_index += 1) {
                    RudaFailAllocator state;
                    RudaAllocator allocator;
                    RudaMirInterpretStatus status;
                    ruda_fail_allocator_init(
                        &state,
                        ruda_system_allocator(),
                        failure_index
                    );
                    allocator = ruda_fail_allocator(&state);
                    status = ruda_check_interpret_function(
                        &checked,
                        2,
                        &argument,
                        1,
                        allocator,
                        ruda_check_interpret_default_options(),
                        &result
                    );
                    if (status == RUDA_MIR_INTERPRET_OK) {
                        reached_success = true;
                        RUDA_EXPECT(result.as.i64 == 5);
                    } else {
                        RUDA_EXPECT(
                            status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED
                        );
                    }
                    RUDA_EXPECT(state.live_allocations == 0);
                    RUDA_EXPECT(state.live_bytes == 0);
                    if (reached_success) {
                        break;
                    }
                }
                RUDA_EXPECT(reached_success);
            }
        }
        if (across_call != NULL) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                4,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 5);
        }
        if (order != NULL) {
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                7,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        }
        if (make != NULL && read != NULL) {
            argument.as.i64 = 7;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                0,
                &argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
            RUDA_EXPECT(ruda_mir_execution_init(
                &execution,
                ruda_system_allocator(),
                ruda_mir_execution_default_options()
            ));
            interpret_options.execution = &execution;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                0,
                &argument,
                1,
                ruda_system_allocator(),
                interpret_options,
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == make->mir.result_type);
            RUDA_EXPECT(ruda_mir_execution_owns_reference(
                &execution,
                result
            ));
            RUDA_EXPECT(execution.objects.length == 1);
            record_argument = result;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                1,
                &record_argument,
                1,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                1,
                &record_argument,
                1,
                ruda_system_allocator(),
                interpret_options,
                &result
            ) == RUDA_MIR_INTERPRET_OK);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 7);
            record_argument.as.reference = RUDA_MIR_INVALID_REFERENCE;
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                1,
                &record_argument,
                1,
                ruda_system_allocator(),
                interpret_options,
                &result
            ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
            ruda_mir_execution_dispose(&execution);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "fn bad(): Counter { return Counter { other: 1 }; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
        "other",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "fn bad(): Counter { return Counter { value: 1, value: 2 }; }\n",
        RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
        "value",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { seed: int; value: int; }\n"
        "fn bad(): Counter { return Counter { value: 1 }; }\n",
        RUDA_DIAGNOSTIC_MISSING_FIELD,
        "Counter { value: 1 }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "fn bad(): Counter { return Counter { value: true }; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { seed: int; }\n"
        "fn bad(counter: Counter): int { counter.seed = 1; return 0; }\n",
        RUDA_DIAGNOSTIC_ASSIGN_IMMUTABLE,
        "seed",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "fn bad(counter: Counter): int { return counter.other; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
        "other",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Counter { value: int; }\n"
        "fn bad(): Counter { return Counter { value }; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "value",
        1
    );
}

void ruda_test_checker_record_identity(void) {
    static const char source[] =
        "type Point { mutable x: int; }\n"
        "type Other { x: int; }\n"
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
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirValue argument;
    RudaMirValue first;
    RudaMirValue second;
    RudaMirValue comparison_arguments[2];
    RudaMirValue result;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options =
        ruda_check_interpret_default_options();
    const RudaCheckedFunction *same = NULL;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/record-identity",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.records.length == 2);
        RUDA_EXPECT(checked.type_aliases.length == 1);
        RUDA_EXPECT(checked.functions.length == 8);
        same = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(same != NULL);
        if (same != NULL) {
            RUDA_EXPECT(same->mir.records.length == 1);
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &same->mir,
                RUDA_MIR_RECORD_IDENTITY_SAME
            ) == 1);
        }
        argument.type = RUDA_TYPE_I64;
        argument.as.i64 = 7;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_BOOL);
        RUDA_EXPECT(result.as.boolean);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(!result.as.boolean);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            &argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.boolean);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            7,
            NULL,
            0,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);

        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &first
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            &argument,
            1,
            ruda_system_allocator(),
            options,
            &second
        ) == RUDA_MIR_INTERPRET_OK);
        comparison_arguments[0] = first;
        comparison_arguments[1] = first;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            comparison_arguments,
            2,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.boolean);
        comparison_arguments[1] = second;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            comparison_arguments,
            2,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(!result.as.boolean);
        comparison_arguments[1].as.reference = RUDA_MIR_INVALID_REFERENCE;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            comparison_arguments,
            2,
            ruda_system_allocator(),
            options,
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "type Other { x: int; }\n"
        "fn bad(): bool { return identity.same(Point { x: 1 }, Other { x: 1 }); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Other { x: 1 }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): bool { return identity.same(1, 1); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "fn bad(): bool { return identity.same(\"a\", \"a\"); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "\"a\"",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Choice { A, }\n"
        "fn bad(): bool { return identity.same(Choice.A, Choice.A); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Choice.A",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(value: Point): bool { return identity.same(value); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "(value)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(value: Point): bool { return identity.same(value, value, value); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "(value, value, value)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(value: Point): bool { return identity.same<Point>(value, value); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "identity.same<Point>(value, value)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(value: Point): bool { return identity.same(value, 1); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "1",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "const same: bool = identity.same(Point { x: 1 }, Point { x: 1 });\n",
        RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
        "identity.same(Point { x: 1 }, Point { x: 1 })",
        0
    );
}

void ruda_test_checker_record_match(void) {
    static const char source[] =
        "type Point { x: int; y: int; }\n"
        "type Empty {}\n"
        "type PointAlias = Point;\n"
        "fn make(x: int, y: int): Point { return Point { x, y }; }\n"
        "fn sum(point: Point): int {\n"
        "  return match point { case Point { x, y } => x + y; };\n"
        "}\n"
        "fn renamed(point: Point): int {\n"
        "  return match point { case PointAlias { x: left, y: right } => left - right; };\n"
        "}\n"
        "fn ignored(point: Point): int {\n"
        "  return match point { case Point { x: _, y } => y; };\n"
        "}\n"
        "fn guarded(point: Point, ready: bool): int {\n"
        "  return match point {\n"
        "    case Point { x, y } if ready => x;\n"
        "    case Point { x: _, y } => y;\n"
        "  };\n"
        "}\n"
        "fn whole(point: Point): Point {\n"
        "  return match point { case value => value; };\n"
        "}\n"
        "fn block(point: Point): int {\n"
        "  return match point { case Point { x, y } => { yield x + y; } };\n"
        "}\n"
        "fn makeEmpty(): Empty { return Empty {}; }\n"
        "fn emptyValue(value: Empty): int {\n"
        "  return match value { case Empty {} => 14; };\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaMirExecution execution;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirValue arguments[2];
    RudaMirValue point;
    RudaMirValue result;
    size_t index = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/record-match",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedFunction *sum = ruda_checked_function(&checked, 1);
        const RudaCheckedFunction *ignored = ruda_checked_function(&checked, 3);
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 9);
        RUDA_EXPECT(checked.records.length == 2);
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
        RUDA_EXPECT(sum != NULL && ignored != NULL);
        if (sum != NULL) {
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &sum->mir,
                RUDA_MIR_RECORD_GET
            ) == 2);
        }
        if (ignored != NULL) {
            RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
                &ignored->mir,
                RUDA_MIR_RECORD_GET
            ) == 1);
        }
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        options.execution = &execution;
        arguments[0].type = RUDA_TYPE_I64;
        arguments[0].as.i64 = 9;
        arguments[1].type = RUDA_TYPE_I64;
        arguments[1].as.i64 = 4;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 0, arguments, 2, ruda_system_allocator(), options,
            &point
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 1, &point, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 13);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 2, &point, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 5);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 3, &point, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 4);
        arguments[0] = point;
        arguments[1].type = RUDA_TYPE_BOOL;
        arguments[1].as.boolean = true;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, arguments, 2, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 9);
        arguments[1].as.boolean = false;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 4, arguments, 2, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 4);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, &point, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == point.type
            && ruda_mir_reference_equal(
                result.as.reference,
                point.as.reference
            ));
        arguments[0] = point;
        arguments[0].as.reference = RUDA_MIR_INVALID_REFERENCE;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 5, arguments, 1, ruda_system_allocator(), options,
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 6, &point, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 13);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 7, NULL, 0, ruda_system_allocator(), options, &point
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, 8, &point, 1, ruda_system_allocator(), options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.as.i64 == 14);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; y: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { x } => x; }; }\n",
        RUDA_DIAGNOSTIC_MISSING_FIELD,
        "{ x }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { z } => z; }; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
        "z",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { x, x } => x; }; }\n",
        RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
        "x",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; y: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { x: value, y: value } => value; }; }\n",
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "value",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "type Other { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Other { x } => x; }; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Other",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { x } | _ => x; }; }\n",
        RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
        "_",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { x } if true => x; }; }\n",
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "match point { case Point { x } if true => x; }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Point(x) => x; }; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Point(x)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "type Point { x: int; }\n"
        "type Alias = int;\n"
        "fn bad(point: Point): int { return match point { case Alias { x } => x; }; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Alias",
        1
    );
    ruda_checker_expect_diagnostic_message(
        "type Empty {}\n"
        "fn bad(value: Empty): int { return match value {}; }\n",
        "record match is missing witness: Empty {}"
    );
    ruda_checker_expect_diagnostic_message(
        "type Point { x: int; }\n"
        "fn bad(point: Point): int { return match point { case Point { x } if true => x; }; }\n",
        "record match is missing witness: Point { x: _ }; guarded patterns do not count toward exhaustiveness"
    );
}

void ruda_test_checker_public_type_visibility(void) {
    static const char source[] =
        "public type PublicId = int;\n"
        "type PrivateId = int;\n"
        "public type PublicUser {\n"
        "  public id: PublicId;\n"
        "  internal: PrivateId;\n"
        "}\n"
        "type PrivateUser { public id: int; }\n"
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
        "public type Visible { public value: Hidden; internal: Hidden; }\n";
    static const char public_constant_private[] =
        "type Hidden = int;\n"
        "public const answer: Hidden = 1;\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedTypeAlias *public_id = NULL;
    const RudaCheckedTypeAlias *private_id = NULL;
    const RudaCheckedRecord *public_user = NULL;
    const RudaCheckedRecord *private_user = NULL;
    const RudaCheckedConstant *answer = NULL;
    const RudaCheckedConstant *hidden = NULL;
    const RudaCheckedFunction *identity = NULL;
    const RudaCheckedFunction *local = NULL;

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/visibility",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.type_aliases.length == 2);
        RUDA_EXPECT(checked.records.length == 2);
        RUDA_EXPECT(checked.constants.length == 2);
        RUDA_EXPECT(checked.functions.length == 2);
        public_id = ruda_checked_type_alias(&checked, 0);
        private_id = ruda_checked_type_alias(&checked, 1);
        public_user = ruda_checked_record(&checked, 0);
        private_user = ruda_checked_record(&checked, 1);
        answer = ruda_checked_constant(&checked, 0);
        hidden = ruda_checked_constant(&checked, 1);
        identity = ruda_checked_function(&checked, 0);
        local = ruda_checked_function(&checked, 1);
        RUDA_EXPECT(public_id != NULL && public_id->is_public);
        RUDA_EXPECT(private_id != NULL && !private_id->is_public);
        RUDA_EXPECT(public_user != NULL && public_user->is_public);
        RUDA_EXPECT(private_user != NULL && !private_user->is_public);
        RUDA_EXPECT(answer != NULL && answer->is_public);
        RUDA_EXPECT(hidden != NULL && !hidden->is_public);
        RUDA_EXPECT(identity != NULL && identity->is_public);
        RUDA_EXPECT(local != NULL && !local->is_public);
        if (public_user != NULL) {
            const RudaCheckedRecordField *id = ruda_vector_at_const(
                &public_user->fields,
                0
            );
            const RudaCheckedRecordField *internal = ruda_vector_at_const(
                &public_user->fields,
                1
            );
            RUDA_EXPECT(id != NULL && id->is_public);
            RUDA_EXPECT(internal != NULL && !internal->is_public);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        public_alias_private,
        RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
    ruda_checker_expect_one_diagnostic(
        public_parameter_private,
        RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
    ruda_checker_expect_one_diagnostic(
        public_return_private,
        RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
    ruda_checker_expect_one_diagnostic(
        public_field_private,
        RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
    ruda_checker_expect_one_diagnostic(
        public_constant_private,
        RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );
}
