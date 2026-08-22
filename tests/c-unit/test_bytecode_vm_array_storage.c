#include "test_bytecode_vm_support.h"

typedef struct MarimayTestVmArrayStorageCase {
    MarimayBytecodeType type;
    uint64_t bits;
    MarimayManagedArrayStorageKind storage_kind;
    size_t stride;
    bool has_element;
} MarimayTestVmArrayStorageCase;

static bool mmay_test_vm_build_storage_array(
    MarimayBytecodeFunction *function,
    MarimayBytecodeType element_type,
    uint64_t bits
) {
    MarimayBytecodeArrayArgument argument;
    MarimayBytecodeArrayConstructionId construction =
        MMAY_BYTECODE_NO_ARRAY_CONSTRUCTION;
    uint32_t layout = MMAY_BYTECODE_NO_LAYOUT;
    uint32_t block = UINT32_MAX;
    uint32_t element = UINT32_MAX;
    uint32_t array = UINT32_MAX;
    uint64_t span = 4000;
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(
            function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_ARRAY, 1,
            NULL, 0, mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_register(function, element_type, &element)
        || !mmay_bytecode_add_register(
            function, MMAY_BYTECODE_TYPE_ARRAY, &array
        ) || !mmay_bytecode_add_array_type(
            function, element_type, MMAY_BYTECODE_NO_LAYOUT, &layout
        ) || element != 0 || array != 1
        || !mmay_bytecode_set_register_layout(function, array, layout)
        || !mmay_bytecode_add_block(function, &block)) {
        goto failure;
    }
    function->result_layout = layout;
    memset(&argument, 0, sizeof(argument));
    argument.register_id = 0;
    argument.type = element_type;
    argument.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_add_array_construction_with_layout(
            function, MMAY_BYTECODE_TYPE_ARRAY, layout,
            element_type == MMAY_BYTECODE_TYPE_F32 ? NULL : &argument,
            element_type == MMAY_BYTECODE_TYPE_F32 ? 0 : 1,
            &construction
        )) {
        goto failure;
    }
    if (element_type == MMAY_BYTECODE_TYPE_BOOL) {
        if (!mmay_test_vm_append(
                function, block, MMAY_OPCODE_CONST_BOOL, 0,
                bits != 0, 0, &span
            )) goto failure;
    } else if (element_type == MMAY_BYTECODE_TYPE_F64) {
        if (!mmay_test_vm_append(
                function, block, MMAY_OPCODE_CONST_F64, 0, bits, 0, &span
            )) goto failure;
    } else if (element_type != MMAY_BYTECODE_TYPE_F32
        && !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_INTEGER, 0, bits, 0, &span
        )) {
        goto failure;
    }
    if (!mmay_test_vm_append(
            function, block, MMAY_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, 1, 0, 0, &span
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_bytecode_function_dispose(function);
    return false;
}

static void mmay_test_vm_storage_case(
    const MarimayTestVmArrayStorageCase *test_case
) {
    MarimayBytecodeFunction function;
    MarimayBytecodeFunction decoded;
    MarimayBytecodeExecution execution;
    MarimayBytecodeRunOptions run_options;
    MarimayBytecodeValue result;
    MarimayOwnedBytes encoded;
    const MarimayManagedArrayDescriptor *descriptor = NULL;
    MarimayManagedCell cell;
    size_t length = 0;
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&result, 0, sizeof(result));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_vm_build_storage_array(
        &function, test_case->type, test_case->bits
    ));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &function, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(
        &decoded, NULL, 0, mmay_system_allocator(), run_options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_ARRAY);
    MMAY_EXPECT(mmay_managed_array_describe(
        &execution.managed_heap, result.as.managed_reference, &descriptor
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(descriptor != NULL
        && descriptor->storage_kind == test_case->storage_kind
        && descriptor->storage_stride == test_case->stride);
    if (test_case->has_element) {
        MMAY_EXPECT(mmay_managed_array_get(
            &execution.managed_heap, result.as.managed_reference, 0,
            &cell, &length
        ) == MMAY_MANAGED_OK && length == 1);
        MMAY_EXPECT(cell.f64_bits == test_case->bits);
    } else {
        MMAY_EXPECT(mmay_managed_array_length(
            &execution.managed_heap, result.as.managed_reference, &length
        ) == MMAY_MANAGED_OK && length == 0);
    }
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&function);
}

void mmay_test_vm_array_storage_layouts(void) {
    static const MarimayTestVmArrayStorageCase cases[] = {
        {MMAY_BYTECODE_TYPE_BOOL, 1, MMAY_MANAGED_ARRAY_STORAGE_RAW8, 1, true},
        {MMAY_BYTECODE_TYPE_I8, UINT8_MAX,
            MMAY_MANAGED_ARRAY_STORAGE_RAW8, 1, true},
        {MMAY_BYTECODE_TYPE_U8, UINT8_MAX,
            MMAY_MANAGED_ARRAY_STORAGE_RAW8, 1, true},
        {MMAY_BYTECODE_TYPE_I16, UINT16_MAX,
            MMAY_MANAGED_ARRAY_STORAGE_RAW16, 2, true},
        {MMAY_BYTECODE_TYPE_U16, UINT16_MAX,
            MMAY_MANAGED_ARRAY_STORAGE_RAW16, 2, true},
        {MMAY_BYTECODE_TYPE_I32, UINT32_MAX,
            MMAY_MANAGED_ARRAY_STORAGE_RAW32, 4, true},
        {MMAY_BYTECODE_TYPE_U32, UINT32_MAX,
            MMAY_MANAGED_ARRAY_STORAGE_RAW32, 4, true},
        {MMAY_BYTECODE_TYPE_F32, UINT64_C(0x7fc01234),
            MMAY_MANAGED_ARRAY_STORAGE_RAW32, 4, false},
        {MMAY_BYTECODE_TYPE_I64, UINT64_MAX,
            MMAY_MANAGED_ARRAY_STORAGE_RAW64, 8, true},
        {MMAY_BYTECODE_TYPE_U64, UINT64_MAX,
            MMAY_MANAGED_ARRAY_STORAGE_RAW64, 8, true},
        {MMAY_BYTECODE_TYPE_F64, UINT64_C(0x7ff8000000000042),
            MMAY_MANAGED_ARRAY_STORAGE_RAW64, 8, true}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        mmay_test_vm_storage_case(&cases[index]);
    }
}
