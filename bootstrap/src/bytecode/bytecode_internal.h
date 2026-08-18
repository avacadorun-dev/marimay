#ifndef RUDA_BYTECODE_INTERNAL_H
#define RUDA_BYTECODE_INTERNAL_H

#include <stddef.h>

#include "ruda/bytecode/bytecode.h"

#define RUDA_BYTECODE_MAGIC_LENGTH 8U

/* Shared bytecode model predicates; public APIs remain in bytecode.h. */
bool ruda_bytecode_type_is_register(RudaBytecodeType type);
bool ruda_bytecode_type_is_integer(RudaBytecodeType type);
bool ruda_bytecode_type_is_signed_integer(RudaBytecodeType type);
bool ruda_bytecode_type_is_extended_scalar(RudaBytecodeType type);
bool ruda_bytecode_type_is_array(RudaBytecodeType type);
bool ruda_bytecode_type_is_managed(RudaBytecodeType type);
RudaBytecodeType ruda_bytecode_array_element_type(RudaBytecodeType array_type);
RudaBytecodeType ruda_bytecode_function_array_element_type(
    const RudaBytecodeFunction *function,
    RudaBytecodeType array_type,
    uint32_t array_layout,
    uint32_t *element_layout
);
bool ruda_bytecode_type_is_result(RudaBytecodeType type);
bool ruda_bytecode_type_is_local(RudaBytecodeType type);
const char *ruda_bytecode_type_name(RudaBytecodeType type);
bool ruda_bytecode_function_uses_arrays(const RudaBytecodeFunction *function);
bool ruda_bytecode_function_uses_f64(const RudaBytecodeFunction *function);
bool ruda_bytecode_type_layout_is_valid(
    const RudaBytecodeFunction *function,
    RudaBytecodeType type,
    uint32_t layout
);

/* Instruction validation/dataflow helpers owned by verifier instruction TU. */
const RudaBytecodeInstruction *ruda_bytecode_block_terminator(
    const RudaBytecodeBlock *block
);
bool ruda_bytecode_opcode_writes_register(RudaOpcode opcode);
bool ruda_bytecode_opcode_is_numeric(RudaOpcode opcode);
bool ruda_bytecode_numeric_option_symbol(
    RudaBytecodeType payload_type,
    RudaSymbolId *symbol
);
RudaBytecodeVerifyStatus ruda_bytecode_verify_numeric_instruction(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    unsigned char *assigned
);
RudaBytecodeVerifyStatus ruda_bytecode_verify_io_instruction(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction
);
void ruda_bytecode_transfer_assignments(
    const RudaBytecodeBlock *block,
    unsigned char *assigned,
    size_t register_count
);
RudaBytecodeVerifyStatus ruda_bytecode_verify_value_instruction(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    unsigned char *assigned
);
RudaBytecodeVerifyStatus ruda_bytecode_verify_terminator(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    const unsigned char *assigned
);
RudaBytecodeVerifyStatus ruda_bytecode_verify_nominal_metadata(
    const RudaBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
);
RudaBytecodeVerifyStatus ruda_bytecode_verify_defer_control_flow(
    const RudaBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
);

/* Internal entry point for composing a function verifier into a shared budget. */
RudaBytecodeVerifyStatus ruda_bytecode_verify_with_work_limit(
    const RudaBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
);

#endif
