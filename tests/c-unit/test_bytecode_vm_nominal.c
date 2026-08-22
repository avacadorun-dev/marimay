#include "test_bytecode_vm_support.h"

#include <stddef.h>

static bool mmay_test_vm_build_record_mutation(
    MarimayBytecodeFunction *function
) {
    MarimayBytecodeRecordField field;
    MarimayBytecodeRecordArgument argument;
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordConstructionId construction =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    uint32_t initial = UINT32_MAX;
    uint32_t object = UINT32_MAX;
    uint32_t replacement = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 0;
    MarimaySymbolId symbol = {{0x71}};
    memset(function, 0, sizeof(*function));
    memset(&field, 0, sizeof(field));
    memset(&argument, 0, sizeof(argument));
    field.type = MMAY_BYTECODE_TYPE_I64;
    field.layout = MMAY_BYTECODE_NO_LAYOUT;
    field.is_mutable = true;
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_I64,
            1,
            NULL,
            0,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &initial
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_RECORD, &object
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &replacement
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &projected
        ) || !mmay_bytecode_add_record_layout(
            function, symbol, &field, 1, &record
        ) || !mmay_bytecode_set_register_layout(function, object, record)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    argument.register_id = initial;
    argument.type = MMAY_BYTECODE_TYPE_I64;
    argument.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_add_record_construction(
            function, record, &argument, 1, &construction
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_I64,
            initial, 7, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RECORD_NEW,
            object, construction, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_I64,
            replacement, 9, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RECORD_SET,
            replacement, object, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RECORD_GET,
            projected, object, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN,
            projected, 0, 0, &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static bool mmay_test_vm_build_enum_payload(
    MarimayBytecodeFunction *function
) {
    MarimayBytecodeEnumVariant variant;
    MarimayBytecodeEnumPayload payload;
    MarimayBytecodeEnumArgument argument;
    MarimayBytecodeEnumId enumeration = MMAY_BYTECODE_NO_ENUM;
    MarimayBytecodeEnumConstructionId construction =
        MMAY_BYTECODE_NO_ENUM_CONSTRUCTION;
    uint32_t input = UINT32_MAX;
    uint32_t value = UINT32_MAX;
    uint32_t checked = UINT32_MAX;
    uint32_t projected = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 20;
    MarimaySymbolId symbol = {{0x72}};
    memset(function, 0, sizeof(*function));
    memset(&variant, 0, sizeof(variant));
    memset(&payload, 0, sizeof(payload));
    memset(&argument, 0, sizeof(argument));
    variant.payload_count = 1;
    payload.type = MMAY_BYTECODE_TYPE_I64;
    payload.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_I64,
            1,
            NULL,
            0,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &input
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ENUM, &value
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ENUM, &checked
        ) || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_I64, &projected
        ) || !mmay_bytecode_add_enum_layout(
            function, symbol, &variant, 1, &payload, 1, &enumeration
        ) || !mmay_bytecode_set_register_layout(
            function, value, enumeration
        ) || !mmay_bytecode_set_register_layout(
            function, checked, enumeration
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    argument.register_id = input;
    argument.type = MMAY_BYTECODE_TYPE_I64;
    argument.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_add_enum_construction(
            function, enumeration, 0, &argument, 1, &construction
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_I64,
            input, 13, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ENUM_NEW,
            value, construction, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ENUM_REQUIRE_VARIANT,
            checked, value, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ENUM_GET_PAYLOAD,
            projected, checked, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN,
            projected, 0, 0, &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void mmay_test_vm_enum_provenance_rejections(void) {
    MarimayBytecodeFunction function;
    MarimayBytecodeInstruction *require = NULL;
    MarimayBytecodeEnumArgument *argument = NULL;
    MMAY_EXPECT(mmay_test_vm_build_enum_payload(&function));
    require = mmay_test_vm_at(&function, 0, 2);
    MMAY_EXPECT(require != NULL);
    if (require != NULL) {
        require->opcode = MMAY_OPCODE_COPY;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
        require->opcode = MMAY_OPCODE_ENUM_REQUIRE_VARIANT;
    }
    argument = mmay_vector_at(&function.enum_arguments, 0);
    MMAY_EXPECT(argument != NULL);
    if (argument != NULL) {
        argument->type = MMAY_BYTECODE_TYPE_F64;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        argument->type = MMAY_BYTECODE_TYPE_I64;
    }
    mmay_bytecode_function_dispose(&function);
}

static bool mmay_test_vm_build_nominal_result(
    MarimayBytecodeFunction *function
) {
    MarimayBytecodeType parameter = MMAY_BYTECODE_TYPE_RECORD;
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    MarimaySymbolId symbol = {{0x73}};
    uint32_t block = UINT32_MAX;
    uint64_t span = 40;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_RECORD,
            1,
            &parameter,
            1,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_record_layout(
            function,
            symbol,
            NULL,
            0,
            &record
        ) || !mmay_bytecode_set_register_layout(function, 0, record)
        || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RETURN,
            0,
            0,
            0,
            &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    function->result_layout = record;
    return true;
}

static bool mmay_test_vm_build_interface_metadata(
    MarimayBytecodeFunction *function
) {
    MarimayBytecodeType parameter = MMAY_BYTECODE_TYPE_INTERFACE;
    MarimayBytecodeInterfaceId interface_id = MMAY_BYTECODE_NO_INTERFACE;
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordConstructionId record_construction =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeInterfaceConstructionId construction =
        MMAY_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    MarimayBytecodeCallArgument argument;
    MarimayBytecodeCallId call_id = MMAY_BYTECODE_NO_CALL;
    MarimayBytecodeCall *call = NULL;
    MarimaySymbolId interface_symbol = {{0x74}};
    MarimaySymbolId record_symbol = {{0x77}};
    MarimaySymbolId method = {{0x75}};
    MarimaySymbolId callee = {{0x76}};
    uint32_t scalar = UINT32_MAX;
    uint32_t object = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 50;
    memset(function, 0, sizeof(*function));
    memset(&argument, 0, sizeof(argument));
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_I64,
            1,
            &parameter,
            1,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_I64,
            &scalar
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_RECORD,
            &object
        ) || !mmay_bytecode_add_record_layout(
            function,
            record_symbol,
            NULL,
            0,
            &record
        ) || !mmay_bytecode_set_register_layout(function, object, record)
        || !mmay_bytecode_add_record_construction(
            function,
            record,
            NULL,
            0,
            &record_construction
        ) || !mmay_bytecode_add_interface_layout(
            function,
            interface_symbol,
            1,
            &interface_id
        ) || !mmay_bytecode_set_register_layout(function, 0, interface_id)
        || !mmay_bytecode_add_interface_construction(
            function,
            interface_id,
            object,
            MMAY_BYTECODE_TYPE_RECORD,
            record,
            &method,
            1,
            &construction
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    argument.register_id = 0;
    argument.type = MMAY_BYTECODE_TYPE_INTERFACE;
    argument.layout = interface_id;
    if (!mmay_bytecode_add_call(
            function,
            callee,
            MMAY_BYTECODE_TYPE_I64,
            &argument,
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
            function,
            block,
            MMAY_OPCODE_CONST_I64,
            scalar,
            3,
            0,
            &span
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RETURN,
            scalar,
            0,
            0,
            &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return construction == 0 && record_construction == 0;
}

static bool mmay_test_vm_build_nominal_caller(
    MarimayBytecodeFunction *function,
    MarimaySymbolId callee
) {
    MarimayBytecodeType parameter = MMAY_BYTECODE_TYPE_RECORD;
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeCallArgument argument;
    MarimayBytecodeCallId call_id = MMAY_BYTECODE_NO_CALL;
    MarimayBytecodeCall *call = NULL;
    MarimaySymbolId record_symbol = {{0x73}};
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 60;
    memset(function, 0, sizeof(*function));
    memset(&argument, 0, sizeof(argument));
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_RECORD,
            1,
            &parameter,
            1,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_RECORD,
            &result
        ) || !mmay_bytecode_add_record_layout(
            function,
            record_symbol,
            NULL,
            0,
            &record
        ) || !mmay_bytecode_set_register_layout(function, 0, record)
        || !mmay_bytecode_set_register_layout(function, result, record)) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    function->result_layout = record;
    argument.register_id = 0;
    argument.type = MMAY_BYTECODE_TYPE_RECORD;
    argument.layout = record;
    if (!mmay_bytecode_add_call(
            function,
            callee,
            MMAY_BYTECODE_TYPE_RECORD,
            &argument,
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
    call->result_layout = record;
    if (!mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_CALL_DIRECT,
            result,
            call_id,
            0,
            &span
        ) || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RETURN,
            result,
            0,
            0,
            &span
        )) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    return true;
}

static void mmay_test_vm_nominal_program_signatures(void) {
    MarimayBytecodeProgram program;
    MarimayBytecodeProgram decoded;
    MarimayBytecodeFunction caller;
    MarimayBytecodeFunction callee;
    MarimayOwnedBytes encoded;
    MarimaySymbolId caller_symbol = {{0x10}};
    MarimaySymbolId callee_symbol = {{0x20}};
    uint32_t function_id = UINT32_MAX;
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&caller, 0, sizeof(caller));
    memset(&callee, 0, sizeof(callee));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_vm_build_nominal_caller(&caller, callee_symbol));
    MMAY_EXPECT(mmay_test_vm_build_nominal_result(&callee));
    MMAY_EXPECT(mmay_bytecode_program_init(
        &program,
        mmay_system_allocator(),
        1,
        mmay_bytecode_program_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_program_take_function(
        &program,
        caller_symbol,
        &caller,
        &function_id
    ) && function_id == 0);
    MMAY_EXPECT(mmay_bytecode_program_take_function(
        &program,
        callee_symbol,
        &callee,
        &function_id
    ) && function_id == 1);
    MMAY_EXPECT(mmay_bytecode_program_set_entry(&program, caller_symbol));
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_encode_program(
        &program,
        mmay_system_allocator(),
        &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_program(
        encoded.data,
        encoded.length,
        mmay_system_allocator(),
        mmay_bytecode_program_default_options(),
        &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    {
        MarimayBytecodeProgramFunction *decoded_callee = mmay_vector_at(
            &decoded.functions,
            1
        );
        MarimayBytecodeRecordLayout *layout = decoded_callee != NULL
            ? mmay_vector_at(&decoded_callee->body.record_layouts, 0)
            : NULL;
        MMAY_EXPECT(layout != NULL);
        if (layout != NULL) {
            layout->symbol.bytes[0] ^= 0xffU;
            MMAY_EXPECT(mmay_bytecode_program_verify(&decoded)
                == MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH);
        }
    }
    mmay_bytecode_program_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&program);
    mmay_bytecode_function_dispose(&callee);
    mmay_bytecode_function_dispose(&caller);
}

static bool mmay_test_vm_build_nominal_limit_function(
    MarimayBytecodeFunction *function
) {
    MarimayBytecodeRecordField field;
    MarimayBytecodeRecordArgument record_argument;
    MarimayBytecodeEnumVariant variant;
    MarimayBytecodeEnumPayload payload;
    MarimayBytecodeEnumArgument enum_argument;
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeRecordConstructionId record_construction =
        MMAY_BYTECODE_NO_RECORD_CONSTRUCTION;
    MarimayBytecodeEnumId enum_id = MMAY_BYTECODE_NO_ENUM;
    MarimayBytecodeEnumConstructionId enum_construction =
        MMAY_BYTECODE_NO_ENUM_CONSTRUCTION;
    MarimayBytecodeInterfaceId interface_id = MMAY_BYTECODE_NO_INTERFACE;
    MarimayBytecodeInterfaceConstructionId interface_construction =
        MMAY_BYTECODE_NO_INTERFACE_CONSTRUCTION;
    MarimaySymbolId record_symbol = {{0x81}};
    MarimaySymbolId enum_symbol = {{0x82}};
    MarimaySymbolId interface_symbol = {{0x83}};
    MarimaySymbolId method_symbol = {{0x84}};
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
    field.type = MMAY_BYTECODE_TYPE_I64;
    field.layout = MMAY_BYTECODE_NO_LAYOUT;
    record_argument.type = MMAY_BYTECODE_TYPE_I64;
    record_argument.layout = MMAY_BYTECODE_NO_LAYOUT;
    variant.payload_count = 1;
    payload.type = MMAY_BYTECODE_TYPE_I64;
    payload.layout = MMAY_BYTECODE_NO_LAYOUT;
    enum_argument.type = MMAY_BYTECODE_TYPE_I64;
    enum_argument.layout = MMAY_BYTECODE_NO_LAYOUT;
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
        ) || !mmay_bytecode_add_record_layout(
            function, record_symbol, &field, 1, &record
        ) || !mmay_bytecode_set_register_layout(function, object, record)) {
        goto failure;
    }
    record_argument.register_id = scalar;
    enum_argument.register_id = scalar;
    if (!mmay_bytecode_add_record_construction(
            function, record, &record_argument, 1, &record_construction
        ) || !mmay_bytecode_add_enum_layout(
            function, enum_symbol, &variant, 1, &payload, 1, &enum_id
        ) || !mmay_bytecode_add_enum_construction(
            function, enum_id, 0, &enum_argument, 1, &enum_construction
        ) || !mmay_bytecode_add_interface_layout(
            function, interface_symbol, 1, &interface_id
        ) || !mmay_bytecode_add_interface_construction(
            function,
            interface_id,
            object,
            MMAY_BYTECODE_TYPE_RECORD,
            record,
            &method_symbol,
            1,
            &interface_construction
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_I64, scalar, 7, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, scalar, 0, 0, &span
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_bytecode_function_dispose(function);
    return false;
}

static void mmay_test_vm_nominal_program_limits(void) {
    const size_t offsets[] = {
        offsetof(MarimayBytecodeProgramOptions, max_total_record_layouts),
        offsetof(MarimayBytecodeProgramOptions, max_total_record_fields),
        offsetof(MarimayBytecodeProgramOptions, max_total_record_constructions),
        offsetof(MarimayBytecodeProgramOptions, max_total_record_arguments),
        offsetof(MarimayBytecodeProgramOptions, max_total_enum_layouts),
        offsetof(MarimayBytecodeProgramOptions, max_total_enum_variants),
        offsetof(MarimayBytecodeProgramOptions, max_total_enum_payloads),
        offsetof(MarimayBytecodeProgramOptions, max_total_enum_constructions),
        offsetof(MarimayBytecodeProgramOptions, max_total_enum_arguments),
        offsetof(MarimayBytecodeProgramOptions, max_total_interface_layouts),
        offsetof(MarimayBytecodeProgramOptions, max_total_interface_constructions),
        offsetof(MarimayBytecodeProgramOptions, max_total_interface_methods)
    };
    MarimayBytecodeProgram program;
    MarimayBytecodeFunction first;
    MarimayBytecodeFunction second;
    MarimayOwnedBytes encoded;
    MarimaySymbolId first_symbol = {{0x10}};
    MarimaySymbolId second_symbol = {{0x20}};
    uint32_t function_id = UINT32_MAX;
    size_t index = 0;
    memset(&program, 0, sizeof(program));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_vm_build_nominal_limit_function(&first));
    MMAY_EXPECT(mmay_test_vm_build_nominal_limit_function(&second));
    MMAY_EXPECT(mmay_bytecode_program_init(
        &program,
        mmay_system_allocator(),
        1,
        mmay_bytecode_program_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_program_take_function(
        &program, first_symbol, &first, &function_id
    ));
    MMAY_EXPECT(mmay_bytecode_program_take_function(
        &program, second_symbol, &second, &function_id
    ));
    MMAY_EXPECT(mmay_bytecode_program_set_entry(&program, first_symbol));
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    for (index = 0; index < sizeof(offsets) / sizeof(offsets[0]); index += 1) {
        size_t *limit = (size_t *)((unsigned char *)&program.options
            + offsets[index]);
        size_t saved = *limit;
        *limit = 1;
        MMAY_EXPECT(mmay_bytecode_program_verify(&program)
            == MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
        *limit = saved;
    }
    MMAY_EXPECT(mmay_bytecode_encode_program(
        &program, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    for (index = 0; index < sizeof(offsets) / sizeof(offsets[0]); index += 1) {
        MarimayBytecodeProgramOptions limited =
            mmay_bytecode_program_default_options();
        MarimayBytecodeProgram decoded;
        MarimayFailAllocator tracking;
        size_t *limit = (size_t *)((unsigned char *)&limited + offsets[index]);
        memset(&decoded, 0, sizeof(decoded));
        *limit = 1;
        mmay_fail_allocator_init(&tracking, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_bytecode_decode_program(
            encoded.data,
            encoded.length,
            mmay_fail_allocator(&tracking),
            limited,
            &decoded
        ) == MMAY_BYTECODE_CODEC_LIMIT_REACHED);
        mmay_bytecode_program_dispose(&decoded);
        MMAY_EXPECT(tracking.live_allocations == 0);
        MMAY_EXPECT(tracking.live_bytes == 0);
    }
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&program);
    mmay_bytecode_function_dispose(&second);
    mmay_bytecode_function_dispose(&first);
}

static void mmay_test_vm_nominal_round_trip(
    MarimayBytecodeFunction *function
) {
    MarimayOwnedBytes encoded;
    MarimayOwnedBytes encoded_again;
    MarimayBytecodeFunction decoded;
    size_t index = 0;
    memset(&encoded, 0, sizeof(encoded));
    memset(&encoded_again, 0, sizeof(encoded_again));
    memset(&decoded, 0, sizeof(decoded));
    MMAY_EXPECT(mmay_bytecode_verify(function) == MMAY_BYTECODE_VERIFY_OK);
    {
        bool reached_success = false;
        size_t failure_index = 0;
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            MarimayFailAllocator state;
            MarimayOwnedBytes attempted;
            MarimayBytecodeCodecStatus status;
            memset(&attempted, 0, sizeof(attempted));
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                failure_index
            );
            status = mmay_bytecode_encode_function(
                function,
                mmay_fail_allocator(&state),
                &attempted
            );
            if (status == MMAY_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            }
            mmay_owned_bytes_dispose(&attempted);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(reached_success);
    }
    MMAY_EXPECT(mmay_bytecode_encode_function(
        function,
        mmay_system_allocator(),
        &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(encoded.length > 10 && encoded.data[9] < 0x80
        && (encoded.data[9] & (1U << 4)) != 0);
    for (index = 0; index < encoded.length; index += 1) {
        MMAY_EXPECT(mmay_bytecode_decode_function(
            encoded.data,
            index,
            mmay_system_allocator(),
            mmay_bytecode_function_default_options(),
            &decoded
        ) == MMAY_BYTECODE_CODEC_TRUNCATED);
        MMAY_EXPECT(!decoded.initialized);
    }
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data,
        encoded.length,
        mmay_system_allocator(),
        mmay_bytecode_function_default_options(),
        &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &decoded,
        mmay_system_allocator(),
        &encoded_again
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(encoded.length == encoded_again.length
        && memcmp(encoded.data, encoded_again.data, encoded.length) == 0);
    mmay_owned_bytes_dispose(&encoded_again);
    mmay_bytecode_function_dispose(&decoded);
    {
        unsigned char saved_feature = encoded.data[9];
        encoded.data[9] = (unsigned char)(saved_feature & ~(1U << 4));
        MMAY_EXPECT(mmay_bytecode_decode_function(
            encoded.data,
            encoded.length,
            mmay_system_allocator(),
            mmay_bytecode_function_default_options(),
            &decoded
        ) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
        MMAY_EXPECT(!decoded.initialized);
        encoded.data[9] = saved_feature;
    }
    {
        bool reached_success = false;
        size_t failure_index = 0;
        for (failure_index = 0; failure_index < 128 && !reached_success;
            failure_index += 1) {
            MarimayFailAllocator state;
            MarimayBytecodeCodecStatus status;
            memset(&decoded, 0, sizeof(decoded));
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                failure_index
            );
            status = mmay_bytecode_decode_function(
                encoded.data,
                encoded.length,
                mmay_fail_allocator(&state),
                mmay_bytecode_function_default_options(),
                &decoded
            );
            if (status == MMAY_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            }
            mmay_bytecode_function_dispose(&decoded);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(reached_success);
    }
    mmay_owned_bytes_dispose(&encoded);
}

void mmay_test_vm_nominal_codec(void) {
    MarimayBytecodeFunction function;
    MarimayTestBuffer disassembly;
    MMAY_EXPECT(mmay_test_vm_build_record_mutation(&function));
    mmay_test_buffer_init(&disassembly);
    MMAY_EXPECT(mmay_bytecode_disassemble(
        &function,
        mmay_test_buffer_writer(&disassembly)
    ) == MMAY_BYTECODE_DISASSEMBLE_OK);
    MMAY_EXPECT(strstr(disassembly.bytes, "record_new r1, rc0") != NULL);
    mmay_test_vm_nominal_round_trip(&function);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_vm_build_enum_payload(&function));
    mmay_test_buffer_init(&disassembly);
    MMAY_EXPECT(mmay_bytecode_disassemble(
        &function,
        mmay_test_buffer_writer(&disassembly)
    ) == MMAY_BYTECODE_DISASSEMBLE_OK);
    MMAY_EXPECT(strstr(disassembly.bytes, "enum_new r1, ec0") != NULL);
    mmay_test_vm_nominal_round_trip(&function);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_vm_build_nominal_result(&function));
    mmay_test_vm_nominal_round_trip(&function);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_vm_build_interface_metadata(&function));
    mmay_test_vm_nominal_round_trip(&function);
    mmay_bytecode_function_dispose(&function);

    mmay_test_vm_nominal_program_signatures();
    mmay_test_vm_nominal_program_limits();
}

void mmay_test_vm_nominal_values_and_verifier(void) {
    MarimayBytecodeFunction function;
    MarimayBytecodeExecution execution;
    MarimayBytecodeRunOptions options;
    MarimayBytecodeValue result;
    MarimayBytecodeRecordField *field = NULL;
    MarimayBytecodeRecordArgument *argument = NULL;
    memset(&execution, 0, sizeof(execution));
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution,
        mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    options = mmay_bytecode_run_default_options();
    options.execution = &execution;

    MMAY_EXPECT(mmay_test_vm_build_record_mutation(&function));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    result = mmay_test_vm_i64(-1);
    MMAY_EXPECT(mmay_bytecode_run(
        &function, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 9);
    field = mmay_vector_at(&function.record_fields, 0);
    MMAY_EXPECT(field != NULL);
    if (field != NULL) {
        field->is_mutable = false;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        field->is_mutable = true;
    }
    argument = mmay_vector_at(&function.record_arguments, 0);
    MMAY_EXPECT(argument != NULL);
    if (argument != NULL) {
        argument->type = MMAY_BYTECODE_TYPE_F64;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        argument->type = MMAY_BYTECODE_TYPE_I64;
    }
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_vm_build_enum_payload(&function));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    result = mmay_test_vm_i64(-1);
    MMAY_EXPECT(mmay_bytecode_run(
        &function, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_I64 && result.as.i64 == 13);
    mmay_bytecode_function_dispose(&function);

    mmay_test_vm_enum_provenance_rejections();
    mmay_bytecode_execution_dispose(&execution);
}
