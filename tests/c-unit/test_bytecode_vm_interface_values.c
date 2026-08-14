#include "test_bytecode_vm_interface_support.h"

static bool ruda_test_vm_build_nested_interface_value(
    RudaBytecodeFunction *function,
    bool through_enum
) {
    RudaBytecodeRecordField implementation_field;
    RudaBytecodeRecordField container_field;
    RudaBytecodeRecordArgument implementation_argument;
    RudaBytecodeRecordArgument container_argument;
    RudaBytecodeEnumPayload enum_payload;
    RudaBytecodeEnumVariant enum_variant;
    RudaBytecodeEnumArgument enum_argument;
    RudaBytecodeRecordId implementation = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordId container = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordConstructionId implementation_new =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeRecordConstructionId container_new =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeEnumId enumeration = RUDA_BYTECODE_NO_ENUM;
    RudaBytecodeEnumConstructionId enum_new =
        RUDA_BYTECODE_NO_ENUM_CONSTRUCTION;
    RudaBytecodeInterfaceId interface_id = RUDA_BYTECODE_NO_INTERFACE;
    RudaBytecodeInterfaceConstructionId interface_new =
        RUDA_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    RudaSymbolId implementation_symbol = {{0x61}};
    RudaSymbolId container_symbol = {{0x62}};
    RudaSymbolId enum_symbol = {{0x63}};
    RudaSymbolId interface_symbol = {{0x64}};
    uint32_t scalar = UINT32_MAX;
    uint32_t implementation_value = UINT32_MAX;
    uint32_t interface_value = UINT32_MAX;
    uint32_t aggregate = UINT32_MAX;
    uint32_t required = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = through_enum ? 200 : 100;
    memset(function, 0, sizeof(*function));
    memset(&implementation_field, 0, sizeof(implementation_field));
    memset(&container_field, 0, sizeof(container_field));
    memset(&implementation_argument, 0, sizeof(implementation_argument));
    memset(&container_argument, 0, sizeof(container_argument));
    memset(&enum_payload, 0, sizeof(enum_payload));
    memset(&enum_variant, 0, sizeof(enum_variant));
    memset(&enum_argument, 0, sizeof(enum_argument));
    implementation_field.type = RUDA_BYTECODE_TYPE_I64;
    implementation_field.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_INTERFACE,
            1,
            NULL,
            0,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &scalar
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &implementation_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_INTERFACE, &interface_value
        ) || !ruda_bytecode_add_register(
            function,
            through_enum ? RUDA_BYTECODE_TYPE_ENUM : RUDA_BYTECODE_TYPE_RECORD,
            &aggregate
        ) || (through_enum && !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_ENUM, &required
        )) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_INTERFACE, &projected
        ) || !ruda_bytecode_add_record_layout(
            function,
            implementation_symbol,
            &implementation_field,
            1,
            &implementation
        ) || !ruda_bytecode_set_register_layout(
            function,
            implementation_value,
            implementation
        ) || !ruda_bytecode_add_interface_layout(
            function,
            interface_symbol,
            0,
            &interface_id
        ) || !ruda_bytecode_set_register_layout(
            function,
            interface_value,
            interface_id
        ) || !ruda_bytecode_set_register_layout(
            function,
            projected,
            interface_id
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    implementation_argument.register_id = scalar;
    implementation_argument.type = RUDA_BYTECODE_TYPE_I64;
    implementation_argument.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_add_record_construction(
            function,
            implementation,
            &implementation_argument,
            1,
            &implementation_new
        ) || !ruda_bytecode_add_interface_construction(
            function,
            interface_id,
            implementation_value,
            RUDA_BYTECODE_TYPE_RECORD,
            implementation,
            NULL,
            0,
            &interface_new
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    if (through_enum) {
        enum_payload.type = RUDA_BYTECODE_TYPE_INTERFACE;
        enum_payload.layout = interface_id;
        enum_variant.payload_count = 1;
        if (!ruda_bytecode_add_enum_layout(
                function,
                enum_symbol,
                &enum_variant,
                1,
                &enum_payload,
                1,
                &enumeration
            ) || !ruda_bytecode_set_register_layout(
                function,
                aggregate,
                enumeration
            ) || !ruda_bytecode_set_register_layout(
                function,
                required,
                enumeration
            )) {
            ruda_bytecode_function_dispose(function);
            return false;
        }
        enum_argument.register_id = interface_value;
        enum_argument.type = RUDA_BYTECODE_TYPE_INTERFACE;
        enum_argument.layout = interface_id;
        if (!ruda_bytecode_add_enum_construction(
                function,
                enumeration,
                0,
                &enum_argument,
                1,
                &enum_new
            )) {
            ruda_bytecode_function_dispose(function);
            return false;
        }
    } else {
        container_field.type = RUDA_BYTECODE_TYPE_INTERFACE;
        container_field.layout = interface_id;
        if (!ruda_bytecode_add_record_layout(
                function,
                container_symbol,
                &container_field,
                1,
                &container
            ) || !ruda_bytecode_set_register_layout(
                function,
                aggregate,
                container
            )) {
            ruda_bytecode_function_dispose(function);
            return false;
        }
        container_argument.register_id = interface_value;
        container_argument.type = RUDA_BYTECODE_TYPE_INTERFACE;
        container_argument.layout = interface_id;
        if (!ruda_bytecode_add_record_construction(
                function,
                container,
                &container_argument,
                1,
                &container_new
            )) {
            ruda_bytecode_function_dispose(function);
            return false;
        }
    }
    function->result_layout = interface_id;
    if (!ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_I64, scalar, 77, 0, &span
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RECORD_NEW,
            implementation_value,
            implementation_new,
            0,
            &span
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_INTERFACE_NEW,
            interface_value,
            interface_new,
            0,
            &span
        ) || !ruda_test_vm_append(
            function,
            block,
            through_enum ? RUDA_OPCODE_ENUM_NEW : RUDA_OPCODE_RECORD_NEW,
            aggregate,
            through_enum ? enum_new : container_new,
            0,
            &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    if (through_enum && (!ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_ENUM_REQUIRE_VARIANT,
            required,
            aggregate,
            0,
            &span
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_ENUM_GET_PAYLOAD,
            projected,
            required,
            0,
            &span
        ))) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    if (!through_enum && !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RECORD_GET,
            projected,
            aggregate,
            0,
            &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    if (!ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN, projected, 0, 0, &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_vm_nested_interface_path(bool through_enum) {
    RudaBytecodeFunction function;
    RudaBytecodeExecution execution;
    RudaBytecodeExecutionOptions execution_options =
        ruda_bytecode_execution_default_options();
    RudaBytecodeRunOptions run_options = ruda_bytecode_run_default_options();
    RudaBytecodeValue result = ruda_test_vm_i64(-1);
    bool reached_success = false;
    size_t failure_index = 0;
    memset(&function, 0, sizeof(function));
    memset(&execution, 0, sizeof(execution));
    RUDA_EXPECT(ruda_test_vm_build_nested_interface_value(
        &function,
        through_enum
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    execution_options.managed_heap.collect_before_allocation = true;
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution,
        ruda_system_allocator(),
        execution_options
    ));
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run(
        &function,
        NULL,
        0,
        ruda_system_allocator(),
        run_options,
        &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_INTERFACE);
    RUDA_EXPECT(!ruda_managed_reference_is_invalid(
        result.as.managed_reference
    ));
    RUDA_EXPECT(execution.managed_heap.roots == NULL);
    ruda_bytecode_execution_dispose(&execution);

    for (failure_index = 0; failure_index < 128 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeExecution failed_execution;
        RudaBytecodeRunStatus status;
        memset(&failed_execution, 0, sizeof(failed_execution));
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            SIZE_MAX
        );
        RUDA_EXPECT(ruda_bytecode_execution_init(
            &failed_execution,
            ruda_fail_allocator(&state),
            execution_options
        ));
        state.failure_index = state.attempts + failure_index;
        run_options = ruda_bytecode_run_default_options();
        run_options.execution = &failed_execution;
        result = ruda_test_vm_i64(-1);
        status = ruda_bytecode_run(
            &function,
            NULL,
            0,
            ruda_fail_allocator(&state),
            run_options,
            &result
        );
        if (status == RUDA_BYTECODE_RUN_OK) {
            reached_success = true;
            RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_INTERFACE);
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED);
            RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64
                && result.as.i64 == -1);
        }
        RUDA_EXPECT(failed_execution.managed_heap.roots == NULL);
        ruda_bytecode_execution_dispose(&failed_execution);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_bytecode_function_dispose(&function);
}

static bool ruda_test_vm_build_interface_container_factory(
    RudaBytecodeFunction *function,
    RudaSymbolId implementation_symbol,
    RudaSymbolId container_symbol,
    RudaSymbolId interface_symbol,
    RudaSymbolId method_symbol
) {
    RudaBytecodeRecordField implementation_field;
    RudaBytecodeRecordField container_field;
    RudaBytecodeRecordArgument implementation_argument;
    RudaBytecodeRecordArgument container_argument;
    RudaBytecodeRecordId implementation = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordId container = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordConstructionId implementation_new =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeRecordConstructionId container_new =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeInterfaceId interface_id = RUDA_BYTECODE_NO_INTERFACE;
    RudaBytecodeInterfaceConstructionId interface_new =
        RUDA_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    uint32_t scalar = UINT32_MAX;
    uint32_t implementation_value = UINT32_MAX;
    uint32_t interface_value = UINT32_MAX;
    uint32_t container_value = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 400;
    memset(function, 0, sizeof(*function));
    memset(&implementation_field, 0, sizeof(implementation_field));
    memset(&container_field, 0, sizeof(container_field));
    memset(&implementation_argument, 0, sizeof(implementation_argument));
    memset(&container_argument, 0, sizeof(container_argument));
    implementation_field.type = RUDA_BYTECODE_TYPE_I64;
    implementation_field.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_RECORD,
            1,
            NULL,
            0,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &scalar
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &implementation_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_INTERFACE, &interface_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &container_value
        ) || !ruda_bytecode_add_record_layout(
            function,
            implementation_symbol,
            &implementation_field,
            1,
            &implementation
        ) || !ruda_bytecode_set_register_layout(
            function, implementation_value, implementation
        ) || !ruda_bytecode_add_interface_layout(
            function, interface_symbol, 1, &interface_id
        ) || !ruda_bytecode_set_register_layout(
            function, interface_value, interface_id
        )) {
        goto failure;
    }
    container_field.type = RUDA_BYTECODE_TYPE_INTERFACE;
    container_field.layout = interface_id;
    if (!ruda_bytecode_add_record_layout(
            function, container_symbol, &container_field, 1, &container
        ) || !ruda_bytecode_set_register_layout(
            function, container_value, container
        )) {
        goto failure;
    }
    function->result_layout = container;
    implementation_argument.register_id = scalar;
    implementation_argument.type = RUDA_BYTECODE_TYPE_I64;
    implementation_argument.layout = RUDA_BYTECODE_NO_LAYOUT;
    container_argument.register_id = interface_value;
    container_argument.type = RUDA_BYTECODE_TYPE_INTERFACE;
    container_argument.layout = interface_id;
    if (!ruda_bytecode_add_record_construction(
            function,
            implementation,
            &implementation_argument,
            1,
            &implementation_new
        ) || !ruda_bytecode_add_interface_construction(
            function,
            interface_id,
            implementation_value,
            RUDA_BYTECODE_TYPE_RECORD,
            implementation,
            &method_symbol,
            1,
            &interface_new
        ) || !ruda_bytecode_add_record_construction(
            function,
            container,
            &container_argument,
            1,
            &container_new
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_I64, scalar, 91, 0, &span
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RECORD_NEW,
            implementation_value,
            implementation_new,
            0,
            &span
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_INTERFACE_NEW,
            interface_value,
            interface_new,
            0,
            &span
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RECORD_NEW,
            container_value,
            container_new,
            0,
            &span
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RETURN,
            container_value,
            0,
            0,
            &span
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_bytecode_function_dispose(function);
    return false;
}

static bool ruda_test_vm_build_interface_echo(
    RudaBytecodeFunction *function,
    RudaSymbolId interface_symbol
) {
    RudaBytecodeType parameter = RUDA_BYTECODE_TYPE_INTERFACE;
    RudaBytecodeInterfaceId interface_id = RUDA_BYTECODE_NO_INTERFACE;
    uint32_t block = UINT32_MAX;
    uint64_t span = 500;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_INTERFACE,
            1,
            &parameter,
            1,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_interface_layout(
            function, interface_symbol, 1, &interface_id
        ) || !ruda_bytecode_set_register_layout(function, 0, interface_id)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN, 0, 0, 0, &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    function->result_layout = interface_id;
    return true;
}

static bool ruda_test_vm_build_interface_container_entry(
    RudaBytecodeFunction *function,
    RudaSymbolId container_symbol,
    RudaSymbolId interface_symbol
) {
    RudaBytecodeType parameter = RUDA_BYTECODE_TYPE_RECORD;
    RudaBytecodeRecordField container_field;
    RudaBytecodeCallArgument call_argument;
    RudaBytecodeRecordId container = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeInterfaceId interface_id = RUDA_BYTECODE_NO_INTERFACE;
    RudaBytecodeCallId call_id = RUDA_BYTECODE_NO_CALL;
    RudaBytecodeCall *call = NULL;
    RudaSymbolId placeholder = {{0}};
    uint32_t interface_value = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 600;
    memset(function, 0, sizeof(*function));
    memset(&container_field, 0, sizeof(container_field));
    memset(&call_argument, 0, sizeof(call_argument));
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_I64,
            1,
            &parameter,
            1,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_INTERFACE, &interface_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &result
        ) || !ruda_bytecode_add_interface_layout(
            function, interface_symbol, 1, &interface_id
        )) {
        goto failure;
    }
    container_field.type = RUDA_BYTECODE_TYPE_INTERFACE;
    container_field.layout = interface_id;
    if (!ruda_bytecode_add_record_layout(
            function, container_symbol, &container_field, 1, &container
        ) || !ruda_bytecode_set_register_layout(function, 0, container)
        || !ruda_bytecode_set_register_layout(
            function, interface_value, interface_id
        )) {
        goto failure;
    }
    call_argument.register_id = interface_value;
    call_argument.type = RUDA_BYTECODE_TYPE_INTERFACE;
    call_argument.layout = interface_id;
    if (!ruda_bytecode_add_call(
            function,
            placeholder,
            RUDA_BYTECODE_TYPE_I64,
            &call_argument,
            1,
            &call_id
        )) {
        goto failure;
    }
    call = ruda_vector_at(&function->calls, call_id);
    if (call == NULL) {
        goto failure;
    }
    call->interface_slot = 0;
    if (!ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RECORD_GET,
            interface_value,
            0,
            0,
            &span
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_CALL_INTERFACE,
            result,
            call_id,
            0,
            &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN, result, 0, 0, &span
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_bytecode_function_dispose(function);
    return false;
}

static bool ruda_test_vm_build_interface_container_program(
    RudaBytecodeProgram *program,
    RudaBytecodeFunction *factory,
    RudaSymbolId implementation_symbol,
    RudaSymbolId container_symbol,
    RudaSymbolId interface_symbol,
    RudaSymbolId method_symbol
) {
    const RudaSymbolId factory_symbol = {{0x20}};
    const RudaSymbolId entry_symbol = {{0x30}};
    RudaBytecodeFunction method;
    RudaBytecodeFunction entry;
    uint32_t function_id = UINT32_MAX;
    memset(program, 0, sizeof(*program));
    memset(factory, 0, sizeof(*factory));
    memset(&method, 0, sizeof(method));
    memset(&entry, 0, sizeof(entry));
    if (!ruda_test_vm_build_interface_method(
            &method, implementation_symbol
        ) || !ruda_test_vm_build_interface_container_factory(
            factory,
            implementation_symbol,
            container_symbol,
            interface_symbol,
            method_symbol
        ) || !ruda_test_vm_build_interface_container_entry(
            &entry, container_symbol, interface_symbol
        ) || !ruda_bytecode_program_init(
            program,
            ruda_system_allocator(),
            1,
            ruda_bytecode_program_default_options()
        ) || !ruda_bytecode_program_take_function(
            program, method_symbol, &method, &function_id
        ) || !ruda_bytecode_program_take_function(
            program, factory_symbol, factory, &function_id
        ) || !ruda_bytecode_program_take_function(
            program, entry_symbol, &entry, &function_id
        ) || !ruda_bytecode_program_set_entry(program, entry_symbol)) {
        ruda_bytecode_function_dispose(&entry);
        ruda_bytecode_function_dispose(factory);
        ruda_bytecode_function_dispose(&method);
        ruda_bytecode_program_dispose(program);
        return false;
    }
    return true;
}

static void ruda_test_vm_forged_interface_payload(void) {
    static const RudaManagedRecordDescriptor wrong_descriptor = {0, NULL};
    const RudaSymbolId method_symbol = {{0x10}};
    const RudaSymbolId implementation_symbol = {{0xd1}};
    const RudaSymbolId container_symbol = {{0xd2}};
    const RudaSymbolId interface_symbol = {{0xd3}};
    RudaBytecodeProgram program;
    RudaBytecodeFunction factory;
    RudaBytecodeFunction echo;
    RudaBytecodeExecution execution;
    RudaBytecodeExecutionOptions execution_options =
        ruda_bytecode_execution_default_options();
    RudaBytecodeRunOptions run_options = ruda_bytecode_run_default_options();
    RudaFailAllocator tracking;
    RudaManagedRootFrame roots;
    RudaManagedReference valid_container = ruda_managed_invalid_reference();
    RudaManagedReference valid_interface = ruda_managed_invalid_reference();
    RudaManagedReference valid_payload = ruda_managed_invalid_reference();
    RudaManagedReference wrong_payload = ruda_managed_invalid_reference();
    RudaManagedReference forged_interface = ruda_managed_invalid_reference();
    RudaManagedReference forged_container = ruda_managed_invalid_reference();
    RudaManagedReference *locations[] = {
        &valid_container,
        &valid_interface,
        &valid_payload,
        &wrong_payload,
        &forged_interface,
        &forged_container
    };
    const RudaManagedRecordDescriptor *container_descriptor = NULL;
    const RudaManagedInterfaceDescriptor *interface_descriptor = NULL;
    const RudaBytecodeProgramFunction *factory_owner = NULL;
    const void *method_table = NULL;
    RudaManagedCell cell;
    RudaBytecodeValue result = ruda_test_vm_i64(-1);
    RudaBytecodeValue argument;
    size_t attempts = 0;
    memset(&program, 0, sizeof(program));
    memset(&factory, 0, sizeof(factory));
    memset(&echo, 0, sizeof(echo));
    memset(&execution, 0, sizeof(execution));
    memset(&roots, 0, sizeof(roots));
    memset(&cell, 0, sizeof(cell));
    RUDA_EXPECT(ruda_test_vm_build_interface_container_program(
        &program,
        &factory,
        implementation_symbol,
        container_symbol,
        interface_symbol,
        method_symbol
    ));
    RUDA_EXPECT(ruda_test_vm_build_interface_echo(
        &echo, interface_symbol
    ));
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    {
        const RudaSymbolId factory_symbol = {{0x20}};
        factory_owner = ruda_bytecode_program_function_by_symbol(
            &program, factory_symbol
        );
    }
    RUDA_EXPECT(factory_owner != NULL);
    execution_options.managed_heap.collect_before_allocation = true;
    ruda_fail_allocator_init(
        &tracking, ruda_system_allocator(), SIZE_MAX
    );
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_fail_allocator(&tracking), execution_options
    ));
    RUDA_EXPECT(ruda_managed_root_push(
        &execution.managed_heap,
        &roots,
        locations,
        sizeof(locations) / sizeof(locations[0])
    ) == RUDA_MANAGED_OK);
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run(
        &factory_owner->body,
        NULL,
        0,
        ruda_fail_allocator(&tracking),
        run_options,
        &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_RECORD);
    valid_container = result.as.managed_reference;
    RUDA_EXPECT(ruda_managed_record_describe(
        &execution.managed_heap,
        valid_container,
        &container_descriptor
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_record_get(
        &execution.managed_heap, valid_container, 0, &cell
    ) == RUDA_MANAGED_OK);
    valid_interface = cell.reference;
    RUDA_EXPECT(ruda_managed_interface_describe(
        &execution.managed_heap,
        valid_interface,
        &interface_descriptor,
        &valid_payload,
        &method_table
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_record_new(
        &execution.managed_heap,
        &wrong_descriptor,
        NULL,
        0,
        &wrong_payload
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_interface_new(
        &execution.managed_heap,
        interface_descriptor,
        wrong_payload,
        method_table,
        &forged_interface
    ) == RUDA_MANAGED_OK);
    cell.reference = forged_interface;
    RUDA_EXPECT(ruda_managed_record_new(
        &execution.managed_heap,
        container_descriptor,
        &cell,
        1,
        &forged_container
    ) == RUDA_MANAGED_OK);
    argument.type = RUDA_BYTECODE_TYPE_INTERFACE;
    argument.as.managed_reference = forged_interface;
    attempts = tracking.attempts;
    tracking.failure_index = attempts;
    result = ruda_test_vm_i64(-1);
    RUDA_EXPECT(ruda_bytecode_run(
        &echo,
        &argument,
        1,
        ruda_fail_allocator(&tracking),
        run_options,
        &result
    ) == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
    RUDA_EXPECT(tracking.attempts == attempts);
    tracking.failure_index = SIZE_MAX;
    argument.type = RUDA_BYTECODE_TYPE_RECORD;
    argument.as.managed_reference = forged_container;
    RUDA_EXPECT(ruda_bytecode_run_program(
        &program,
        &argument,
        1,
        ruda_fail_allocator(&tracking),
        run_options,
        &result
    ) == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64
        && result.as.i64 == -1);
    RUDA_EXPECT(ruda_managed_root_pop(
        &execution.managed_heap, &roots
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(execution.managed_heap.roots == NULL);
    ruda_bytecode_execution_dispose(&execution);
    RUDA_EXPECT(tracking.live_allocations == 0);
    RUDA_EXPECT(tracking.live_bytes == 0);
    ruda_bytecode_function_dispose(&echo);
    ruda_bytecode_function_dispose(&factory);
    ruda_bytecode_program_dispose(&program);
}


void ruda_test_vm_interface_values(void) {
    ruda_test_vm_nested_interface_path(false);
    ruda_test_vm_nested_interface_path(true);
    ruda_test_vm_forged_interface_payload();
}
