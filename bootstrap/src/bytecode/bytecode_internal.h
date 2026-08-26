#ifndef MMAY_BYTECODE_INTERNAL_H
#define MMAY_BYTECODE_INTERNAL_H

#include <stddef.h>

#include "mmay/bytecode/bytecode.h"

#define MMAY_BYTECODE_MAGIC_LENGTH 8U

/* Shared bytecode model predicates; public APIs remain in bytecode.h. */
bool mmay_bytecode_type_is_register(MarimayBytecodeType type);
bool mmay_bytecode_type_is_integer(MarimayBytecodeType type);
bool mmay_bytecode_type_is_signed_integer(MarimayBytecodeType type);
bool mmay_bytecode_type_is_extended_scalar(MarimayBytecodeType type);
bool mmay_bytecode_type_is_array(MarimayBytecodeType type);
bool mmay_bytecode_type_is_managed(MarimayBytecodeType type);
bool mmay_bytecode_type_requires_layout(MarimayBytecodeType type);
MarimayBytecodeType mmay_bytecode_array_element_type(MarimayBytecodeType array_type);
MarimayBytecodeType mmay_bytecode_function_array_element_type(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeType array_type,
    uint32_t array_layout,
    uint32_t *element_layout
);
bool mmay_bytecode_type_is_result(MarimayBytecodeType type);
bool mmay_bytecode_type_is_local(MarimayBytecodeType type);
const char *mmay_bytecode_type_name(MarimayBytecodeType type);
bool mmay_bytecode_function_uses_arrays(const MarimayBytecodeFunction *function);
bool mmay_bytecode_function_uses_f64(const MarimayBytecodeFunction *function);
bool mmay_bytecode_type_layout_is_valid(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeType type,
    uint32_t layout
);

/* Instruction validation/dataflow helpers owned by verifier instruction TU. */
const MarimayBytecodeInstruction *mmay_bytecode_block_terminator(
    const MarimayBytecodeBlock *block
);
bool mmay_bytecode_opcode_writes_register(MarimayOpcode opcode);
bool mmay_bytecode_opcode_is_numeric(MarimayOpcode opcode);
bool mmay_bytecode_opcode_is_text(MarimayOpcode opcode);
bool mmay_bytecode_numeric_option_symbol(
    MarimayBytecodeType payload_type,
    MarimaySymbolId *symbol
);
MarimayBytecodeVerifyStatus mmay_bytecode_verify_numeric_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    unsigned char *assigned
);
MarimayBytecodeVerifyStatus mmay_bytecode_verify_io_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction
);
MarimayBytecodeVerifyStatus mmay_bytecode_verify_format_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    const unsigned char *assigned
);
MarimayBytecodeVerifyStatus mmay_bytecode_verify_parse_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    const unsigned char *assigned
);
MarimayBytecodeVerifyStatus mmay_bytecode_verify_text_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    const unsigned char *assigned
);
void mmay_bytecode_transfer_assignments(
    const MarimayBytecodeBlock *block,
    unsigned char *assigned,
    size_t register_count
);
MarimayBytecodeVerifyStatus mmay_bytecode_verify_value_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    unsigned char *assigned
);
MarimayBytecodeVerifyStatus mmay_bytecode_verify_terminator(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    const unsigned char *assigned
);
MarimayBytecodeVerifyStatus mmay_bytecode_verify_call_instruction(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeInstruction *instruction,
    unsigned char *assigned
);
MarimayBytecodeVerifyStatus mmay_bytecode_verify_nominal_metadata(
    const MarimayBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
);
MarimayBytecodeVerifyStatus mmay_bytecode_verify_defer_control_flow(
    const MarimayBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
);
MarimayBytecodeVerifyStatus mmay_bytecode_verify_integer_range_proofs(
    const MarimayBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
);

/* Internal entry point for composing a function verifier into a shared budget. */
MarimayBytecodeVerifyStatus mmay_bytecode_verify_with_work_limit(
    const MarimayBytecodeFunction *function,
    size_t available_work,
    size_t *work_used
);

#endif
