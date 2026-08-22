#ifndef MMAY_MIR_FUNCTION_H
#define MMAY_MIR_FUNCTION_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/base/vector.h"
#include "mmay/mir/model.h"
#include "mmay/types/type.h"

typedef struct MarimayMirBlock {
    MarimayAllocator allocator;
    MarimayVector instructions;
    MarimayMirTerminator terminator;
    bool initialized;
} MarimayMirBlock;

typedef struct MarimayMirCallArgument {
    MarimayMirRegister register_id;
    MarimayTypeId type;
} MarimayMirCallArgument;

typedef struct MarimayMirCall {
    MarimaySymbolId callee;
    MarimayTypeId result_type;
    uint32_t interface_slot;
    size_t argument_start;
    size_t argument_count;
} MarimayMirCall;

typedef struct MarimayMirStringConstant {
    MarimayOwnedBytes value;
} MarimayMirStringConstant;

typedef struct MarimayMirArray {
    MarimayTypeId type;
    MarimayTypeId element_type;
} MarimayMirArray;

typedef struct MarimayMirArrayArgument {
    MarimayMirRegister register_id;
    MarimayTypeId type;
} MarimayMirArrayArgument;

typedef struct MarimayMirArrayConstruction {
    MarimayMirArrayId array;
    size_t argument_start;
    size_t argument_count;
} MarimayMirArrayConstruction;

typedef struct MarimayMirFunctionOptions {
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
} MarimayMirFunctionOptions;

typedef struct MarimayMirFunction {
    MarimayAllocator allocator;
    MarimaySymbolId symbol;
    MarimayTypeId result_type;
    MarimayVector register_types;
    MarimayVector blocks;
    MarimayVector calls;
    MarimayVector call_arguments;
    MarimayVector records;
    MarimayVector record_fields;
    MarimayVector record_constructions;
    MarimayVector record_arguments;
    MarimayVector enums;
    MarimayVector enum_variants;
    MarimayVector enum_payloads;
    MarimayVector enum_constructions;
    MarimayVector enum_arguments;
    MarimayVector string_constants;
    MarimayVector arrays;
    MarimayVector array_constructions;
    MarimayVector array_arguments;
    MarimayVector interfaces;
    MarimayVector interface_constructions;
    MarimayVector interface_methods;
    size_t parameter_count;
    size_t instruction_count;
    size_t string_bytes;
    MarimayMirFunctionOptions options;
    bool initialized;
} MarimayMirFunction;

MarimayMirFunctionOptions mmay_mir_function_default_options(void);

#endif
