#include "test_mir_support.h"

#include <string.h>

#include "ruda/base/allocator.h"

enum {
    RUDA_TEST_MIR_INTERFACE_RECORD_TYPE = RUDA_TYPE_FIRST_DYNAMIC + 100,
    RUDA_TEST_MIR_INTERFACE_VALUE_TYPE
};

typedef struct RudaTestMirInterfaceCallContext {
    RudaSymbolId expected_method;
    RudaMirExecution *execution;
    size_t calls;
} RudaTestMirInterfaceCallContext;

static bool ruda_test_mir_interface_execution_symbol(
    const char *name,
    uint32_t discriminator,
    RudaSymbolId *result
) {
    return ruda_symbol_id_from_parts(
        "example.test/mir-interface-execution",
        "src/interface-execution.ruda",
        name,
        discriminator,
        result
    );
}

static bool ruda_test_mir_interface_creator_init(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaSymbolId method
) {
    RudaMirRecordField field;
    RudaMirRecordArgument record_argument;
    RudaMirRecordId record = RUDA_MIR_NO_RECORD;
    RudaMirRecordConstructionId record_construction =
        RUDA_MIR_NO_RECORD_CONSTRUCTION;
    RudaMirInterfaceId interface_id = RUDA_MIR_NO_INTERFACE;
    RudaMirInterfaceConstructionId interface_construction =
        RUDA_MIR_NO_INTERFACE_CONSTRUCTION;
    RudaMirRegister scalar = RUDA_MIR_NO_REGISTER;
    RudaMirRegister payload = RUDA_MIR_NO_REGISTER;
    RudaMirRegister interface_value = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    bool ok = false;
    if (function == NULL) {
        return false;
    }
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function,
            ruda_system_allocator(),
            symbol,
            NULL,
            0,
            RUDA_TEST_MIR_INTERFACE_VALUE_TYPE,
            ruda_mir_function_default_options()
        )) {
        return false;
    }
    field.type = RUDA_TYPE_I64;
    field.is_mutable = false;
    ok = ruda_mir_add_register(function, RUDA_TYPE_I64, &scalar)
        && ruda_mir_add_register(
            function,
            RUDA_TEST_MIR_INTERFACE_RECORD_TYPE,
            &payload
        ) && ruda_mir_add_register(
            function,
            RUDA_TEST_MIR_INTERFACE_VALUE_TYPE,
            &interface_value
        ) && ruda_mir_add_block(function, &block)
        && ruda_mir_add_record(
            function,
            RUDA_TEST_MIR_INTERFACE_RECORD_TYPE,
            &field,
            1,
            &record
        );
    record_argument.register_id = scalar;
    record_argument.type = RUDA_TYPE_I64;
    ok = ok && ruda_mir_add_record_construction(
        function,
        record,
        &record_argument,
        1,
        &record_construction
    ) && ruda_mir_add_interface(
        function,
        RUDA_TEST_MIR_INTERFACE_VALUE_TYPE,
        1,
        &interface_id
    ) && ruda_mir_add_interface_construction(
        function,
        interface_id,
        payload,
        RUDA_TEST_MIR_INTERFACE_RECORD_TYPE,
        &method,
        1,
        &interface_construction
    );
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_I64;
    instruction.target = scalar;
    instruction.integer = 3;
    ok = ok && ruda_mir_append_instruction(function, block, &instruction);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_RECORD_NEW;
    instruction.target = payload;
    instruction.record = record;
    instruction.record_construction = record_construction;
    ok = ok && ruda_mir_append_instruction(function, block, &instruction);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_INTERFACE_NEW;
    instruction.target = interface_value;
    instruction.interface_construction = interface_construction;
    ok = ok && ruda_mir_append_instruction(function, block, &instruction)
        && ruda_mir_set_terminator(
            function,
            block,
            ruda_test_mir_return(interface_value)
        );
    if (!ok) {
        ruda_mir_function_dispose(function);
    }
    return ok;
}

static bool ruda_test_mir_interface_dispatcher_init(
    RudaMirFunction *function,
    RudaSymbolId symbol,
    RudaSymbolId interface_symbol
) {
    RudaTypeId parameters[2] = {
        RUDA_TEST_MIR_INTERFACE_VALUE_TYPE,
        RUDA_TYPE_I64
    };
    RudaMirCallArgument arguments[2];
    RudaMirRegister result = RUDA_MIR_NO_REGISTER;
    RudaMirInterfaceId interface_id = RUDA_MIR_NO_INTERFACE;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirBlockId block = RUDA_MIR_NO_BLOCK;
    RudaMirInstruction instruction;
    bool ok = false;
    if (function == NULL) {
        return false;
    }
    memset(function, 0, sizeof(*function));
    if (!ruda_mir_function_init(
            function,
            ruda_system_allocator(),
            symbol,
            parameters,
            2,
            RUDA_TYPE_I64,
            ruda_mir_function_default_options()
        )) {
        return false;
    }
    arguments[0].register_id = 0;
    arguments[0].type = RUDA_TEST_MIR_INTERFACE_VALUE_TYPE;
    arguments[1].register_id = 1;
    arguments[1].type = RUDA_TYPE_I64;
    ok = ruda_mir_add_register(function, RUDA_TYPE_I64, &result)
        && ruda_mir_add_block(function, &block)
        && ruda_mir_add_interface(
            function,
            RUDA_TEST_MIR_INTERFACE_VALUE_TYPE,
            1,
            &interface_id
        ) && ruda_mir_add_call(
            function,
            interface_symbol,
            RUDA_TYPE_I64,
            arguments,
            2,
            &call
        ) && ruda_mir_set_call_interface_slot(function, call, 0);
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CALL_INTERFACE;
    instruction.target = result;
    instruction.call = call;
    ok = ok && ruda_mir_append_instruction(function, block, &instruction)
        && ruda_mir_set_terminator(
            function,
            block,
            ruda_test_mir_return(result)
        );
    if (!ok) {
        ruda_mir_function_dispose(function);
    }
    return ok;
}

static RudaMirInterpretStatus ruda_test_mir_interface_call(
    void *opaque,
    RudaSymbolId callee,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
) {
    RudaTestMirInterfaceCallContext *context = opaque;
    if (context == NULL || context->execution == NULL || arguments == NULL
        || result == NULL || argument_count != 2
        || !ruda_symbol_id_equal(callee, context->expected_method)
        || arguments[0].type != RUDA_TEST_MIR_INTERFACE_RECORD_TYPE
        || !ruda_mir_execution_owns_reference(
            context->execution,
            arguments[0]
        ) || arguments[1].type != RUDA_TYPE_I64
        || arguments[1].as.i64 != 4) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    context->calls += 1;
    result->type = RUDA_TYPE_I64;
    result->as.i64 = 7;
    return RUDA_MIR_INTERPRET_OK;
}

static RudaMirInterpretStatus ruda_test_mir_interface_create(
    const RudaMirFunction *creator,
    RudaMirExecution *execution,
    RudaAllocator allocator,
    RudaMirValue *result
) {
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    options.execution = execution;
    return ruda_mir_interpret(
        creator,
        NULL,
        0,
        allocator,
        options,
        result
    );
}

static RudaMirInterpretStatus ruda_test_mir_interface_dispatch(
    const RudaMirFunction *dispatcher,
    RudaMirValue interface_value,
    RudaMirExecution *execution,
    RudaAllocator allocator,
    RudaTestMirInterfaceCallContext *context,
    RudaMirValue *result
) {
    RudaMirValue arguments[2];
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    arguments[0] = interface_value;
    arguments[1].type = RUDA_TYPE_I64;
    arguments[1].as.i64 = 4;
    options.execution = execution;
    options.call_context = context;
    options.call = context != NULL ? ruda_test_mir_interface_call : NULL;
    return ruda_mir_interpret(
        dispatcher,
        arguments,
        2,
        allocator,
        options,
        result
    );
}

static void ruda_test_mir_interface_object_limit(
    const RudaMirFunction *creator
) {
    RudaMirExecution execution;
    RudaMirExecutionOptions execution_options =
        ruda_mir_execution_default_options();
    RudaMirValue result;
    execution_options.max_objects = 1;
    RUDA_EXPECT(ruda_mir_execution_init(
        &execution,
        ruda_system_allocator(),
        execution_options
    ));
    RUDA_EXPECT(ruda_test_mir_interface_create(
        creator,
        &execution,
        ruda_system_allocator(),
        &result
    ) == RUDA_MIR_INTERPRET_OBJECT_LIMIT);
    ruda_mir_execution_dispose(&execution);
}

static void ruda_test_mir_interface_allocation_failures(
    const RudaMirFunction *creator,
    const RudaMirFunction *dispatcher,
    RudaSymbolId method
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 32; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaMirExecution execution;
        RudaMirValue interface_value;
        RudaMirValue result;
        RudaMirInterpretStatus status;
        RudaTestMirInterfaceCallContext context;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        RUDA_EXPECT(ruda_mir_execution_init(
            &execution,
            allocator,
            ruda_mir_execution_default_options()
        ));
        context.expected_method = method;
        context.execution = &execution;
        context.calls = 0;
        status = ruda_test_mir_interface_create(
            creator,
            &execution,
            allocator,
            &interface_value
        );
        if (status == RUDA_MIR_INTERPRET_OK) {
            status = ruda_test_mir_interface_dispatch(
                dispatcher,
                interface_value,
                &execution,
                allocator,
                &context,
                &result
            );
        }
        if (status == RUDA_MIR_INTERPRET_OK) {
            reached_success = true;
            RUDA_EXPECT(context.calls == 1);
            RUDA_EXPECT(result.type == RUDA_TYPE_I64);
            RUDA_EXPECT(result.as.i64 == 7);
        } else {
            RUDA_EXPECT(status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED);
        }
        ruda_mir_execution_dispose(&execution);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_mir_interface_execution(void) {
    RudaSymbolId creator_symbol;
    RudaSymbolId dispatcher_symbol;
    RudaSymbolId interface_symbol;
    RudaSymbolId method_symbol;
    RudaMirFunction creator;
    RudaMirFunction dispatcher;
    RudaMirExecution execution;
    RudaMirExecution foreign_execution;
    RudaMirValue interface_value;
    RudaMirValue result;
    RudaTestMirInterfaceCallContext context;
    RUDA_EXPECT(ruda_test_mir_interface_execution_symbol(
        "create",
        0,
        &creator_symbol
    ));
    RUDA_EXPECT(ruda_test_mir_interface_execution_symbol(
        "dispatch",
        1,
        &dispatcher_symbol
    ));
    RUDA_EXPECT(ruda_test_mir_interface_execution_symbol(
        "Contract.show",
        2,
        &interface_symbol
    ));
    RUDA_EXPECT(ruda_test_mir_interface_execution_symbol(
        "Record.show",
        3,
        &method_symbol
    ));
    RUDA_EXPECT(ruda_test_mir_interface_creator_init(
        &creator,
        creator_symbol,
        method_symbol
    ));
    RUDA_EXPECT(ruda_test_mir_interface_dispatcher_init(
        &dispatcher,
        dispatcher_symbol,
        interface_symbol
    ));
    RUDA_EXPECT(ruda_mir_verify(&creator) == RUDA_MIR_VERIFY_OK);
    RUDA_EXPECT(ruda_mir_verify(&dispatcher) == RUDA_MIR_VERIFY_OK);
    ruda_test_mir_interface_object_limit(&creator);
    ruda_test_mir_interface_allocation_failures(
        &creator,
        &dispatcher,
        method_symbol
    );

    RUDA_EXPECT(ruda_mir_execution_init(
        &execution,
        ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    RUDA_EXPECT(ruda_test_mir_interface_create(
        &creator,
        &execution,
        ruda_system_allocator(),
        &interface_value
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(interface_value.type == RUDA_TEST_MIR_INTERFACE_VALUE_TYPE);
    RUDA_EXPECT(ruda_mir_execution_owns_reference(
        &execution,
        interface_value
    ));
    ruda_mir_function_dispose(&creator);

    context.expected_method = method_symbol;
    context.execution = &execution;
    context.calls = 0;
    RUDA_EXPECT(ruda_test_mir_interface_dispatch(
        &dispatcher,
        interface_value,
        &execution,
        ruda_system_allocator(),
        &context,
        &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(context.calls == 1);
    RUDA_EXPECT(result.type == RUDA_TYPE_I64);
    RUDA_EXPECT(result.as.i64 == 7);
    RUDA_EXPECT(ruda_test_mir_interface_dispatch(
        &dispatcher,
        interface_value,
        &execution,
        ruda_system_allocator(),
        NULL,
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_ARGUMENT);

    RUDA_EXPECT(ruda_mir_execution_init(
        &foreign_execution,
        ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    context.execution = &foreign_execution;
    RUDA_EXPECT(ruda_test_mir_interface_dispatch(
        &dispatcher,
        interface_value,
        &foreign_execution,
        ruda_system_allocator(),
        &context,
        &result
    ) == RUDA_MIR_INTERPRET_INVALID_REFERENCE);
    RUDA_EXPECT(context.calls == 1);
    ruda_mir_execution_dispose(&foreign_execution);
    ruda_mir_execution_dispose(&execution);
    ruda_mir_function_dispose(&dispatcher);
}
