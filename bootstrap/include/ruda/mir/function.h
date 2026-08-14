#ifndef RUDA_MIR_FUNCTION_H
#define RUDA_MIR_FUNCTION_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/vector.h"
#include "ruda/mir/model.h"
#include "ruda/types/type.h"

typedef struct RudaMirBlock {
    RudaAllocator allocator;
    RudaVector instructions;
    RudaMirTerminator terminator;
    bool initialized;
} RudaMirBlock;

typedef struct RudaMirCallArgument {
    RudaMirRegister register_id;
    RudaTypeId type;
} RudaMirCallArgument;

typedef struct RudaMirCall {
    RudaSymbolId callee;
    RudaTypeId result_type;
    uint32_t interface_slot;
    size_t argument_start;
    size_t argument_count;
} RudaMirCall;

typedef struct RudaMirStringConstant {
    RudaOwnedBytes value;
} RudaMirStringConstant;

typedef struct RudaMirArray {
    RudaTypeId type;
    RudaTypeId element_type;
} RudaMirArray;

typedef struct RudaMirArrayArgument {
    RudaMirRegister register_id;
    RudaTypeId type;
} RudaMirArrayArgument;

typedef struct RudaMirArrayConstruction {
    RudaMirArrayId array;
    size_t argument_start;
    size_t argument_count;
} RudaMirArrayConstruction;

typedef struct RudaMirFunctionOptions {
    size_t max_registers;
    size_t max_blocks;
    size_t max_instructions;
    size_t max_calls;
    size_t max_call_arguments;
    size_t max_record_layouts;
    size_t max_record_fields;
    size_t max_record_constructions;
    size_t max_record_arguments;
    size_t max_enum_layouts;
    size_t max_enum_variants;
    size_t max_enum_payloads;
    size_t max_enum_constructions;
    size_t max_enum_arguments;
    size_t max_string_constants;
    size_t max_string_bytes;
    size_t max_array_layouts;
    size_t max_array_constructions;
    size_t max_array_arguments;
    size_t max_interface_layouts;
    size_t max_interface_constructions;
    size_t max_interface_methods;
    size_t max_verify_work;
} RudaMirFunctionOptions;

typedef struct RudaMirFunction {
    RudaAllocator allocator;
    RudaSymbolId symbol;
    RudaTypeId result_type;
    RudaVector register_types;
    RudaVector blocks;
    RudaVector calls;
    RudaVector call_arguments;
    RudaVector records;
    RudaVector record_fields;
    RudaVector record_constructions;
    RudaVector record_arguments;
    RudaVector enums;
    RudaVector enum_variants;
    RudaVector enum_payloads;
    RudaVector enum_constructions;
    RudaVector enum_arguments;
    RudaVector string_constants;
    RudaVector arrays;
    RudaVector array_constructions;
    RudaVector array_arguments;
    RudaVector interfaces;
    RudaVector interface_constructions;
    RudaVector interface_methods;
    size_t parameter_count;
    size_t instruction_count;
    size_t string_bytes;
    RudaMirFunctionOptions options;
    bool initialized;
} RudaMirFunction;

RudaMirFunctionOptions ruda_mir_function_default_options(void);

#endif
