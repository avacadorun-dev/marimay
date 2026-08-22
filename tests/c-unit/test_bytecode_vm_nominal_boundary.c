#include "test_bytecode_vm_support.h"

static bool mmay_test_vm_build_nominal_factory(
    MarimayBytecodeFunction *function,
    MarimayBytecodeType type,
    MarimaySymbolId symbol
) {
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeEnumId enumeration = MMAY_BYTECODE_NO_ENUM;
    MarimayBytecodeRecordConstructionId record_construction =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeEnumConstructionId enum_construction =
        MMAY_BYTECODE_NO_ENUM_CONSTRUCTION;
    MarimayBytecodeEnumVariant variant;
    uint32_t value = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 1;
    bool layout_ready = false;
    bool construction_ready = false;
    memset(function, 0, sizeof(*function));
    memset(&variant, 0, sizeof(variant));
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            type,
            1,
            NULL,
            0,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(function, type, &value)) {
        goto failure;
    }
    if (type == MMAY_BYTECODE_TYPE_RECORD) {
        layout_ready = mmay_bytecode_add_record_layout(
            function, symbol, NULL, 0, &record
        ) && mmay_bytecode_set_register_layout(function, value, record);
        function->result_layout = record;
        construction_ready = layout_ready
            && mmay_bytecode_add_record_construction(
                function, record, NULL, 0, &record_construction
            );
    } else {
        layout_ready = type == MMAY_BYTECODE_TYPE_ENUM
            && mmay_bytecode_add_enum_layout(
                function, symbol, &variant, 1, NULL, 0, &enumeration
            ) && mmay_bytecode_set_register_layout(
                function, value, enumeration
            );
        function->result_layout = enumeration;
        construction_ready = layout_ready
            && mmay_bytecode_add_enum_construction(
                function,
                enumeration,
                0,
                NULL,
                0,
                &enum_construction
            );
    }
    if (!construction_ready || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function,
            block,
            type == MMAY_BYTECODE_TYPE_RECORD
                ? MMAY_OPCODE_RECORD_NEW
                : MMAY_OPCODE_ENUM_NEW,
            value,
            type == MMAY_BYTECODE_TYPE_RECORD
                ? record_construction
                : enum_construction,
            0,
            &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, value, 0, 0, &span
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_bytecode_function_dispose(function);
    return false;
}

static bool mmay_test_vm_build_nominal_echo(
    MarimayBytecodeFunction *function,
    MarimayBytecodeType type,
    MarimaySymbolId symbol
) {
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeEnumId enumeration = MMAY_BYTECODE_NO_ENUM;
    MarimayBytecodeEnumVariant variant;
    uint32_t block = UINT32_MAX;
    uint64_t span = 20;
    bool layout_ready = false;
    memset(function, 0, sizeof(*function));
    memset(&variant, 0, sizeof(variant));
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            type,
            1,
            &type,
            1,
            mmay_bytecode_function_default_options()
        )) {
        goto failure;
    }
    if (type == MMAY_BYTECODE_TYPE_RECORD) {
        layout_ready = mmay_bytecode_add_record_layout(
            function, symbol, NULL, 0, &record
        ) && mmay_bytecode_set_register_layout(function, 0, record);
        function->result_layout = record;
    } else {
        layout_ready = type == MMAY_BYTECODE_TYPE_ENUM
            && mmay_bytecode_add_enum_layout(
                function, symbol, &variant, 1, NULL, 0, &enumeration
            ) && mmay_bytecode_set_register_layout(
                function, 0, enumeration
            );
        function->result_layout = enumeration;
    }
    if (!layout_ready || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, 0, 0, 0, &span
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_bytecode_function_dispose(function);
    return false;
}

static bool mmay_test_vm_build_nominal_echo_program(
    MarimayBytecodeProgram *program,
    MarimayBytecodeType type,
    MarimaySymbolId nominal_symbol,
    MarimaySymbolId function_symbol
) {
    MarimayBytecodeFunction function;
    uint32_t function_id = UINT32_MAX;
    memset(program, 0, sizeof(*program));
    memset(&function, 0, sizeof(function));
    if (!mmay_test_vm_build_nominal_echo(
            &function, type, nominal_symbol
        ) || !mmay_bytecode_program_init(
            program,
            mmay_system_allocator(),
            1,
            mmay_bytecode_program_default_options()
        ) || !mmay_bytecode_program_take_function(
            program, function_symbol, &function, &function_id
        ) || !mmay_bytecode_program_set_entry(program, function_symbol)) {
        mmay_bytecode_function_dispose(&function);
        mmay_bytecode_program_dispose(program);
        return false;
    }
    return true;
}

static MarimayBytecodeValue mmay_test_vm_make_nominal(
    MarimayBytecodeFunction *factory,
    MarimayBytecodeExecution *execution,
    MarimayAllocator allocator
) {
    MarimayBytecodeValue result = mmay_test_vm_i64(-1);
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.execution = execution;
    MMAY_EXPECT(mmay_bytecode_run(
        factory, NULL, 0, allocator, options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    return result;
}

static void mmay_test_vm_expect_nominal_host_rejection(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeProgram *program,
    MarimayBytecodeExecution *execution,
    MarimayFailAllocator *tracking,
    MarimayBytecodeValue argument
) {
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue result = mmay_test_vm_i64(-1);
    size_t attempts = tracking->attempts;
    options.execution = execution;
    tracking->failure_index = attempts;
    MMAY_EXPECT(mmay_bytecode_run(
        function,
        &argument,
        1,
        mmay_fail_allocator(tracking),
        options,
        &result
    ) == MMAY_BYTECODE_RUN_INVALID_REFERENCE);
    MMAY_EXPECT(tracking->attempts == attempts);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64
        && result.as.i64 == -1);
    MMAY_EXPECT(mmay_bytecode_run_program(
        program,
        &argument,
        1,
        mmay_fail_allocator(tracking),
        options,
        &result
    ) == MMAY_BYTECODE_RUN_INVALID_REFERENCE);
    MMAY_EXPECT(tracking->attempts == attempts);
    tracking->failure_index = SIZE_MAX;
}

void mmay_test_vm_nominal_host_boundary(void) {
    const MarimaySymbolId record_a_symbol = {{0xa1}};
    const MarimaySymbolId record_b_symbol = {{0xa2}};
    const MarimaySymbolId enum_a_symbol = {{0xb1}};
    const MarimaySymbolId enum_b_symbol = {{0xb2}};
    const MarimaySymbolId record_entry_symbol = {{0xc1}};
    const MarimaySymbolId enum_entry_symbol = {{0xc2}};
    MarimayBytecodeFunction record_a_factory;
    MarimayBytecodeFunction record_b_factory;
    MarimayBytecodeFunction enum_a_factory;
    MarimayBytecodeFunction enum_b_factory;
    MarimayBytecodeFunction record_echo;
    MarimayBytecodeFunction enum_echo;
    MarimayBytecodeProgram record_program;
    MarimayBytecodeProgram enum_program;
    MarimayBytecodeExecution first;
    MarimayBytecodeExecution second;
    MarimayBytecodeExecutionOptions execution_options =
        mmay_bytecode_execution_default_options();
    MarimayBytecodeRunOptions run_options = mmay_bytecode_run_default_options();
    MarimayFailAllocator first_tracking;
    MarimayFailAllocator second_tracking;
    MarimayManagedRootFrame first_roots;
    MarimayManagedRootFrame second_roots;
    MarimayManagedReference record_a = mmay_managed_invalid_reference();
    MarimayManagedReference record_b = mmay_managed_invalid_reference();
    MarimayManagedReference enum_a = mmay_managed_invalid_reference();
    MarimayManagedReference enum_b = mmay_managed_invalid_reference();
    MarimayManagedReference foreign_record = mmay_managed_invalid_reference();
    MarimayManagedReference *first_locations[] = {
        &record_a, &record_b, &enum_a, &enum_b
    };
    MarimayManagedReference *second_locations[] = {&foreign_record};
    MarimayBytecodeValue value;
    MarimayBytecodeValue result = mmay_test_vm_i64(-1);
    MarimayManagedReference stale_record;
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
    MMAY_EXPECT(mmay_test_vm_build_nominal_factory(
        &record_a_factory, MMAY_BYTECODE_TYPE_RECORD, record_a_symbol
    ));
    MMAY_EXPECT(mmay_test_vm_build_nominal_factory(
        &record_b_factory, MMAY_BYTECODE_TYPE_RECORD, record_b_symbol
    ));
    MMAY_EXPECT(mmay_test_vm_build_nominal_factory(
        &enum_a_factory, MMAY_BYTECODE_TYPE_ENUM, enum_a_symbol
    ));
    MMAY_EXPECT(mmay_test_vm_build_nominal_factory(
        &enum_b_factory, MMAY_BYTECODE_TYPE_ENUM, enum_b_symbol
    ));
    MMAY_EXPECT(mmay_test_vm_build_nominal_echo(
        &record_echo, MMAY_BYTECODE_TYPE_RECORD, record_a_symbol
    ));
    MMAY_EXPECT(mmay_test_vm_build_nominal_echo(
        &enum_echo, MMAY_BYTECODE_TYPE_ENUM, enum_a_symbol
    ));
    MMAY_EXPECT(mmay_test_vm_build_nominal_echo_program(
        &record_program,
        MMAY_BYTECODE_TYPE_RECORD,
        record_a_symbol,
        record_entry_symbol
    ));
    MMAY_EXPECT(mmay_test_vm_build_nominal_echo_program(
        &enum_program,
        MMAY_BYTECODE_TYPE_ENUM,
        enum_a_symbol,
        enum_entry_symbol
    ));
    MMAY_EXPECT(mmay_bytecode_program_verify(&record_program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_program_verify(&enum_program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    execution_options.managed_heap.collect_before_allocation = true;
    mmay_fail_allocator_init(
        &first_tracking, mmay_system_allocator(), SIZE_MAX
    );
    mmay_fail_allocator_init(
        &second_tracking, mmay_system_allocator(), SIZE_MAX
    );
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &first, mmay_fail_allocator(&first_tracking), execution_options
    ));
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &second, mmay_fail_allocator(&second_tracking), execution_options
    ));
    MMAY_EXPECT(mmay_managed_root_push(
        &first.managed_heap,
        &first_roots,
        first_locations,
        sizeof(first_locations) / sizeof(first_locations[0])
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_push(
        &second.managed_heap,
        &second_roots,
        second_locations,
        sizeof(second_locations) / sizeof(second_locations[0])
    ) == MMAY_MANAGED_OK);
    value = mmay_test_vm_make_nominal(
        &record_a_factory, &first, mmay_fail_allocator(&first_tracking)
    );
    record_a = value.as.managed_reference;
    value = mmay_test_vm_make_nominal(
        &record_b_factory, &first, mmay_fail_allocator(&first_tracking)
    );
    record_b = value.as.managed_reference;
    value = mmay_test_vm_make_nominal(
        &enum_a_factory, &first, mmay_fail_allocator(&first_tracking)
    );
    enum_a = value.as.managed_reference;
    value = mmay_test_vm_make_nominal(
        &enum_b_factory, &first, mmay_fail_allocator(&first_tracking)
    );
    enum_b = value.as.managed_reference;
    value = mmay_test_vm_make_nominal(
        &record_a_factory, &second, mmay_fail_allocator(&second_tracking)
    );
    foreign_record = value.as.managed_reference;
    value = mmay_test_vm_make_nominal(
        &record_a_factory, &first, mmay_fail_allocator(&first_tracking)
    );
    stale_record = value.as.managed_reference;
    MMAY_EXPECT(mmay_managed_heap_collect(&first.managed_heap)
        == MMAY_MANAGED_OK);
    run_options.execution = &first;
    value.type = MMAY_BYTECODE_TYPE_RECORD;
    value.as.managed_reference = record_a;
    MMAY_EXPECT(mmay_bytecode_run(
        &record_echo,
        &value,
        1,
        mmay_fail_allocator(&first_tracking),
        run_options,
        &result
    ) == MMAY_BYTECODE_RUN_OK);
    value.type = MMAY_BYTECODE_TYPE_ENUM;
    value.as.managed_reference = enum_a;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &enum_program,
        &value,
        1,
        mmay_fail_allocator(&first_tracking),
        run_options,
        &result
    ) == MMAY_BYTECODE_RUN_OK);
    value.type = MMAY_BYTECODE_TYPE_RECORD;
    value.as.managed_reference = enum_a;
    mmay_test_vm_expect_nominal_host_rejection(
        &record_echo, &record_program, &first, &first_tracking, value
    );
    value.as.managed_reference = record_b;
    mmay_test_vm_expect_nominal_host_rejection(
        &record_echo, &record_program, &first, &first_tracking, value
    );
    value.as.managed_reference = foreign_record;
    mmay_test_vm_expect_nominal_host_rejection(
        &record_echo, &record_program, &first, &first_tracking, value
    );
    value.as.managed_reference = stale_record;
    mmay_test_vm_expect_nominal_host_rejection(
        &record_echo, &record_program, &first, &first_tracking, value
    );
    value.type = MMAY_BYTECODE_TYPE_ENUM;
    value.as.managed_reference = record_a;
    mmay_test_vm_expect_nominal_host_rejection(
        &enum_echo, &enum_program, &first, &first_tracking, value
    );
    value.as.managed_reference = enum_b;
    mmay_test_vm_expect_nominal_host_rejection(
        &enum_echo, &enum_program, &first, &first_tracking, value
    );
    MMAY_EXPECT(mmay_managed_root_pop(
        &second.managed_heap, &second_roots
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_root_pop(
        &first.managed_heap, &first_roots
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(first.managed_heap.roots == NULL);
    MMAY_EXPECT(second.managed_heap.roots == NULL);
    mmay_bytecode_execution_dispose(&second);
    mmay_bytecode_execution_dispose(&first);
    MMAY_EXPECT(first_tracking.live_allocations == 0);
    MMAY_EXPECT(first_tracking.live_bytes == 0);
    MMAY_EXPECT(second_tracking.live_allocations == 0);
    MMAY_EXPECT(second_tracking.live_bytes == 0);
    mmay_bytecode_program_dispose(&enum_program);
    mmay_bytecode_program_dispose(&record_program);
    mmay_bytecode_function_dispose(&enum_echo);
    mmay_bytecode_function_dispose(&record_echo);
    mmay_bytecode_function_dispose(&enum_b_factory);
    mmay_bytecode_function_dispose(&enum_a_factory);
    mmay_bytecode_function_dispose(&record_b_factory);
    mmay_bytecode_function_dispose(&record_a_factory);
}
