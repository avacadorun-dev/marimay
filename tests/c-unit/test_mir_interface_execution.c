#include "test_mir_support.h"

#include <string.h>

#include "mmay/base/allocator.h"

enum {
    MMAY_TEST_MIR_INTERFACE_RECORD_TYPE = MMAY_TYPE_FIRST_DYNAMIC + 100,
    MMAY_TEST_MIR_INTERFACE_VALUE_TYPE
};

typedef struct MarimayTestMirInterfaceCallContext {
    MarimaySymbolId expected_method;
    MarimayMirExecution *execution;
    size_t calls;
} MarimayTestMirInterfaceCallContext;

static bool mmay_test_mir_interface_execution_symbol(
    const char *name,
    uint32_t discriminator,
    MarimaySymbolId *result
) {
    return mmay_symbol_id_from_parts(
        "example.test/mir-interface-execution",
        "src/interface-execution.my",
        name,
        discriminator,
        result
    );
}

static bool mmay_test_mir_interface_creator_init(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimaySymbolId method
) {
    MarimayMirRecordField field;
    MarimayMirRecordArgument record_argument;
    MarimayMirRecordId record = MMAY_MIR_NO_RECORD;
    MarimayMirRecordConstructionId record_construction =
        MMAY_MIR_NO_RECORD_CONSTRUCTION;
    MarimayMirInterfaceId interface_id = MMAY_MIR_NO_INTERFACE;
    MarimayMirInterfaceConstructionId interface_construction =
        MMAY_MIR_NO_INTERFACE_CONSTRUCTION;
    MarimayMirRegister scalar = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister payload = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister interface_value = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    bool ok = false;
    if (function == NULL) {
        return false;
    }
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function,
            mmay_system_allocator(),
            symbol,
            NULL,
            0,
            MMAY_TEST_MIR_INTERFACE_VALUE_TYPE,
            mmay_mir_function_default_options()
        )) {
        return false;
    }
    field.type = MMAY_TYPE_I64;
    field.is_mutable = false;
    ok = mmay_mir_add_register(function, MMAY_TYPE_I64, &scalar)
        && mmay_mir_add_register(
            function,
            MMAY_TEST_MIR_INTERFACE_RECORD_TYPE,
            &payload
        ) && mmay_mir_add_register(
            function,
            MMAY_TEST_MIR_INTERFACE_VALUE_TYPE,
            &interface_value
        ) && mmay_mir_add_block(function, &block)
        && mmay_mir_add_record(
            function,
            MMAY_TEST_MIR_INTERFACE_RECORD_TYPE,
            &field,
            1,
            &record
        );
    record_argument.register_id = scalar;
    record_argument.type = MMAY_TYPE_I64;
    ok = ok && mmay_mir_add_record_construction(
        function,
        record,
        &record_argument,
        1,
        &record_construction
    ) && mmay_mir_add_interface(
        function,
        MMAY_TEST_MIR_INTERFACE_VALUE_TYPE,
        1,
        &interface_id
    ) && mmay_mir_add_interface_construction(
        function,
        interface_id,
        payload,
        MMAY_TEST_MIR_INTERFACE_RECORD_TYPE,
        &method,
        1,
        &interface_construction
    );
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_I64;
    instruction.target = scalar;
    instruction.integer = 3;
    ok = ok && mmay_mir_append_instruction(function, block, &instruction);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_RECORD_NEW;
    instruction.target = payload;
    instruction.record = record;
    instruction.record_construction = record_construction;
    ok = ok && mmay_mir_append_instruction(function, block, &instruction);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_INTERFACE_NEW;
    instruction.target = interface_value;
    instruction.interface_construction = interface_construction;
    ok = ok && mmay_mir_append_instruction(function, block, &instruction)
        && mmay_mir_set_terminator(
            function,
            block,
            mmay_test_mir_return(interface_value)
        );
    if (!ok) {
        mmay_mir_function_dispose(function);
    }
    return ok;
}

static bool mmay_test_mir_interface_dispatcher_init(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimaySymbolId interface_symbol
) {
    MarimayTypeId parameters[2] = {
        MMAY_TEST_MIR_INTERFACE_VALUE_TYPE,
        MMAY_TYPE_I64
    };
    MarimayMirCallArgument arguments[2];
    MarimayMirRegister result = MMAY_MIR_NO_REGISTER;
    MarimayMirInterfaceId interface_id = MMAY_MIR_NO_INTERFACE;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirBlockId block = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    bool ok = false;
    if (function == NULL) {
        return false;
    }
    memset(function, 0, sizeof(*function));
    if (!mmay_mir_function_init(
            function,
            mmay_system_allocator(),
            symbol,
            parameters,
            2,
            MMAY_TYPE_I64,
            mmay_mir_function_default_options()
        )) {
        return false;
    }
    arguments[0].register_id = 0;
    arguments[0].type = MMAY_TEST_MIR_INTERFACE_VALUE_TYPE;
    arguments[1].register_id = 1;
    arguments[1].type = MMAY_TYPE_I64;
    ok = mmay_mir_add_register(function, MMAY_TYPE_I64, &result)
        && mmay_mir_add_block(function, &block)
        && mmay_mir_add_interface(
            function,
            MMAY_TEST_MIR_INTERFACE_VALUE_TYPE,
            1,
            &interface_id
        ) && mmay_mir_add_call(
            function,
            interface_symbol,
            MMAY_TYPE_I64,
            arguments,
            2,
            &call
        ) && mmay_mir_set_call_interface_slot(function, call, 0);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CALL_INTERFACE;
    instruction.target = result;
    instruction.call = call;
    ok = ok && mmay_mir_append_instruction(function, block, &instruction)
        && mmay_mir_set_terminator(
            function,
            block,
            mmay_test_mir_return(result)
        );
    if (!ok) {
        mmay_mir_function_dispose(function);
    }
    return ok;
}

static MarimayMirInterpretStatus mmay_test_mir_interface_call(
    void *opaque,
    MarimaySymbolId callee,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayMirValue *result
) {
    MarimayTestMirInterfaceCallContext *context = opaque;
    if (context == NULL || context->execution == NULL || arguments == NULL
        || result == NULL || argument_count != 2
        || !mmay_symbol_id_equal(callee, context->expected_method)
        || arguments[0].type != MMAY_TEST_MIR_INTERFACE_RECORD_TYPE
        || !mmay_mir_execution_owns_reference(
            context->execution,
            arguments[0]
        ) || arguments[1].type != MMAY_TYPE_I64
        || arguments[1].as.i64 != 4) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    context->calls += 1;
    result->type = MMAY_TYPE_I64;
    result->as.i64 = 7;
    return MMAY_MIR_INTERPRET_OK;
}

static MarimayMirInterpretStatus mmay_test_mir_interface_create(
    const MarimayMirFunction *creator,
    MarimayMirExecution *execution,
    MarimayAllocator allocator,
    MarimayMirValue *result
) {
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    options.execution = execution;
    return mmay_mir_interpret(
        creator,
        NULL,
        0,
        allocator,
        options,
        result
    );
}

static MarimayMirInterpretStatus mmay_test_mir_interface_dispatch(
    const MarimayMirFunction *dispatcher,
    MarimayMirValue interface_value,
    MarimayMirExecution *execution,
    MarimayAllocator allocator,
    MarimayTestMirInterfaceCallContext *context,
    MarimayMirValue *result
) {
    MarimayMirValue arguments[2];
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    arguments[0] = interface_value;
    arguments[1].type = MMAY_TYPE_I64;
    arguments[1].as.i64 = 4;
    options.execution = execution;
    options.call_context = context;
    options.call = context != NULL ? mmay_test_mir_interface_call : NULL;
    return mmay_mir_interpret(
        dispatcher,
        arguments,
        2,
        allocator,
        options,
        result
    );
}

static void mmay_test_mir_interface_object_limit(
    const MarimayMirFunction *creator
) {
    MarimayMirExecution execution;
    MarimayMirExecutionOptions execution_options =
        mmay_mir_execution_default_options();
    MarimayMirValue result;
    execution_options.max_objects = 1;
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution,
        mmay_system_allocator(),
        execution_options
    ));
    MMAY_EXPECT(mmay_test_mir_interface_create(
        creator,
        &execution,
        mmay_system_allocator(),
        &result
    ) == MMAY_MIR_INTERPRET_OBJECT_LIMIT);
    mmay_mir_execution_dispose(&execution);
}

static void mmay_test_mir_interface_allocation_failures(
    const MarimayMirFunction *creator,
    const MarimayMirFunction *dispatcher,
    MarimaySymbolId method
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 32; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayMirExecution execution;
        MarimayMirValue interface_value;
        MarimayMirValue result = {0};
        MarimayMirInterpretStatus status;
        MarimayTestMirInterfaceCallContext context;
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_index
        );
        allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_mir_execution_init(
            &execution,
            allocator,
            mmay_mir_execution_default_options()
        ));
        context.expected_method = method;
        context.execution = &execution;
        context.calls = 0;
        status = mmay_test_mir_interface_create(
            creator,
            &execution,
            allocator,
            &interface_value
        );
        if (status == MMAY_MIR_INTERPRET_OK) {
            status = mmay_test_mir_interface_dispatch(
                dispatcher,
                interface_value,
                &execution,
                allocator,
                &context,
                &result
            );
        }
        if (status == MMAY_MIR_INTERPRET_OK) {
            reached_success = true;
            MMAY_EXPECT(context.calls == 1);
            MMAY_EXPECT(result.type == MMAY_TYPE_I64);
            MMAY_EXPECT(result.as.i64 == 7);
        } else {
            MMAY_EXPECT(status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED);
        }
        mmay_mir_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_mir_interface_execution(void) {
    MarimaySymbolId creator_symbol;
    MarimaySymbolId dispatcher_symbol;
    MarimaySymbolId interface_symbol;
    MarimaySymbolId method_symbol;
    MarimayMirFunction creator;
    MarimayMirFunction dispatcher;
    MarimayMirExecution execution;
    MarimayMirExecution foreign_execution;
    MarimayMirValue interface_value;
    MarimayMirValue result;
    MarimayTestMirInterfaceCallContext context;
    MMAY_EXPECT(mmay_test_mir_interface_execution_symbol(
        "create",
        0,
        &creator_symbol
    ));
    MMAY_EXPECT(mmay_test_mir_interface_execution_symbol(
        "dispatch",
        1,
        &dispatcher_symbol
    ));
    MMAY_EXPECT(mmay_test_mir_interface_execution_symbol(
        "Contract.show",
        2,
        &interface_symbol
    ));
    MMAY_EXPECT(mmay_test_mir_interface_execution_symbol(
        "Record.show",
        3,
        &method_symbol
    ));
    MMAY_EXPECT(mmay_test_mir_interface_creator_init(
        &creator,
        creator_symbol,
        method_symbol
    ));
    MMAY_EXPECT(mmay_test_mir_interface_dispatcher_init(
        &dispatcher,
        dispatcher_symbol,
        interface_symbol
    ));
    MMAY_EXPECT(mmay_mir_verify(&creator) == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_mir_verify(&dispatcher) == MMAY_MIR_VERIFY_OK);
    mmay_test_mir_interface_object_limit(&creator);
    mmay_test_mir_interface_allocation_failures(
        &creator,
        &dispatcher,
        method_symbol
    );

    MMAY_EXPECT(mmay_mir_execution_init(
        &execution,
        mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    MMAY_EXPECT(mmay_test_mir_interface_create(
        &creator,
        &execution,
        mmay_system_allocator(),
        &interface_value
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(interface_value.type == MMAY_TEST_MIR_INTERFACE_VALUE_TYPE);
    MMAY_EXPECT(mmay_mir_execution_owns_reference(
        &execution,
        interface_value
    ));
    mmay_mir_function_dispose(&creator);

    context.expected_method = method_symbol;
    context.execution = &execution;
    context.calls = 0;
    MMAY_EXPECT(mmay_test_mir_interface_dispatch(
        &dispatcher,
        interface_value,
        &execution,
        mmay_system_allocator(),
        &context,
        &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(context.calls == 1);
    MMAY_EXPECT(result.type == MMAY_TYPE_I64);
    MMAY_EXPECT(result.as.i64 == 7);
    MMAY_EXPECT(mmay_test_mir_interface_dispatch(
        &dispatcher,
        interface_value,
        &execution,
        mmay_system_allocator(),
        NULL,
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_ARGUMENT);

    MMAY_EXPECT(mmay_mir_execution_init(
        &foreign_execution,
        mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    context.execution = &foreign_execution;
    MMAY_EXPECT(mmay_test_mir_interface_dispatch(
        &dispatcher,
        interface_value,
        &foreign_execution,
        mmay_system_allocator(),
        &context,
        &result
    ) == MMAY_MIR_INTERPRET_INVALID_REFERENCE);
    MMAY_EXPECT(context.calls == 1);
    mmay_mir_execution_dispose(&foreign_execution);
    mmay_mir_execution_dispose(&execution);
    mmay_mir_function_dispose(&dispatcher);
}
