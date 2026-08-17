#include "test_checker_support.h"

void ruda_test_checker_enum_declarations(void) {
    static const char source[] =
        "public type Code = int;\n"
        "public type Alias = Message;\n"
        "public enum Message {\n"
        "  Quit,\n"
        "  Write(string, Code),\n"
        "  Move { x: int; y: Code; },\n"
        "  Chain(Message),\n"
        "}\n"
        "enum Shadow {\n"
        "  Quit,\n"
        "  Write(string, Code),\n"
        "  Move { x: int; y: Code; },\n"
        "  Chain(Message),\n"
        "}\n"
        "public fn keep(value: Message): Alias { return value; }\n";
    static const char mutual[] =
        "type Node { event: Event; }\n"
        "enum Event { Attach(Node), Next(Event), }\n";
    static const char duplicate_variant[] =
        "enum State { Ready, Ready, }\n";
    static const char duplicate_payload_field[] =
        "enum Event { Move { x: int; x: int; }, }\n";
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
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckOptions options;
    const RudaCheckedEnum *message = NULL;
    const RudaCheckedEnum *shadow = NULL;
    const RudaCheckedEnumVariant *quit = NULL;
    const RudaCheckedEnumVariant *write = NULL;
    const RudaCheckedEnumVariant *move = NULL;
    const RudaCheckedEnumVariant *chain = NULL;
    const RudaCheckedTypeAlias *alias = NULL;
    const RudaCheckedFunction *keep = NULL;
    const RudaTypeDescriptor *signature = NULL;

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.enums.length == 2);
        RUDA_EXPECT(checked.type_aliases.length == 2);
        RUDA_EXPECT(checked.functions.length == 1);
        message = ruda_checked_enum(&checked, 0);
        shadow = ruda_checked_enum(&checked, 1);
        alias = ruda_checked_type_alias(&checked, 1);
        keep = ruda_checked_function(&checked, 0);
        RUDA_EXPECT(message != NULL);
        RUDA_EXPECT(shadow != NULL);
        RUDA_EXPECT(alias != NULL);
        RUDA_EXPECT(keep != NULL);
        if (message != NULL) {
            const RudaTypeDescriptor *nominal = ruda_type_descriptor(
                &checked.types,
                message->type
            );
            RUDA_EXPECT(message->is_public);
            RUDA_EXPECT(message->variants.length == 4);
            RUDA_EXPECT(nominal != NULL);
            if (nominal != NULL) {
                RUDA_EXPECT(nominal->kind == RUDA_TYPE_KIND_NOMINAL);
                RUDA_EXPECT(ruda_symbol_id_equal(
                    nominal->symbol,
                    message->symbol
                ));
            }
            quit = ruda_vector_at_const(&message->variants, 0);
            write = ruda_vector_at_const(&message->variants, 1);
            move = ruda_vector_at_const(&message->variants, 2);
            chain = ruda_vector_at_const(&message->variants, 3);
            RUDA_EXPECT(quit != NULL);
            RUDA_EXPECT(write != NULL);
            RUDA_EXPECT(move != NULL);
            RUDA_EXPECT(chain != NULL);
            if (quit != NULL) {
                RUDA_EXPECT(quit->kind == RUDA_CHECKED_ENUM_VARIANT_UNIT);
                RUDA_EXPECT(quit->payload.length == 0);
            }
            if (write != NULL) {
                const RudaCheckedEnumPayloadItem *text =
                    ruda_vector_at_const(&write->payload, 0);
                const RudaCheckedEnumPayloadItem *code =
                    ruda_vector_at_const(&write->payload, 1);
                RUDA_EXPECT(write->kind
                    == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL);
                RUDA_EXPECT(write->payload.length == 2);
                RUDA_EXPECT(text != NULL && !text->has_name
                    && text->type == RUDA_TYPE_STRING);
                RUDA_EXPECT(code != NULL && !code->has_name
                    && code->type == RUDA_TYPE_I64);
            }
            if (move != NULL) {
                const RudaCheckedEnumPayloadItem *x = ruda_vector_at_const(
                    &move->payload,
                    0
                );
                const RudaCheckedEnumPayloadItem *y = ruda_vector_at_const(
                    &move->payload,
                    1
                );
                RUDA_EXPECT(move->kind == RUDA_CHECKED_ENUM_VARIANT_NAMED);
                RUDA_EXPECT(move->payload.length == 2);
                RUDA_EXPECT(x != NULL && x->has_name
                    && x->name.length == strlen("x")
                    && x->type == RUDA_TYPE_I64);
                RUDA_EXPECT(y != NULL && y->has_name
                    && y->name.length == strlen("y")
                    && y->type == RUDA_TYPE_I64);
            }
            if (chain != NULL) {
                const RudaCheckedEnumPayloadItem *next =
                    ruda_vector_at_const(&chain->payload, 0);
                RUDA_EXPECT(next != NULL && next->type == message->type);
            }
        }
        if (message != NULL && shadow != NULL) {
            RUDA_EXPECT(!shadow->is_public);
            RUDA_EXPECT(shadow->variants.length == message->variants.length);
            RUDA_EXPECT(shadow->type != message->type);
            RUDA_EXPECT(!ruda_symbol_id_equal(shadow->symbol, message->symbol));
        }
        if (message != NULL && alias != NULL) {
            RUDA_EXPECT(alias->target_type == message->type);
            RUDA_EXPECT(alias->is_public);
        }
        if (message != NULL && keep != NULL) {
            RUDA_EXPECT(keep->is_public);
            signature = ruda_type_descriptor(&checked.types, keep->signature_type);
            RUDA_EXPECT(signature != NULL);
            if (signature != NULL) {
                RUDA_EXPECT(signature->arguments[0] == message->type);
                RUDA_EXPECT(signature->arguments[1] == message->type);
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, mutual));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedRecord *node = ruda_checked_record(&checked, 0);
        const RudaCheckedEnum *event = ruda_checked_enum(&checked, 0);
        const RudaCheckedRecordField *event_field = node != NULL
            ? ruda_vector_at_const(&node->fields, 0)
            : NULL;
        const RudaCheckedEnumVariant *attach = event != NULL
            ? ruda_vector_at_const(&event->variants, 0)
            : NULL;
        const RudaCheckedEnumVariant *next = event != NULL
            ? ruda_vector_at_const(&event->variants, 1)
            : NULL;
        const RudaCheckedEnumPayloadItem *attached = attach != NULL
            ? ruda_vector_at_const(&attach->payload, 0)
            : NULL;
        const RudaCheckedEnumPayloadItem *continued = next != NULL
            ? ruda_vector_at_const(&next->payload, 0)
            : NULL;
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(node != NULL && event != NULL);
        RUDA_EXPECT(event_field != NULL && attached != NULL
            && continued != NULL);
        if (node != NULL && event != NULL && event_field != NULL
            && attached != NULL && continued != NULL) {
            RUDA_EXPECT(event_field->type == event->type);
            RUDA_EXPECT(attached->type == node->type);
            RUDA_EXPECT(continued->type == event->type);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        duplicate_variant,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "Ready",
        1
    );
    ruda_checker_expect_one_diagnostic(
        duplicate_payload_field,
        RUDA_DIAGNOSTIC_DUPLICATE_NAME,
        "x",
        1
    );
    ruda_checker_expect_one_diagnostic(
        unknown_payload_type,
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
    ruda_checker_expect_one_diagnostic(
        public_private_payload,
        RUDA_DIAGNOSTIC_PRIVATE_TYPE_IN_PUBLIC_API,
        "Hidden",
        1
    );

    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    options = ruda_check_default_options();
    options.max_enums = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.max_variants_per_enum = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.max_payload_items_per_variant = 1;
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "enum Generic<T> { Value(T), }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized);
    ruda_check_result_dispose(&checked);
    ruda_checker_fixture_dispose(&fixture);

    RUDA_EXPECT(ruda_checker_fixture_init(
        &fixture,
        "@derive(Debug) enum State { Ready, }\n"
    ));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enums",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_INCOMPLETE_LANGUAGE);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_checker_enum_package_order(void) {
    static const char entry_source[] =
        "fn keep(value: Event): Event { return value; }\n"
        "fn empty(): Event { return Event.Empty; }\n"
        "fn kind(value: Event): int {\n"
        "  return match value {\n"
        "    case Empty => 0;\n"
        "    case Link(node) | Peer(node) => 1;\n"
        "  };\n"
        "}\n";
    static const char types_source[] =
        "enum Event { Empty, Link(Node), Peer(Node), }\n"
        "type Node { event: Event; }\n";
    RudaCheckerFixture entry;
    RudaCheckerFixture types;
    RudaCheckerFixture missing_entry;
    const RudaAstTree *asts[2];
    RudaCheckResult checked;
    const RudaCheckedEnum *event = NULL;
    const RudaCheckedRecord *node = NULL;
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
        "test/enum-package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckedEnumVariant *link = NULL;
        const RudaCheckedEnumPayloadItem *linked = NULL;
        const RudaCheckedRecordField *field = NULL;
        event = ruda_checked_enum(&checked, 0);
        node = ruda_checked_record(&checked, 0);
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.functions.length == 3);
        RUDA_EXPECT(event != NULL && node != NULL);
        if (event != NULL && node != NULL) {
            link = ruda_vector_at_const(&event->variants, 1);
            linked = link != NULL
                ? ruda_vector_at_const(&link->payload, 0)
                : NULL;
            field = ruda_vector_at_const(&node->fields, 0);
            RUDA_EXPECT(linked != NULL && linked->type == node->type);
            RUDA_EXPECT(field != NULL && field->type == event->type);
            symbol = event->symbol;
            type = event->type;
        }
        {
            const RudaCheckedFunction *empty = ruda_checked_function(
                &checked,
                1
            );
            RUDA_EXPECT(empty != NULL);
            if (empty != NULL) {
                RUDA_EXPECT(empty->mir.enums.length == 1);
                RUDA_EXPECT(empty->mir.enum_constructions.length == 1);
            }
            {
                const RudaCheckedFunction *kind = ruda_checked_function(
                    &checked,
                    2
                );
                RUDA_EXPECT(kind != NULL);
                if (kind != NULL) {
                    RUDA_EXPECT(kind->mir.enums.length == 1);
                    RUDA_EXPECT(ruda_mir_verify(&kind->mir)
                        == RUDA_MIR_VERIFY_OK);
                }
            }
        }
        ruda_check_result_dispose(&checked);
    }

    asts[0] = &entry.ast;
    asts[1] = &types.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/enum-package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        event = ruda_checked_enum(&checked, 0);
        RUDA_EXPECT(event != NULL);
        RUDA_EXPECT(checked.functions.length == 3);
        if (event != NULL) {
            RUDA_EXPECT(ruda_symbol_id_equal(event->symbol, symbol));
            RUDA_EXPECT(event->type == type);
        }
        ruda_check_result_dispose(&checked);
    }
    RUDA_EXPECT(ruda_checker_fixture_init_path(
        &missing_entry,
        "b_missing.ruda",
        "type Alias = Event;\n"
        "fn kind(value: Alias): int {\n"
        "  return match value { case Empty => 0; case Link(_) => 1; };\n"
        "}\n"
    ));
    asts[0] = &types.ast;
    asts[1] = &missing_entry.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "test/enum-package-witness",
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
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                diagnostic->message,
                "enum match is missing witness: Peer(_)"
            ) == 0);
            RUDA_EXPECT(diagnostic->owned_message.data != NULL);
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&missing_entry);
    ruda_checker_fixture_dispose(&types);
    ruda_checker_fixture_dispose(&entry);
}

void ruda_test_checker_enum_execution(void) {
    static const char source[] =
        "enum Message {\n"
        "  Quit,\n"
        "  Write(int, int),\n"
        "  Move { x: int; y: int; },\n"
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
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedEnum *message = NULL;
    const RudaCheckedFunction *quit = NULL;
    const RudaCheckedFunction *write = NULL;
    const RudaCheckedFunction *move = NULL;
    RudaMirExecution execution;
    RudaCheckInterpretOptions interpret_options =
        ruda_check_interpret_default_options();
    RudaMirValue arguments[2];
    RudaMirValue result;
    RudaMirValue payload;
    RudaMirValue enum_argument;
    size_t selected_variant = 0;
    size_t payload_count = 0;
    size_t retained_count = 0;
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/enum-execution",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(checked.enums.length == 1);
        RUDA_EXPECT(checked.type_aliases.length == 1);
        RUDA_EXPECT(checked.functions.length == 9);
        message = ruda_checked_enum(&checked, 0);
        quit = ruda_checked_function(&checked, 0);
        write = ruda_checked_function(&checked, 1);
        move = ruda_checked_function(&checked, 2);
        RUDA_EXPECT(message != NULL);
        RUDA_EXPECT(quit != NULL && write != NULL && move != NULL);
        if (quit != NULL) {
            RUDA_EXPECT(quit->mir.enums.length == 1);
            RUDA_EXPECT(quit->mir.enum_variants.length == 3);
            RUDA_EXPECT(quit->mir.enum_payloads.length == 4);
            RUDA_EXPECT(quit->mir.enum_constructions.length == 1);
            RUDA_EXPECT(quit->mir.enum_arguments.length == 0);
            RUDA_EXPECT(ruda_check_interpret_function(
                &checked,
                0,
                NULL,
                0,
                ruda_system_allocator(),
                ruda_check_interpret_default_options(),
                &result
            ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
        }
        if (write != NULL) {
            RUDA_EXPECT(write->mir.enums.length == 1);
            RUDA_EXPECT(write->mir.enum_constructions.length == 1);
            RUDA_EXPECT(write->mir.enum_arguments.length == 2);
        }
        if (move != NULL) {
            RUDA_EXPECT(move->mir.enums.length == 1);
            RUDA_EXPECT(move->mir.enum_constructions.length == 1);
            RUDA_EXPECT(move->mir.enum_arguments.length == 2);
        }
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            ruda_system_allocator(),
            ruda_mir_execution_default_options()
        ));
        interpret_options.execution = &execution;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            0,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(message != NULL && result.type == message->type);
        RUDA_EXPECT(ruda_mir_execution_owns_reference(&execution, result));
        RUDA_EXPECT(ruda_mir_execution_inspect_enum(
            &execution,
            result,
            &selected_variant,
            &payload_count
        ));
        RUDA_EXPECT(selected_variant == 0 && payload_count == 0);

        arguments[0].type = RUDA_TYPE_I64;
        arguments[0].as.i64 = 10;
        arguments[1].type = RUDA_TYPE_I64;
        arguments[1].as.i64 = 20;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            1,
            arguments,
            2,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_execution_owns_reference(&execution, result));
        RUDA_EXPECT(ruda_mir_execution_inspect_enum(
            &execution,
            result,
            &selected_variant,
            &payload_count
        ));
        RUDA_EXPECT(selected_variant == 1 && payload_count == 2);
        RUDA_EXPECT(ruda_mir_execution_enum_payload(
            &execution,
            result,
            0,
            &payload
        ));
        RUDA_EXPECT(payload.type == RUDA_TYPE_I64 && payload.as.i64 == 10);
        RUDA_EXPECT(ruda_mir_execution_enum_payload(
            &execution,
            result,
            1,
            &payload
        ));
        RUDA_EXPECT(payload.as.i64 == 20);

        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            2,
            arguments,
            2,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_execution_owns_reference(&execution, result));
        RUDA_EXPECT(ruda_mir_execution_inspect_enum(
            &execution,
            result,
            &selected_variant,
            &payload_count
        ));
        RUDA_EXPECT(selected_variant == 2 && payload_count == 2);
        RUDA_EXPECT(ruda_mir_execution_enum_payload(
            &execution,
            result,
            0,
            &payload
        ));
        RUDA_EXPECT(payload.as.i64 == 10);
        RUDA_EXPECT(ruda_mir_execution_enum_payload(
            &execution,
            result,
            1,
            &payload
        ));
        RUDA_EXPECT(payload.as.i64 == 20);

        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            3,
            arguments,
            2,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_execution_owns_reference(&execution, result));
        enum_argument = result;
        retained_count = execution.enums.length;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            &enum_argument,
            1,
            ruda_system_allocator(),
            ruda_check_interpret_default_options(),
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            &enum_argument,
            1,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(ruda_mir_reference_equal(
            result.as.reference,
            enum_argument.as.reference
        ));
        RUDA_EXPECT(execution.enums.length == retained_count);
        enum_argument.as.reference = RUDA_MIR_INVALID_REFERENCE;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            4,
            &enum_argument,
            1,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);

        retained_count = execution.enums.length;
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            6,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        RUDA_EXPECT(execution.enums.length == retained_count);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            8,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &result
        ) == RUDA_MIR_INTERPRET_DIVISION_BY_ZERO);
        RUDA_EXPECT(execution.enums.length == retained_count);
        RUDA_EXPECT(execution.objects.length == 0);
        ruda_mir_execution_dispose(&execution);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_fixture_dispose(&fixture);

    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(): Message { return Message.Missing; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_VARIANT,
        "Missing",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Write(int), }\n"
        "fn bad(): Message { return Message.Write; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Message.Write",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(): Message { return Message.Quit(); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Message.Quit()",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; }, }\n"
        "fn bad(): Message { return Message.Move(1); }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "Message.Move(1)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Write(int, int), }\n"
        "fn bad(): Message { return Message.Write(1); }\n",
        RUDA_DIAGNOSTIC_CALL_ARITY,
        "(1)",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; }, }\n"
        "fn bad(): Message { return Message.Move { y: 1 }; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
        "y",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; }, }\n"
        "fn bad(): Message { return Message.Move { x: 1, x: 2 }; }\n",
        RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
        "x",
        2
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; y: int; }, }\n"
        "fn bad(): Message { return Message.Move { x: 1 }; }\n",
        RUDA_DIAGNOSTIC_MISSING_FIELD,
        "Message.Move { x: 1 }",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; }, }\n"
        "fn bad(): Message { return Message.Move { x: true }; }\n",
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "true",
        0
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Move { x: int; }, }\n"
        "fn bad(): Message { return Message.Move { x }; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "x",
        1
    );
    ruda_checker_expect_one_diagnostic(
        "enum Message { Quit, }\n"
        "fn bad(): Message { return Quit; }\n",
        RUDA_DIAGNOSTIC_UNKNOWN_NAME,
        "Quit",
        1
    );
}
