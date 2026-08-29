#ifndef MMAY_BYTECODE_MODEL_H
#define MMAY_BYTECODE_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/base/vector.h"
#include "mmay/generated/opcodes.h"
#include "mmay/types/type.h"

enum {
    MMAY_BYTECODE_FORMAT_VERSION = 1
};

typedef enum MarimayBytecodeOperandKind {
    MMAY_BYTECODE_OPERAND_NONE,
    MMAY_BYTECODE_OPERAND_REGISTER,
    MMAY_BYTECODE_OPERAND_BLOCK,
    MMAY_BYTECODE_OPERAND_CALL,
    MMAY_BYTECODE_OPERAND_STRING,
    MMAY_BYTECODE_OPERAND_I64,
    MMAY_BYTECODE_OPERAND_BOOL,
    MMAY_BYTECODE_OPERAND_F64_BITS,
    MMAY_BYTECODE_OPERAND_ARRAY_CONSTRUCTION,
    MMAY_BYTECODE_OPERAND_RECORD_CONSTRUCTION,
    MMAY_BYTECODE_OPERAND_ENUM_CONSTRUCTION,
    MMAY_BYTECODE_OPERAND_INTERFACE_CONSTRUCTION,
    MMAY_BYTECODE_OPERAND_U64
} MarimayBytecodeOperandKind;

enum {
    MMAY_OPCODE_FLAG_INVALID = 1U << 0,
    MMAY_OPCODE_FLAG_TERMINATOR = 1U << 1,
    MMAY_OPCODE_FLAG_BRANCH = 1U << 2,
    MMAY_OPCODE_FLAG_MAY_PANIC = 1U << 3
};

typedef struct MarimayOpcodeMetadata {
    MarimayOpcode opcode;
    uint8_t wire_id;
    const char *mnemonic;
    uint8_t operand_count;
    MarimayBytecodeOperandKind operands[3];
    uint32_t flags;
} MarimayOpcodeMetadata;

typedef enum MarimayBytecodeType {
    MMAY_BYTECODE_TYPE_INVALID,
    MMAY_BYTECODE_TYPE_BOOL,
    MMAY_BYTECODE_TYPE_I64,
    MMAY_BYTECODE_TYPE_VOID,
    MMAY_BYTECODE_TYPE_STRING,
    MMAY_BYTECODE_TYPE_NEVER,
    MMAY_BYTECODE_TYPE_F64,
    MMAY_BYTECODE_TYPE_ARRAY_F64,
    MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64,
    /* Generic managed array; its exact element is in the array type table. */
    MMAY_BYTECODE_TYPE_ARRAY,
    MMAY_BYTECODE_TYPE_RECORD,
    MMAY_BYTECODE_TYPE_ENUM,
    MMAY_BYTECODE_TYPE_INTERFACE,
    /* Appended to preserve the established bytecode type wire values. */
    MMAY_BYTECODE_TYPE_I8,
    MMAY_BYTECODE_TYPE_I16,
    MMAY_BYTECODE_TYPE_I32,
    MMAY_BYTECODE_TYPE_U8,
    MMAY_BYTECODE_TYPE_U16,
    MMAY_BYTECODE_TYPE_U32,
    MMAY_BYTECODE_TYPE_U64,
    MMAY_BYTECODE_TYPE_F32,
    MMAY_BYTECODE_TYPE_MAP,
    MMAY_BYTECODE_TYPE_SET,
    /* Appended to preserve rune identity instead of erasing it to u32. */
    MMAY_BYTECODE_TYPE_RUNE
} MarimayBytecodeType;

typedef struct MarimayBytecodeSpan {
    uint64_t source;
    uint64_t start;
    uint64_t end;
} MarimayBytecodeSpan;

typedef struct MarimayBytecodeOperand {
    uint64_t unsigned_value;
    int64_t signed_value;
} MarimayBytecodeOperand;

typedef struct MarimayBytecodeInstruction {
    MarimayOpcode opcode;
    MarimayBytecodeOperand operands[3];
    MarimayBytecodeSpan span;
} MarimayBytecodeInstruction;

typedef struct MarimayBytecodeBlock {
    MarimayAllocator allocator;
    MarimayVector instructions;
    bool initialized;
} MarimayBytecodeBlock;

typedef uint32_t MarimayBytecodeCallId;
typedef uint32_t MarimayBytecodeStringId;
typedef uint32_t MarimayBytecodeArrayConstructionId;

#define MMAY_BYTECODE_NO_CALL UINT32_MAX
#define MMAY_BYTECODE_NO_STRING UINT32_MAX
#define MMAY_BYTECODE_NO_ARRAY_CONSTRUCTION UINT32_MAX
#define MMAY_BYTECODE_NO_LAYOUT UINT32_MAX

typedef struct MarimayBytecodeStringConstant {
    MarimayOwnedBytes value;
} MarimayBytecodeStringConstant;

typedef struct MarimayBytecodeCallArgument {
    uint32_t register_id;
    MarimayBytecodeType type;
    uint32_t layout;
} MarimayBytecodeCallArgument;

typedef struct MarimayBytecodeCall {
    MarimaySymbolId callee;
    MarimayBytecodeType result_type;
    uint32_t result_layout;
    uint32_t interface_slot;
    size_t argument_start;
    size_t argument_count;
} MarimayBytecodeCall;

typedef struct MarimayBytecodeArrayArgument {
    uint32_t register_id;
    MarimayBytecodeType type;
    uint32_t layout;
} MarimayBytecodeArrayArgument;

typedef struct MarimayBytecodeArrayType {
    MarimayBytecodeType element_type;
    uint32_t element_layout;
} MarimayBytecodeArrayType;

typedef struct MarimayBytecodeArrayConstruction {
    MarimayBytecodeType array_type;
    uint32_t array_layout;
    size_t argument_start;
    size_t argument_count;
} MarimayBytecodeArrayConstruction;

const MarimayOpcodeMetadata *mmay_opcode_metadata(MarimayOpcode opcode);
const MarimayOpcodeMetadata *mmay_opcode_metadata_from_wire(uint64_t wire_id);

#endif
