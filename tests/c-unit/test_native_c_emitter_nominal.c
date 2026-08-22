#include "test_native_c_emitter_support.h"

#include "test_checker_support.h"

#include <string.h>

static const RudaCheckedFunction *ruda_test_native_nominal_function(
    const RudaCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    if (checked == NULL || name == NULL) {
        return NULL;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            checked,
            index
        );
        if (function != NULL && function->name.length == strlen(name)
            && memcmp(function->name.data, name, function->name.length) == 0) {
            return function;
        }
    }
    return NULL;
}

static void ruda_test_native_interface_without_construction(void) {
    const RudaSymbolId dispatch_symbol = {{0x91}};
    const RudaSymbolId entry_symbol = {{0x92}};
    const RudaSymbolId unused_symbol = {{0x93}};
    const RudaTypeId interface_type = RUDA_TYPE_FIRST_DYNAMIC + 900;
    RudaMirFunction dispatch;
    RudaMirFunction entry;
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInterfaceId interface_id = RUDA_MIR_NO_INTERFACE;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirCallArgument argument;
    RudaMirInstruction instruction;
    RudaMirTerminator terminator;
    const RudaMirFunction *functions[2];
    RudaNativeCProgram program;
    RudaOwnedBytes output;
    memset(&dispatch, 0, sizeof(dispatch));
    memset(&entry, 0, sizeof(entry));
    memset(&argument, 0, sizeof(argument));
    memset(&instruction, 0, sizeof(instruction));
    memset(&terminator, 0, sizeof(terminator));
    memset(&program, 0, sizeof(program));
    memset(&output, 0, sizeof(output));
    RUDA_EXPECT(ruda_mir_function_init(
        &dispatch,
        ruda_system_allocator(),
        dispatch_symbol,
        &interface_type,
        1,
        RUDA_TYPE_I64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&dispatch, RUDA_TYPE_I64, &result));
    RUDA_EXPECT(ruda_mir_add_interface(
        &dispatch, interface_type, 1, &interface_id
    ));
    argument.register_id = 0;
    argument.type = interface_type;
    RUDA_EXPECT(ruda_mir_add_call(
        &dispatch, unused_symbol, RUDA_TYPE_I64, &argument, 1, &call
    ));
    RUDA_EXPECT(ruda_mir_set_call_interface_slot(&dispatch, call, 0));
    RUDA_EXPECT(ruda_mir_add_block(&dispatch, &block));
    instruction.kind = RUDA_MIR_CALL_INTERFACE;
    instruction.target = result;
    instruction.call = call;
    instruction.span.start = 1;
    instruction.span.end = 2;
    RUDA_EXPECT(ruda_mir_append_instruction(&dispatch, block, &instruction));
    terminator.kind = RUDA_MIR_RETURN;
    terminator.value = result;
    terminator.span.start = 2;
    terminator.span.end = 3;
    RUDA_EXPECT(ruda_mir_set_terminator(&dispatch, block, terminator));
    RUDA_EXPECT(ruda_mir_verify(&dispatch) == RUDA_MIR_VERIFY_OK);

    block = RUDA_MIR_NO_BLOCK;
    result = RUDA_MIR_NO_REGISTER;
    RUDA_EXPECT(ruda_mir_function_init(
        &entry,
        ruda_system_allocator(),
        entry_symbol,
        NULL,
        0,
        RUDA_TYPE_I64,
        ruda_mir_function_default_options()
    ));
    RUDA_EXPECT(ruda_mir_add_register(&entry, RUDA_TYPE_I64, &result));
    RUDA_EXPECT(ruda_mir_add_block(&entry, &block));
    instruction = ruda_test_native_instruction(
        RUDA_MIR_CONSTANT_I64,
        result,
        RUDA_MIR_NO_REGISTER,
        RUDA_MIR_NO_REGISTER,
        0,
        false,
        3
    );
    RUDA_EXPECT(ruda_mir_append_instruction(&entry, block, &instruction));
    terminator = ruda_test_native_terminator(
        RUDA_MIR_RETURN,
        result,
        RUDA_MIR_NO_BLOCK,
        RUDA_MIR_NO_BLOCK,
        4
    );
    RUDA_EXPECT(ruda_mir_set_terminator(&entry, block, terminator));
    RUDA_EXPECT(ruda_mir_verify(&entry) == RUDA_MIR_VERIFY_OK);
    functions[0] = &dispatch;
    functions[1] = &entry;
    program.functions = functions;
    program.function_count = 2;
    program.entry_symbol = entry_symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &program,
        ruda_system_allocator(),
        ruda_native_c_emit_default_options(),
        &output
    ) == RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL);
    RUDA_EXPECT(output.data == NULL && output.length == 0);
    ruda_owned_bytes_dispose(&output);
    ruda_mir_function_dispose(&entry);
    ruda_mir_function_dispose(&dispatch);
}

static void ruda_test_native_nominal_boundary_cases(void) {
    static const char source[] =
        "type Counter { label: string; mutable value: int; }\n"
        "enum Message {\n"
        "  Unit,\n"
        "  Pair(int, int),\n"
        "  Text(string, Counter),\n"
        "  Named { x: int; y: int; },\n"
        "  Nested(Message),\n"
        "}\n"
        "interface Show { show(): int; }\n"
        "fn show(value: Counter): int { return value.value; }\n"
        "fn make(seed: int): Counter {\n"
        "  return Counter { label: \"counter\", value: seed };\n"
        "}\n"
        "fn mutate(value: Counter): int {\n"
        "  value.value += 1;\n"
        "  return value.value;\n"
        "}\n"
        "fn same(left: Counter, right: Counter): bool {\n"
        "  return identity.same(left, right);\n"
        "}\n"
        "fn relay(value: Counter): Counter { return value; }\n"
        "fn text(value: Counter): Message {\n"
        "  return Message.Text(\"payload\", value);\n"
        "}\n"
        "fn positionalOrder(): Message {\n"
        "  return Message.Pair(divideZero(), overflow());\n"
        "}\n"
        "fn namedOrder(): Message {\n"
        "  return Message.Named { y: divideZero(), x: overflow() };\n"
        "}\n"
        "fn nested(value: Message): Message { return Message.Nested(value); }\n"
        "fn dispatch(value: Show): int { return value.show(); }\n"
        "fn overflow(): int { return 9223372036854775807 + 1; }\n"
        "fn divideZero(): int { return 1 / 0; }\n"
        "fn main(): bool {\n"
        "  const value = make(4);\n"
        "  mutate(value);\n"
        "  const message = text(value);\n"
        "  nested(message);\n"
        "  dispatch(value);\n"
        "  return same(value, relay(value));\n"
        "}\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *make = NULL;
    const RudaCheckedFunction *mutate = NULL;
    const RudaCheckedFunction *same = NULL;
    const RudaCheckedFunction *text = NULL;
    const RudaCheckedFunction *positional = NULL;
    const RudaCheckedFunction *named = NULL;
    const RudaCheckedFunction *nested = NULL;
    const RudaCheckedFunction *dispatch = NULL;
    const RudaCheckedFunction *main = NULL;
    const RudaMirFunction *functions[16];
    RudaNativeCProgram program;
    RudaOwnedBytes generated;
    size_t function_count = 0;
    size_t index = 0;
    bool reached_boundary = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(functions, 0, sizeof(functions));
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    if (!fixture.ast_initialized) {
        ruda_checker_fixture_dispose(&fixture);
        return;
    }
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/native-nominal-boundary",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (!checked.initialized) {
        ruda_checker_fixture_dispose(&fixture);
        return;
    }
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(checked.records.length == 1 && checked.enums.length == 1
        && checked.interfaces.length == 1);
    RUDA_EXPECT(checked.functions.length <= sizeof(functions) / sizeof(functions[0]));
    for (index = 0; index < checked.functions.length
        && index < sizeof(functions) / sizeof(functions[0]); index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked,
            index
        );
        RUDA_EXPECT(function != NULL);
        if (function != NULL) {
            functions[function_count] = &function->mir;
            function_count += 1;
        }
    }
    make = ruda_test_native_nominal_function(&checked, "make");
    mutate = ruda_test_native_nominal_function(&checked, "mutate");
    same = ruda_test_native_nominal_function(&checked, "same");
    text = ruda_test_native_nominal_function(&checked, "text");
    positional = ruda_test_native_nominal_function(&checked, "positionalOrder");
    named = ruda_test_native_nominal_function(&checked, "namedOrder");
    nested = ruda_test_native_nominal_function(&checked, "nested");
    dispatch = ruda_test_native_nominal_function(&checked, "dispatch");
    main = ruda_test_native_nominal_function(&checked, "main");
    RUDA_EXPECT(make != NULL && mutate != NULL && same != NULL && text != NULL
        && positional != NULL && named != NULL && nested != NULL
        && dispatch != NULL && main != NULL);
    if (make != NULL) {
        RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
            &make->mir, RUDA_MIR_RECORD_NEW
        ) == 1);
    }
    if (mutate != NULL) {
        RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
            &mutate->mir, RUDA_MIR_RECORD_SET
        ) == 1);
    }
    if (same != NULL) {
        RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
            &same->mir, RUDA_MIR_RECORD_IDENTITY_SAME
        ) == 1);
    }
    if (text != NULL) {
        RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
            &text->mir, RUDA_MIR_ENUM_NEW
        ) == 1 && text->mir.string_constants.length == 1);
    }
    if (positional != NULL && named != NULL && nested != NULL) {
        RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
            &positional->mir, RUDA_MIR_ENUM_NEW
        ) == 1);
        RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
            &named->mir, RUDA_MIR_ENUM_NEW
        ) == 1);
        RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
            &nested->mir, RUDA_MIR_ENUM_NEW
        ) == 1);
    }
    if (dispatch != NULL && main != NULL) {
        RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
            &dispatch->mir, RUDA_MIR_CALL_INTERFACE
        ) == 1);
        RUDA_EXPECT(ruda_test_mir_instruction_kind_count(
            &main->mir, RUDA_MIR_INTERFACE_NEW
        ) == 1);
    }
    if (main != NULL) {
        program.functions = functions;
        program.function_count = function_count;
        program.entry_symbol = main->mir.symbol;
        for (index = 0; index < 32; index += 1) {
            RudaFailAllocator state;
            RudaOwnedBytes output;
            RudaNativeCEmitStatus status;
            ruda_fail_allocator_init(&state, ruda_system_allocator(), index);
            memset(&output, 0, sizeof(output));
            status = ruda_native_c_emit_program(
                &program,
                ruda_fail_allocator(&state),
                ruda_native_c_emit_default_options(),
                &output
            );
            RUDA_EXPECT(status == RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED
                || status == RUDA_NATIVE_C_EMIT_OK);
            if (status != RUDA_NATIVE_C_EMIT_OK) {
                RUDA_EXPECT(output.data == NULL && output.length == 0);
            }
            reached_boundary = reached_boundary
                || status == RUDA_NATIVE_C_EMIT_OK;
            ruda_owned_bytes_dispose(&output);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
            if (reached_boundary) {
                break;
            }
        }
    }
    RUDA_EXPECT(reached_boundary);
    if (reached_boundary) {
        RUDA_EXPECT(ruda_native_c_emit_program(
            &program,
            ruda_system_allocator(),
            ruda_native_c_emit_default_options(),
            &generated
        ) == RUDA_NATIVE_C_EMIT_OK);
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "RudaManagedRecordView ruda_g_rv_0 = {0};"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "ruda_managed_record_view_bind("
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "ruda_g_rv_0.fields["
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "RudaManagedReference *ruda_g_location_storage["
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated,
            "#if RUDA_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
            "#if RUDA_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
        ));
#if !defined(_WIN32)
        if (generated.data != NULL) {
            ruda_test_native_compile_and_run(
                &generated,
                "ok:bool:true\n",
                0
            );
        }
#endif
    }
    if (positional != NULL) {
        RudaOwnedBytes entry_output;
        memset(&entry_output, 0, sizeof(entry_output));
        program.entry_symbol = positional->mir.symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(
            &program,
            ruda_system_allocator(),
            ruda_native_c_emit_default_options(),
            &entry_output
        ) == RUDA_NATIVE_C_EMIT_UNSUPPORTED_TYPE);
        RUDA_EXPECT(entry_output.data == NULL && entry_output.length == 0);
        ruda_owned_bytes_dispose(&entry_output);
    }
    ruda_owned_bytes_dispose(&generated);
    ruda_check_result_dispose(&checked);
    ruda_checker_fixture_dispose(&fixture);
}

void ruda_test_native_nominal_boundary(void) {
    ruda_test_native_interface_without_construction();
    ruda_test_native_nominal_boundary_cases();
}
