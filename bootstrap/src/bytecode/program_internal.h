#ifndef MMAY_BYTECODE_PROGRAM_INTERNAL_H
#define MMAY_BYTECODE_PROGRAM_INTERNAL_H

#include "mmay/bytecode/program.h"

enum {
    MMAY_BYTECODE_PROGRAM_MINIMUM_ENCODED_BYTES = 46
};

typedef struct MarimayBytecodeProgramTotals {
    size_t registers;
    size_t blocks;
    size_t instructions;
    size_t calls;
    size_t call_arguments;
    size_t string_constants;
    size_t string_bytes;
    size_t array_constructions;
    size_t array_arguments;
    size_t record_layouts;
    size_t record_fields;
    size_t record_constructions;
    size_t record_arguments;
    size_t enum_layouts;
    size_t enum_variants;
    size_t enum_payloads;
    size_t enum_constructions;
    size_t enum_arguments;
    size_t collection_layouts;
    size_t interface_layouts;
    size_t interface_constructions;
    size_t interface_methods;
} MarimayBytecodeProgramTotals;

bool mmay_bytecode_program_valid_options(MarimayBytecodeProgramOptions options);
bool mmay_bytecode_program_function_options_within(
    MarimayBytecodeFunctionOptions actual,
    MarimayBytecodeFunctionOptions limit
);
bool mmay_bytecode_program_accumulate_function(
    MarimayBytecodeProgramTotals *totals,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeProgramOptions options
);

bool mmay_bytecode_program_same_type(
    const MarimayBytecodeFunction *left_function,
    MarimayBytecodeType left_type,
    uint32_t left_layout,
    const MarimayBytecodeFunction *right_function,
    MarimayBytecodeType right_type,
    uint32_t right_layout
);

MarimayBytecodeProgramVerifyStatus mmay_bytecode_program_verify_interface_call(
    const MarimayBytecodeProgram *program,
    const MarimayBytecodeProgramFunction *caller,
    const MarimayBytecodeCall *call,
    size_t *work
);

bool mmay_bytecode_program_consume_nominal_work(
    const MarimayBytecodeFunction *function,
    size_t *work,
    size_t limit
);

MarimayBytecodeProgramVerifyStatus mmay_bytecode_program_verify_nominal_schemas(
    const MarimayBytecodeProgram *program,
    size_t *work
);

#endif
