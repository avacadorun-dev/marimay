#include "test_bytecode_vm_interface_support.h"

bool mmay_test_vm_build_interface_method(
    MarimayBytecodeFunction *function,
    MarimaySymbolId record_symbol
) {
    MarimayBytecodeType parameter = MMAY_BYTECODE_TYPE_RECORD;
    MarimayBytecodeRecordField field;
    MarimayBytecodeRecordId record = MMAY_BYTECODE_NO_RECORD;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 1;
    memset(function, 0, sizeof(*function));
    memset(&field, 0, sizeof(field));
    field.type = MMAY_BYTECODE_TYPE_I64;
    field.layout = MMAY_BYTECODE_NO_LAYOUT;
    if (!mmay_bytecode_function_init(
            function,
            mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_I64,
            1,
            &parameter,
            1,
            mmay_bytecode_function_default_options()
        ) || !mmay_bytecode_add_record_layout(
            function,
            record_symbol,
            &field,
            1,
            &record
        ) || !mmay_bytecode_set_register_layout(function, 0, record)
        || !mmay_bytecode_add_register(
            function,
            MMAY_BYTECODE_TYPE_I64,
            &result
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_test_vm_append(
            function,
            block,
            MMAY_OPCODE_RECORD_GET,
            result,
            0,
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
