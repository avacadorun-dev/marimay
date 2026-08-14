#include "test_bytecode_vm_support.h"

typedef RudaBytecodeRunStatus (*RudaTestNominalRunner)(
    const void *subject,
    RudaAllocator allocator,
    RudaBytecodeRunOptions options,
    RudaBytecodeValue *result
);

typedef bool (*RudaTestNominalResultCheck)(
    RudaBytecodeExecution *execution,
    RudaBytecodeValue result
);

static RudaBytecodeRunStatus ruda_test_vm_run_nominal_function(
    const void *subject,
    RudaAllocator allocator,
    RudaBytecodeRunOptions options,
    RudaBytecodeValue *result
) {
    return ruda_bytecode_run(
        subject, NULL, 0, allocator, options, result
    );
}

static RudaBytecodeRunStatus ruda_test_vm_run_nominal_program(
    const void *subject,
    RudaAllocator allocator,
    RudaBytecodeRunOptions options,
    RudaBytecodeValue *result
) {
    return ruda_bytecode_run_program(
        subject, NULL, 0, allocator, options, result
    );
}

static bool ruda_test_vm_nominal_string_result(
    RudaBytecodeExecution *execution,
    RudaBytecodeValue result
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    static const unsigned char expected[] = {
        'n', 'o', 'm', 'i', 'n', 'a', 'l'
    };
    return result.type == RUDA_BYTECODE_TYPE_STRING
        && ruda_bytecode_execution_string_bytes(
            execution, result, &bytes, &length
        )
        && length == sizeof(expected)
        && memcmp(bytes, expected, sizeof(expected)) == 0;
}

static bool ruda_test_vm_nominal_true_result(
    RudaBytecodeExecution *execution,
    RudaBytecodeValue result
) {
    (void)execution;
    return result.type == RUDA_BYTECODE_TYPE_BOOL && result.as.boolean;
}

static void ruda_test_vm_nominal_failure_retry(
    const void *subject,
    RudaTestNominalRunner runner,
    RudaTestNominalResultCheck check
) {
    bool reached_success = false;
    bool observed_failure = false;
    size_t failure_offset = 0;
    for (failure_offset = 0;
        failure_offset < 128 && !reached_success;
        failure_offset += 1) {
        RudaFailAllocator state;
        RudaBytecodeExecution execution;
        RudaBytecodeExecutionOptions execution_options =
            ruda_bytecode_execution_default_options();
        RudaBytecodeRunOptions run_options =
            ruda_bytecode_run_default_options();
        RudaBytecodeValue result;
        RudaBytecodeRunStatus status;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        execution_options.managed_heap.collect_before_allocation = true;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), SIZE_MAX
        );
        RUDA_EXPECT(ruda_bytecode_execution_init(
            &execution, ruda_fail_allocator(&state), execution_options
        ));
        run_options.execution = &execution;
        state.failure_index = state.attempts + failure_offset;
        status = runner(
            subject,
            ruda_fail_allocator(&state),
            run_options,
            &result
        );
        if (status == RUDA_BYTECODE_RUN_OK) {
            reached_success = true;
            RUDA_EXPECT(check(&execution, result));
        } else {
            observed_failure = true;
            RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED);
            state.failure_index = SIZE_MAX;
            memset(&result, 0, sizeof(result));
            RUDA_EXPECT(runner(
                subject,
                ruda_fail_allocator(&state),
                run_options,
                &result
            ) == RUDA_BYTECODE_RUN_OK);
            RUDA_EXPECT(check(&execution, result));
        }
        ruda_bytecode_execution_dispose(&execution);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(observed_failure);
    RUDA_EXPECT(reached_success);
}

static bool ruda_test_vm_build_string_nominals(
    RudaBytecodeFunction *function
) {
    RudaBytecodeRecordField record_field;
    RudaBytecodeRecordArgument record_argument;
    RudaBytecodeEnumVariant enum_variant;
    RudaBytecodeEnumPayload enum_payload;
    RudaBytecodeEnumArgument enum_argument;
    RudaBytecodeRecordId record = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeEnumId enumeration = RUDA_BYTECODE_NO_ENUM;
    RudaBytecodeRecordConstructionId record_construction =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeEnumConstructionId enum_construction =
        RUDA_BYTECODE_NO_ENUM_CONSTRUCTION;
    uint32_t literal = UINT32_MAX;
    uint32_t object = UINT32_MAX;
    uint32_t record_value = UINT32_MAX;
    uint32_t enum_value = UINT32_MAX;
    uint32_t checked = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint32_t string_id = UINT32_MAX;
    uint64_t span = 0;
    const RudaSymbolId record_symbol = {{0x81}};
    const RudaSymbolId enum_symbol = {{0x82}};
    static const unsigned char text[] = {'n', 'o', 'm', 'i', 'n', 'a', 'l'};
    memset(function, 0, sizeof(*function));
    memset(&record_field, 0, sizeof(record_field));
    memset(&record_argument, 0, sizeof(record_argument));
    memset(&enum_variant, 0, sizeof(enum_variant));
    memset(&enum_payload, 0, sizeof(enum_payload));
    memset(&enum_argument, 0, sizeof(enum_argument));
    record_field.type = RUDA_BYTECODE_TYPE_STRING;
    record_field.layout = RUDA_BYTECODE_NO_LAYOUT;
    enum_variant.payload_count = 1;
    enum_payload.type = RUDA_BYTECODE_TYPE_STRING;
    enum_payload.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_function_init(
            function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_STRING,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_STRING, &literal
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &object
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_STRING, &record_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_ENUM, &enum_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_ENUM, &checked
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_STRING, &result
        ) || !ruda_bytecode_add_string_constant(
            function, text, sizeof(text), &string_id
        ) || !ruda_bytecode_add_record_layout(
            function, record_symbol, &record_field, 1, &record
        ) || !ruda_bytecode_add_enum_layout(
            function, enum_symbol, &enum_variant, 1, &enum_payload, 1,
            &enumeration
        ) || !ruda_bytecode_set_register_layout(function, object, record)
        || !ruda_bytecode_set_register_layout(
            function, enum_value, enumeration
        ) || !ruda_bytecode_set_register_layout(
            function, checked, enumeration
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    record_argument.register_id = literal;
    record_argument.type = RUDA_BYTECODE_TYPE_STRING;
    record_argument.layout = RUDA_BYTECODE_NO_LAYOUT;
    enum_argument.register_id = record_value;
    enum_argument.type = RUDA_BYTECODE_TYPE_STRING;
    enum_argument.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_add_record_construction(
            function, record, &record_argument, 1, &record_construction
        ) || !ruda_bytecode_add_enum_construction(
            function, enumeration, 0, &enum_argument, 1, &enum_construction
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_STRING,
            literal, string_id, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RECORD_NEW,
            object, record_construction, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RECORD_GET,
            record_value, object, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ENUM_NEW,
            enum_value, enum_construction, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ENUM_REQUIRE_VARIANT,
            checked, enum_value, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ENUM_GET_PAYLOAD,
            result, checked, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN, result, 0, 0, &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_vm_string_nominal_payloads(void) {
    RudaBytecodeFunction function;
    RUDA_EXPECT(ruda_test_vm_build_string_nominals(&function));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    ruda_test_vm_nominal_failure_retry(
        &function,
        ruda_test_vm_run_nominal_function,
        ruda_test_vm_nominal_string_result
    );
    ruda_bytecode_function_dispose(&function);
}

static bool ruda_test_vm_build_recursive_nominals(
    RudaBytecodeFunction *function
) {
    RudaBytecodeRecordField record_field;
    RudaBytecodeRecordArgument record_argument;
    RudaBytecodeEnumVariant variants[2];
    RudaBytecodeEnumPayload payload;
    RudaBytecodeRecordId record = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeEnumId enumeration = RUDA_BYTECODE_NO_ENUM;
    RudaBytecodeRecordConstructionId record_construction =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeEnumConstructionId enum_construction =
        RUDA_BYTECODE_NO_ENUM_CONSTRUCTION;
    uint32_t enum_value = UINT32_MAX;
    uint32_t record_value = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 20;
    const RudaSymbolId record_symbol = {{0x83}};
    const RudaSymbolId enum_symbol = {{0x84}};
    memset(function, 0, sizeof(*function));
    memset(&record_field, 0, sizeof(record_field));
    memset(&record_argument, 0, sizeof(record_argument));
    memset(variants, 0, sizeof(variants));
    memset(&payload, 0, sizeof(payload));
    record_field.type = RUDA_BYTECODE_TYPE_ENUM;
    record_field.layout = 0;
    variants[1].payload_count = 1;
    payload.type = RUDA_BYTECODE_TYPE_RECORD;
    payload.layout = 0;
    if (!ruda_bytecode_function_init(
            function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_BOOL,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_ENUM, &enum_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &record_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_ENUM, &projected
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_BOOL, &result
        ) || !ruda_bytecode_add_record_layout(
            function, record_symbol, &record_field, 1, &record
        ) || !ruda_bytecode_add_enum_layout(
            function, enum_symbol, variants, 2, &payload, 1, &enumeration
        ) || record != 0 || enumeration != 0
        || !ruda_bytecode_set_register_layout(
            function, enum_value, enumeration
        ) || !ruda_bytecode_set_register_layout(
            function, record_value, record
        ) || !ruda_bytecode_set_register_layout(
            function, projected, enumeration
        ) || !ruda_bytecode_add_block(function, &block)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    record_argument.register_id = enum_value;
    record_argument.type = RUDA_BYTECODE_TYPE_ENUM;
    record_argument.layout = enumeration;
    if (!ruda_bytecode_add_enum_construction(
            function, enumeration, 0, NULL, 0, &enum_construction
        ) || !ruda_bytecode_add_record_construction(
            function, record, &record_argument, 1, &record_construction
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ENUM_NEW,
            enum_value, enum_construction, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RECORD_NEW,
            record_value, record_construction, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RECORD_GET,
            projected, record_value, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ENUM_IS_VARIANT,
            result, projected, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN, result, 0, 0, &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_vm_recursive_nominal_descriptors(void) {
    RudaBytecodeFunction function;
    RUDA_EXPECT(ruda_test_vm_build_recursive_nominals(&function));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    ruda_test_vm_nominal_failure_retry(
        &function,
        ruda_test_vm_run_nominal_function,
        ruda_test_vm_nominal_true_result
    );
    ruda_bytecode_function_dispose(&function);
}

static bool ruda_test_vm_build_point_callee(
    RudaBytecodeFunction *function
) {
    RudaBytecodeRecordField field;
    RudaBytecodeRecordArgument argument;
    RudaBytecodeRecordId point = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordConstructionId construction =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    uint32_t scalar = UINT32_MAX;
    uint32_t value = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 40;
    const RudaSymbolId point_symbol = {{0x91}};
    memset(function, 0, sizeof(*function));
    memset(&field, 0, sizeof(field));
    memset(&argument, 0, sizeof(argument));
    field.type = RUDA_BYTECODE_TYPE_I64;
    field.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_function_init(
            function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_RECORD,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &scalar
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &value
        ) || !ruda_bytecode_add_record_layout(
            function, point_symbol, &field, 1, &point
        ) || !ruda_bytecode_set_register_layout(function, value, point)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    function->result_layout = point;
    argument.register_id = scalar;
    argument.type = RUDA_BYTECODE_TYPE_I64;
    argument.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_add_record_construction(
            function, point, &argument, 1, &construction
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_I64,
            scalar, 7, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RECORD_NEW,
            value, construction, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN, value, 0, 0, &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_holder_caller(
    RudaBytecodeFunction *function,
    RudaSymbolId callee
) {
    RudaBytecodeRecordField point_field;
    RudaBytecodeRecordField holder_field;
    RudaBytecodeRecordArgument holder_argument;
    RudaBytecodeRecordId dummy = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordId point = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordId holder = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordConstructionId holder_construction =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeCallId call_id = RUDA_BYTECODE_NO_CALL;
    RudaBytecodeCall *call = NULL;
    uint32_t point_value = UINT32_MAX;
    uint32_t holder_value = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t same = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 60;
    const RudaSymbolId dummy_symbol = {{0x90}};
    const RudaSymbolId point_symbol = {{0x91}};
    const RudaSymbolId holder_symbol = {{0x92}};
    memset(function, 0, sizeof(*function));
    memset(&point_field, 0, sizeof(point_field));
    memset(&holder_field, 0, sizeof(holder_field));
    memset(&holder_argument, 0, sizeof(holder_argument));
    point_field.type = RUDA_BYTECODE_TYPE_I64;
    point_field.layout = RUDA_BYTECODE_NO_LAYOUT;
    holder_field.type = RUDA_BYTECODE_TYPE_RECORD;
    holder_field.layout = 1;
    if (!ruda_bytecode_function_init(
            function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_BOOL,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &point_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &holder_value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &projected
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_BOOL, &same
        ) || !ruda_bytecode_add_record_layout(
            function, dummy_symbol, NULL, 0, &dummy
        ) || !ruda_bytecode_add_record_layout(
            function, point_symbol, &point_field, 1, &point
        ) || !ruda_bytecode_add_record_layout(
            function, holder_symbol, &holder_field, 1, &holder
        ) || dummy != 0 || point != 1 || holder != 2
        || !ruda_bytecode_set_register_layout(function, point_value, point)
        || !ruda_bytecode_set_register_layout(function, holder_value, holder)
        || !ruda_bytecode_set_register_layout(function, projected, point)
        || !ruda_bytecode_add_call(
            function, callee, RUDA_BYTECODE_TYPE_RECORD,
            NULL, 0, &call_id
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    call = ruda_vector_at(&function->calls, call_id);
    if (call == NULL) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    call->result_layout = point;
    holder_argument.register_id = point_value;
    holder_argument.type = RUDA_BYTECODE_TYPE_RECORD;
    holder_argument.layout = point;
    if (!ruda_bytecode_add_record_construction(
            function, holder, &holder_argument, 1, &holder_construction
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CALL_DIRECT,
            point_value, call_id, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RECORD_NEW,
            holder_value, holder_construction, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RECORD_GET,
            projected, holder_value, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RECORD_IDENTITY_SAME,
            same, point_value, projected, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN, same, 0, 0, &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_vm_cross_function_nominal_identity(void) {
    RudaBytecodeProgram program;
    RudaBytecodeFunction caller;
    RudaBytecodeFunction callee;
    RudaSymbolId caller_symbol = {{0x10}};
    RudaSymbolId callee_symbol = {{0x20}};
    uint32_t function_id = UINT32_MAX;
    RudaBytecodeProgramVerifyStatus verify_status;
    memset(&program, 0, sizeof(program));
    memset(&caller, 0, sizeof(caller));
    memset(&callee, 0, sizeof(callee));
    RUDA_EXPECT(ruda_test_vm_build_holder_caller(&caller, callee_symbol));
    RUDA_EXPECT(ruda_test_vm_build_point_callee(&callee));
    RUDA_EXPECT(ruda_bytecode_program_init(
        &program, ruda_system_allocator(), 1,
        ruda_bytecode_program_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_program_take_function(
        &program, caller_symbol, &caller, &function_id
    ) && function_id == 0);
    RUDA_EXPECT(ruda_bytecode_program_take_function(
        &program, callee_symbol, &callee, &function_id
    ) && function_id == 1);
    RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, caller_symbol));
    verify_status = ruda_bytecode_program_verify(&program);
    RUDA_EXPECT(verify_status == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    ruda_test_vm_nominal_failure_retry(
        &program,
        ruda_test_vm_run_nominal_program,
        ruda_test_vm_nominal_true_result
    );
    ruda_bytecode_program_dispose(&program);
    ruda_bytecode_function_dispose(&callee);
    ruda_bytecode_function_dispose(&caller);
}

void ruda_test_vm_nominal_identity_and_strings(void) {
    ruda_test_vm_string_nominal_payloads();
    ruda_test_vm_recursive_nominal_descriptors();
    ruda_test_vm_cross_function_nominal_identity();
}
