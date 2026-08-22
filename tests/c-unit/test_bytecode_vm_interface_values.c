#include "test_bytecode_vm_interface_support.h"

static bool mmay_test_vm_build_nested_interface_value(
    MarimayBytecodeFunction *function,
    bool through_enum
) {
    MarimayBytecodeRecordField implementation_field;
    MarimayBytecodeRecordField container_field;
    MarimayBytecodeRecordArgument implementation_argument;
    MarimayBytecodeRecordArgument container_argument;
    MarimayBytecodeEnumPayload enum_payload;
    MarimayBytecodeEnumVariant enum_variant;
    MarimayBytecodeEnumArgument enum_argument;
    MarimayBytecodeRecordId implementation = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordId container = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordConstructionId implementation_new =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeRecordConstructionId container_new =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeEnumId enumeration = MMAY_BYTECODE_NO_ENUM;
    MarimayBytecodeEnumConstructionId enum_new =
        MMAY_BYTECODE_NO_ENUM_CONSTRUCTION;
    MarimayBytecodeInterfaceId interface_id = MMAY_BYTECODE_NO_INTERFACE;
    MarimayBytecodeInterfaceConstructionId interface_new =
        MMAY_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    MarimaySymbolId implementation_symbol = {{0x61}};
    MarimaySymbolId container_symbol = {{0x62}};
    MarimaySymbolId enum_symbol = {{0x63}};
    MarimaySymbolId interface_symbol = {{0x64}};
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
    implementation_field.type = MMAY_BYTECODE_TYPE_I64;
    implementation_field.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_INTERFACE,
            1,
            NULL,
            0,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &scalar
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &implementation_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_INTERFACE, &interface_value
        ) || !mmay_bytecode_add_register(
            function,
            through_enum ? MMAY_BYTECODE_TYPE_ENUM : MMAY_BYTECODE_TYPE_RECORD,
            &aggregate
        ) || (through_enum && !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ENUM, &required
        )) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_INTERFACE, &projected
        ) || !mmay_bytecode_add_record_layout(
            function,
            implementation_symbol,
            &implementation_field,
            1,
            &implementation
        ) || !mmay_bytecode_set_register_layout(
            function,
            implementation_value,
            implementation
        ) || !mmay_bytecode_add_interface_layout(
            function,
            interface_symbol,
            0,
            &interface_id
        ) || !mmay_bytecode_set_register_layout(
            function,
            interface_value,
            interface_id
        ) || !mmay_bytecode_set_register_layout(
            function,
            projected,
            interface_id
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    implementation_argument.register_id = scalar;
    implementation_argument.type = MMAY_BYTECODE_TYPE_I64;
    implementation_argument.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_add_record_construction(
            function,
            implementation,
            &implementation_argument,
            1,
            &implementation_new
        ) || !mmay_bytecode_add_interface_construction(
            function,
            interface_id,
            implementation_value,
            MMAY_BYTECODE_TYPE_RECORD,
            implementation,
            NULL,
            0,
            &interface_new
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    if (through_enum) {
        enum_payload.type = MMAY_BYTECODE_TYPE_INTERFACE;
        enum_payload.layout = interface_id;
        enum_variant.payload_count = 1;
        if (!mmay_bytecode_add_enum_layout(
                function,
                enum_symbol,
                &enum_variant,
                1,
                &enum_payload,
                1,
                &enumeration
            ) || !mmay_bytecode_set_register_layout(
                function,
                aggregate,
                enumeration
            ) || !mmay_bytecode_set_register_layout(
                function,
                required,
                enumeration
            )) {
            mmay_bytecode_function_dispose(function);
            return false;
        }
        enum_argument.register_id = interface_value;
        enum_argument.type = MMAY_BYTECODE_TYPE_INTERFACE;
        enum_argument.layout = interface_id;
        if (!mmay_bytecode_add_enum_construction(
                function,
                enumeration,
                0,
                &enum_argument,
                1,
                &enum_new
            )) {
            mmay_bytecode_function_dispose(function);
            return false;
        }
    } else {
        container_field.type = MMAY_BYTECODE_TYPE_INTERFACE;
        container_field.layout = interface_id;
        if (!mmay_bytecode_add_record_layout(
                function,
                container_symbol,
                &container_field,
                1,
                &container
            ) || !mmay_bytecode_set_register_layout(
                function,
                aggregate,
                container
            )) {
            mmay_bytecode_function_dispose(function);
            return false;
        }
        container_argument.register_id = interface_value;
        container_argument.type = MMAY_BYTECODE_TYPE_INTERFACE;
        container_argument.layout = interface_id;
        if (!mmay_bytecode_add_record_construction(
                function,
                container,
                &container_argument,
                1,
                &container_new
            )) {
            mmay_bytecode_function_dispose(function);
            return false;
        }
    }
    function->result_layout = interface_id;
    if (!mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_I64, scalar, 77, 0, &span
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RECORD_NEW,
            implementation_value,
            implementation_new,
            0,
            &span
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_INTERFACE_NEW,
            interface_value,
            interface_new,
            0,
            &span
        ) || !mmay_test_vm_append(
            function,
            block,
            through_enum ? MMAY_OPCODE_ENUM_NEW : MMAY_OPCODE_RECORD_NEW,
            aggregate,
            through_enum ? enum_new : container_new,
            0,
            &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    if (through_enum && (!mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_ENUM_REQUIRE_VARIANT,
            required,
            aggregate,
            0,
            &span
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_ENUM_GET_PAYLOAD,
            projected,
            required,
            0,
            &span
        ))) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    if (!through_enum && !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RECORD_GET,
            projected,
            aggregate,
            0,
            &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    if (!mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, projected, 0, 0, &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void mmay_test_vm_nested_interface_path(bool through_enum) {
    MarimayBytecodeFunction function;
    MarimayBytecodeExecution execution;
    MarimayBytecodeExecutionOptions execution_options =
        mmay_bytecode_execution_default_options();
    MarimayBytecodeRunOptions run_options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue result = mmay_test_vm_i64(-1);
    MarimayTestBuffer disassembly;
    bool reached_success = false;
    size_t failure_index = 0;
    memset(&function, 0, sizeof(function));
    memset(&execution, 0, sizeof(execution));
    MMAY_EXPECT(mmay_test_vm_build_nested_interface_value(
        &function,
        through_enum
    ));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    mmay_test_buffer_init(&disassembly);
    MMAY_EXPECT(mmay_bytecode_disassemble(
        &function,
        mmay_test_buffer_writer(&disassembly)
    ) == MMAY_BYTECODE_DISASSEMBLE_OK);
    MMAY_EXPECT(strstr(disassembly.bytes, "interface_new r2, ic0") != NULL);
    execution_options.managed_heap.collect_before_allocation = true;
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution,
        mmay_system_allocator(),
        execution_options
    ));
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(
        &function,
        NULL,
        0,
        mmay_system_allocator(),
        run_options,
        &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_INTERFACE);
    MMAY_EXPECT(!mmay_managed_reference_is_invalid(
        result.as.managed_reference
    ));
    MMAY_EXPECT(execution.managed_heap.roots == NULL);
    mmay_bytecode_execution_dispose(&execution);

    for (failure_index = 0; failure_index < 128 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeExecution failed_execution;
        MarimayBytecodeRunStatus status;
        memset(&failed_execution, 0, sizeof(failed_execution));
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            SIZE_MAX
        );
        MMAY_EXPECT(mmay_bytecode_execution_init(
            &failed_execution,
            mmay_fail_allocator(&state),
            execution_options
        ));
        state.failure_index = state.attempts + failure_index;
        run_options = mmay_bytecode_run_default_options();
        run_options.execution = &failed_execution;
        result = mmay_test_vm_i64(-1);
        status = mmay_bytecode_run(
            &function,
            NULL,
            0,
            mmay_fail_allocator(&state),
            run_options,
            &result
        );
        if (status == MMAY_BYTECODE_RUN_OK) {
            reached_success = true;
            MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_INTERFACE);
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
            MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64
                && result.as.i64 == -1);
        }
        MMAY_EXPECT(failed_execution.managed_heap.roots == NULL);
        mmay_bytecode_execution_dispose(&failed_execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
    mmay_bytecode_function_dispose(&function);
}

static bool mmay_test_vm_build_interface_container_factory(
    MarimayBytecodeFunction *function,
    MarimaySymbolId implementation_symbol,
    MarimaySymbolId container_symbol,
    MarimaySymbolId interface_symbol,
    MarimaySymbolId method_symbol
) {
    MarimayBytecodeRecordField implementation_field;
    MarimayBytecodeRecordField container_field;
    MarimayBytecodeRecordArgument implementation_argument;
    MarimayBytecodeRecordArgument container_argument;
    MarimayBytecodeRecordId implementation = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordId container = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordConstructionId implementation_new =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeRecordConstructionId container_new =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeInterfaceId interface_id = MMAY_BYTECODE_NO_INTERFACE;
    MarimayBytecodeInterfaceConstructionId interface_new =
        MMAY_BYTECODE_NO_INTERFACE_CONSTRUCTION;
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
    implementation_field.type = MMAY_BYTECODE_TYPE_I64;
    implementation_field.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_RECORD,
            1,
            NULL,
            0,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &scalar
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &implementation_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_INTERFACE, &interface_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &container_value
        ) || !mmay_bytecode_add_record_layout(
            function,
            implementation_symbol,
            &implementation_field,
            1,
            &implementation
        ) || !mmay_bytecode_set_register_layout(
            function, implementation_value, implementation
        ) || !mmay_bytecode_add_interface_layout(
            function, interface_symbol, 1, &interface_id
        ) || !mmay_bytecode_set_register_layout(
            function, interface_value, interface_id
        )) {
        goto failure;
    }
    container_field.type = MMAY_BYTECODE_TYPE_INTERFACE;
    container_field.layout = interface_id;
    if (!mmay_bytecode_add_record_layout(
            function, container_symbol, &container_field, 1, &container
        ) || !mmay_bytecode_set_register_layout(
            function, container_value, container
        )) {
        goto failure;
    }
    function->result_layout = container;
    implementation_argument.register_id = scalar;
    implementation_argument.type = MMAY_BYTECODE_TYPE_I64;
    implementation_argument.layout = MMAY_BYTECODE_NO_LAYOUT;
    container_argument.register_id = interface_value;
    container_argument.type = MMAY_BYTECODE_TYPE_INTERFACE;
    container_argument.layout = interface_id;
    if (!mmay_bytecode_add_record_construction(
            function,
            implementation,
            &implementation_argument,
            1,
            &implementation_new
        ) || !mmay_bytecode_add_interface_construction(
            function,
            interface_id,
            implementation_value,
            MMAY_BYTECODE_TYPE_RECORD,
            implementation,
            &method_symbol,
            1,
            &interface_new
        ) || !mmay_bytecode_add_record_construction(
            function,
            container,
            &container_argument,
            1,
            &container_new
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_I64, scalar, 91, 0, &span
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RECORD_NEW,
            implementation_value,
            implementation_new,
            0,
            &span
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_INTERFACE_NEW,
            interface_value,
            interface_new,
            0,
            &span
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RECORD_NEW,
            container_value,
            container_new,
            0,
            &span
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RETURN,
            container_value,
            0,
            0,
            &span
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_bytecode_function_dispose(function);
    return false;
}

static bool mmay_test_vm_build_interface_echo(
    MarimayBytecodeFunction *function,
    MarimaySymbolId interface_symbol
) {
    MarimayBytecodeType parameter = MMAY_BYTECODE_TYPE_INTERFACE;
    MarimayBytecodeInterfaceId interface_id = MMAY_BYTECODE_NO_INTERFACE;
    uint32_t block = UINT32_MAX;
    uint64_t span = 500;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_INTERFACE,
            1,
            &parameter,
            1,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_interface_layout(
            function, interface_symbol, 1, &interface_id
        ) || !mmay_bytecode_set_register_layout(function, 0, interface_id)
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, 0, 0, 0, &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    function->result_layout = interface_id;
    return true;
}

static bool mmay_test_vm_build_interface_container_entry(
    MarimayBytecodeFunction *function,
    MarimaySymbolId container_symbol,
    MarimaySymbolId interface_symbol
) {
    MarimayBytecodeType parameter = MMAY_BYTECODE_TYPE_RECORD;
    MarimayBytecodeRecordField container_field;
    MarimayBytecodeCallArgument call_argument;
    MarimayBytecodeRecordId container = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeInterfaceId interface_id = MMAY_BYTECODE_NO_INTERFACE;
    MarimayBytecodeCallId call_id = MMAY_BYTECODE_NO_CALL;
    MarimayBytecodeCall *call = NULL;
    MarimaySymbolId placeholder = {{0}};
    uint32_t interface_value = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 600;
    memset(function, 0, sizeof(*function));
    memset(&container_field, 0, sizeof(container_field));
    memset(&call_argument, 0, sizeof(call_argument));
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_I64,
            1,
            &parameter,
            1,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_INTERFACE, &interface_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &result
        ) || !mmay_bytecode_add_interface_layout(
            function, interface_symbol, 1, &interface_id
        )) {
        goto failure;
    }
    container_field.type = MMAY_BYTECODE_TYPE_INTERFACE;
    container_field.layout = interface_id;
    if (!mmay_bytecode_add_record_layout(
            function, container_symbol, &container_field, 1, &container
        ) || !mmay_bytecode_set_register_layout(function, 0, container)
        || !mmay_bytecode_set_register_layout(
            function, interface_value, interface_id
        )) {
        goto failure;
    }
    call_argument.register_id = interface_value;
    call_argument.type = MMAY_BYTECODE_TYPE_INTERFACE;
    call_argument.layout = interface_id;
    if (!mmay_bytecode_add_call(
            function,
            placeholder,
            MMAY_BYTECODE_TYPE_I64,
            &call_argument,
            1,
            &call_id
        )) {
        goto failure;
    }
    call = mmay_vector_at(&function->calls, call_id);
    if (call == NULL) {
        goto failure;
    }
    call->interface_slot = 0;
    if (!mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RECORD_GET,
            interface_value,
            0,
            0,
            &span
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_CALL_INTERFACE,
            result,
            call_id,
            0,
            &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, result, 0, 0, &span
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_bytecode_function_dispose(function);
    return false;
}

static bool mmay_test_vm_build_interface_container_program(
    MarimayBytecodeProgram *program,
    MarimayBytecodeFunction *factory,
    MarimaySymbolId implementation_symbol,
    MarimaySymbolId container_symbol,
    MarimaySymbolId interface_symbol,
    MarimaySymbolId method_symbol
) {
    const MarimaySymbolId factory_symbol = {{0x20}};
    const MarimaySymbolId entry_symbol = {{0x30}};
    MarimayBytecodeFunction method;
    MarimayBytecodeFunction entry;
    uint32_t function_id = UINT32_MAX;
    memset(program, 0, sizeof(*program));
    memset(factory, 0, sizeof(*factory));
    memset(&method, 0, sizeof(method));
    memset(&entry, 0, sizeof(entry));
    if (!mmay_test_vm_build_interface_method(
            &method, implementation_symbol
        ) || !mmay_test_vm_build_interface_container_factory(
            factory,
            implementation_symbol,
            container_symbol,
            interface_symbol,
            method_symbol
        ) || !mmay_test_vm_build_interface_container_entry(
            &entry, container_symbol, interface_symbol
        ) || !mmay_bytecode_program_init(
            program,
            mmay_system_allocator(),
            1,
            mmay_bytecode_program_default_options()
        ) || !mmay_bytecode_program_take_function(
            program, method_symbol, &method, &function_id
        ) || !mmay_bytecode_program_take_function(
            program, factory_symbol, factory, &function_id
        ) || !mmay_bytecode_program_take_function(
            program, entry_symbol, &entry, &function_id
        ) || !mmay_bytecode_program_set_entry(program, entry_symbol)) {
        mmay_bytecode_function_dispose(&entry);
        mmay_bytecode_function_dispose(factory);
        mmay_bytecode_function_dispose(&method);
        mmay_bytecode_program_dispose(program);
        return false;
    }
    return true;
}

static void mmay_test_vm_forged_interface_payload(void) {
    static const MarimayManagedRecordDescriptor wrong_descriptor = {0, NULL};
    const MarimaySymbolId method_symbol = {{0x10}};
    const MarimaySymbolId implementation_symbol = {{0xd1}};
    const MarimaySymbolId container_symbol = {{0xd2}};
    const MarimaySymbolId interface_symbol = {{0xd3}};
    MarimayBytecodeProgram program;
    MarimayBytecodeFunction factory;
    MarimayBytecodeFunction echo;
    MarimayBytecodeExecution execution;
    MarimayBytecodeExecutionOptions execution_options =
        mmay_bytecode_execution_default_options();
    MarimayBytecodeRunOptions run_options = mmay_bytecode_run_default_options();
    MarimayFailAllocator tracking;
    MarimayManagedRootFrame roots;
    MarimayManagedReference valid_container = mmay_managed_invalid_reference();
    MarimayManagedReference valid_interface = mmay_managed_invalid_reference();
    MarimayManagedReference valid_payload = mmay_managed_invalid_reference();
    MarimayManagedReference wrong_payload = mmay_managed_invalid_reference();
    MarimayManagedReference forged_interface = mmay_managed_invalid_reference();
    MarimayManagedReference forged_container = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[] = {
        &valid_container,
        &valid_interface,
        &valid_payload,
        &wrong_payload,
        &forged_interface,
        &forged_container
    };
    const MarimayManagedRecordDescriptor *container_descriptor = NULL;
    const MarimayManagedInterfaceDescriptor *interface_descriptor = NULL;
    const MarimayBytecodeProgramFunction *factory_owner = NULL;
    const void *method_table = NULL;
    MarimayManagedCell cell;
    MarimayBytecodeValue result = mmay_test_vm_i64(-1);
    MarimayBytecodeValue argument;
    size_t attempts = 0;
    memset(&program, 0, sizeof(program));
    memset(&factory, 0, sizeof(factory));
    memset(&echo, 0, sizeof(echo));
    memset(&execution, 0, sizeof(execution));
    memset(&roots, 0, sizeof(roots));
    memset(&cell, 0, sizeof(cell));
    MMAY_EXPECT(mmay_test_vm_build_interface_container_program(
        &program,
        &factory,
        implementation_symbol,
        container_symbol,
        interface_symbol,
        method_symbol
    ));
    MMAY_EXPECT(mmay_test_vm_build_interface_echo(
        &echo, interface_symbol
    ));
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    {
        const MarimaySymbolId factory_symbol = {{0x20}};
        factory_owner = mmay_bytecode_program_function_by_symbol(
            &program, factory_symbol
        );
    }
    MMAY_EXPECT(factory_owner != NULL);
    execution_options.managed_heap.collect_before_allocation = true;
    mmay_fail_allocator_init(
        &tracking, mmay_system_allocator(), SIZE_MAX
    );
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_fail_allocator(&tracking), execution_options
    ));
    MMAY_EXPECT(mmay_managed_root_push(
        &execution.managed_heap,
        &roots,
        locations,
        sizeof(locations) / sizeof(locations[0])
    ) == MMAY_MANAGED_OK);
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(
        &factory_owner->body,
        NULL,
        0,
        mmay_fail_allocator(&tracking),
        run_options,
        &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_RECORD);
    valid_container = result.as.managed_reference;
    MMAY_EXPECT(mmay_managed_record_describe(
        &execution.managed_heap,
        valid_container,
        &container_descriptor
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_record_get(
        &execution.managed_heap, valid_container, 0, &cell
    ) == MMAY_MANAGED_OK);
    valid_interface = cell.reference;
    MMAY_EXPECT(mmay_managed_interface_describe(
        &execution.managed_heap,
        valid_interface,
        &interface_descriptor,
        &valid_payload,
        &method_table
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_record_new(
        &execution.managed_heap,
        &wrong_descriptor,
        NULL,
        0,
        &wrong_payload
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_interface_new(
        &execution.managed_heap,
        interface_descriptor,
        wrong_payload,
        method_table,
        &forged_interface
    ) == MMAY_MANAGED_OK);
    cell.reference = forged_interface;
    MMAY_EXPECT(mmay_managed_record_new(
        &execution.managed_heap,
        container_descriptor,
        &cell,
        1,
        &forged_container
    ) == MMAY_MANAGED_OK);
    argument.type = MMAY_BYTECODE_TYPE_INTERFACE;
    argument.as.managed_reference = forged_interface;
    attempts = tracking.attempts;
    tracking.failure_index = attempts;
    result = mmay_test_vm_i64(-1);
    MMAY_EXPECT(mmay_bytecode_run(
        &echo,
        &argument,
        1,
        mmay_fail_allocator(&tracking),
        run_options,
        &result
    ) == MMAY_BYTECODE_RUN_INVALID_REFERENCE);
    MMAY_EXPECT(tracking.attempts == attempts);
    tracking.failure_index = SIZE_MAX;
    argument.type = MMAY_BYTECODE_TYPE_RECORD;
    argument.as.managed_reference = forged_container;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &program,
        &argument,
        1,
        mmay_fail_allocator(&tracking),
        run_options,
        &result
    ) == MMAY_BYTECODE_RUN_INVALID_REFERENCE);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64
        && result.as.i64 == -1);
    MMAY_EXPECT(mmay_managed_root_pop(
        &execution.managed_heap, &roots
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(execution.managed_heap.roots == NULL);
    mmay_bytecode_execution_dispose(&execution);
    MMAY_EXPECT(tracking.live_allocations == 0);
    MMAY_EXPECT(tracking.live_bytes == 0);
    mmay_bytecode_function_dispose(&echo);
    mmay_bytecode_function_dispose(&factory);
    mmay_bytecode_program_dispose(&program);
}


void mmay_test_vm_interface_values(void) {
    mmay_test_vm_nested_interface_path(false);
    mmay_test_vm_nested_interface_path(true);
    mmay_test_vm_forged_interface_payload();
}
