#include "test_native_c_emitter_support.h"

#include "test_checker_support.h"

#include <string.h>

static const MarimayCheckedFunction *mmay_test_native_nominal_function(
    const MarimayCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    if (checked == NULL || name == NULL) {
        return NULL;
    }
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
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

static void mmay_test_native_interface_without_construction(void) {
    const MarimaySymbolId dispatch_symbol = {{0x91}};
    const MarimaySymbolId entry_symbol = {{0x92}};
    const MarimaySymbolId unused_symbol = {{0x93}};
    const MarimayTypeId interface_type = MMAY_TYPE_FIRST_DYNAMIC + 900;
    MarimayMirFunction dispatch;
    MarimayMirFunction entry;
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInterfaceId interface_id = MMAY_MIR_NO_INTERFACE;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirCallArgument argument;
    MarimayMirInstruction instruction;
    MarimayMirTerminator terminator;
    const MarimayMirFunction *functions[2];
    MarimayNativeCProgram program;
    MarimayOwnedBytes output;
    memset(&dispatch, 0, sizeof(dispatch));
    memset(&entry, 0, sizeof(entry));
    memset(&argument, 0, sizeof(argument));
    memset(&instruction, 0, sizeof(instruction));
    memset(&terminator, 0, sizeof(terminator));
    memset(&program, 0, sizeof(program));
    memset(&output, 0, sizeof(output));
    MMAY_EXPECT(mmay_mir_function_init(
        &dispatch,
        mmay_system_allocator(),
        dispatch_symbol,
        &interface_type,
        1,
        MMAY_TYPE_I64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&dispatch, MMAY_TYPE_I64, &result));
    MMAY_EXPECT(mmay_mir_add_interface(
        &dispatch, interface_type, 1, &interface_id
    ));
    argument.register_id = 0;
    argument.type = interface_type;
    MMAY_EXPECT(mmay_mir_add_call(
        &dispatch, unused_symbol, MMAY_TYPE_I64, &argument, 1, &call
    ));
    MMAY_EXPECT(mmay_mir_set_call_interface_slot(&dispatch, call, 0));
    MMAY_EXPECT(mmay_mir_add_block(&dispatch, &block));
    instruction.kind = MMAY_MIR_CALL_INTERFACE;
    instruction.target = result;
    instruction.call = call;
    instruction.span.start = 1;
    instruction.span.end = 2;
    MMAY_EXPECT(mmay_mir_append_instruction(&dispatch, block, &instruction));
    terminator.kind = MMAY_MIR_RETURN;
    terminator.value = result;
    terminator.span.start = 2;
    terminator.span.end = 3;
    MMAY_EXPECT(mmay_mir_set_terminator(&dispatch, block, terminator));
    MMAY_EXPECT(mmay_mir_verify(&dispatch) == MMAY_MIR_VERIFY_OK);

    block = MMAY_MIR_NO_BLOCK;
    result = MMAY_MIR_NO_REGISTER;
    MMAY_EXPECT(mmay_mir_function_init(
        &entry,
        mmay_system_allocator(),
        entry_symbol,
        NULL,
        0,
        MMAY_TYPE_I64,
        mmay_mir_function_default_options()
    ));
    MMAY_EXPECT(mmay_mir_add_register(&entry, MMAY_TYPE_I64, &result));
    MMAY_EXPECT(mmay_mir_add_block(&entry, &block));
    instruction = mmay_test_native_instruction(
        MMAY_MIR_CONSTANT_I64,
        result,
        MMAY_MIR_NO_REGISTER,
        MMAY_MIR_NO_REGISTER,
        0,
        false,
        3
    );
    MMAY_EXPECT(mmay_mir_append_instruction(&entry, block, &instruction));
    terminator = mmay_test_native_terminator(
        MMAY_MIR_RETURN,
        result,
        MMAY_MIR_NO_BLOCK,
        MMAY_MIR_NO_BLOCK,
        4
    );
    MMAY_EXPECT(mmay_mir_set_terminator(&entry, block, terminator));
    MMAY_EXPECT(mmay_mir_verify(&entry) == MMAY_MIR_VERIFY_OK);
    functions[0] = &dispatch;
    functions[1] = &entry;
    program.functions = functions;
    program.function_count = 2;
    program.entry_symbol = entry_symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program,
        mmay_system_allocator(),
        mmay_native_c_emit_default_options(),
        &output
    ) == MMAY_NATIVE_C_EMIT_UNSUPPORTED_CALL);
    MMAY_EXPECT(output.data == NULL && output.length == 0);
    mmay_owned_bytes_dispose(&output);
    mmay_mir_function_dispose(&entry);
    mmay_mir_function_dispose(&dispatch);
}

static void mmay_test_native_nominal_boundary_cases(void) {
    static const char source[] =
        "type Counter { label: string, mutable value: int, }\n"
        "enum Message {\n"
        "  Unit,\n"
        "  Pair(int, int),\n"
        "  Text(string, Counter),\n"
        "  Named { x: int, y: int, },\n"
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
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *make = NULL;
    const MarimayCheckedFunction *mutate = NULL;
    const MarimayCheckedFunction *same = NULL;
    const MarimayCheckedFunction *text = NULL;
    const MarimayCheckedFunction *positional = NULL;
    const MarimayCheckedFunction *named = NULL;
    const MarimayCheckedFunction *nested = NULL;
    const MarimayCheckedFunction *dispatch = NULL;
    const MarimayCheckedFunction *main = NULL;
    const MarimayMirFunction *functions[16];
    MarimayNativeCProgram program;
    MarimayOwnedBytes generated;
    size_t function_count = 0;
    size_t index = 0;
    bool reached_boundary = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(functions, 0, sizeof(functions));
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    if (!fixture.ast_initialized) {
        mmay_checker_fixture_dispose(&fixture);
        return;
    }
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/native-nominal-boundary",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (!checked.initialized) {
        mmay_checker_fixture_dispose(&fixture);
        return;
    }
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(checked.records.length == 1 && checked.enums.length == 1
        && checked.interfaces.length == 1);
    MMAY_EXPECT(checked.functions.length <= sizeof(functions) / sizeof(functions[0]));
    for (index = 0; index < checked.functions.length
        && index < sizeof(functions) / sizeof(functions[0]); index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked,
            index
        );
        MMAY_EXPECT(function != NULL);
        if (function != NULL) {
            functions[function_count] = &function->mir;
            function_count += 1;
        }
    }
    make = mmay_test_native_nominal_function(&checked, "make");
    mutate = mmay_test_native_nominal_function(&checked, "mutate");
    same = mmay_test_native_nominal_function(&checked, "same");
    text = mmay_test_native_nominal_function(&checked, "text");
    positional = mmay_test_native_nominal_function(&checked, "positionalOrder");
    named = mmay_test_native_nominal_function(&checked, "namedOrder");
    nested = mmay_test_native_nominal_function(&checked, "nested");
    dispatch = mmay_test_native_nominal_function(&checked, "dispatch");
    main = mmay_test_native_nominal_function(&checked, "main");
    MMAY_EXPECT(make != NULL && mutate != NULL && same != NULL && text != NULL
        && positional != NULL && named != NULL && nested != NULL
        && dispatch != NULL && main != NULL);
    if (make != NULL) {
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &make->mir, MMAY_MIR_RECORD_NEW
        ) == 1);
    }
    if (mutate != NULL) {
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &mutate->mir, MMAY_MIR_RECORD_SET
        ) == 1);
    }
    if (same != NULL) {
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &same->mir, MMAY_MIR_RECORD_IDENTITY_SAME
        ) == 1);
    }
    if (text != NULL) {
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &text->mir, MMAY_MIR_ENUM_NEW
        ) == 1 && text->mir.string_constants.length == 1);
    }
    if (positional != NULL && named != NULL && nested != NULL) {
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &positional->mir, MMAY_MIR_ENUM_NEW
        ) == 1);
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &named->mir, MMAY_MIR_ENUM_NEW
        ) == 1);
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &nested->mir, MMAY_MIR_ENUM_NEW
        ) == 1);
    }
    if (dispatch != NULL && main != NULL) {
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &dispatch->mir, MMAY_MIR_CALL_INTERFACE
        ) == 1);
        MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
            &main->mir, MMAY_MIR_INTERFACE_NEW
        ) == 1);
    }
    if (main != NULL) {
        program.functions = functions;
        program.function_count = function_count;
        program.entry_symbol = main->mir.symbol;
        for (index = 0; index < 4096; index += 1) {
            MarimayFailAllocator state;
            MarimayOwnedBytes output;
            MarimayNativeCEmitStatus status;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
            memset(&output, 0, sizeof(output));
            status = mmay_native_c_emit_program(
                &program,
                mmay_fail_allocator(&state),
                mmay_native_c_emit_default_options(),
                &output
            );
            MMAY_EXPECT(status == MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED
                || status == MMAY_NATIVE_C_EMIT_OK);
            if (status != MMAY_NATIVE_C_EMIT_OK) {
                MMAY_EXPECT(output.data == NULL && output.length == 0);
            }
            reached_boundary = reached_boundary
                || status == MMAY_NATIVE_C_EMIT_OK;
            mmay_owned_bytes_dispose(&output);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
            if (reached_boundary) {
                break;
            }
        }
    }
    MMAY_EXPECT(reached_boundary);
    if (reached_boundary) {
        MMAY_EXPECT(mmay_native_c_emit_program(
            &program,
            mmay_system_allocator(),
            mmay_native_c_emit_default_options(),
            &generated
        ) == MMAY_NATIVE_C_EMIT_OK);
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "MarimayManagedRecordGeneratedView mmay_g_rv_0 = {0};"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_managed_record_generated_bind("
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_g_rv_0.object->fields["
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "MarimayManagedReference *mmay_g_location_storage["
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated,
            "#if MMAY_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
            "#if MMAY_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
        ));
#if !defined(_WIN32)
        if (generated.data != NULL) {
            mmay_test_native_compile_and_run(
                &generated,
                "ok:bool:true\n",
                0
            );
        }
#endif
    }
    if (positional != NULL) {
        MarimayOwnedBytes entry_output;
        memset(&entry_output, 0, sizeof(entry_output));
        program.entry_symbol = positional->mir.symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &program,
            mmay_system_allocator(),
            mmay_native_c_emit_default_options(),
            &entry_output
        ) == MMAY_NATIVE_C_EMIT_UNSUPPORTED_TYPE);
        MMAY_EXPECT(entry_output.data == NULL && entry_output.length == 0);
        mmay_owned_bytes_dispose(&entry_output);
    }
    mmay_owned_bytes_dispose(&generated);
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_native_nominal_boundary(void) {
    mmay_test_native_interface_without_construction();
    mmay_test_native_nominal_boundary_cases();
}
