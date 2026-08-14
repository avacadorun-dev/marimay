#ifndef RUDA_BYTECODE_PROGRAM_INTERNAL_H
#define RUDA_BYTECODE_PROGRAM_INTERNAL_H

#include "ruda/bytecode/program.h"

enum {
    RUDA_BYTECODE_PROGRAM_MINIMUM_ENCODED_BYTES = 46
};

typedef struct RudaBytecodeProgramTotals {
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
    size_t interface_layouts;
    size_t interface_constructions;
    size_t interface_methods;
} RudaBytecodeProgramTotals;

bool ruda_bytecode_program_valid_options(RudaBytecodeProgramOptions options);
bool ruda_bytecode_program_function_options_within(
    RudaBytecodeFunctionOptions actual,
    RudaBytecodeFunctionOptions limit
);
bool ruda_bytecode_program_accumulate_function(
    RudaBytecodeProgramTotals *totals,
    const RudaBytecodeFunction *function,
    RudaBytecodeProgramOptions options
);

bool ruda_bytecode_program_same_type(
    const RudaBytecodeFunction *left_function,
    RudaBytecodeType left_type,
    uint32_t left_layout,
    const RudaBytecodeFunction *right_function,
    RudaBytecodeType right_type,
    uint32_t right_layout
);

RudaBytecodeProgramVerifyStatus ruda_bytecode_program_verify_interface_call(
    const RudaBytecodeProgram *program,
    const RudaBytecodeProgramFunction *caller,
    const RudaBytecodeCall *call,
    size_t *work
);

bool ruda_bytecode_program_consume_nominal_work(
    const RudaBytecodeFunction *function,
    size_t *work,
    size_t limit
);

RudaBytecodeProgramVerifyStatus ruda_bytecode_program_verify_nominal_schemas(
    const RudaBytecodeProgram *program,
    size_t *work
);

#endif
