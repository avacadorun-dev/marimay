#ifndef RUDA_BYTECODE_FUNCTION_H
#define RUDA_BYTECODE_FUNCTION_H

#include "ruda/bytecode/model.h"

typedef struct RudaBytecodeFunctionOptions {
    size_t max_registers;
    size_t max_blocks;
    size_t max_instructions;
    size_t max_calls;
    size_t max_call_arguments;
    size_t max_string_constants;
    size_t max_string_bytes;
    size_t max_array_constructions;
    size_t max_array_arguments;
    size_t max_record_layouts;
    size_t max_record_fields;
    size_t max_record_constructions;
    size_t max_record_arguments;
    size_t max_enum_layouts;
    size_t max_enum_variants;
    size_t max_enum_payloads;
    size_t max_enum_constructions;
    size_t max_enum_arguments;
    size_t max_interface_layouts;
    size_t max_interface_constructions;
    size_t max_interface_methods;
    size_t max_source_files;
    size_t max_encoded_bytes;
    size_t max_verify_work;
} RudaBytecodeFunctionOptions;

typedef struct RudaBytecodeFunction {
    RudaAllocator allocator;
    RudaBytecodeType result_type;
    uint32_t result_layout;
    size_t source_count;
    size_t parameter_count;
    RudaVector register_types;
    RudaVector blocks;
    RudaVector calls;
    RudaVector call_arguments;
    RudaVector string_constants;
    RudaVector array_constructions;
    RudaVector array_arguments;
    RudaVector register_layouts;
    RudaVector record_layouts;
    RudaVector record_fields;
    RudaVector record_constructions;
    RudaVector record_arguments;
    RudaVector enum_layouts;
    RudaVector enum_variants;
    RudaVector enum_payloads;
    RudaVector enum_constructions;
    RudaVector enum_arguments;
    RudaVector interface_layouts;
    RudaVector interface_constructions;
    RudaVector interface_methods;
    size_t string_bytes;
    size_t instruction_count;
    RudaBytecodeFunctionOptions options;
    bool initialized;
} RudaBytecodeFunction;

RudaBytecodeFunctionOptions ruda_bytecode_function_default_options(void);

bool ruda_bytecode_function_init(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaBytecodeType result_type,
    size_t source_count,
    const RudaBytecodeType *parameter_types,
    size_t parameter_count,
    RudaBytecodeFunctionOptions options
);
void ruda_bytecode_function_dispose(RudaBytecodeFunction *function);
bool ruda_bytecode_add_register(
    RudaBytecodeFunction *function,
    RudaBytecodeType type,
    uint32_t *result
);
bool ruda_bytecode_add_block(
    RudaBytecodeFunction *function,
    uint32_t *result
);
bool ruda_bytecode_append_instruction(
    RudaBytecodeFunction *function,
    uint32_t block,
    const RudaBytecodeInstruction *instruction
);
bool ruda_bytecode_add_call(
    RudaBytecodeFunction *function,
    RudaSymbolId callee,
    RudaBytecodeType result_type,
    const RudaBytecodeCallArgument *arguments,
    size_t argument_count,
    RudaBytecodeCallId *result
);
const RudaBytecodeCall *ruda_bytecode_call(
    const RudaBytecodeFunction *function,
    RudaBytecodeCallId call
);
const RudaBytecodeCallArgument *ruda_bytecode_call_argument(
    const RudaBytecodeFunction *function,
    size_t index
);
bool ruda_bytecode_add_string_constant(
    RudaBytecodeFunction *function,
    const unsigned char *bytes,
    size_t length,
    RudaBytecodeStringId *result
);
const RudaBytecodeStringConstant *ruda_bytecode_string_constant(
    const RudaBytecodeFunction *function,
    RudaBytecodeStringId string_id
);
bool ruda_bytecode_add_array_construction(
    RudaBytecodeFunction *function,
    RudaBytecodeType array_type,
    const RudaBytecodeArrayArgument *arguments,
    size_t argument_count,
    RudaBytecodeArrayConstructionId *result
);
const RudaBytecodeArrayConstruction *ruda_bytecode_array_construction(
    const RudaBytecodeFunction *function,
    RudaBytecodeArrayConstructionId construction
);
const RudaBytecodeArrayArgument *ruda_bytecode_array_argument(
    const RudaBytecodeFunction *function,
    size_t index
);
bool ruda_bytecode_set_register_layout(
    RudaBytecodeFunction *function,
    uint32_t register_id,
    uint32_t layout
);
uint32_t ruda_bytecode_register_layout(
    const RudaBytecodeFunction *function,
    uint32_t register_id
);

#endif
