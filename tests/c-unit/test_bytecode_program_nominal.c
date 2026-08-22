#include "test_bytecode_support.h"

#include "mmay/base/hash.h"

typedef enum MarimayTestNominalSchemaCase {
    MMAY_TEST_NOMINAL_SCHEMA_MATCH_RECORD,
    MMAY_TEST_NOMINAL_SCHEMA_RECORD_COUNT_MISMATCH,
    MMAY_TEST_NOMINAL_SCHEMA_RECORD_MUTABILITY_MISMATCH,
    MMAY_TEST_NOMINAL_SCHEMA_RECORD_TYPE_MISMATCH,
    MMAY_TEST_NOMINAL_SCHEMA_ENUM_COUNT_MISMATCH,
    MMAY_TEST_NOMINAL_SCHEMA_ENUM_DISTRIBUTION_MISMATCH,
    MMAY_TEST_NOMINAL_SCHEMA_ENUM_PAYLOAD_TYPE_MISMATCH,
    MMAY_TEST_NOMINAL_SCHEMA_INTERFACE_MISMATCH,
    MMAY_TEST_NOMINAL_SCHEMA_KIND_MISMATCH,
    MMAY_TEST_NOMINAL_SCHEMA_MATCH_RECURSIVE,
    MMAY_TEST_NOMINAL_SCHEMA_RECURSIVE_MISMATCH
} MarimayTestNominalSchemaCase;

typedef struct MarimayTestNominalBytes {
    unsigned char *data;
    size_t length;
    size_t capacity;
} MarimayTestNominalBytes;

static bool mmay_test_nominal_bytes_append(
    MarimayTestNominalBytes *bytes,
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

static bool mmay_test_nominal_bytes_u64(
    MarimayTestNominalBytes *bytes,
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
    return mmay_test_nominal_bytes_append(bytes, encoded, length);
}

static bool mmay_test_nominal_schema_function(
    MarimayBytecodeFunction *function,
    MarimayTestNominalSchemaCase schema_case,
    bool second
) {
    const MarimaySymbolId nominal_symbol = {{0xa0}};
    const MarimaySymbolId recursive_enum_symbol = {{0xb0}};
    MarimayBytecodeRecordField field;
    MarimayBytecodeEnumVariant variants[2];
    MarimayBytecodeEnumPayload payload;
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    MarimayBytecodeEnumId enum_id = MMAY_BYTECODE_NO_ENUM;
    MarimayBytecodeInterfaceId interface_id = MMAY_BYTECODE_NO_INTERFACE;
    uint32_t value = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    size_t span = 1;
    bool metadata_ok = false;
    memset(function, 0, sizeof(*function));
    memset(&field, 0, sizeof(field));
    memset(variants, 0, sizeof(variants));
    memset(&payload, 0, sizeof(payload));
    field.type = MMAY_BYTECODE_TYPE_I64;
    field.layout = MMAY_BYTECODE_NO_LAYOUT;
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
            function,
            MMAY_BYTECODE_TYPE_I64,
            &value
        )) {
        goto failure;
    }
    if (schema_case == MMAY_TEST_NOMINAL_SCHEMA_MATCH_RECORD) {
        metadata_ok = mmay_bytecode_add_record_layout(
            function, nominal_symbol, &field, 1, &record
        );
    } else if (schema_case
            == MMAY_TEST_NOMINAL_SCHEMA_RECORD_COUNT_MISMATCH) {
        metadata_ok = mmay_bytecode_add_record_layout(
            function,
            nominal_symbol,
            second ? &field : NULL,
            second ? 1 : 0,
            &record
        );
    } else if (schema_case
            == MMAY_TEST_NOMINAL_SCHEMA_RECORD_MUTABILITY_MISMATCH) {
        field.is_mutable = second;
        metadata_ok = mmay_bytecode_add_record_layout(
            function, nominal_symbol, &field, 1, &record
        );
    } else if (schema_case
            == MMAY_TEST_NOMINAL_SCHEMA_RECORD_TYPE_MISMATCH) {
        field.type = second
            ? MMAY_BYTECODE_TYPE_BOOL
            : MMAY_BYTECODE_TYPE_I64;
        metadata_ok = mmay_bytecode_add_record_layout(
            function, nominal_symbol, &field, 1, &record
        );
    } else if (schema_case
            == MMAY_TEST_NOMINAL_SCHEMA_ENUM_COUNT_MISMATCH) {
        metadata_ok = mmay_bytecode_add_enum_layout(
            function,
            nominal_symbol,
            variants,
            second ? 2 : 1,
            NULL,
            0,
            &enum_id
        );
    } else if (schema_case
            == MMAY_TEST_NOMINAL_SCHEMA_ENUM_DISTRIBUTION_MISMATCH) {
        variants[0].payload_count = second ? 0 : 1;
        variants[1].payload_start = second ? 0 : 1;
        variants[1].payload_count = second ? 1 : 0;
        metadata_ok = mmay_bytecode_add_enum_layout(
            function,
            nominal_symbol,
            variants,
            2,
            &payload,
            1,
            &enum_id
        );
    } else if (schema_case
            == MMAY_TEST_NOMINAL_SCHEMA_ENUM_PAYLOAD_TYPE_MISMATCH) {
        variants[0].payload_count = 1;
        payload.type = second
            ? MMAY_BYTECODE_TYPE_BOOL
            : MMAY_BYTECODE_TYPE_I64;
        metadata_ok = mmay_bytecode_add_enum_layout(
            function,
            nominal_symbol,
            variants,
            1,
            &payload,
            1,
            &enum_id
        );
    } else if (schema_case == MMAY_TEST_NOMINAL_SCHEMA_INTERFACE_MISMATCH) {
        metadata_ok = mmay_bytecode_add_interface_layout(
            function,
            nominal_symbol,
            second ? 2 : 1,
            &interface_id
        );
    } else if (schema_case == MMAY_TEST_NOMINAL_SCHEMA_MATCH_RECURSIVE
            || schema_case == MMAY_TEST_NOMINAL_SCHEMA_RECURSIVE_MISMATCH) {
        variants[0].payload_count = 1;
        payload.type = schema_case == MMAY_TEST_NOMINAL_SCHEMA_RECURSIVE_MISMATCH
                && second
            ? MMAY_BYTECODE_TYPE_I64
            : MMAY_BYTECODE_TYPE_RECORD;
        payload.layout = payload.type == MMAY_BYTECODE_TYPE_RECORD
            ? 0
            : MMAY_BYTECODE_NO_LAYOUT;
        field.type = MMAY_BYTECODE_TYPE_ENUM;
        field.layout = 0;
        metadata_ok = mmay_bytecode_add_enum_layout(
            function,
            recursive_enum_symbol,
            variants,
            1,
            &payload,
            1,
            &enum_id
        ) && mmay_bytecode_add_record_layout(
            function,
            nominal_symbol,
            &field,
            1,
            &record
        );
    } else if (!second) {
        metadata_ok = mmay_bytecode_add_record_layout(
            function, nominal_symbol, NULL, 0, &record
        );
    } else {
        metadata_ok = mmay_bytecode_add_enum_layout(
            function, nominal_symbol, variants, 1, NULL, 0, &enum_id
        );
    }
    if (!metadata_ok || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_bytecode_append(
            function,
            block,
            MMAY_OPCODE_CONST_I64,
            value,
            second ? 2 : 1,
            0,
            &span
        ) || !mmay_test_bytecode_append(
            function,
            block,
            MMAY_OPCODE_RETURN,
            value,
            0,
            0,
            &span
        ) || mmay_bytecode_verify(function) != MMAY_BYTECODE_VERIFY_OK) {
        goto failure;
    }
    return true;

failure:
    mmay_bytecode_function_dispose(function);
    return false;
}

static bool mmay_test_nominal_schema_bundle(
    MarimayTestNominalSchemaCase schema_case,
    unsigned char *bundle_storage,
    size_t bundle_capacity,
    size_t *bundle_length
) {
    static const unsigned char magic[8] = {
        'R', 'U', 'D', 'A', 'B', 'C', 'P', 0
    };
    const MarimaySymbolId symbols[2] = {{{0x10}}, {{0x20}}};
    unsigned char payload_storage[32768];
    unsigned char digest[32];
    MarimayTestNominalBytes payload;
    MarimayTestNominalBytes bundle;
    MarimayBytecodeFunction functions[2];
    MarimayOwnedBytes encoded[2];
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
        || !mmay_test_nominal_schema_function(&functions[0], schema_case, false)
        || !mmay_test_nominal_schema_function(&functions[1], schema_case, true)
        || mmay_bytecode_encode_function(
            &functions[0], mmay_system_allocator(), &encoded[0]
        ) != MMAY_BYTECODE_CODEC_OK
        || mmay_bytecode_encode_function(
            &functions[1], mmay_system_allocator(), &encoded[1]
        ) != MMAY_BYTECODE_CODEC_OK
        || !mmay_test_nominal_bytes_u64(&payload, 1)
        || !mmay_test_nominal_bytes_u64(&payload, 2)
        || !mmay_test_nominal_bytes_u64(&payload, 0)) {
        goto cleanup;
    }
    for (index = 0; index < 2; index += 1) {
        if (!mmay_test_nominal_bytes_append(
                &payload,
                symbols[index].bytes,
                sizeof(symbols[index].bytes)
            ) || !mmay_test_nominal_bytes_u64(&payload, encoded[index].length)
            || !mmay_test_nominal_bytes_append(
                &payload,
                encoded[index].data,
                encoded[index].length
            )) {
            goto cleanup;
        }
    }
    if (!mmay_sha256(payload.data, payload.length, digest)
        || !mmay_test_nominal_bytes_append(&bundle, magic, sizeof(magic))
        || !mmay_test_nominal_bytes_u64(&bundle, 1)
        || !mmay_test_nominal_bytes_u64(&bundle, 0)
        || !mmay_test_nominal_bytes_u64(&bundle, payload.length)
        || !mmay_test_nominal_bytes_append(
            &bundle, payload.data, payload.length
        ) || !mmay_test_nominal_bytes_append(
            &bundle, digest, sizeof(digest)
        )) {
        goto cleanup;
    }
    *bundle_length = bundle.length;
    success = true;

cleanup:
    for (index = 0; index < 2; index += 1) {
        mmay_owned_bytes_dispose(&encoded[index]);
        mmay_bytecode_function_dispose(&functions[index]);
    }
    return success;
}

static bool mmay_test_nominal_schema_program(
    MarimayTestNominalSchemaCase schema_case,
    MarimayBytecodeProgram *program
) {
    const MarimaySymbolId symbols[2] = {{{0x10}}, {{0x20}}};
    MarimayBytecodeFunction functions[2];
    uint32_t function_id = UINT32_MAX;
    size_t index = 0;
    bool success = false;
    memset(functions, 0, sizeof(functions));
    memset(program, 0, sizeof(*program));
    if (!mmay_test_nominal_schema_function(&functions[0], schema_case, false)
        || !mmay_test_nominal_schema_function(
            &functions[1], schema_case, true
        ) || !mmay_bytecode_program_init(
            program,
            mmay_system_allocator(),
            1,
            mmay_bytecode_program_default_options()
        )) {
        goto cleanup;
    }
    for (index = 0; index < 2; index += 1) {
        if (!mmay_bytecode_program_take_function(
                program,
                symbols[index],
                &functions[index],
                &function_id
            )) {
            goto cleanup;
        }
    }
    if (!mmay_bytecode_program_set_entry(program, symbols[0])) {
        goto cleanup;
    }
    success = true;

cleanup:
    for (index = 0; index < 2; index += 1) {
        mmay_bytecode_function_dispose(&functions[index]);
    }
    if (!success) {
        mmay_bytecode_program_dispose(program);
    }
    return success;
}

void mmay_test_bytecode_nominal_program_schemas(void) {
    const MarimayTestNominalSchemaCase hostile[] = {
        MMAY_TEST_NOMINAL_SCHEMA_RECORD_COUNT_MISMATCH,
        MMAY_TEST_NOMINAL_SCHEMA_RECORD_MUTABILITY_MISMATCH,
        MMAY_TEST_NOMINAL_SCHEMA_RECORD_TYPE_MISMATCH,
        MMAY_TEST_NOMINAL_SCHEMA_ENUM_COUNT_MISMATCH,
        MMAY_TEST_NOMINAL_SCHEMA_ENUM_DISTRIBUTION_MISMATCH,
        MMAY_TEST_NOMINAL_SCHEMA_ENUM_PAYLOAD_TYPE_MISMATCH,
        MMAY_TEST_NOMINAL_SCHEMA_INTERFACE_MISMATCH,
        MMAY_TEST_NOMINAL_SCHEMA_KIND_MISMATCH,
        MMAY_TEST_NOMINAL_SCHEMA_RECURSIVE_MISMATCH
    };
    unsigned char bundle[32768];
    size_t bundle_length = 0;
    size_t index = 0;
    for (index = 0; index < sizeof(hostile) / sizeof(hostile[0]); index += 1) {
        MarimayBytecodeProgram decoded;
        MarimayBytecodeProgram direct;
        memset(&decoded, 0, sizeof(decoded));
        memset(&direct, 0, sizeof(direct));
        MMAY_EXPECT(mmay_test_nominal_schema_program(
            hostile[index],
            &direct
        ));
        MMAY_EXPECT(mmay_bytecode_program_verify(&direct)
            == MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH);
        mmay_bytecode_program_dispose(&direct);
        MMAY_EXPECT(mmay_test_nominal_schema_bundle(
            hostile[index], bundle, sizeof(bundle), &bundle_length
        ));
        MMAY_EXPECT(mmay_bytecode_decode_program(
            bundle,
            bundle_length,
            mmay_system_allocator(),
            mmay_bytecode_program_default_options(),
            &decoded
        ) == MMAY_BYTECODE_CODEC_VERIFY_FAILED);
        MMAY_EXPECT(!decoded.initialized);
        mmay_bytecode_program_dispose(&decoded);
    }
    {
        MarimayBytecodeProgram recursive;
        MarimayBytecodeProgram decoded;
        memset(&recursive, 0, sizeof(recursive));
        memset(&decoded, 0, sizeof(decoded));
        MMAY_EXPECT(mmay_test_nominal_schema_program(
            MMAY_TEST_NOMINAL_SCHEMA_MATCH_RECURSIVE,
            &recursive
        ));
        MMAY_EXPECT(mmay_bytecode_program_verify(&recursive)
            == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
        mmay_bytecode_program_dispose(&recursive);
        MMAY_EXPECT(mmay_test_nominal_schema_bundle(
            MMAY_TEST_NOMINAL_SCHEMA_MATCH_RECURSIVE,
            bundle,
            sizeof(bundle),
            &bundle_length
        ));
        MMAY_EXPECT(mmay_bytecode_decode_program(
            bundle,
            bundle_length,
            mmay_system_allocator(),
            mmay_bytecode_program_default_options(),
            &decoded
        ) == MMAY_BYTECODE_CODEC_OK);
        mmay_bytecode_program_dispose(&decoded);
    }
    MMAY_EXPECT(mmay_test_nominal_schema_bundle(
        MMAY_TEST_NOMINAL_SCHEMA_INTERFACE_MISMATCH,
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
            MarimayBytecodeProgram decoded;
            MarimayBytecodeProgramOptions options =
                mmay_bytecode_program_default_options();
            MarimayBytecodeCodecStatus status;
            memset(&decoded, 0, sizeof(decoded));
            options.max_verify_work = work_limit;
            status = mmay_bytecode_decode_program(
                bundle,
                bundle_length,
                mmay_system_allocator(),
                options,
                &decoded
            );
            observed_limit = observed_limit
                || status == MMAY_BYTECODE_CODEC_LIMIT_REACHED;
            reached_schema_check = status == MMAY_BYTECODE_CODEC_VERIFY_FAILED;
            MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_LIMIT_REACHED
                || reached_schema_check);
            MMAY_EXPECT(!decoded.initialized);
            mmay_bytecode_program_dispose(&decoded);
        }
        MMAY_EXPECT(observed_limit);
        MMAY_EXPECT(reached_schema_check);
    }
    MMAY_EXPECT(mmay_test_nominal_schema_bundle(
        MMAY_TEST_NOMINAL_SCHEMA_RECORD_MUTABILITY_MISMATCH,
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
            MarimayFailAllocator state;
            MarimayBytecodeProgram decoded;
            MarimayBytecodeCodecStatus status;
            memset(&decoded, 0, sizeof(decoded));
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                failure_index
            );
            status = mmay_bytecode_decode_program(
                bundle,
                bundle_length,
                mmay_fail_allocator(&state),
                mmay_bytecode_program_default_options(),
                &decoded
            );
            reached_schema_check = status == MMAY_BYTECODE_CODEC_VERIFY_FAILED;
            MMAY_EXPECT(reached_schema_check
                || status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            MMAY_EXPECT(!decoded.initialized);
            mmay_bytecode_program_dispose(&decoded);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(reached_schema_check);
    }
    MMAY_EXPECT(mmay_test_nominal_schema_bundle(
        MMAY_TEST_NOMINAL_SCHEMA_MATCH_RECORD,
        bundle,
        sizeof(bundle),
        &bundle_length
    ));
    {
        bool reached_success = false;
        size_t failure_index = 0;
        for (failure_index = 0; failure_index < 512 && !reached_success;
            failure_index += 1) {
            MarimayFailAllocator state;
            MarimayBytecodeProgram decoded;
            MarimayBytecodeCodecStatus status;
            memset(&decoded, 0, sizeof(decoded));
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                failure_index
            );
            status = mmay_bytecode_decode_program(
                bundle,
                bundle_length,
                mmay_fail_allocator(&state),
                mmay_bytecode_program_default_options(),
                &decoded
            );
            reached_success = status == MMAY_BYTECODE_CODEC_OK;
            MMAY_EXPECT(reached_success
                || status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            mmay_bytecode_program_dispose(&decoded);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(reached_success);
    }
}
