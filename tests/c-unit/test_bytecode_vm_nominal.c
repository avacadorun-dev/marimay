#include "test_bytecode_vm_support.h"

#include <stddef.h>

static bool ruda_test_vm_build_record_mutation(
    RudaBytecodeFunction *function
) {
    RudaBytecodeRecordField field;
    RudaBytecodeRecordArgument argument;
    RudaBytecodeRecordId record = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordConstructionId construction =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    uint32_t initial = UINT32_MAX;
    uint32_t object = UINT32_MAX;
    uint32_t replacement = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 0;
    RudaSymbolId symbol = {{0x71}};
    memset(function, 0, sizeof(*function));
    memset(&field, 0, sizeof(field));
    memset(&argument, 0, sizeof(argument));
    field.type = RUDA_BYTECODE_TYPE_I64;
    field.layout = RUDA_BYTECODE_NO_LAYOUT;
    field.is_mutable = true;
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_I64,
            1,
            NULL,
            0,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &initial
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_RECORD, &object
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &replacement
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &projected
        ) || !ruda_bytecode_add_record_layout(
            function, symbol, &field, 1, &record
        ) || !ruda_bytecode_set_register_layout(function, object, record)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    argument.register_id = initial;
    argument.type = RUDA_BYTECODE_TYPE_I64;
    argument.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_add_record_construction(
            function, record, &argument, 1, &construction
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_I64,
            initial, 7, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RECORD_NEW,
            object, construction, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_I64,
            replacement, 9, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RECORD_SET,
            replacement, object, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RECORD_GET,
            projected, object, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN,
            projected, 0, 0, &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool ruda_test_vm_build_enum_payload(
    RudaBytecodeFunction *function
) {
    RudaBytecodeEnumVariant variant;
    RudaBytecodeEnumPayload payload;
    RudaBytecodeEnumArgument argument;
    RudaBytecodeEnumId enumeration = RUDA_BYTECODE_NO_ENUM;
    RudaBytecodeEnumConstructionId construction =
        RUDA_BYTECODE_NO_ENUM_CONSTRUCTION;
    uint32_t input = UINT32_MAX;
    uint32_t value = UINT32_MAX;
    uint32_t checked = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 20;
    RudaSymbolId symbol = {{0x72}};
    memset(function, 0, sizeof(*function));
    memset(&variant, 0, sizeof(variant));
    memset(&payload, 0, sizeof(payload));
    memset(&argument, 0, sizeof(argument));
    variant.payload_count = 1;
    payload.type = RUDA_BYTECODE_TYPE_I64;
    payload.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_I64,
            1,
            NULL,
            0,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &input
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_ENUM, &value
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_ENUM, &checked
        ) || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_I64, &projected
        ) || !ruda_bytecode_add_enum_layout(
            function, symbol, &variant, 1, &payload, 1, &enumeration
        ) || !ruda_bytecode_set_register_layout(
            function, value, enumeration
        ) || !ruda_bytecode_set_register_layout(
            function, checked, enumeration
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    argument.register_id = input;
    argument.type = RUDA_BYTECODE_TYPE_I64;
    argument.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_add_enum_construction(
            function, enumeration, 0, &argument, 1, &construction
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_I64,
            input, 13, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ENUM_NEW,
            value, construction, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ENUM_REQUIRE_VARIANT,
            checked, value, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ENUM_GET_PAYLOAD,
            projected, checked, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN,
            projected, 0, 0, &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_vm_enum_provenance_rejections(void) {
    RudaBytecodeFunction function;
    RudaBytecodeInstruction *require = NULL;
    RudaBytecodeEnumArgument *argument = NULL;
    RUDA_EXPECT(ruda_test_vm_build_enum_payload(&function));
    require = ruda_test_vm_at(&function, 0, 2);
    RUDA_EXPECT(require != NULL);
    if (require != NULL) {
        require->opcode = RUDA_OPCODE_COPY;
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
        require->opcode = RUDA_OPCODE_ENUM_REQUIRE_VARIANT;
    }
    argument = ruda_vector_at(&function.enum_arguments, 0);
    RUDA_EXPECT(argument != NULL);
    if (argument != NULL) {
        argument->type = RUDA_BYTECODE_TYPE_F64;
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
        argument->type = RUDA_BYTECODE_TYPE_I64;
    }
    ruda_bytecode_function_dispose(&function);
}

static bool ruda_test_vm_build_nominal_result(
    RudaBytecodeFunction *function
) {
    RudaBytecodeType parameter = RUDA_BYTECODE_TYPE_RECORD;
    RudaBytecodeRecordId record = RUDA_BYTECODE_NO_RECORD;
    RudaSymbolId symbol = {{0x73}};
    uint32_t block = UINT32_MAX;
    uint64_t span = 40;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_RECORD,
            1,
            &parameter,
            1,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_record_layout(
            function,
            symbol,
            NULL,
            0,
            &record
        ) || !ruda_bytecode_set_register_layout(function, 0, record)
        || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RETURN,
            0,
            0,
            0,
            &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    function->result_layout = record;
    return true;
}

static bool ruda_test_vm_build_interface_metadata(
    RudaBytecodeFunction *function
) {
    RudaBytecodeType parameter = RUDA_BYTECODE_TYPE_INTERFACE;
    RudaBytecodeInterfaceId interface_id = RUDA_BYTECODE_NO_INTERFACE;
    RudaBytecodeRecordId record = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordConstructionId record_construction =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeInterfaceConstructionId construction =
        RUDA_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    RudaBytecodeCallArgument argument;
    RudaBytecodeCallId call_id = RUDA_BYTECODE_NO_CALL;
    RudaBytecodeCall *call = NULL;
    RudaSymbolId interface_symbol = {{0x74}};
    RudaSymbolId record_symbol = {{0x77}};
    RudaSymbolId method = {{0x75}};
    RudaSymbolId callee = {{0x76}};
    uint32_t scalar = UINT32_MAX;
    uint32_t object = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 50;
    memset(function, 0, sizeof(*function));
    memset(&argument, 0, sizeof(argument));
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_I64,
            1,
            &parameter,
            1,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_I64,
            &scalar
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_RECORD,
            &object
        ) || !ruda_bytecode_add_record_layout(
            function,
            record_symbol,
            NULL,
            0,
            &record
        ) || !ruda_bytecode_set_register_layout(function, object, record)
        || !ruda_bytecode_add_record_construction(
            function,
            record,
            NULL,
            0,
            &record_construction
        ) || !ruda_bytecode_add_interface_layout(
            function,
            interface_symbol,
            1,
            &interface_id
        ) || !ruda_bytecode_set_register_layout(function, 0, interface_id)
        || !ruda_bytecode_add_interface_construction(
            function,
            interface_id,
            object,
            RUDA_BYTECODE_TYPE_RECORD,
            record,
            &method,
            1,
            &construction
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    argument.register_id = 0;
    argument.type = RUDA_BYTECODE_TYPE_INTERFACE;
    argument.layout = interface_id;
    if (!ruda_bytecode_add_call(
            function,
            callee,
            RUDA_BYTECODE_TYPE_I64,
            &argument,
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
            function,
            block,
            RUDA_OPCODE_CONST_I64,
            scalar,
            3,
            0,
            &span
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RETURN,
            scalar,
            0,
            0,
            &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return construction == 0 && record_construction == 0;
}

static bool ruda_test_vm_build_nominal_caller(
    RudaBytecodeFunction *function,
    RudaSymbolId callee
) {
    RudaBytecodeType parameter = RUDA_BYTECODE_TYPE_RECORD;
    RudaBytecodeRecordId record = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeCallArgument argument;
    RudaBytecodeCallId call_id = RUDA_BYTECODE_NO_CALL;
    RudaBytecodeCall *call = NULL;
    RudaSymbolId record_symbol = {{0x73}};
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 60;
    memset(function, 0, sizeof(*function));
    memset(&argument, 0, sizeof(argument));
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_RECORD,
            1,
            &parameter,
            1,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_RECORD,
            &result
        ) || !ruda_bytecode_add_record_layout(
            function,
            record_symbol,
            NULL,
            0,
            &record
        ) || !ruda_bytecode_set_register_layout(function, 0, record)
        || !ruda_bytecode_set_register_layout(function, result, record)) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    function->result_layout = record;
    argument.register_id = 0;
    argument.type = RUDA_BYTECODE_TYPE_RECORD;
    argument.layout = record;
    if (!ruda_bytecode_add_call(
            function,
            callee,
            RUDA_BYTECODE_TYPE_RECORD,
            &argument,
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
    call->result_layout = record;
    if (!ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_CALL_DIRECT,
            result,
            call_id,
            0,
            &span
        ) || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RETURN,
            result,
            0,
            0,
            &span
        )) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void ruda_test_vm_nominal_program_signatures(void) {
    RudaBytecodeProgram program;
    RudaBytecodeProgram decoded;
    RudaBytecodeFunction caller;
    RudaBytecodeFunction callee;
    RudaOwnedBytes encoded;
    RudaSymbolId caller_symbol = {{0x10}};
    RudaSymbolId callee_symbol = {{0x20}};
    uint32_t function_id = UINT32_MAX;
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&caller, 0, sizeof(caller));
    memset(&callee, 0, sizeof(callee));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_test_vm_build_nominal_caller(&caller, callee_symbol));
    RUDA_EXPECT(ruda_test_vm_build_nominal_result(&callee));
    RUDA_EXPECT(ruda_bytecode_program_init(
        &program,
        ruda_system_allocator(),
        1,
        ruda_bytecode_program_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_program_take_function(
        &program,
        caller_symbol,
        &caller,
        &function_id
    ) && function_id == 0);
    RUDA_EXPECT(ruda_bytecode_program_take_function(
        &program,
        callee_symbol,
        &callee,
        &function_id
    ) && function_id == 1);
    RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, caller_symbol));
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_encode_program(
        &program,
        ruda_system_allocator(),
        &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_program(
        encoded.data,
        encoded.length,
        ruda_system_allocator(),
        ruda_bytecode_program_default_options(),
        &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    {
        RudaBytecodeProgramFunction *decoded_callee = ruda_vector_at(
            &decoded.functions,
            1
        );
        RudaBytecodeRecordLayout *layout = decoded_callee != NULL
            ? ruda_vector_at(&decoded_callee->body.record_layouts, 0)
            : NULL;
        RUDA_EXPECT(layout != NULL);
        if (layout != NULL) {
            layout->symbol.bytes[0] ^= 0xffU;
            RUDA_EXPECT(ruda_bytecode_program_verify(&decoded)
                == RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH);
        }
    }
    ruda_bytecode_program_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_program_dispose(&program);
    ruda_bytecode_function_dispose(&callee);
    ruda_bytecode_function_dispose(&caller);
}

static bool ruda_test_vm_build_nominal_limit_function(
    RudaBytecodeFunction *function
) {
    RudaBytecodeRecordField field;
    RudaBytecodeRecordArgument record_argument;
    RudaBytecodeEnumVariant variant;
    RudaBytecodeEnumPayload payload;
    RudaBytecodeEnumArgument enum_argument;
    RudaBytecodeRecordId record = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeRecordConstructionId record_construction =
        RUDA_BYTECODE_NO_RECORD_CONSTRUCTION;
    RudaBytecodeEnumId enum_id = RUDA_BYTECODE_NO_ENUM;
    RudaBytecodeEnumConstructionId enum_construction =
        RUDA_BYTECODE_NO_ENUM_CONSTRUCTION;
    RudaBytecodeInterfaceId interface_id = RUDA_BYTECODE_NO_INTERFACE;
    RudaBytecodeInterfaceConstructionId interface_construction =
        RUDA_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    RudaSymbolId record_symbol = {{0x81}};
    RudaSymbolId enum_symbol = {{0x82}};
    RudaSymbolId interface_symbol = {{0x83}};
    RudaSymbolId method_symbol = {{0x84}};
    uint32_t scalar = UINT32_MAX;
    uint32_t object = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 70;
    memset(function, 0, sizeof(*function));
    memset(&field, 0, sizeof(field));
    memset(&record_argument, 0, sizeof(record_argument));
    memset(&variant, 0, sizeof(variant));
    memset(&payload, 0, sizeof(payload));
    memset(&enum_argument, 0, sizeof(enum_argument));
    field.type = RUDA_BYTECODE_TYPE_I64;
    field.layout = RUDA_BYTECODE_NO_LAYOUT;
    record_argument.type = RUDA_BYTECODE_TYPE_I64;
    record_argument.layout = RUDA_BYTECODE_NO_LAYOUT;
    variant.payload_count = 1;
    payload.type = RUDA_BYTECODE_TYPE_I64;
    payload.layout = RUDA_BYTECODE_NO_LAYOUT;
    enum_argument.type = RUDA_BYTECODE_TYPE_I64;
    enum_argument.layout = RUDA_BYTECODE_NO_LAYOUT;
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
        ) || !ruda_bytecode_add_record_layout(
            function, record_symbol, &field, 1, &record
        ) || !ruda_bytecode_set_register_layout(function, object, record)) {
        goto failure;
    }
    record_argument.register_id = scalar;
    enum_argument.register_id = scalar;
    if (!ruda_bytecode_add_record_construction(
            function, record, &record_argument, 1, &record_construction
        ) || !ruda_bytecode_add_enum_layout(
            function, enum_symbol, &variant, 1, &payload, 1, &enum_id
        ) || !ruda_bytecode_add_enum_construction(
            function, enum_id, 0, &enum_argument, 1, &enum_construction
        ) || !ruda_bytecode_add_interface_layout(
            function, interface_symbol, 1, &interface_id
        ) || !ruda_bytecode_add_interface_construction(
            function,
            interface_id,
            object,
            RUDA_BYTECODE_TYPE_RECORD,
            record,
            &method_symbol,
            1,
            &interface_construction
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_I64, scalar, 7, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN, scalar, 0, 0, &span
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_bytecode_function_dispose(function);
    return false;
}

static void ruda_test_vm_nominal_program_limits(void) {
    const size_t offsets[] = {
        offsetof(RudaBytecodeProgramOptions, max_total_record_layouts),
        offsetof(RudaBytecodeProgramOptions, max_total_record_fields),
        offsetof(RudaBytecodeProgramOptions, max_total_record_constructions),
        offsetof(RudaBytecodeProgramOptions, max_total_record_arguments),
        offsetof(RudaBytecodeProgramOptions, max_total_enum_layouts),
        offsetof(RudaBytecodeProgramOptions, max_total_enum_variants),
        offsetof(RudaBytecodeProgramOptions, max_total_enum_payloads),
        offsetof(RudaBytecodeProgramOptions, max_total_enum_constructions),
        offsetof(RudaBytecodeProgramOptions, max_total_enum_arguments),
        offsetof(RudaBytecodeProgramOptions, max_total_interface_layouts),
        offsetof(RudaBytecodeProgramOptions, max_total_interface_constructions),
        offsetof(RudaBytecodeProgramOptions, max_total_interface_methods)
    };
    RudaBytecodeProgram program;
    RudaBytecodeFunction first;
    RudaBytecodeFunction second;
    RudaOwnedBytes encoded;
    RudaSymbolId first_symbol = {{0x10}};
    RudaSymbolId second_symbol = {{0x20}};
    uint32_t function_id = UINT32_MAX;
    size_t index = 0;
    memset(&program, 0, sizeof(program));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_test_vm_build_nominal_limit_function(&first));
    RUDA_EXPECT(ruda_test_vm_build_nominal_limit_function(&second));
    RUDA_EXPECT(ruda_bytecode_program_init(
        &program,
        ruda_system_allocator(),
        1,
        ruda_bytecode_program_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_program_take_function(
        &program, first_symbol, &first, &function_id
    ));
    RUDA_EXPECT(ruda_bytecode_program_take_function(
        &program, second_symbol, &second, &function_id
    ));
    RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, first_symbol));
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    for (index = 0; index < sizeof(offsets) / sizeof(offsets[0]); index += 1) {
        size_t *limit = (size_t *)((unsigned char *)&program.options
            + offsets[index]);
        size_t saved = *limit;
        *limit = 1;
        RUDA_EXPECT(ruda_bytecode_program_verify(&program)
            == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
        *limit = saved;
    }
    RUDA_EXPECT(ruda_bytecode_encode_program(
        &program, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    for (index = 0; index < sizeof(offsets) / sizeof(offsets[0]); index += 1) {
        RudaBytecodeProgramOptions limited =
            ruda_bytecode_program_default_options();
        RudaBytecodeProgram decoded;
        RudaFailAllocator tracking;
        size_t *limit = (size_t *)((unsigned char *)&limited + offsets[index]);
        memset(&decoded, 0, sizeof(decoded));
        *limit = 1;
        ruda_fail_allocator_init(&tracking, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_bytecode_decode_program(
            encoded.data,
            encoded.length,
            ruda_fail_allocator(&tracking),
            limited,
            &decoded
        ) == RUDA_BYTECODE_CODEC_LIMIT_REACHED);
        ruda_bytecode_program_dispose(&decoded);
        RUDA_EXPECT(tracking.live_allocations == 0);
        RUDA_EXPECT(tracking.live_bytes == 0);
    }
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_program_dispose(&program);
    ruda_bytecode_function_dispose(&second);
    ruda_bytecode_function_dispose(&first);
}

static void ruda_test_vm_nominal_round_trip(
    RudaBytecodeFunction *function
) {
    RudaOwnedBytes encoded;
    RudaOwnedBytes encoded_again;
    RudaBytecodeFunction decoded;
    size_t index = 0;
    memset(&encoded, 0, sizeof(encoded));
    memset(&encoded_again, 0, sizeof(encoded_again));
    memset(&decoded, 0, sizeof(decoded));
    RUDA_EXPECT(ruda_bytecode_verify(function) == RUDA_BYTECODE_VERIFY_OK);
    {
        bool reached_success = false;
        size_t failure_index = 0;
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaOwnedBytes attempted;
            RudaBytecodeCodecStatus status;
            memset(&attempted, 0, sizeof(attempted));
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            status = ruda_bytecode_encode_function(
                function,
                ruda_fail_allocator(&state),
                &attempted
            );
            if (status == RUDA_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            }
            ruda_owned_bytes_dispose(&attempted);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
    }
    RUDA_EXPECT(ruda_bytecode_encode_function(
        function,
        ruda_system_allocator(),
        &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(encoded.length > 10 && encoded.data[9] < 0x80
        && (encoded.data[9] & (1U << 4)) != 0);
    for (index = 0; index < encoded.length; index += 1) {
        RUDA_EXPECT(ruda_bytecode_decode_function(
            encoded.data,
            index,
            ruda_system_allocator(),
            ruda_bytecode_function_default_options(),
            &decoded
        ) == RUDA_BYTECODE_CODEC_TRUNCATED);
        RUDA_EXPECT(!decoded.initialized);
    }
    RUDA_EXPECT(ruda_bytecode_decode_function(
        encoded.data,
        encoded.length,
        ruda_system_allocator(),
        ruda_bytecode_function_default_options(),
        &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &decoded,
        ruda_system_allocator(),
        &encoded_again
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(encoded.length == encoded_again.length
        && memcmp(encoded.data, encoded_again.data, encoded.length) == 0);
    ruda_owned_bytes_dispose(&encoded_again);
    ruda_bytecode_function_dispose(&decoded);
    {
        unsigned char saved_feature = encoded.data[9];
        encoded.data[9] = (unsigned char)(saved_feature & ~(1U << 4));
        RUDA_EXPECT(ruda_bytecode_decode_function(
            encoded.data,
            encoded.length,
            ruda_system_allocator(),
            ruda_bytecode_function_default_options(),
            &decoded
        ) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
        RUDA_EXPECT(!decoded.initialized);
        encoded.data[9] = saved_feature;
    }
    {
        bool reached_success = false;
        size_t failure_index = 0;
        for (failure_index = 0; failure_index < 128 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeCodecStatus status;
            memset(&decoded, 0, sizeof(decoded));
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            status = ruda_bytecode_decode_function(
                encoded.data,
                encoded.length,
                ruda_fail_allocator(&state),
                ruda_bytecode_function_default_options(),
                &decoded
            );
            if (status == RUDA_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            }
            ruda_bytecode_function_dispose(&decoded);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
    }
    ruda_owned_bytes_dispose(&encoded);
}

void ruda_test_vm_nominal_codec(void) {
    RudaBytecodeFunction function;
    RudaTestBuffer disassembly;
    RUDA_EXPECT(ruda_test_vm_build_record_mutation(&function));
    ruda_test_buffer_init(&disassembly);
    RUDA_EXPECT(ruda_bytecode_disassemble(
        &function,
        ruda_test_buffer_writer(&disassembly)
    ) == RUDA_BYTECODE_DISASSEMBLE_OK);
    RUDA_EXPECT(strstr(disassembly.bytes, "record_new r1, rc0") != NULL);
    ruda_test_vm_nominal_round_trip(&function);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_vm_build_enum_payload(&function));
    ruda_test_buffer_init(&disassembly);
    RUDA_EXPECT(ruda_bytecode_disassemble(
        &function,
        ruda_test_buffer_writer(&disassembly)
    ) == RUDA_BYTECODE_DISASSEMBLE_OK);
    RUDA_EXPECT(strstr(disassembly.bytes, "enum_new r1, ec0") != NULL);
    ruda_test_vm_nominal_round_trip(&function);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_vm_build_nominal_result(&function));
    ruda_test_vm_nominal_round_trip(&function);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_vm_build_interface_metadata(&function));
    ruda_test_vm_nominal_round_trip(&function);
    ruda_bytecode_function_dispose(&function);

    ruda_test_vm_nominal_program_signatures();
    ruda_test_vm_nominal_program_limits();
}

void ruda_test_vm_nominal_values_and_verifier(void) {
    RudaBytecodeFunction function;
    RudaBytecodeExecution execution;
    RudaBytecodeRunOptions options;
    RudaBytecodeValue result;
    RudaBytecodeRecordField *field = NULL;
    RudaBytecodeRecordArgument *argument = NULL;
    memset(&execution, 0, sizeof(execution));
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution,
        ruda_system_allocator(),
        ruda_bytecode_execution_default_options()
    ));
    options = ruda_bytecode_run_default_options();
    options.execution = &execution;

    RUDA_EXPECT(ruda_test_vm_build_record_mutation(&function));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    result = ruda_test_vm_i64(-1);
    RUDA_EXPECT(ruda_bytecode_run(
        &function, NULL, 0, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 9);
    field = ruda_vector_at(&function.record_fields, 0);
    RUDA_EXPECT(field != NULL);
    if (field != NULL) {
        field->is_mutable = false;
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
        field->is_mutable = true;
    }
    argument = ruda_vector_at(&function.record_arguments, 0);
    RUDA_EXPECT(argument != NULL);
    if (argument != NULL) {
        argument->type = RUDA_BYTECODE_TYPE_F64;
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
        argument->type = RUDA_BYTECODE_TYPE_I64;
    }
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_vm_build_enum_payload(&function));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    result = ruda_test_vm_i64(-1);
    RUDA_EXPECT(ruda_bytecode_run(
        &function, NULL, 0, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 13);
    ruda_bytecode_function_dispose(&function);

    ruda_test_vm_enum_provenance_rejections();
    ruda_bytecode_execution_dispose(&execution);
}
