#include "test_bytecode_vm_interface_support.h"

typedef struct MarimayTestInterfaceProgram {
    MarimayBytecodeProgram program;
    MarimaySymbolId record_symbol;
    MarimaySymbolId interface_symbol;
    MarimaySymbolId method_symbol;
    uint32_t entry_index;
} MarimayTestInterfaceProgram;


static bool mmay_test_vm_build_interface_entry(
    MarimayBytecodeFunction *function,
    MarimaySymbolId record_symbol,
    MarimaySymbolId interface_symbol,
    MarimaySymbolId method_symbol
) {
    MarimayBytecodeRecordField field;
    MarimayBytecodeRecordArgument record_argument;
    MarimayBytecodeCallArgument call_argument;
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordConstructionId record_construction =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeInterfaceId interface_id = MMAY_BYTECODE_NO_INTERFACE;
    MarimayBytecodeInterfaceConstructionId interface_construction =
        MMAY_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    MarimayBytecodeCallId call_id = MMAY_BYTECODE_NO_CALL;
    MarimayBytecodeCall *call = NULL;
    MarimaySymbolId placeholder = {{0}};
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
    field.type = MMAY_BYTECODE_TYPE_I64;
    field.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_I64,
            1,
            NULL,
            0,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &scalar
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &object
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_INTERFACE, &interface_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &result
        ) || !mmay_bytecode_add_record_layout(
            function, record_symbol, &field, 1, &record
        ) || !mmay_bytecode_set_register_layout(function, object, record)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    record_argument.register_id = scalar;
    record_argument.type = MMAY_BYTECODE_TYPE_I64;
    record_argument.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_add_record_construction(
            function,
            record,
            &record_argument,
            1,
            &record_construction
        ) || !mmay_bytecode_add_interface_layout(
            function,
            interface_symbol,
            1,
            &interface_id
        ) || !mmay_bytecode_set_register_layout(
            function,
            interface_value,
            interface_id
        ) || !mmay_bytecode_add_interface_construction(
            function,
            interface_id,
            object,
            MMAY_BYTECODE_TYPE_RECORD,
            record,
            &method_symbol,
            1,
            &interface_construction
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
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
        mmay_bytecode_function_dispose(function);
        return false;
    }
    call = mmay_vector_at(&function->calls, call_id);
    if (call == NULL) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    call->interface_slot = 0;
    if (!mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_I64, scalar, 41, 0, &span
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RECORD_NEW,
            object,
            record_construction,
            0,
            &span
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_INTERFACE_NEW,
            interface_value,
            interface_construction,
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
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_vm_build_interface_program(
    MarimayTestInterfaceProgram *built
) {
    MarimayBytecodeFunction method;
    MarimayBytecodeFunction entry;
    uint32_t method_index = UINT32_MAX;
    memset(built, 0, sizeof(*built));
    memset(&method, 0, sizeof(method));
    memset(&entry, 0, sizeof(entry));
    built->method_symbol.bytes[0] = 0x10;
    built->record_symbol.bytes[0] = 0x40;
    built->interface_symbol.bytes[0] = 0x50;
    {
        MarimaySymbolId entry_symbol = {{0x20}};
        if (!mmay_test_vm_build_interface_method(
                &method,
                built->record_symbol
            ) || !mmay_test_vm_build_interface_entry(
                &entry,
                built->record_symbol,
                built->interface_symbol,
                built->method_symbol
            ) || !mmay_bytecode_program_init(
                &built->program,
                mmay_system_allocator(),
                1,
                mmay_bytecode_program_default_options()
            ) || !mmay_bytecode_program_take_function(
                &built->program,
                built->method_symbol,
                &method,
                &method_index
            ) || !mmay_bytecode_program_take_function(
                &built->program,
                entry_symbol,
                &entry,
                &built->entry_index
            ) || !mmay_bytecode_program_set_entry(
                &built->program,
                entry_symbol
            )) {
            mmay_bytecode_function_dispose(&method);
            mmay_bytecode_function_dispose(&entry);
            mmay_bytecode_program_dispose(&built->program);
            return false;
        }
    }
    return method_index == 0 && built->entry_index == 1;
}

static bool mmay_test_vm_add_interface_scan_functions(
    MarimayBytecodeProgram *program,
    size_t count
) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        MarimayBytecodeFunction function;
        MarimaySymbolId symbol = {{0}};
        uint32_t function_id = UINT32_MAX;
        memset(&function, 0, sizeof(function));
        symbol.bytes[0] = (unsigned char)(0x30U + index);
        if (!mmay_test_vm_build_direct_constant(
                &function,
                mmay_system_allocator(),
                (int64_t)index,
                300 + index
            ) || !mmay_bytecode_program_take_function(
                program,
                symbol,
                &function,
                &function_id
            )) {
            mmay_bytecode_function_dispose(&function);
            return false;
        }
    }
    return true;
}

static void mmay_test_vm_interface_scan_work_accounting(void) {
    enum {
        EXTRA_FUNCTION_COUNT = 16,
        WORK_WITHOUT_FUNCTION_SCAN = 204,
        WORK_WITH_FUNCTION_SCAN = 222
    };
    MarimayTestInterfaceProgram built;
    memset(&built, 0, sizeof(built));
    MMAY_EXPECT(mmay_test_vm_build_interface_program(&built));
    MMAY_EXPECT(mmay_test_vm_add_interface_scan_functions(
        &built.program,
        EXTRA_FUNCTION_COUNT
    ));
    MMAY_EXPECT(built.program.functions.length
        == EXTRA_FUNCTION_COUNT + 2);
    MMAY_EXPECT(WORK_WITH_FUNCTION_SCAN - WORK_WITHOUT_FUNCTION_SCAN
        == EXTRA_FUNCTION_COUNT + 2);
    built.program.options.max_verify_work = WORK_WITHOUT_FUNCTION_SCAN;
    MMAY_EXPECT(mmay_bytecode_program_verify(&built.program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
    built.program.options.max_verify_work = WORK_WITH_FUNCTION_SCAN;
    MMAY_EXPECT(mmay_bytecode_program_verify(&built.program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    mmay_bytecode_program_dispose(&built.program);
}

static void mmay_test_vm_interface_verifier(void) {
    MarimayBytecodeFunction entry;
    MarimaySymbolId record_symbol = {{0x40}};
    MarimaySymbolId interface_symbol = {{0x50}};
    MarimaySymbolId method_symbol = {{0x10}};
    MarimayBytecodeBlock *block = NULL;
    MarimayBytecodeInstruction *interface_new = NULL;
    MarimayBytecodeCall *call = NULL;
    memset(&entry, 0, sizeof(entry));
    MMAY_EXPECT(mmay_test_vm_build_interface_entry(
        &entry, record_symbol, interface_symbol, method_symbol
    ));
    MMAY_EXPECT(mmay_bytecode_verify(&entry) == MMAY_BYTECODE_VERIFY_OK);
    call = mmay_vector_at(&entry.calls, 0);
    MMAY_EXPECT(call != NULL);
    call->interface_slot = 1;
    MMAY_EXPECT(mmay_bytecode_verify(&entry)
        == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
    call->interface_slot = 0;
    block = mmay_vector_at(&entry.blocks, 0);
    interface_new = block != NULL
        ? mmay_vector_at(&block->instructions, 2)
        : NULL;
    MMAY_EXPECT(interface_new != NULL);
    interface_new->operands[0].unsigned_value = 1;
    MMAY_EXPECT(mmay_bytecode_verify(&entry)
        == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
    interface_new->operands[0].unsigned_value = 2;
    {
        MarimayBytecodeInstruction saved = *interface_new;
        MarimayBytecodeInstruction *record_new = mmay_vector_at(
            &block->instructions,
            1
        );
        MMAY_EXPECT(record_new != NULL);
        *interface_new = *record_new;
        *record_new = saved;
        MMAY_EXPECT(mmay_bytecode_verify(&entry)
            == MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
        saved = *interface_new;
        *interface_new = *record_new;
        *record_new = saved;
    }
    mmay_bytecode_function_dispose(&entry);
}

static void mmay_test_vm_interface_program_and_failures(void) {
    MarimayTestInterfaceProgram built;
    MarimayBytecodeExecution execution;
    MarimayBytecodeExecutionOptions execution_options =
        mmay_bytecode_execution_default_options();
    MarimayBytecodeRunOptions run_options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue result = mmay_test_vm_i64(-1);
    bool reached_success = false;
    size_t failure_index = 0;
    memset(&built, 0, sizeof(built));
    memset(&execution, 0, sizeof(execution));
    MMAY_EXPECT(mmay_test_vm_build_interface_program(&built));
    MMAY_EXPECT(mmay_bytecode_program_verify(&built.program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    execution_options.managed_heap.collect_before_allocation = true;
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(), execution_options
    ));
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &built.program,
        NULL,
        0,
        mmay_system_allocator(),
        run_options,
        &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64
        && result.as.i64 == 41);
    MMAY_EXPECT(execution.managed_heap.roots == NULL);
    mmay_bytecode_execution_dispose(&execution);

    {
        MarimayBytecodeProgramFunction *method = mmay_vector_at(
            &built.program.functions,
            0
        );
        MarimayBytecodeRecordLayout *layout = method != NULL
            ? mmay_vector_at(&method->body.record_layouts, 0)
            : NULL;
        MMAY_EXPECT(layout != NULL);
        layout->symbol.bytes[0] ^= 1;
        MMAY_EXPECT(mmay_bytecode_program_verify(&built.program)
            == MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH);
        layout->symbol.bytes[0] ^= 1;
    }

    for (failure_index = 0; failure_index < 96 && !reached_success;
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
            mmay_bytecode_execution_default_options()
        ));
        state.failure_index = state.attempts + failure_index;
        run_options = mmay_bytecode_run_default_options();
        run_options.execution = &failed_execution;
        result = mmay_test_vm_i64(-1);
        status = mmay_bytecode_run_program(
            &built.program,
            NULL,
            0,
            mmay_fail_allocator(&state),
            run_options,
            &result
        );
        if (status == MMAY_BYTECODE_RUN_OK) {
            reached_success = true;
            MMAY_EXPECT(result.as.i64 == 41);
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
            MMAY_EXPECT(result.as.i64 == -1);
        }
        mmay_bytecode_execution_dispose(&failed_execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
    mmay_bytecode_program_dispose(&built.program);
}

void mmay_test_vm_interfaces(void) {
    mmay_test_vm_interface_verifier();
    mmay_test_vm_interface_scan_work_accounting();
    mmay_test_vm_interface_program_and_failures();
}
