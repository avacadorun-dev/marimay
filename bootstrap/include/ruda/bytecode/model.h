#ifndef RUDA_BYTECODE_MODEL_H
#define RUDA_BYTECODE_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/vector.h"
#include "ruda/generated/opcodes.h"
#include "ruda/types/type.h"

enum {
    RUDA_BYTECODE_FORMAT_VERSION = 1
};

typedef enum RudaBytecodeOperandKind {
    RUDA_BYTECODE_OPERAND_NONE,
    RUDA_BYTECODE_OPERAND_REGISTER,
    RUDA_BYTECODE_OPERAND_BLOCK,
    RUDA_BYTECODE_OPERAND_CALL,
    RUDA_BYTECODE_OPERAND_STRING,
    RUDA_BYTECODE_OPERAND_I64,
    RUDA_BYTECODE_OPERAND_BOOL,
    RUDA_BYTECODE_OPERAND_F64_BITS,
    RUDA_BYTECODE_OPERAND_ARRAY_CONSTRUCTION,
    RUDA_BYTECODE_OPERAND_RECORD_CONSTRUCTION,
    RUDA_BYTECODE_OPERAND_ENUM_CONSTRUCTION,
    RUDA_BYTECODE_OPERAND_INTERFACE_CONSTRUCTION,
    RUDA_BYTECODE_OPERAND_U64
} RudaBytecodeOperandKind;

enum {
    RUDA_OPCODE_FLAG_INVALID = 1U << 0,
    RUDA_OPCODE_FLAG_TERMINATOR = 1U << 1,
    RUDA_OPCODE_FLAG_BRANCH = 1U << 2,
    RUDA_OPCODE_FLAG_MAY_PANIC = 1U << 3
};

typedef struct RudaOpcodeMetadata {
    RudaOpcode opcode;
    uint8_t wire_id;
    const char *mnemonic;
    uint8_t operand_count;
    RudaBytecodeOperandKind operands[3];
    uint32_t flags;
} RudaOpcodeMetadata;

typedef enum RudaBytecodeType {
    RUDA_BYTECODE_TYPE_INVALID,
    RUDA_BYTECODE_TYPE_BOOL,
    RUDA_BYTECODE_TYPE_I64,
    RUDA_BYTECODE_TYPE_VOID,
    RUDA_BYTECODE_TYPE_STRING,
    RUDA_BYTECODE_TYPE_NEVER,
    RUDA_BYTECODE_TYPE_F64,
    RUDA_BYTECODE_TYPE_ARRAY_F64,
    RUDA_BYTECODE_TYPE_ARRAY_ARRAY_F64,
    /* Generic managed array; its exact element is in the array type table. */
    RUDA_BYTECODE_TYPE_ARRAY,
    RUDA_BYTECODE_TYPE_RECORD,
    RUDA_BYTECODE_TYPE_ENUM,
    RUDA_BYTECODE_TYPE_INTERFACE,
    /* Appended to preserve the established bytecode type wire values. */
    RUDA_BYTECODE_TYPE_I8,
    RUDA_BYTECODE_TYPE_I16,
    RUDA_BYTECODE_TYPE_I32,
    RUDA_BYTECODE_TYPE_U8,
    RUDA_BYTECODE_TYPE_U16,
    RUDA_BYTECODE_TYPE_U32,
    RUDA_BYTECODE_TYPE_U64,
    RUDA_BYTECODE_TYPE_F32
} RudaBytecodeType;

typedef struct RudaBytecodeSpan {
    uint64_t source;
    uint64_t start;
    uint64_t end;
} RudaBytecodeSpan;

typedef struct RudaBytecodeOperand {
    uint64_t unsigned_value;
    int64_t signed_value;
} RudaBytecodeOperand;

typedef struct RudaBytecodeInstruction {
    RudaOpcode opcode;
    RudaBytecodeOperand operands[3];
    RudaBytecodeSpan span;
} RudaBytecodeInstruction;

typedef struct RudaBytecodeBlock {
    RudaAllocator allocator;
    RudaVector instructions;
    bool initialized;
} RudaBytecodeBlock;

typedef uint32_t RudaBytecodeCallId;
typedef uint32_t RudaBytecodeStringId;
typedef uint32_t RudaBytecodeArrayConstructionId;

#define RUDA_BYTECODE_NO_CALL UINT32_MAX
#define RUDA_BYTECODE_NO_STRING UINT32_MAX
#define RUDA_BYTECODE_NO_ARRAY_CONSTRUCTION UINT32_MAX
#define RUDA_BYTECODE_NO_LAYOUT UINT32_MAX

typedef struct RudaBytecodeStringConstant {
    RudaOwnedBytes value;
} RudaBytecodeStringConstant;

typedef struct RudaBytecodeCallArgument {
    uint32_t register_id;
    RudaBytecodeType type;
    uint32_t layout;
} RudaBytecodeCallArgument;

typedef struct RudaBytecodeCall {
    RudaSymbolId callee;
    RudaBytecodeType result_type;
    uint32_t result_layout;
    uint32_t interface_slot;
    size_t argument_start;
    size_t argument_count;
} RudaBytecodeCall;

typedef struct RudaBytecodeArrayArgument {
    uint32_t register_id;
    RudaBytecodeType type;
    uint32_t layout;
} RudaBytecodeArrayArgument;

typedef struct RudaBytecodeArrayType {
    RudaBytecodeType element_type;
    uint32_t element_layout;
} RudaBytecodeArrayType;

typedef struct RudaBytecodeArrayConstruction {
    RudaBytecodeType array_type;
    uint32_t array_layout;
    size_t argument_start;
    size_t argument_count;
} RudaBytecodeArrayConstruction;

const RudaOpcodeMetadata *ruda_opcode_metadata(RudaOpcode opcode);
const RudaOpcodeMetadata *ruda_opcode_metadata_from_wire(uint64_t wire_id);

#endif
