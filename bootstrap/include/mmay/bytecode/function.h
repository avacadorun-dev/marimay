#ifndef MMAY_BYTECODE_FUNCTION_H
#define MMAY_BYTECODE_FUNCTION_H

#include "mmay/bytecode/model.h"

typedef struct MarimayBytecodeFunctionOptions {
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
} MarimayBytecodeFunctionOptions;

typedef struct MarimayBytecodeFunction {
    MarimayAllocator allocator;
    MarimayBytecodeType result_type;
    uint32_t result_layout;
    size_t source_count;
    size_t parameter_count;
    MarimayVector register_types;
    MarimayVector blocks;
    MarimayVector calls;
    MarimayVector call_arguments;
    MarimayVector string_constants;
    MarimayVector array_constructions;
    MarimayVector array_arguments;
    /* Canonical serialized type table for generic []T descriptors. */
    MarimayVector array_types;
    MarimayVector register_layouts;
    MarimayVector record_layouts;
    MarimayVector record_fields;
    MarimayVector record_constructions;
    MarimayVector record_arguments;
    MarimayVector enum_layouts;
    MarimayVector enum_variants;
    MarimayVector enum_payloads;
    MarimayVector enum_constructions;
    MarimayVector enum_arguments;
    MarimayVector interface_layouts;
    MarimayVector interface_constructions;
    MarimayVector interface_methods;
    size_t string_bytes;
    size_t instruction_count;
    MarimayBytecodeFunctionOptions options;
    bool initialized;
} MarimayBytecodeFunction;
MarimayBytecodeFunctionOptions mmay_bytecode_function_default_options(void);

bool mmay_bytecode_function_init(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayBytecodeType result_type,
    size_t source_count,
    const MarimayBytecodeType *parameter_types,
    size_t parameter_count,
    MarimayBytecodeFunctionOptions options
);
void mmay_bytecode_function_dispose(MarimayBytecodeFunction *function);
bool mmay_bytecode_add_register(
    MarimayBytecodeFunction *function,
    MarimayBytecodeType type,
    uint32_t *result
);
bool mmay_bytecode_add_block(
    MarimayBytecodeFunction *function,
    uint32_t *result
);
bool mmay_bytecode_append_instruction(
    MarimayBytecodeFunction *function,
    uint32_t block,
    const MarimayBytecodeInstruction *instruction
);
bool mmay_bytecode_add_call(
    MarimayBytecodeFunction *function,
    MarimaySymbolId callee,
    MarimayBytecodeType result_type,
    const MarimayBytecodeCallArgument *arguments,
    size_t argument_count,
    MarimayBytecodeCallId *result
);
const MarimayBytecodeCall *mmay_bytecode_call(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeCallId call
);
const MarimayBytecodeCallArgument *mmay_bytecode_call_argument(
    const MarimayBytecodeFunction *function,
    size_t index
);
bool mmay_bytecode_add_string_constant(
    MarimayBytecodeFunction *function,
    const unsigned char *bytes,
    size_t length,
    MarimayBytecodeStringId *result
);
const MarimayBytecodeStringConstant *mmay_bytecode_string_constant(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeStringId string_id
);
bool mmay_bytecode_add_array_construction(
    MarimayBytecodeFunction *function,
    MarimayBytecodeType array_type,
    const MarimayBytecodeArrayArgument *arguments,
    size_t argument_count,
    MarimayBytecodeArrayConstructionId *result
);
bool mmay_bytecode_add_array_type(
    MarimayBytecodeFunction *function,
    MarimayBytecodeType element_type,
    uint32_t element_layout,
    uint32_t *result
);
const MarimayBytecodeArrayType *mmay_bytecode_array_type(
    const MarimayBytecodeFunction *function,
    uint32_t layout
);
bool mmay_bytecode_add_array_construction_with_layout(
    MarimayBytecodeFunction *function,
    MarimayBytecodeType array_type,
    uint32_t array_layout,
    const MarimayBytecodeArrayArgument *arguments,
    size_t argument_count,
    MarimayBytecodeArrayConstructionId *result
);
const MarimayBytecodeArrayConstruction *mmay_bytecode_array_construction(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeArrayConstructionId construction
);
const MarimayBytecodeArrayArgument *mmay_bytecode_array_argument(
    const MarimayBytecodeFunction *function,
    size_t index
);
bool mmay_bytecode_set_register_layout(
    MarimayBytecodeFunction *function,
    uint32_t register_id,
    uint32_t layout
);
uint32_t mmay_bytecode_register_layout(
    const MarimayBytecodeFunction *function,
    uint32_t register_id
);
#endif
