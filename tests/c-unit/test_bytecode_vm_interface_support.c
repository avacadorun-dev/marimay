#include "test_bytecode_vm_interface_support.h"

bool ruda_test_vm_build_interface_method(
    RudaBytecodeFunction *function,
    RudaSymbolId record_symbol
) {
    RudaBytecodeType parameter = RUDA_BYTECODE_TYPE_RECORD;
    RudaBytecodeRecordField field;
    RudaBytecodeRecordId record = RUDA_BYTECODE_NO_RECORD;
    uint32_t result = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 1;
    memset(function, 0, sizeof(*function));
    memset(&field, 0, sizeof(field));
    field.type = RUDA_BYTECODE_TYPE_I64;
    field.layout = RUDA_BYTECODE_NO_LAYOUT;
    if (!ruda_bytecode_function_init(
            function,
            ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_I64,
            1,
            &parameter,
            1,
            ruda_bytecode_function_default_options()
        ) || !ruda_bytecode_add_record_layout(
            function,
            record_symbol,
            &field,
            1,
            &record
        ) || !ruda_bytecode_set_register_layout(function, 0, record)
        || !ruda_bytecode_add_register(
            function,
            RUDA_BYTECODE_TYPE_I64,
            &result
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_test_vm_append(
            function,
            block,
            RUDA_OPCODE_RECORD_GET,
            result,
            0,
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
