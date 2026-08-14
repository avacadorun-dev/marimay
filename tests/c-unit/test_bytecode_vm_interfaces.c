#include "test_bytecode_vm_interface_support.h"

typedef struct RudaTestInterfaceProgram {
    RudaBytecodeProgram program;
    RudaSymbolId record_symbol;
    RudaSymbolId interface_symbol;
    RudaSymbolId method_symbol;
    uint32_t entry_index;
} RudaTestInterfaceProgram;


static bool ruda_test_vm_build_interface_entry(
    RudaBytecodeFunction *function,
    RudaSymbolId record_symbol,
    RudaSymbolId interface_symbol,
    RudaSymbolId method_symbol
) {
    RudaBytecodeRecordField field;
    RudaBytecodeRecordArgument record_argument;
    RudaBytecodeCallArgument call_argument;
    RudaBytecodeRecordId record = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordConstructionId record_construction =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeInterfaceId interface_id = RUDA_BYTECODE_NO_INTERFACE;
    RudaBytecodeInterfaceConstructionId interface_construction =
        RUDA_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    RudaBytecodeCallId call_id = RUDA_BYTECODE_NO_CALL;
    RudaBytecodeCall *call = NULL;
    RudaSymbolId placeholder = {{0}};
    uint32_t scalar = UINT32_MAX;
    uint32_t object = UINT32_MAX;
    uint32_t interface_value = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 10;
    memset(function, 0, sizeof(*function));
    memset(&field, 0, sizeof(field));
    memset(&record_argument, 0, sizeof(record_argument));
    memset(&call_argument, 0, sizeof(call_argument));
    field.type = RUDA_BYTECODE_TYPE_I64;
    field.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_I64,
            1,
            NULL,
            0,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &scalar
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &object
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_INTERFACE, &interface_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &result
        ) || !ruda_bytecode_add_record_layout(
            function, record_symbol, &field, 1, &record
        ) || !ruda_bytecode_set_register_layout(function, object, record)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    record_argument.register_id = scalar;
    record_argument.type = RUDA_BYTECODE_TYPE_I64;
    record_argument.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_add_record_construction(
            function,
            record,
            &record_argument,
            1,
            &record_construction
        ) || !ruda_bytecode_add_interface_layout(
            function,
            interface_symbol,
            1,
            &interface_id
        ) || !ruda_bytecode_set_register_layout(
            function,
            interface_value,
            interface_id
        ) || !ruda_bytecode_add_interface_construction(
            function,
            interface_id,
            object,
            RUDA_BYTECODE_TYPE_RECORD,
            record,
            &method_symbol,
            1,
            &interface_construction
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
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
        ruda_bytecode_function_dispose(function);
        return false;
    }
    call = ruda_vector_at(&function->calls, call_id);
    if (call == NULL) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    call->interface_slot = 0;
    if (!ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_I64, scalar, 41, 0, &span
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RECORD_NEW,
            object,
            record_construction,
            0,
            &span
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_INTERFACE_NEW,
            interface_value,
            interface_construction,
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
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_interface_program(
    RudaTestInterfaceProgram *built
) {
    RudaBytecodeFunction method;
    RudaBytecodeFunction entry;
    uint32_t method_index = UINT32_MAX;
    memset(built, 0, sizeof(*built));
    memset(&method, 0, sizeof(method));
    memset(&entry, 0, sizeof(entry));
    built->method_symbol.bytes[0] = 0x10;
    built->record_symbol.bytes[0] = 0x40;
    built->interface_symbol.bytes[0] = 0x50;
    {
        RudaSymbolId entry_symbol = {{0x20}};
        if (!ruda_test_vm_build_interface_method(
                &method,
                built->record_symbol
            ) || !ruda_test_vm_build_interface_entry(
                &entry,
                built->record_symbol,
                built->interface_symbol,
                built->method_symbol
            ) || !ruda_bytecode_program_init(
                &built->program,
                ruda_system_allocator(),
                1,
                ruda_bytecode_program_default_options()
            ) || !ruda_bytecode_program_take_function(
                &built->program,
                built->method_symbol,
                &method,
                &method_index
            ) || !ruda_bytecode_program_take_function(
                &built->program,
                entry_symbol,
                &entry,
                &built->entry_index
            ) || !ruda_bytecode_program_set_entry(
                &built->program,
                entry_symbol
            )) {
            ruda_bytecode_function_dispose(&method);
            ruda_bytecode_function_dispose(&entry);
            ruda_bytecode_program_dispose(&built->program);
            return false;
        }
    }
    return method_index == 0 && built->entry_index == 1;
}

static bool ruda_test_vm_add_interface_scan_functions(
    RudaBytecodeProgram *program,
    size_t count
) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        RudaBytecodeFunction function;
        RudaSymbolId symbol = {{0}};
        uint32_t function_id = UINT32_MAX;
        memset(&function, 0, sizeof(function));
        symbol.bytes[0] = (unsigned char)(0x30U + index);
        if (!ruda_test_vm_build_direct_constant(
                &function,
                ruda_system_allocator(),
                (int64_t)index,
                300 + index
            ) || !ruda_bytecode_program_take_function(
                program,
                symbol,
                &function,
                &function_id
            )) {
            ruda_bytecode_function_dispose(&function);
            return false;
        }
    }
    return true;
}

static void ruda_test_vm_interface_scan_work_accounting(void) {
    enum {
        EXTRA_FUNCTION_COUNT = 16,
        WORK_WITHOUT_FUNCTION_SCAN = 204,
        WORK_WITH_FUNCTION_SCAN = 222
    };
    RudaTestInterfaceProgram built;
    memset(&built, 0, sizeof(built));
    RUDA_EXPECT(ruda_test_vm_build_interface_program(&built));
    RUDA_EXPECT(ruda_test_vm_add_interface_scan_functions(
        &built.program,
        EXTRA_FUNCTION_COUNT
    ));
    RUDA_EXPECT(built.program.functions.length
        == EXTRA_FUNCTION_COUNT + 2);
    RUDA_EXPECT(WORK_WITH_FUNCTION_SCAN - WORK_WITHOUT_FUNCTION_SCAN
        == EXTRA_FUNCTION_COUNT + 2);
    built.program.options.max_verify_work = WORK_WITHOUT_FUNCTION_SCAN;
    RUDA_EXPECT(ruda_bytecode_program_verify(&built.program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
    built.program.options.max_verify_work = WORK_WITH_FUNCTION_SCAN;
    RUDA_EXPECT(ruda_bytecode_program_verify(&built.program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    ruda_bytecode_program_dispose(&built.program);
}

static void ruda_test_vm_interface_verifier(void) {
    RudaBytecodeFunction entry;
    RudaSymbolId record_symbol = {{0x40}};
    RudaSymbolId interface_symbol = {{0x50}};
    RudaSymbolId method_symbol = {{0x10}};
    RudaBytecodeBlock *block = NULL;
    RudaBytecodeInstruction *interface_new = NULL;
    RudaBytecodeCall *call = NULL;
    memset(&entry, 0, sizeof(entry));
    RUDA_EXPECT(ruda_test_vm_build_interface_entry(
        &entry, record_symbol, interface_symbol, method_symbol
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&entry) == RUDA_BYTECODE_VERIFY_OK);
    call = ruda_vector_at(&entry.calls, 0);
    RUDA_EXPECT(call != NULL);
    call->interface_slot = 1;
    RUDA_EXPECT(ruda_bytecode_verify(&entry)
        == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
    call->interface_slot = 0;
    block = ruda_vector_at(&entry.blocks, 0);
    interface_new = block != NULL
        ? ruda_vector_at(&block->instructions, 2)
        : NULL;
    RUDA_EXPECT(interface_new != NULL);
    interface_new->operands[0].unsigned_value = 1;
    RUDA_EXPECT(ruda_bytecode_verify(&entry)
        == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
    interface_new->operands[0].unsigned_value = 2;
    {
        RudaBytecodeInstruction saved = *interface_new;
        RudaBytecodeInstruction *record_new = ruda_vector_at(
            &block->instructions,
            1
        );
        RUDA_EXPECT(record_new != NULL);
        *interface_new = *record_new;
        *record_new = saved;
        RUDA_EXPECT(ruda_bytecode_verify(&entry)
            == RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
        saved = *interface_new;
        *interface_new = *record_new;
        *record_new = saved;
    }
    ruda_bytecode_function_dispose(&entry);
}

static void ruda_test_vm_interface_program_and_failures(void) {
    RudaTestInterfaceProgram built;
    RudaBytecodeExecution execution;
    RudaBytecodeExecutionOptions execution_options =
        ruda_bytecode_execution_default_options();
    RudaBytecodeRunOptions run_options = ruda_bytecode_run_default_options();
    RudaBytecodeValue result = ruda_test_vm_i64(-1);
    bool reached_success = false;
    size_t failure_index = 0;
    memset(&built, 0, sizeof(built));
    memset(&execution, 0, sizeof(execution));
    RUDA_EXPECT(ruda_test_vm_build_interface_program(&built));
    RUDA_EXPECT(ruda_bytecode_program_verify(&built.program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    execution_options.managed_heap.collect_before_allocation = true;
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(), execution_options
    ));
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run_program(
        &built.program,
        NULL,
        0,
        ruda_system_allocator(),
        run_options,
        &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64
        && result.as.i64 == 41);
    RUDA_EXPECT(execution.managed_heap.roots == NULL);
    ruda_bytecode_execution_dispose(&execution);

    {
        RudaBytecodeProgramFunction *method = ruda_vector_at(
            &built.program.functions,
            0
        );
        RudaBytecodeRecordLayout *layout = method != NULL
            ? ruda_vector_at(&method->body.record_layouts, 0)
            : NULL;
        RUDA_EXPECT(layout != NULL);
        layout->symbol.bytes[0] ^= 1;
        RUDA_EXPECT(ruda_bytecode_program_verify(&built.program)
            == RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH);
        layout->symbol.bytes[0] ^= 1;
    }

    for (failure_index = 0; failure_index < 96 && !reached_success;
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
            ruda_bytecode_execution_default_options()
        ));
        state.failure_index = state.attempts + failure_index;
        run_options = ruda_bytecode_run_default_options();
        run_options.execution = &failed_execution;
        result = ruda_test_vm_i64(-1);
        status = ruda_bytecode_run_program(
            &built.program,
            NULL,
            0,
            ruda_fail_allocator(&state),
            run_options,
            &result
        );
        if (status == RUDA_BYTECODE_RUN_OK) {
            reached_success = true;
            RUDA_EXPECT(result.as.i64 == 41);
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED);
            RUDA_EXPECT(result.as.i64 == -1);
        }
        ruda_bytecode_execution_dispose(&failed_execution);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_bytecode_program_dispose(&built.program);
}

void ruda_test_vm_interfaces(void) {
    ruda_test_vm_interface_verifier();
    ruda_test_vm_interface_scan_work_accounting();
    ruda_test_vm_interface_program_and_failures();
}
