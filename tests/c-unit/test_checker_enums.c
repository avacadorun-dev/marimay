#include "test_checker_support.h"

void mmay_test_checker_enum_declarations(void) {
    static const char source[] =
        "public type Code = int;\n"
        "public type Alias = Message;\n"
        "public enum Message {\n"
        "  Quit,\n"
        "  Write(string, Code),\n"
        "  Move { x: int, y: Code, },\n"
        "  Chain(Message),\n"
        "}\n"
        "enum Shadow {\n"
        "  Quit,\n"
        "  Write(string, Code),\n"
        "  Move { x: int, y: Code, },\n"
        "  Chain(Message),\n"
        "}\n"
        "public fn keep(value: Message): Alias { return value; }\n";
    static const char mutual[] =
        "type Node { event: Event, }\n"
        "enum Event { Attach(Node), Next(Event), }\n";
    static const char duplicate_variant[] =
        "enum State { Ready, Ready, }\n";
    static const char duplicate_payload_field[] =
        "enum Event { Move { x: int, x: int, }, }\n";
    static const char unknown_payload_type[] =
        "enum Event { Value(Missing), }\n";
    static const char primitive_redefinition[] =
        "enum int { Value, }\n";
    static const char duplicate_top_level[] =
        "type Shared {}\n"
        "enum Shared { Value, }\n";
    static const char public_private_payload[] =
        "type Hidden {}\n"
        "public enum Exposed { Value(Hidden), }\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckOptions options;
    const MarimayCheckedEnum *message = NULL;
    const MarimayCheckedEnum *shadow = NULL;
    const MarimayCheckedEnumVariant *quit = NULL;
    const MarimayCheckedEnumVariant *write = NULL;
    const MarimayCheckedEnumVariant *move = NULL;
    const MarimayCheckedEnumVariant *chain = NULL;
    const MarimayCheckedTypeAlias *alias = NULL;
    const MarimayCheckedFunction *keep = NULL;
    const MarimayTypeDescriptor *signature = NULL;

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/enums",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.enums.length == 2);
        MMAY_EXPECT(checked.type_aliases.length == 2);
        MMAY_EXPECT(checked.functions.length == 1);
        message = mmay_checked_enum(&checked, 0);
        shadow = mmay_checked_enum(&checked, 1);
        alias = mmay_checked_type_alias(&checked, 1);
        keep = mmay_checked_function(&checked, 0);
        MMAY_EXPECT(message != NULL);
        MMAY_EXPECT(shadow != NULL);
        MMAY_EXPECT(alias != NULL);
        MMAY_EXPECT(keep != NULL);
        if (message != NULL) {
            const MarimayTypeDescriptor *nominal = mmay_type_descriptor(
                &checked.types,
                message->type
            );
            MMAY_EXPECT(message->is_public);
            MMAY_EXPECT(message->variants.length == 4);
            MMAY_EXPECT(nominal != NULL);
            if (nominal != NULL) {
                MMAY_EXPECT(nominal->kind == MMAY_TYPE_KIND_NOMINAL);
                MMAY_EXPECT(mmay_symbol_id_equal(
                    nominal->symbol,
                    message->symbol
                ));
            }
            quit = mmay_vector_at_const(&message->variants, 0);
            write = mmay_vector_at_const(&message->variants, 1);
            move = mmay_vector_at_const(&message->variants, 2);
            chain = mmay_vector_at_const(&message->variants, 3);
            MMAY_EXPECT(quit != NULL);
            MMAY_EXPECT(write != NULL);
            MMAY_EXPECT(move != NULL);
            MMAY_EXPECT(chain != NULL);
            if (quit != NULL) {
                MMAY_EXPECT(quit->kind == MMAY_CHECKED_ENUM_VARIANT_UNIT);
                MMAY_EXPECT(quit->payload.length == 0);
            }
            if (write != NULL) {
                const MarimayCheckedEnumPayloadItem *text =
                    mmay_vector_at_const(&write->payload, 0);
                const MarimayCheckedEnumPayloadItem *code =
                    mmay_vector_at_const(&write->payload, 1);
                MMAY_EXPECT(write->kind
                    == MMAY_CHECKED_ENUM_VARIANT_POSITIONAL);
                MMAY_EXPECT(write->payload.length == 2);
                MMAY_EXPECT(text != NULL && !text->has_name
                    && text->type == MMAY_TYPE_STRING);
                MMAY_EXPECT(code != NULL && !code->has_name
                    && code->type == MMAY_TYPE_I64);
            }
            if (move != NULL) {
                const MarimayCheckedEnumPayloadItem *x = mmay_vector_at_const(
                    &move->payload,
                    0
                );
                const MarimayCheckedEnumPayloadItem *y = mmay_vector_at_const(
                    &move->payload,
                    1
                );
                MMAY_EXPECT(move->kind == MMAY_CHECKED_ENUM_VARIANT_NAMED);
                MMAY_EXPECT(move->payload.length == 2);
                MMAY_EXPECT(x != NULL && x->has_name
                    && x->name.length == strlen("x")
                    && x->type == MMAY_TYPE_I64);
                MMAY_EXPECT(y != NULL && y->has_name
                    && y->name.length == strlen("y")
                    && y->type == MMAY_TYPE_I64);
            }
            if (chain != NULL) {
                const MarimayCheckedEnumPayloadItem *next =
                    mmay_vector_at_const(&chain->payload, 0);
                MMAY_EXPECT(next != NULL && next->type == message->type);
            }
        }
        if (message != NULL && shadow != NULL) {
            MMAY_EXPECT(!shadow->is_public);
            MMAY_EXPECT(shadow->variants.length == message->variants.length);
            MMAY_EXPECT(shadow->type != message->type);
            MMAY_EXPECT(!mmay_symbol_id_equal(shadow->symbol, message->symbol));
        }
        if (message != NULL && alias != NULL) {
            MMAY_EXPECT(alias->target_type == message->type);
            MMAY_EXPECT(alias->is_public);
        }
        if (message != NULL && keep != NULL) {
            MMAY_EXPECT(keep->is_public);
            signature = mmay_type_descriptor(&checked.types, keep->signature_type);
            MMAY_EXPECT(signature != NULL);
            if (signature != NULL) {
                MMAY_EXPECT(signature->arguments[0] == message->type);
                MMAY_EXPECT(signature->arguments[1] == message->type);
            }
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, mutual));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/enums",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        const MarimayCheckedRecord *node = mmay_checked_record(&checked, 0);
        const MarimayCheckedEnum *event = mmay_checked_enum(&checked, 0);
        const MarimayCheckedRecordField *event_field = node != NULL
            ? mmay_vector_at_const(&node->fields, 0)
            : NULL;
        const MarimayCheckedEnumVariant *attach = event != NULL
            ? mmay_vector_at_const(&event->variants, 0)
            : NULL;
        const MarimayCheckedEnumVariant *next = event != NULL
            ? mmay_vector_at_const(&event->variants, 1)
            : NULL;
        const MarimayCheckedEnumPayloadItem *attached = attach != NULL
            ? mmay_vector_at_const(&attach->payload, 0)
            : NULL;
        const MarimayCheckedEnumPayloadItem *continued = next != NULL
            ? mmay_vector_at_const(&next->payload, 0)
            : NULL;
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(node != NULL && event != NULL);
        MMAY_EXPECT(event_field != NULL && attached != NULL
            && continued != NULL);
        if (node != NULL && event != NULL && event_field != NULL
            && attached != NULL && continued != NULL) {
            MMAY_EXPECT(event_field->type == event->type);
            MMAY_EXPECT(attached->type == node->type);
            MMAY_EXPECT(continued->type == event->type);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        duplicate_variant,
        MMAY_DIAGNOSTIC_DUPLICATE_NAME,
        "Ready",
        1
    );
    mmay_checker_expect_one_diagnostic(
        duplicate_payload_field,
        MMAY_DIAGNOSTIC_DUPLICATE_NAME,
        "x",
        1
    );
    mmay_checker_expect_one_diagnostic(
        unknown_payload_type,
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
    mmay_checker_expect_one_diagnostic(
        public_private_payload,
        MMAY_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );

    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    options = mmay_check_default_options();
    options.max_enums = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/enums",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.max_variants_per_enum = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/enums",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.max_payload_items_per_variant = 1;
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/enums",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "enum Generic<T> { Value(T), }\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/enums",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);

    MMAY_EXPECT(mmay_checker_fixture_init(
        &fixture,
        "@derive(Debug) enum State { Ready, }\n"
    ));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/enums",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    if (checked.initialized) {
        const MarimayCheckedEnum *enum_value = mmay_checked_enum(&checked, 0);
        MMAY_EXPECT(enum_value != NULL);
        if (enum_value != NULL) {
            MMAY_EXPECT(enum_value->derives_debug);
            MMAY_EXPECT(!enum_value->derives_eq);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_enum_package_order(void) {
    static const char entry_source[] =
        "fn keep(value: Event): Event { return value; }\n"
        "fn empty(): Event { return Event.Empty; }\n"
        "fn kind(value: Event): int {\n"
        "  return match value {\n"
        "    Empty => 0,\n"
        "    Link(node) | Peer(node) => 1,\n"
        "  };\n"
        "}\n";
    static const char types_source[] =
        "enum Event { Empty, Link(Node), Peer(Node), }\n"
        "type Node { event: Event, }\n";
    MarimayCheckerFixture entry;
    MarimayCheckerFixture types;
    MarimayCheckerFixture missing_entry;
    const MarimayAstTree *asts[2];
    MarimayCheckResult checked;
    const MarimayCheckedEnum *event = NULL;
    const MarimayCheckedRecord *node = NULL;
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
        "test/enum-package",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        const MarimayCheckedEnumVariant *link = NULL;
        const MarimayCheckedEnumPayloadItem *linked = NULL;
        const MarimayCheckedRecordField *field = NULL;
        event = mmay_checked_enum(&checked, 0);
        node = mmay_checked_record(&checked, 0);
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.functions.length == 3);
        MMAY_EXPECT(event != NULL && node != NULL);
        if (event != NULL && node != NULL) {
            link = mmay_vector_at_const(&event->variants, 1);
            linked = link != NULL
                ? mmay_vector_at_const(&link->payload, 0)
                : NULL;
            field = mmay_vector_at_const(&node->fields, 0);
            MMAY_EXPECT(linked != NULL && linked->type == node->type);
            MMAY_EXPECT(field != NULL && field->type == event->type);
            symbol = event->symbol;
            type = event->type;
        }
        {
            const MarimayCheckedFunction *empty = mmay_checked_function(
                &checked,
                1
            );
            MMAY_EXPECT(empty != NULL);
            if (empty != NULL) {
                MMAY_EXPECT(empty->mir.enums.length == 1);
                MMAY_EXPECT(empty->mir.enum_constructions.length == 1);
            }
            {
                const MarimayCheckedFunction *kind = mmay_checked_function(
                    &checked,
                    2
                );
                MMAY_EXPECT(kind != NULL);
                if (kind != NULL) {
                    MMAY_EXPECT(kind->mir.enums.length == 1);
                    MMAY_EXPECT(mmay_mir_verify(&kind->mir)
                        == MMAY_MIR_VERIFY_OK);
                }
            }
        }
        mmay_check_result_dispose(&checked);
    }

    asts[0] = &entry.ast;
    asts[1] = &types.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        2,
        "test/enum-package",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        event = mmay_checked_enum(&checked, 0);
        MMAY_EXPECT(event != NULL);
        MMAY_EXPECT(checked.functions.length == 3);
        if (event != NULL) {
            MMAY_EXPECT(mmay_symbol_id_equal(event->symbol, symbol));
            MMAY_EXPECT(event->type == type);
        }
        mmay_check_result_dispose(&checked);
    }
    MMAY_EXPECT(mmay_checker_fixture_init_path(
        &missing_entry,
        "b_missing.my",
        "type Alias = Event;\n"
        "fn kind(value: Alias): int {\n"
        "  return match value { Empty => 0, Link(_) => 1, };\n"
        "}\n"
    ));
    asts[0] = &types.ast;
    asts[1] = &missing_entry.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        2,
        "test/enum-package-witness",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        const MarimayCheckDiagnostic *diagnostic = mmay_check_diagnostic(
            &checked,
            0
        );
        MMAY_EXPECT(checked.diagnostics.length == 1);
        MMAY_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            MMAY_EXPECT(strcmp(
                diagnostic->message,
                "enum match is missing witness: Peer(_)"
            ) == 0);
            MMAY_EXPECT(diagnostic->owned_message.data != NULL);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&missing_entry);
    mmay_checker_fixture_dispose(&types);
    mmay_checker_fixture_dispose(&entry);
}

void mmay_test_checker_enum_execution(void) {
    static const char source[] =
        "enum Message {\n"
        "  Quit,\n"
        "  Write(int, int),\n"
        "  Move { x: int, y: int, },\n"
        "}\n"
        "type MessageAlias = Message;\n"
        "fn quit(): Message { return Message.Quit; }\n"
        "fn write(x: int, y: int): Message {\n"
        "  return Message.Write(x, y);\n"
        "}\n"
        "fn move(x: int, y: int): MessageAlias {\n"
        "  return MessageAlias.Move { y, x };\n"
        "}\n"
        "fn relay(x: int, y: int): Message { return write(x, y); }\n"
        "fn keep(message: Message): Message { return message; }\n"
        "fn overflow(): int { return 9223372036854775807 + 1; }\n"
        "fn divideZero(): int { return 1 / 0; }\n"
        "fn positionalOrder(): Message {\n"
        "  return Message.Write(divideZero(), overflow());\n"
        "}\n"
        "fn namedOrder(): Message {\n"
        "  return Message.Move { y: divideZero(), x: overflow() };\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedEnum *message = NULL;
    const MarimayCheckedFunction *quit = NULL;
    const MarimayCheckedFunction *write = NULL;
    const MarimayCheckedFunction *move = NULL;
    MarimayMirExecution execution;
    MarimayCheckInterpretOptions interpret_options =
        mmay_check_interpret_default_options();
    MarimayMirValue arguments[2];
    MarimayMirValue result;
    MarimayMirValue payload;
    MarimayMirValue enum_argument;
    size_t selected_variant = 0;
    size_t payload_count = 0;
    size_t retained_count = 0;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/enum-execution",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(checked.enums.length == 1);
        MMAY_EXPECT(checked.type_aliases.length == 1);
        MMAY_EXPECT(checked.functions.length == 9);
        message = mmay_checked_enum(&checked, 0);
        quit = mmay_checked_function(&checked, 0);
        write = mmay_checked_function(&checked, 1);
        move = mmay_checked_function(&checked, 2);
        MMAY_EXPECT(message != NULL);
        MMAY_EXPECT(quit != NULL && write != NULL && move != NULL);
        if (quit != NULL) {
            MMAY_EXPECT(quit->mir.enums.length == 1);
            MMAY_EXPECT(quit->mir.enum_variants.length == 3);
            MMAY_EXPECT(quit->mir.enum_payloads.length == 4);
            MMAY_EXPECT(quit->mir.enum_constructions.length == 1);
            MMAY_EXPECT(quit->mir.enum_arguments.length == 0);
            MMAY_EXPECT(mmay_check_interpret_function(
                &checked,
                0,
                NULL,
                0,
                mmay_system_allocator(),
                mmay_check_interpret_default_options(),
                &result
            ) == MMAY_MIR_INTERPRET_INVALID_ARGUMENT);
        }
        if (write != NULL) {
            MMAY_EXPECT(write->mir.enums.length == 1);
            MMAY_EXPECT(write->mir.enum_constructions.length == 1);
            MMAY_EXPECT(write->mir.enum_arguments.length == 2);
        }
        if (move != NULL) {
            MMAY_EXPECT(move->mir.enums.length == 1);
            MMAY_EXPECT(move->mir.enum_constructions.length == 1);
            MMAY_EXPECT(move->mir.enum_arguments.length == 2);
        }
        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            mmay_system_allocator(),
            mmay_mir_execution_default_options()
        ));
        interpret_options.execution = &execution;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            0,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(message != NULL && result.type == message->type);
        MMAY_EXPECT(mmay_mir_execution_owns_reference(&execution, result));
        MMAY_EXPECT(mmay_mir_execution_inspect_enum(
            &execution,
            result,
            &selected_variant,
            &payload_count
        ));
        MMAY_EXPECT(selected_variant == 0 && payload_count == 0);

        arguments[0].type = MMAY_TYPE_I64;
        arguments[0].as.i64 = 10;
        arguments[1].type = MMAY_TYPE_I64;
        arguments[1].as.i64 = 20;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            1,
            arguments,
            2,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_mir_execution_owns_reference(&execution, result));
        MMAY_EXPECT(mmay_mir_execution_inspect_enum(
            &execution,
            result,
            &selected_variant,
            &payload_count
        ));
        MMAY_EXPECT(selected_variant == 1 && payload_count == 2);
        MMAY_EXPECT(mmay_mir_execution_enum_payload(
            &execution,
            result,
            0,
            &payload
        ));
        MMAY_EXPECT(payload.type == MMAY_TYPE_I64 && payload.as.i64 == 10);
        MMAY_EXPECT(mmay_mir_execution_enum_payload(
            &execution,
            result,
            1,
            &payload
        ));
        MMAY_EXPECT(payload.as.i64 == 20);

        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            2,
            arguments,
            2,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_mir_execution_owns_reference(&execution, result));
        MMAY_EXPECT(mmay_mir_execution_inspect_enum(
            &execution,
            result,
            &selected_variant,
            &payload_count
        ));
        MMAY_EXPECT(selected_variant == 2 && payload_count == 2);
        MMAY_EXPECT(mmay_mir_execution_enum_payload(
            &execution,
            result,
            0,
            &payload
        ));
        MMAY_EXPECT(payload.as.i64 == 10);
        MMAY_EXPECT(mmay_mir_execution_enum_payload(
            &execution,
            result,
            1,
            &payload
        ));
        MMAY_EXPECT(payload.as.i64 == 20);

        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            3,
            arguments,
            2,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_mir_execution_owns_reference(&execution, result));
        enum_argument = result;
        retained_count = execution.enums.length;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            4,
            &enum_argument,
            1,
            mmay_system_allocator(),
            mmay_check_interpret_default_options(),
            &result
        ) == MMAY_MIR_INTERPRET_INVALID_ARGUMENT);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            4,
            &enum_argument,
            1,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(mmay_mir_reference_equal(
            result.as.reference,
            enum_argument.as.reference
        ));
        MMAY_EXPECT(execution.enums.length == retained_count);
        enum_argument.as.reference = MMAY_MIR_INVALID_REFERENCE;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            4,
            &enum_argument,
            1,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_INVALID_REFERENCE);

        retained_count = execution.enums.length;
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            6,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_DIVISION_BY_ZERO);
        MMAY_EXPECT(execution.enums.length == retained_count);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            8,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &result
        ) == MMAY_MIR_INTERPRET_DIVISION_BY_ZERO);
        MMAY_EXPECT(execution.enums.length == retained_count);
        MMAY_EXPECT(execution.objects.length == 0);
        mmay_mir_execution_dispose(&execution);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_fixture_dispose(&fixture);

    mmay_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(): Message { return Message.Missing; }\n",
        MMAY_DIAGNOSTIC_UNKNOWN_VARIANT,
        "Missing",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "enum Message { Write(int), }\n"
        "fn bad(): Message { return Message.Write; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Message.Write",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(): Message { return Message.Quit(); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Message.Quit()",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "enum Message { Move { x: int, }, }\n"
        "fn bad(): Message { return Message.Move(1); }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "Message.Move(1)",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "enum Message { Write(int, int), }\n"
        "fn bad(): Message { return Message.Write(1); }\n",
        MMAY_DIAGNOSTIC_CALL_ARITY,
        "(1)",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "enum Message { Move { x: int, }, }\n"
        "fn bad(): Message { return Message.Move { y: 1 }; }\n",
        MMAY_DIAGNOSTIC_UNKNOWN_FIELD,
        "y",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "enum Message { Move { x: int, }, }\n"
        "fn bad(): Message { return Message.Move { x: 1, x: 2 }; }\n",
        MMAY_DIAGNOSTIC_DUPLICATE_FIELD,
        "x",
        2
    );
    mmay_checker_expect_one_diagnostic(
        "enum Message { Move { x: int, y: int, }, }\n"
        "fn bad(): Message { return Message.Move { x: 1 }; }\n",
        MMAY_DIAGNOSTIC_MISSING_FIELD,
        "Message.Move { x: 1 }",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "enum Message { Move { x: int, }, }\n"
        "fn bad(): Message { return Message.Move { x: true }; }\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    mmay_checker_expect_one_diagnostic(
        "enum Message { Move { x: int, }, }\n"
        "fn bad(): Message { return Message.Move { x }; }\n",
        MMAY_DIAGNOSTIC_UNKNOWN_NAME,
        "x",
        1
    );
    mmay_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(): Message { return Quit; }\n",
        MMAY_DIAGNOSTIC_UNKNOWN_NAME,
        "Quit",
        1
    );
}
