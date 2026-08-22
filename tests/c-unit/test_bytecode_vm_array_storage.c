#include "test_bytecode_vm_support.h"

typedef struct RudaTestVmArrayStorageCase {
    RudaBytecodeType type;
    uint64_t bits;
    RudaManagedArrayStorageKind storage_kind;
    size_t stride;
    bool has_element;
} RudaTestVmArrayStorageCase;

static bool ruda_test_vm_build_storage_array(
    RudaBytecodeFunction *function,
    RudaBytecodeType element_type,
    uint64_t bits
) {
    RudaBytecodeArrayArgument argument;
    RudaBytecodeArrayConstructionId construction =
        RUDA_BYTECODE_NO_ARRAY_CONSTRUCTION;
    uint32_t layout = RUDA_BYTECODE_NO_LAYOUT;
    uint32_t block = UINT32_MAX;
    uint32_t element = UINT32_MAX;
    uint32_t array = UINT32_MAX;
    uint64_t span = 4000;
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(
            function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_ARRAY, 1,
            NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_register(function, element_type, &element)
        || !ruda_bytecode_add_register(
            function, RUDA_BYTECODE_TYPE_ARRAY, &array
        ) || !ruda_bytecode_add_array_type(
            function, element_type, RUDA_BYTECODE_NO_LAYOUT, &layout
        ) || element != 0 || array != 1
        || !ruda_bytecode_set_register_layout(function, array, layout)
        || !ruda_bytecode_add_block(function, &block)) {
        goto failure;
    }
    function->result_layout = layout;
    memset(&argument, 0, sizeof(argument));
    argument.register_id = 0;
    argument.type = element_type;
    argument.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_add_array_construction_with_layout(
            function, RUDA_BYTECODE_TYPE_ARRAY, layout,
            element_type == RUDA_BYTECODE_TYPE_F32 ? NULL : &argument,
            element_type == RUDA_BYTECODE_TYPE_F32 ? 0 : 1,
            &construction
        )) {
        goto failure;
    }
    if (element_type == RUDA_BYTECODE_TYPE_BOOL) {
        if (!ruda_test_vm_append(
                function, block, RUDA_OPCODE_CONST_BOOL, 0,
                bits != 0, 0, &span
            )) goto failure;
    } else if (element_type == RUDA_BYTECODE_TYPE_F64) {
        if (!ruda_test_vm_append(
                function, block, RUDA_OPCODE_CONST_F64, 0, bits, 0, &span
            )) goto failure;
    } else if (element_type != RUDA_BYTECODE_TYPE_F32
        && !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_INTEGER, 0, bits, 0, &span
        )) {
        goto failure;
    }
    if (!ruda_test_vm_append(
            function, block, RUDA_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN, 1, 0, 0, &span
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_bytecode_function_dispose(function);
    return false;
}

static void ruda_test_vm_storage_case(
    const RudaTestVmArrayStorageCase *test_case
) {
    RudaBytecodeFunction function;
    RudaBytecodeFunction decoded;
    RudaBytecodeExecution execution;
    RudaBytecodeRunOptions run_options;
    RudaBytecodeValue result;
    RudaOwnedBytes encoded;
    const RudaManagedArrayDescriptor *descriptor = NULL;
    RudaManagedCell cell;
    size_t length = 0;
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&result, 0, sizeof(result));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_test_vm_build_storage_array(
        &function, test_case->type, test_case->bits
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &function, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_function(
        encoded.data, encoded.length, ruda_system_allocator(),
        ruda_bytecode_function_default_options(), &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()
    ));
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run(
        &decoded, NULL, 0, ruda_system_allocator(), run_options, &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_ARRAY);
    RUDA_EXPECT(ruda_managed_array_describe(
        &execution.managed_heap, result.as.managed_reference, &descriptor
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(descriptor != NULL
        && descriptor->storage_kind == test_case->storage_kind
        && descriptor->storage_stride == test_case->stride);
    if (test_case->has_element) {
        RUDA_EXPECT(ruda_managed_array_get(
            &execution.managed_heap, result.as.managed_reference, 0,
            &cell, &length
        ) == RUDA_MANAGED_OK && length == 1);
        RUDA_EXPECT(cell.f64_bits == test_case->bits);
    } else {
        RUDA_EXPECT(ruda_managed_array_length(
            &execution.managed_heap, result.as.managed_reference, &length
        ) == RUDA_MANAGED_OK && length == 0);
    }
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&function);
}

void ruda_test_vm_array_storage_layouts(void) {
    static const RudaTestVmArrayStorageCase cases[] = {
        {RUDA_BYTECODE_TYPE_BOOL, 1, RUDA_MANAGED_ARRAY_STORAGE_RAW8, 1, true},
        {RUDA_BYTECODE_TYPE_I8, UINT8_MAX,
            RUDA_MANAGED_ARRAY_STORAGE_RAW8, 1, true},
        {RUDA_BYTECODE_TYPE_U8, UINT8_MAX,
            RUDA_MANAGED_ARRAY_STORAGE_RAW8, 1, true},
        {RUDA_BYTECODE_TYPE_I16, UINT16_MAX,
            RUDA_MANAGED_ARRAY_STORAGE_RAW16, 2, true},
        {RUDA_BYTECODE_TYPE_U16, UINT16_MAX,
            RUDA_MANAGED_ARRAY_STORAGE_RAW16, 2, true},
        {RUDA_BYTECODE_TYPE_I32, UINT32_MAX,
            RUDA_MANAGED_ARRAY_STORAGE_RAW32, 4, true},
        {RUDA_BYTECODE_TYPE_U32, UINT32_MAX,
            RUDA_MANAGED_ARRAY_STORAGE_RAW32, 4, true},
        {RUDA_BYTECODE_TYPE_F32, UINT64_C(0x7fc01234),
            RUDA_MANAGED_ARRAY_STORAGE_RAW32, 4, false},
        {RUDA_BYTECODE_TYPE_I64, UINT64_MAX,
            RUDA_MANAGED_ARRAY_STORAGE_RAW64, 8, true},
        {RUDA_BYTECODE_TYPE_U64, UINT64_MAX,
            RUDA_MANAGED_ARRAY_STORAGE_RAW64, 8, true},
        {RUDA_BYTECODE_TYPE_F64, UINT64_C(0x7ff8000000000042),
            RUDA_MANAGED_ARRAY_STORAGE_RAW64, 8, true}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        ruda_test_vm_storage_case(&cases[index]);
    }
}
