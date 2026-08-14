#include "test_bytecode_support.h"

#include "ruda/base/hash.h"

typedef enum RudaTestNominalSchemaCase {
    RUDA_TEST_NOMINAL_SCHEMA_MATCH_RECORD,
    RUDA_TEST_NOMINAL_SCHEMA_RECORD_COUNT_MISMATCH,
    RUDA_TEST_NOMINAL_SCHEMA_RECORD_MUTABILITY_MISMATCH,
    RUDA_TEST_NOMINAL_SCHEMA_RECORD_TYPE_MISMATCH,
    RUDA_TEST_NOMINAL_SCHEMA_ENUM_COUNT_MISMATCH,
    RUDA_TEST_NOMINAL_SCHEMA_ENUM_DISTRIBUTION_MISMATCH,
    RUDA_TEST_NOMINAL_SCHEMA_ENUM_PAYLOAD_TYPE_MISMATCH,
    RUDA_TEST_NOMINAL_SCHEMA_INTERFACE_MISMATCH,
    RUDA_TEST_NOMINAL_SCHEMA_KIND_MISMATCH,
    RUDA_TEST_NOMINAL_SCHEMA_MATCH_RECURSIVE,
    RUDA_TEST_NOMINAL_SCHEMA_RECURSIVE_MISMATCH
} RudaTestNominalSchemaCase;

typedef struct RudaTestNominalBytes {
    unsigned char *data;
    size_t length;
    size_t capacity;
} RudaTestNominalBytes;

static bool ruda_test_nominal_bytes_append(
    RudaTestNominalBytes *bytes,
    const void *source,
    size_t length
) {
    if (bytes == NULL || source == NULL
        || length > bytes->capacity - bytes->length) {
        return false;
    }
    memcpy(bytes->data + bytes->length, source, length);
    bytes->length += length;
    return true;
}

static bool ruda_test_nominal_bytes_u64(
    RudaTestNominalBytes *bytes,
    uint64_t value
) {
    unsigned char encoded[10];
    size_t length = 0;
    do {
        unsigned char next = (unsigned char)(value & UINT64_C(0x7f));
        value >>= 7;
        if (value != 0) {
            next = (unsigned char)(next | 0x80U);
        }
        encoded[length] = next;
        length += 1;
    } while (value != 0);
    return ruda_test_nominal_bytes_append(bytes, encoded, length);
}

static bool ruda_test_nominal_schema_function(
    RudaBytecodeFunction *function,
    RudaTestNominalSchemaCase schema_case,
    bool second
) {
    const RudaSymbolId nominal_symbol = {{0xa0}};
    const RudaSymbolId recursive_enum_symbol = {{0xb0}};
    RudaBytecodeRecordField field;
    RudaBytecodeEnumVariant variants[2];
    RudaBytecodeEnumPayload payload;
    RudaBytecodeRecordId record = RUDA_BYTECODE_NO_RECORD;
    RudaBytecodeEnumId enum_id = RUDA_BYTECODE_NO_ENUM;
    RudaBytecodeInterfaceId interface_id = RUDA_BYTECODE_NO_INTERFACE;
    uint32_t value = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 1;
    bool metadata_ok = false;
    memset(function, 0, sizeof(*function));
    memset(&field, 0, sizeof(field));
    memset(variants, 0, sizeof(variants));
    memset(&payload, 0, sizeof(payload));
    field.type = RUDA_BYTECODE_TYPE_I64;
    field.layout = RUDA_BYTECODE_NO_LAYOUT;
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
            function,
            RUDA_BYTECODE_TYPE_I64,
            &value
        )) {
        goto failure;
    }
    if (schema_case == RUDA_TEST_NOMINAL_SCHEMA_MATCH_RECORD) {
        metadata_ok = ruda_bytecode_add_record_layout(
            function, nominal_symbol, &field, 1, &record
        );
    } else if (schema_case
            == RUDA_TEST_NOMINAL_SCHEMA_RECORD_COUNT_MISMATCH) {
        metadata_ok = ruda_bytecode_add_record_layout(
            function,
            nominal_symbol,
            second ? &field : NULL,
            second ? 1 : 0,
            &record
        );
    } else if (schema_case
            == RUDA_TEST_NOMINAL_SCHEMA_RECORD_MUTABILITY_MISMATCH) {
        field.is_mutable = second;
        metadata_ok = ruda_bytecode_add_record_layout(
            function, nominal_symbol, &field, 1, &record
        );
    } else if (schema_case
            == RUDA_TEST_NOMINAL_SCHEMA_RECORD_TYPE_MISMATCH) {
        field.type = second
            ? RUDA_BYTECODE_TYPE_BOOL
            : RUDA_BYTECODE_TYPE_I64;
        metadata_ok = ruda_bytecode_add_record_layout(
            function, nominal_symbol, &field, 1, &record
        );
    } else if (schema_case
            == RUDA_TEST_NOMINAL_SCHEMA_ENUM_COUNT_MISMATCH) {
        metadata_ok = ruda_bytecode_add_enum_layout(
            function,
            nominal_symbol,
            variants,
            second ? 2 : 1,
            NULL,
            0,
            &enum_id
        );
    } else if (schema_case
            == RUDA_TEST_NOMINAL_SCHEMA_ENUM_DISTRIBUTION_MISMATCH) {
        variants[0].payload_count = second ? 0 : 1;
        variants[1].payload_start = second ? 0 : 1;
        variants[1].payload_count = second ? 1 : 0;
        metadata_ok = ruda_bytecode_add_enum_layout(
            function,
            nominal_symbol,
            variants,
            2,
            &payload,
            1,
            &enum_id
        );
    } else if (schema_case
            == RUDA_TEST_NOMINAL_SCHEMA_ENUM_PAYLOAD_TYPE_MISMATCH) {
        variants[0].payload_count = 1;
        payload.type = second
            ? RUDA_BYTECODE_TYPE_BOOL
            : RUDA_BYTECODE_TYPE_I64;
        metadata_ok = ruda_bytecode_add_enum_layout(
            function,
            nominal_symbol,
            variants,
            1,
            &payload,
            1,
            &enum_id
        );
    } else if (schema_case == RUDA_TEST_NOMINAL_SCHEMA_INTERFACE_MISMATCH) {
        metadata_ok = ruda_bytecode_add_interface_layout(
            function,
            nominal_symbol,
            second ? 2 : 1,
            &interface_id
        );
    } else if (schema_case == RUDA_TEST_NOMINAL_SCHEMA_MATCH_RECURSIVE
            || schema_case == RUDA_TEST_NOMINAL_SCHEMA_RECURSIVE_MISMATCH) {
        variants[0].payload_count = 1;
        payload.type = schema_case == RUDA_TEST_NOMINAL_SCHEMA_RECURSIVE_MISMATCH
                && second
            ? RUDA_BYTECODE_TYPE_I64
            : RUDA_BYTECODE_TYPE_RECORD;
        payload.layout = payload.type == RUDA_BYTECODE_TYPE_RECORD
            ? 0
            : RUDA_BYTECODE_NO_LAYOUT;
        field.type = RUDA_BYTECODE_TYPE_ENUM;
        field.layout = 0;
        metadata_ok = ruda_bytecode_add_enum_layout(
            function,
            recursive_enum_symbol,
            variants,
            1,
            &payload,
            1,
            &enum_id
        ) && ruda_bytecode_add_record_layout(
            function,
            nominal_symbol,
            &field,
            1,
            &record
        );
    } else if (!second) {
        metadata_ok = ruda_bytecode_add_record_layout(
            function, nominal_symbol, NULL, 0, &record
        );
    } else {
        metadata_ok = ruda_bytecode_add_enum_layout(
            function, nominal_symbol, variants, 1, NULL, 0, &enum_id
        );
    }
    if (!metadata_ok || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_bytecode_append(
            function,
            block,
            RUDA_OPCODE_CONST_I64,
            value,
            second ? 2 : 1,
            0,
            &span
        ) || !ruda_test_bytecode_append(
            function,
            block,
            RUDA_OPCODE_RETURN,
            value,
            0,
            0,
            &span
        ) || ruda_bytecode_verify(function) != RUDA_BYTECODE_VERIFY_OK) {
        goto failure;
    }
    return true;

failure:
    ruda_bytecode_function_dispose(function);
    return false;
}

static bool ruda_test_nominal_schema_bundle(
    RudaTestNominalSchemaCase schema_case,
    unsigned char *bundle_storage,
    size_t bundle_capacity,
    size_t *bundle_length
) {
    static const unsigned char magic[8] = {
        'R', 'U', 'D', 'A', 'B', 'C', 'P', 0
    };
    const RudaSymbolId symbols[2] = {{{0x10}}, {{0x20}}};
    unsigned char payload_storage[32768];
    unsigned char digest[32];
    RudaTestNominalBytes payload;
    RudaTestNominalBytes bundle;
    RudaBytecodeFunction functions[2];
    RudaOwnedBytes encoded[2];
    size_t index = 0;
    bool success = false;
    memset(functions, 0, sizeof(functions));
    memset(encoded, 0, sizeof(encoded));
    payload.data = payload_storage;
    payload.length = 0;
    payload.capacity = sizeof(payload_storage);
    bundle.data = bundle_storage;
    bundle.length = 0;
    bundle.capacity = bundle_capacity;
    if (bundle_length == NULL
        || !ruda_test_nominal_schema_function(&functions[0], schema_case, false)
        || !ruda_test_nominal_schema_function(&functions[1], schema_case, true)
        || ruda_bytecode_encode_function(
            &functions[0], ruda_system_allocator(), &encoded[0]
        ) != RUDA_BYTECODE_CODEC_OK
        || ruda_bytecode_encode_function(
            &functions[1], ruda_system_allocator(), &encoded[1]
        ) != RUDA_BYTECODE_CODEC_OK
        || !ruda_test_nominal_bytes_u64(&payload, 1)
        || !ruda_test_nominal_bytes_u64(&payload, 2)
        || !ruda_test_nominal_bytes_u64(&payload, 0)) {
        goto cleanup;
    }
    for (index = 0; index < 2; index += 1) {
        if (!ruda_test_nominal_bytes_append(
                &payload,
                symbols[index].bytes,
                sizeof(symbols[index].bytes)
            ) || !ruda_test_nominal_bytes_u64(&payload, encoded[index].length)
            || !ruda_test_nominal_bytes_append(
                &payload,
                encoded[index].data,
                encoded[index].length
            )) {
            goto cleanup;
        }
    }
    if (!ruda_sha256(payload.data, payload.length, digest)
        || !ruda_test_nominal_bytes_append(&bundle, magic, sizeof(magic))
        || !ruda_test_nominal_bytes_u64(&bundle, 1)
        || !ruda_test_nominal_bytes_u64(&bundle, 0)
        || !ruda_test_nominal_bytes_u64(&bundle, payload.length)
        || !ruda_test_nominal_bytes_append(
            &bundle, payload.data, payload.length
        ) || !ruda_test_nominal_bytes_append(
            &bundle, digest, sizeof(digest)
        )) {
        goto cleanup;
    }
    *bundle_length = bundle.length;
    success = true;

cleanup:
    for (index = 0; index < 2; index += 1) {
        ruda_owned_bytes_dispose(&encoded[index]);
        ruda_bytecode_function_dispose(&functions[index]);
    }
    return success;
}

static bool ruda_test_nominal_schema_program(
    RudaTestNominalSchemaCase schema_case,
    RudaBytecodeProgram *program
) {
    const RudaSymbolId symbols[2] = {{{0x10}}, {{0x20}}};
    RudaBytecodeFunction functions[2];
    uint32_t function_id = UINT32_MAX;
    size_t index = 0;
    bool success = false;
    memset(functions, 0, sizeof(functions));
    memset(program, 0, sizeof(*program));
    if (!ruda_test_nominal_schema_function(&functions[0], schema_case, false)
        || !ruda_test_nominal_schema_function(
            &functions[1], schema_case, true
        ) || !ruda_bytecode_program_init(
            program,
            ruda_system_allocator(),
            1,
            ruda_bytecode_program_default_options()
        )) {
        goto cleanup;
    }
    for (index = 0; index < 2; index += 1) {
        if (!ruda_bytecode_program_take_function(
                program,
                symbols[index],
                &functions[index],
                &function_id
            )) {
            goto cleanup;
        }
    }
    if (!ruda_bytecode_program_set_entry(program, symbols[0])) {
        goto cleanup;
    }
    success = true;

cleanup:
    for (index = 0; index < 2; index += 1) {
        ruda_bytecode_function_dispose(&functions[index]);
    }
    if (!success) {
        ruda_bytecode_program_dispose(program);
    }
    return success;
}

void ruda_test_bytecode_nominal_program_schemas(void) {
    const RudaTestNominalSchemaCase hostile[] = {
        RUDA_TEST_NOMINAL_SCHEMA_RECORD_COUNT_MISMATCH,
        RUDA_TEST_NOMINAL_SCHEMA_RECORD_MUTABILITY_MISMATCH,
        RUDA_TEST_NOMINAL_SCHEMA_RECORD_TYPE_MISMATCH,
        RUDA_TEST_NOMINAL_SCHEMA_ENUM_COUNT_MISMATCH,
        RUDA_TEST_NOMINAL_SCHEMA_ENUM_DISTRIBUTION_MISMATCH,
        RUDA_TEST_NOMINAL_SCHEMA_ENUM_PAYLOAD_TYPE_MISMATCH,
        RUDA_TEST_NOMINAL_SCHEMA_INTERFACE_MISMATCH,
        RUDA_TEST_NOMINAL_SCHEMA_KIND_MISMATCH,
        RUDA_TEST_NOMINAL_SCHEMA_RECURSIVE_MISMATCH
    };
    unsigned char bundle[32768];
    size_t bundle_length = 0;
    size_t index = 0;
    for (index = 0; index < sizeof(hostile) / sizeof(hostile[0]); index += 1) {
        RudaBytecodeProgram decoded;
        RudaBytecodeProgram direct;
        memset(&decoded, 0, sizeof(decoded));
        memset(&direct, 0, sizeof(direct));
        RUDA_EXPECT(ruda_test_nominal_schema_program(
            hostile[index],
            &direct
        ));
        RUDA_EXPECT(ruda_bytecode_program_verify(&direct)
            == RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH);
        ruda_bytecode_program_dispose(&direct);
        RUDA_EXPECT(ruda_test_nominal_schema_bundle(
            hostile[index], bundle, sizeof(bundle), &bundle_length
        ));
        RUDA_EXPECT(ruda_bytecode_decode_program(
            bundle,
            bundle_length,
            ruda_system_allocator(),
            ruda_bytecode_program_default_options(),
            &decoded
        ) == RUDA_BYTECODE_CODEC_VERIFY_FAILED);
        RUDA_EXPECT(!decoded.initialized);
        ruda_bytecode_program_dispose(&decoded);
    }
    {
        RudaBytecodeProgram recursive;
        RudaBytecodeProgram decoded;
        memset(&recursive, 0, sizeof(recursive));
        memset(&decoded, 0, sizeof(decoded));
        RUDA_EXPECT(ruda_test_nominal_schema_program(
            RUDA_TEST_NOMINAL_SCHEMA_MATCH_RECURSIVE,
            &recursive
        ));
        RUDA_EXPECT(ruda_bytecode_program_verify(&recursive)
            == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
        ruda_bytecode_program_dispose(&recursive);
        RUDA_EXPECT(ruda_test_nominal_schema_bundle(
            RUDA_TEST_NOMINAL_SCHEMA_MATCH_RECURSIVE,
            bundle,
            sizeof(bundle),
            &bundle_length
        ));
        RUDA_EXPECT(ruda_bytecode_decode_program(
            bundle,
            bundle_length,
            ruda_system_allocator(),
            ruda_bytecode_program_default_options(),
            &decoded
        ) == RUDA_BYTECODE_CODEC_OK);
        ruda_bytecode_program_dispose(&decoded);
    }
    RUDA_EXPECT(ruda_test_nominal_schema_bundle(
        RUDA_TEST_NOMINAL_SCHEMA_INTERFACE_MISMATCH,
        bundle,
        sizeof(bundle),
        &bundle_length
    ));
    {
        bool reached_schema_check = false;
        bool observed_limit = false;
        size_t work_limit = 1;
        for (work_limit = 1;
            work_limit < 512 && !reached_schema_check;
            work_limit += 1) {
            RudaBytecodeProgram decoded;
            RudaBytecodeProgramOptions options =
                ruda_bytecode_program_default_options();
            RudaBytecodeCodecStatus status;
            memset(&decoded, 0, sizeof(decoded));
            options.max_verify_work = work_limit;
            status = ruda_bytecode_decode_program(
                bundle,
                bundle_length,
                ruda_system_allocator(),
                options,
                &decoded
            );
            observed_limit = observed_limit
                || status == RUDA_BYTECODE_CODEC_LIMIT_REACHED;
            reached_schema_check = status == RUDA_BYTECODE_CODEC_VERIFY_FAILED;
            RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_LIMIT_REACHED
                || reached_schema_check);
            RUDA_EXPECT(!decoded.initialized);
            ruda_bytecode_program_dispose(&decoded);
        }
        RUDA_EXPECT(observed_limit);
        RUDA_EXPECT(reached_schema_check);
    }
    RUDA_EXPECT(ruda_test_nominal_schema_bundle(
        RUDA_TEST_NOMINAL_SCHEMA_RECORD_MUTABILITY_MISMATCH,
        bundle,
        sizeof(bundle),
        &bundle_length
    ));
    {
        bool reached_schema_check = false;
        size_t failure_index = 0;
        for (failure_index = 0;
            failure_index < 512 && !reached_schema_check;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeProgram decoded;
            RudaBytecodeCodecStatus status;
            memset(&decoded, 0, sizeof(decoded));
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            status = ruda_bytecode_decode_program(
                bundle,
                bundle_length,
                ruda_fail_allocator(&state),
                ruda_bytecode_program_default_options(),
                &decoded
            );
            reached_schema_check = status == RUDA_BYTECODE_CODEC_VERIFY_FAILED;
            RUDA_EXPECT(reached_schema_check
                || status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            RUDA_EXPECT(!decoded.initialized);
            ruda_bytecode_program_dispose(&decoded);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_schema_check);
    }
    RUDA_EXPECT(ruda_test_nominal_schema_bundle(
        RUDA_TEST_NOMINAL_SCHEMA_MATCH_RECORD,
        bundle,
        sizeof(bundle),
        &bundle_length
    ));
    {
        bool reached_success = false;
        size_t failure_index = 0;
        for (failure_index = 0; failure_index < 512 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeProgram decoded;
            RudaBytecodeCodecStatus status;
            memset(&decoded, 0, sizeof(decoded));
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            status = ruda_bytecode_decode_program(
                bundle,
                bundle_length,
                ruda_fail_allocator(&state),
                ruda_bytecode_program_default_options(),
                &decoded
            );
            reached_success = status == RUDA_BYTECODE_CODEC_OK;
            RUDA_EXPECT(reached_success
                || status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            ruda_bytecode_program_dispose(&decoded);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
    }
}
