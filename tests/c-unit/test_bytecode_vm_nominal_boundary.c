#include "test_bytecode_vm_support.h"

static bool ruda_test_vm_build_nominal_factory(
    RudaBytecodeFunction *function,
    RudaBytecodeType type,
    RudaSymbolId symbol
) {
    RudaBytecodeRecordId record = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeEnumId enumeration = RUDA_BYTECODE_NO_ENUM;
    RudaBytecodeRecordConstructionId record_construction =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeEnumConstructionId enum_construction =
        RUDA_BYTECODE_NO_ENUM_CONSTRUCTION;
    RudaBytecodeEnumVariant variant;
    uint32_t value = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 1;
    bool layout_ready = false;
    bool construction_ready = false;
    memset(function, 0, sizeof(*function));
    memset(&variant, 0, sizeof(variant));
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            type,
            1,
            NULL,
            0,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(function, type, &value)) {
        goto failure;
    }
    if (type == RUDA_BYTECODE_TYPE_RECORD) {
        layout_ready = ruda_bytecode_add_record_layout(
            function, symbol, NULL, 0, &record
        ) && ruda_bytecode_set_register_layout(function, value, record);
        function->result_layout = record;
        construction_ready = layout_ready
            && ruda_bytecode_add_record_construction(
                function, record, NULL, 0, &record_construction
            );
    } else {
        layout_ready = type == RUDA_BYTECODE_TYPE_ENUM
            && ruda_bytecode_add_enum_layout(
                function, symbol, &variant, 1, NULL, 0, &enumeration
            ) && ruda_bytecode_set_register_layout(
                function, value, enumeration
            );
        function->result_layout = enumeration;
        construction_ready = layout_ready
            && ruda_bytecode_add_enum_construction(
                function,
                enumeration,
                0,
                NULL,
                0,
                &enum_construction
            );
    }
    if (!construction_ready || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function,
            block,
            type == RUDA_BYTECODE_TYPE_RECORD
                ? RUDA_OPCODE_RECORD_NEW
                : RUDA_OPCODE_ENUM_NEW,
            value,
            type == RUDA_BYTECODE_TYPE_RECORD
                ? record_construction
                : enum_construction,
            0,
            &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN, value, 0, 0, &span
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_bytecode_function_dispose(function);
    return false;
}

static bool ruda_test_vm_build_nominal_echo(
    RudaBytecodeFunction *function,
    RudaBytecodeType type,
    RudaSymbolId symbol
) {
    RudaBytecodeRecordId record = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeEnumId enumeration = RUDA_BYTECODE_NO_ENUM;
    RudaBytecodeEnumVariant variant;
    uint32_t block = UINT32_MAX;
    uint64_t span = 20;
    bool layout_ready = false;
    memset(function, 0, sizeof(*function));
    memset(&variant, 0, sizeof(variant));
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            type,
            1,
            &type,
            1,
            ruda_bytecode_function_default_options()
        )) {
        goto failure;
    }
    if (type == RUDA_BYTECODE_TYPE_RECORD) {
        layout_ready = ruda_bytecode_add_record_layout(
            function, symbol, NULL, 0, &record
        ) && ruda_bytecode_set_register_layout(function, 0, record);
        function->result_layout = record;
    } else {
        layout_ready = type == RUDA_BYTECODE_TYPE_ENUM
            && ruda_bytecode_add_enum_layout(
                function, symbol, &variant, 1, NULL, 0, &enumeration
            ) && ruda_bytecode_set_register_layout(
                function, 0, enumeration
            );
        function->result_layout = enumeration;
    }
    if (!layout_ready || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN, 0, 0, 0, &span
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_bytecode_function_dispose(function);
    return false;
}

static bool ruda_test_vm_build_nominal_echo_program(
    RudaBytecodeProgram *program,
    RudaBytecodeType type,
    RudaSymbolId nominal_symbol,
    RudaSymbolId function_symbol
) {
    RudaBytecodeFunction function;
    uint32_t function_id = UINT32_MAX;
    memset(program, 0, sizeof(*program));
    memset(&function, 0, sizeof(function));
    if (!ruda_test_vm_build_nominal_echo(
            &function, type, nominal_symbol
        ) || !ruda_bytecode_program_init(
            program,
            ruda_system_allocator(),
            1,
            ruda_bytecode_program_default_options()
        ) || !ruda_bytecode_program_take_function(
            program, function_symbol, &function, &function_id
        ) || !ruda_bytecode_program_set_entry(program, function_symbol)) {
        ruda_bytecode_function_dispose(&function);
        ruda_bytecode_program_dispose(program);
        return false;
    }
    return true;
}

static RudaBytecodeValue ruda_test_vm_make_nominal(
    RudaBytecodeFunction *factory,
    RudaBytecodeExecution *execution,
    RudaAllocator allocator
) {
    RudaBytecodeValue result = ruda_test_vm_i64(-1);
    RudaBytecodeRunOptions options = ruda_bytecode_run_default_options();
    options.execution = execution;
    RUDA_EXPECT(ruda_bytecode_run(
        factory, NULL, 0, allocator, options, &result
    ) == RUDA_BYTECODE_RUN_OK);
    return result;
}

static void ruda_test_vm_expect_nominal_host_rejection(
    const RudaBytecodeFunction *function,
    const RudaBytecodeProgram *program,
    RudaBytecodeExecution *execution,
    RudaFailAllocator *tracking,
    RudaBytecodeValue argument
) {
    RudaBytecodeRunOptions options = ruda_bytecode_run_default_options();
    RudaBytecodeValue result = ruda_test_vm_i64(-1);
    size_t attempts = tracking->attempts;
    options.execution = execution;
    tracking->failure_index = attempts;
    RUDA_EXPECT(ruda_bytecode_run(
        function,
        &argument,
        1,
        ruda_fail_allocator(tracking),
        options,
        &result
    ) == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
    RUDA_EXPECT(tracking->attempts == attempts);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64
        && result.as.i64 == -1);
    RUDA_EXPECT(ruda_bytecode_run_program(
        program,
        &argument,
        1,
        ruda_fail_allocator(tracking),
        options,
        &result
    ) == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
    RUDA_EXPECT(tracking->attempts == attempts);
    tracking->failure_index = SIZE_MAX;
}

void ruda_test_vm_nominal_host_boundary(void) {
    const RudaSymbolId record_a_symbol = {{0xa1}};
    const RudaSymbolId record_b_symbol = {{0xa2}};
    const RudaSymbolId enum_a_symbol = {{0xb1}};
    const RudaSymbolId enum_b_symbol = {{0xb2}};
    const RudaSymbolId record_entry_symbol = {{0xc1}};
    const RudaSymbolId enum_entry_symbol = {{0xc2}};
    RudaBytecodeFunction record_a_factory;
    RudaBytecodeFunction record_b_factory;
    RudaBytecodeFunction enum_a_factory;
    RudaBytecodeFunction enum_b_factory;
    RudaBytecodeFunction record_echo;
    RudaBytecodeFunction enum_echo;
    RudaBytecodeProgram record_program;
    RudaBytecodeProgram enum_program;
    RudaBytecodeExecution first;
    RudaBytecodeExecution second;
    RudaBytecodeExecutionOptions execution_options =
        ruda_bytecode_execution_default_options();
    RudaBytecodeRunOptions run_options = ruda_bytecode_run_default_options();
    RudaFailAllocator first_tracking;
    RudaFailAllocator second_tracking;
    RudaManagedRootFrame first_roots;
    RudaManagedRootFrame second_roots;
    RudaManagedReference record_a = ruda_managed_invalid_reference();
    RudaManagedReference record_b = ruda_managed_invalid_reference();
    RudaManagedReference enum_a = ruda_managed_invalid_reference();
    RudaManagedReference enum_b = ruda_managed_invalid_reference();
    RudaManagedReference foreign_record = ruda_managed_invalid_reference();
    RudaManagedReference *first_locations[] = {
        &record_a, &record_b, &enum_a, &enum_b
    };
    RudaManagedReference *second_locations[] = {&foreign_record};
    RudaBytecodeValue value;
    RudaBytecodeValue result = ruda_test_vm_i64(-1);
    RudaManagedReference stale_record;
    memset(&record_a_factory, 0, sizeof(record_a_factory));
    memset(&record_b_factory, 0, sizeof(record_b_factory));
    memset(&enum_a_factory, 0, sizeof(enum_a_factory));
    memset(&enum_b_factory, 0, sizeof(enum_b_factory));
    memset(&record_echo, 0, sizeof(record_echo));
    memset(&enum_echo, 0, sizeof(enum_echo));
    memset(&record_program, 0, sizeof(record_program));
    memset(&enum_program, 0, sizeof(enum_program));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&first_roots, 0, sizeof(first_roots));
    memset(&second_roots, 0, sizeof(second_roots));
    RUDA_EXPECT(ruda_test_vm_build_nominal_factory(
        &record_a_factory, RUDA_BYTECODE_TYPE_RECORD, record_a_symbol
    ));
    RUDA_EXPECT(ruda_test_vm_build_nominal_factory(
        &record_b_factory, RUDA_BYTECODE_TYPE_RECORD, record_b_symbol
    ));
    RUDA_EXPECT(ruda_test_vm_build_nominal_factory(
        &enum_a_factory, RUDA_BYTECODE_TYPE_ENUM, enum_a_symbol
    ));
    RUDA_EXPECT(ruda_test_vm_build_nominal_factory(
        &enum_b_factory, RUDA_BYTECODE_TYPE_ENUM, enum_b_symbol
    ));
    RUDA_EXPECT(ruda_test_vm_build_nominal_echo(
        &record_echo, RUDA_BYTECODE_TYPE_RECORD, record_a_symbol
    ));
    RUDA_EXPECT(ruda_test_vm_build_nominal_echo(
        &enum_echo, RUDA_BYTECODE_TYPE_ENUM, enum_a_symbol
    ));
    RUDA_EXPECT(ruda_test_vm_build_nominal_echo_program(
        &record_program,
        RUDA_BYTECODE_TYPE_RECORD,
        record_a_symbol,
        record_entry_symbol
    ));
    RUDA_EXPECT(ruda_test_vm_build_nominal_echo_program(
        &enum_program,
        RUDA_BYTECODE_TYPE_ENUM,
        enum_a_symbol,
        enum_entry_symbol
    ));
    RUDA_EXPECT(ruda_bytecode_program_verify(&record_program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_program_verify(&enum_program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    execution_options.managed_heap.collect_before_allocation = true;
    ruda_fail_allocator_init(
        &first_tracking, ruda_system_allocator(), SIZE_MAX
    );
    ruda_fail_allocator_init(
        &second_tracking, ruda_system_allocator(), SIZE_MAX
    );
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &first, ruda_fail_allocator(&first_tracking), execution_options
    ));
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &second, ruda_fail_allocator(&second_tracking), execution_options
    ));
    RUDA_EXPECT(ruda_managed_root_push(
        &first.managed_heap,
        &first_roots,
        first_locations,
        sizeof(first_locations) / sizeof(first_locations[0])
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_push(
        &second.managed_heap,
        &second_roots,
        second_locations,
        sizeof(second_locations) / sizeof(second_locations[0])
    ) == RUDA_MANAGED_OK);
    value = ruda_test_vm_make_nominal(
        &record_a_factory, &first, ruda_fail_allocator(&first_tracking)
    );
    record_a = value.as.managed_reference;
    value = ruda_test_vm_make_nominal(
        &record_b_factory, &first, ruda_fail_allocator(&first_tracking)
    );
    record_b = value.as.managed_reference;
    value = ruda_test_vm_make_nominal(
        &enum_a_factory, &first, ruda_fail_allocator(&first_tracking)
    );
    enum_a = value.as.managed_reference;
    value = ruda_test_vm_make_nominal(
        &enum_b_factory, &first, ruda_fail_allocator(&first_tracking)
    );
    enum_b = value.as.managed_reference;
    value = ruda_test_vm_make_nominal(
        &record_a_factory, &second, ruda_fail_allocator(&second_tracking)
    );
    foreign_record = value.as.managed_reference;
    value = ruda_test_vm_make_nominal(
        &record_a_factory, &first, ruda_fail_allocator(&first_tracking)
    );
    stale_record = value.as.managed_reference;
    RUDA_EXPECT(ruda_managed_heap_collect(&first.managed_heap)
        == RUDA_MANAGED_OK);
    run_options.execution = &first;
    value.type = RUDA_BYTECODE_TYPE_RECORD;
    value.as.managed_reference = record_a;
    RUDA_EXPECT(ruda_bytecode_run(
        &record_echo,
        &value,
        1,
        ruda_fail_allocator(&first_tracking),
        run_options,
        &result
    ) == RUDA_BYTECODE_RUN_OK);
    value.type = RUDA_BYTECODE_TYPE_ENUM;
    value.as.managed_reference = enum_a;
    RUDA_EXPECT(ruda_bytecode_run_program(
        &enum_program,
        &value,
        1,
        ruda_fail_allocator(&first_tracking),
        run_options,
        &result
    ) == RUDA_BYTECODE_RUN_OK);
    value.type = RUDA_BYTECODE_TYPE_RECORD;
    value.as.managed_reference = enum_a;
    ruda_test_vm_expect_nominal_host_rejection(
        &record_echo, &record_program, &first, &first_tracking, value
    );
    value.as.managed_reference = record_b;
    ruda_test_vm_expect_nominal_host_rejection(
        &record_echo, &record_program, &first, &first_tracking, value
    );
    value.as.managed_reference = foreign_record;
    ruda_test_vm_expect_nominal_host_rejection(
        &record_echo, &record_program, &first, &first_tracking, value
    );
    value.as.managed_reference = stale_record;
    ruda_test_vm_expect_nominal_host_rejection(
        &record_echo, &record_program, &first, &first_tracking, value
    );
    value.type = RUDA_BYTECODE_TYPE_ENUM;
    value.as.managed_reference = record_a;
    ruda_test_vm_expect_nominal_host_rejection(
        &enum_echo, &enum_program, &first, &first_tracking, value
    );
    value.as.managed_reference = enum_b;
    ruda_test_vm_expect_nominal_host_rejection(
        &enum_echo, &enum_program, &first, &first_tracking, value
    );
    RUDA_EXPECT(ruda_managed_root_pop(
        &second.managed_heap, &second_roots
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_root_pop(
        &first.managed_heap, &first_roots
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(first.managed_heap.roots == NULL);
    RUDA_EXPECT(second.managed_heap.roots == NULL);
    ruda_bytecode_execution_dispose(&second);
    ruda_bytecode_execution_dispose(&first);
    RUDA_EXPECT(first_tracking.live_allocations == 0);
    RUDA_EXPECT(first_tracking.live_bytes == 0);
    RUDA_EXPECT(second_tracking.live_allocations == 0);
    RUDA_EXPECT(second_tracking.live_bytes == 0);
    ruda_bytecode_program_dispose(&enum_program);
    ruda_bytecode_program_dispose(&record_program);
    ruda_bytecode_function_dispose(&enum_echo);
    ruda_bytecode_function_dispose(&record_echo);
    ruda_bytecode_function_dispose(&enum_b_factory);
    ruda_bytecode_function_dispose(&enum_a_factory);
    ruda_bytecode_function_dispose(&record_b_factory);
    ruda_bytecode_function_dispose(&record_a_factory);
}
