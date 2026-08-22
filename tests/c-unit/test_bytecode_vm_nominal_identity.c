#include "test_bytecode_vm_support.h"

typedef MarimayBytecodeRunStatus (*MarimayTestNominalRunner)(
    const void *subject,
    MarimayAllocator allocator,
    MarimayBytecodeRunOptions options,
    MarimayBytecodeValue *result
);

typedef bool (*MarimayTestNominalResultCheck)(
    MarimayBytecodeExecution *execution,
    MarimayBytecodeValue result
);

static MarimayBytecodeRunStatus mmay_test_vm_run_nominal_function(
    const void *subject,
    MarimayAllocator allocator,
    MarimayBytecodeRunOptions options,
    MarimayBytecodeValue *result
) {
    return mmay_bytecode_run(
        subject, NULL, 0, allocator, options, result
    );
}

static MarimayBytecodeRunStatus mmay_test_vm_run_nominal_program(
    const void *subject,
    MarimayAllocator allocator,
    MarimayBytecodeRunOptions options,
    MarimayBytecodeValue *result
) {
    return mmay_bytecode_run_program(
        subject, NULL, 0, allocator, options, result
    );
}

static bool mmay_test_vm_nominal_string_result(
    MarimayBytecodeExecution *execution,
    MarimayBytecodeValue result
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    static const unsigned char expected[] = {
        'n', 'o', 'm', 'i', 'n', 'a', 'l'
    };
    return result.type == MMAY_BYTECODE_TYPE_STRING
        && mmay_bytecode_execution_string_bytes(
            execution, result, &bytes, &length
        )
        && length == sizeof(expected)
        && memcmp(bytes, expected, sizeof(expected)) == 0;
}

static bool mmay_test_vm_nominal_true_result(
    MarimayBytecodeExecution *execution,
    MarimayBytecodeValue result
) {
    (void)execution;
    return result.type == MMAY_BYTECODE_TYPE_BOOL && result.as.boolean;
}

static void mmay_test_vm_nominal_failure_retry(
    const void *subject,
    MarimayTestNominalRunner runner,
    MarimayTestNominalResultCheck check
) {
    bool reached_success = false;
    bool observed_failure = false;
    size_t failure_offset = 0;
    for (failure_offset = 0;
        failure_offset < 128 && !reached_success;
        failure_offset += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeExecution execution;
        MarimayBytecodeExecutionOptions execution_options =
            mmay_bytecode_execution_default_options();
        MarimayBytecodeRunOptions run_options =
            mmay_bytecode_run_default_options();
        MarimayBytecodeValue result;
        MarimayBytecodeRunStatus status;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        execution_options.managed_heap.collect_before_allocation = true;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), SIZE_MAX
        );
        MMAY_EXPECT(mmay_bytecode_execution_init(
            &execution, mmay_fail_allocator(&state), execution_options
        ));
        run_options.execution = &execution;
        state.failure_index = state.attempts + failure_offset;
        status = runner(
            subject,
            mmay_fail_allocator(&state),
            run_options,
            &result
        );
        if (status == MMAY_BYTECODE_RUN_OK) {
            reached_success = true;
            MMAY_EXPECT(check(&execution, result));
        } else {
            observed_failure = true;
            MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
            state.failure_index = SIZE_MAX;
            memset(&result, 0, sizeof(result));
            MMAY_EXPECT(runner(
                subject,
                mmay_fail_allocator(&state),
                run_options,
                &result
            ) == MMAY_BYTECODE_RUN_OK);
            MMAY_EXPECT(check(&execution, result));
        }
        mmay_bytecode_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(observed_failure);
    MMAY_EXPECT(reached_success);
}

static bool mmay_test_vm_build_string_nominals(
    MarimayBytecodeFunction *function
) {
    MarimayBytecodeRecordField record_field;
    MarimayBytecodeRecordArgument record_argument;
    MarimayBytecodeEnumVariant enum_variant;
    MarimayBytecodeEnumPayload enum_payload;
    MarimayBytecodeEnumArgument enum_argument;
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeEnumId enumeration = MMAY_BYTECODE_NO_ENUM;
    MarimayBytecodeRecordConstructionId record_construction =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeEnumConstructionId enum_construction =
        MMAY_BYTECODE_NO_ENUM_CONSTRUCTION;
    uint32_t literal = UINT32_MAX;
    uint32_t object = UINT32_MAX;
    uint32_t record_value = UINT32_MAX;
    uint32_t enum_value = UINT32_MAX;
    uint32_t checked = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint32_t string_id = UINT32_MAX;
    uint64_t span = 0;
    const MarimaySymbolId record_symbol = {{0x81}};
    const MarimaySymbolId enum_symbol = {{0x82}};
    static const unsigned char text[] = {'n', 'o', 'm', 'i', 'n', 'a', 'l'};
    memset(function, 0, sizeof(*function));
    memset(&record_field, 0, sizeof(record_field));
    memset(&record_argument, 0, sizeof(record_argument));
    memset(&enum_variant, 0, sizeof(enum_variant));
    memset(&enum_payload, 0, sizeof(enum_payload));
    memset(&enum_argument, 0, sizeof(enum_argument));
    record_field.type = MMAY_BYTECODE_TYPE_STRING;
    record_field.layout = MMAY_BYTECODE_NO_LAYOUT;
    enum_variant.payload_count = 1;
    enum_payload.type = MMAY_BYTECODE_TYPE_STRING;
    enum_payload.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_function_init(
            function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_STRING,
            1, NULL, 0, mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_STRING, &literal
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &object
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_STRING, &record_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ENUM, &enum_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ENUM, &checked
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_STRING, &result
        ) || !mmay_bytecode_add_string_constant(
            function, text, sizeof(text), &string_id
        ) || !mmay_bytecode_add_record_layout(
            function, record_symbol, &record_field, 1, &record
        ) || !mmay_bytecode_add_enum_layout(
            function, enum_symbol, &enum_variant, 1, &enum_payload, 1,
            &enumeration
        ) || !mmay_bytecode_set_register_layout(function, object, record)
        || !mmay_bytecode_set_register_layout(
            function, enum_value, enumeration
        ) || !mmay_bytecode_set_register_layout(
            function, checked, enumeration
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    record_argument.register_id = literal;
    record_argument.type = MMAY_BYTECODE_TYPE_STRING;
    record_argument.layout = MMAY_BYTECODE_NO_LAYOUT;
    enum_argument.register_id = record_value;
    enum_argument.type = MMAY_BYTECODE_TYPE_STRING;
    enum_argument.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_add_record_construction(
            function, record, &record_argument, 1, &record_construction
        ) || !mmay_bytecode_add_enum_construction(
            function, enumeration, 0, &enum_argument, 1, &enum_construction
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_STRING,
            literal, string_id, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RECORD_NEW,
            object, record_construction, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RECORD_GET,
            record_value, object, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ENUM_NEW,
            enum_value, enum_construction, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ENUM_REQUIRE_VARIANT,
            checked, enum_value, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ENUM_GET_PAYLOAD,
            result, checked, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, result, 0, 0, &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void mmay_test_vm_string_nominal_payloads(void) {
    MarimayBytecodeFunction function;
    MMAY_EXPECT(mmay_test_vm_build_string_nominals(&function));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    mmay_test_vm_nominal_failure_retry(
        &function,
        mmay_test_vm_run_nominal_function,
        mmay_test_vm_nominal_string_result
    );
    mmay_bytecode_function_dispose(&function);
}

static bool mmay_test_vm_build_recursive_nominals(
    MarimayBytecodeFunction *function
) {
    MarimayBytecodeRecordField record_field;
    MarimayBytecodeRecordArgument record_argument;
    MarimayBytecodeEnumVariant variants[2];
    MarimayBytecodeEnumPayload payload;
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeEnumId enumeration = MMAY_BYTECODE_NO_ENUM;
    MarimayBytecodeRecordConstructionId record_construction =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeEnumConstructionId enum_construction =
        MMAY_BYTECODE_NO_ENUM_CONSTRUCTION;
    uint32_t enum_value = UINT32_MAX;
    uint32_t record_value = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 20;
    const MarimaySymbolId record_symbol = {{0x83}};
    const MarimaySymbolId enum_symbol = {{0x84}};
    memset(function, 0, sizeof(*function));
    memset(&record_field, 0, sizeof(record_field));
    memset(&record_argument, 0, sizeof(record_argument));
    memset(variants, 0, sizeof(variants));
    memset(&payload, 0, sizeof(payload));
    record_field.type = MMAY_BYTECODE_TYPE_ENUM;
    record_field.layout = 0;
    variants[1].payload_count = 1;
    payload.type = MMAY_BYTECODE_TYPE_RECORD;
    payload.layout = 0;
    if (!mmay_bytecode_function_init(
            function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_BOOL,
            1, NULL, 0, mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ENUM, &enum_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &record_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ENUM, &projected
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_BOOL, &result
        ) || !mmay_bytecode_add_record_layout(
            function, record_symbol, &record_field, 1, &record
        ) || !mmay_bytecode_add_enum_layout(
            function, enum_symbol, variants, 2, &payload, 1, &enumeration
        ) || record != 0 || enumeration != 0
        || !mmay_bytecode_set_register_layout(
            function, enum_value, enumeration
        ) || !mmay_bytecode_set_register_layout(
            function, record_value, record
        ) || !mmay_bytecode_set_register_layout(
            function, projected, enumeration
        ) || !mmay_bytecode_add_block(function, &block)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    record_argument.register_id = enum_value;
    record_argument.type = MMAY_BYTECODE_TYPE_ENUM;
    record_argument.layout = enumeration;
    if (!mmay_bytecode_add_enum_construction(
            function, enumeration, 0, NULL, 0, &enum_construction
        ) || !mmay_bytecode_add_record_construction(
            function, record, &record_argument, 1, &record_construction
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ENUM_NEW,
            enum_value, enum_construction, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RECORD_NEW,
            record_value, record_construction, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RECORD_GET,
            projected, record_value, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ENUM_IS_VARIANT,
            result, projected, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, result, 0, 0, &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void mmay_test_vm_recursive_nominal_descriptors(void) {
    MarimayBytecodeFunction function;
    MMAY_EXPECT(mmay_test_vm_build_recursive_nominals(&function));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    mmay_test_vm_nominal_failure_retry(
        &function,
        mmay_test_vm_run_nominal_function,
        mmay_test_vm_nominal_true_result
    );
    mmay_bytecode_function_dispose(&function);
}

static bool mmay_test_vm_build_point_callee(
    MarimayBytecodeFunction *function
) {
    MarimayBytecodeRecordField field;
    MarimayBytecodeRecordArgument argument;
    MarimayBytecodeRecordId point = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordConstructionId construction =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    uint32_t scalar = UINT32_MAX;
    uint32_t value = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 40;
    const MarimaySymbolId point_symbol = {{0x91}};
    memset(function, 0, sizeof(*function));
    memset(&field, 0, sizeof(field));
    memset(&argument, 0, sizeof(argument));
    field.type = MMAY_BYTECODE_TYPE_I64;
    field.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_function_init(
            function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_RECORD,
            1, NULL, 0, mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &scalar
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &value
        ) || !mmay_bytecode_add_record_layout(
            function, point_symbol, &field, 1, &point
        ) || !mmay_bytecode_set_register_layout(function, value, point)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    function->result_layout = point;
    argument.register_id = scalar;
    argument.type = MMAY_BYTECODE_TYPE_I64;
    argument.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_add_record_construction(
            function, point, &argument, 1, &construction
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_I64,
            scalar, 7, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RECORD_NEW,
            value, construction, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, value, 0, 0, &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_vm_build_holder_caller(
    MarimayBytecodeFunction *function,
    MarimaySymbolId callee
) {
    MarimayBytecodeRecordField point_field;
    MarimayBytecodeRecordField holder_field;
    MarimayBytecodeRecordArgument holder_argument;
    MarimayBytecodeRecordId dummy = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordId point = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordId holder = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordConstructionId holder_construction =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeCallId call_id = MMAY_BYTECODE_NO_CALL;
    MarimayBytecodeCall *call = NULL;
    uint32_t point_value = UINT32_MAX;
    uint32_t holder_value = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t same = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 60;
    const MarimaySymbolId dummy_symbol = {{0x90}};
    const MarimaySymbolId point_symbol = {{0x91}};
    const MarimaySymbolId holder_symbol = {{0x92}};
    memset(function, 0, sizeof(*function));
    memset(&point_field, 0, sizeof(point_field));
    memset(&holder_field, 0, sizeof(holder_field));
    memset(&holder_argument, 0, sizeof(holder_argument));
    point_field.type = MMAY_BYTECODE_TYPE_I64;
    point_field.layout = MMAY_BYTECODE_NO_LAYOUT;
    holder_field.type = MMAY_BYTECODE_TYPE_RECORD;
    holder_field.layout = 1;
    if (!mmay_bytecode_function_init(
            function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_BOOL,
            1, NULL, 0, mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &point_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &holder_value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &projected
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_BOOL, &same
        ) || !mmay_bytecode_add_record_layout(
            function, dummy_symbol, NULL, 0, &dummy
        ) || !mmay_bytecode_add_record_layout(
            function, point_symbol, &point_field, 1, &point
        ) || !mmay_bytecode_add_record_layout(
            function, holder_symbol, &holder_field, 1, &holder
        ) || dummy != 0 || point != 1 || holder != 2
        || !mmay_bytecode_set_register_layout(function, point_value, point)
        || !mmay_bytecode_set_register_layout(function, holder_value, holder)
        || !mmay_bytecode_set_register_layout(function, projected, point)
        || !mmay_bytecode_add_call(
            function, callee, MMAY_BYTECODE_TYPE_RECORD,
            NULL, 0, &call_id
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    call = mmay_vector_at(&function->calls, call_id);
    if (call == NULL) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    call->result_layout = point;
    holder_argument.register_id = point_value;
    holder_argument.type = MMAY_BYTECODE_TYPE_RECORD;
    holder_argument.layout = point;
    if (!mmay_bytecode_add_record_construction(
            function, holder, &holder_argument, 1, &holder_construction
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CALL_DIRECT,
            point_value, call_id, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RECORD_NEW,
            holder_value, holder_construction, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RECORD_GET,
            projected, holder_value, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RECORD_IDENTITY_SAME,
            same, point_value, projected, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, same, 0, 0, &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void mmay_test_vm_cross_function_nominal_identity(void) {
    MarimayBytecodeProgram program;
    MarimayBytecodeFunction caller;
    MarimayBytecodeFunction callee;
    MarimaySymbolId caller_symbol = {{0x10}};
    MarimaySymbolId callee_symbol = {{0x20}};
    uint32_t function_id = UINT32_MAX;
    MarimayBytecodeProgramVerifyStatus verify_status;
    memset(&program, 0, sizeof(program));
    memset(&caller, 0, sizeof(caller));
    memset(&callee, 0, sizeof(callee));
    MMAY_EXPECT(mmay_test_vm_build_holder_caller(&caller, callee_symbol));
    MMAY_EXPECT(mmay_test_vm_build_point_callee(&callee));
    MMAY_EXPECT(mmay_bytecode_program_init(
        &program, mmay_system_allocator(), 1,
        mmay_bytecode_program_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_program_take_function(
        &program, caller_symbol, &caller, &function_id
    ) && function_id == 0);
    MMAY_EXPECT(mmay_bytecode_program_take_function(
        &program, callee_symbol, &callee, &function_id
    ) && function_id == 1);
    MMAY_EXPECT(mmay_bytecode_program_set_entry(&program, caller_symbol));
    verify_status = mmay_bytecode_program_verify(&program);
    MMAY_EXPECT(verify_status == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    mmay_test_vm_nominal_failure_retry(
        &program,
        mmay_test_vm_run_nominal_program,
        mmay_test_vm_nominal_true_result
    );
    mmay_bytecode_program_dispose(&program);
    mmay_bytecode_function_dispose(&callee);
    mmay_bytecode_function_dispose(&caller);
}

void mmay_test_vm_nominal_identity_and_strings(void) {
    mmay_test_vm_string_nominal_payloads();
    mmay_test_vm_recursive_nominal_descriptors();
    mmay_test_vm_cross_function_nominal_identity();
}
