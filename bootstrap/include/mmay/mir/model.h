#ifndef MMAY_MIR_MODEL_H
#define MMAY_MIR_MODEL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mmay/types/model.h"
typedef uint32_t MarimayMirRegister;
typedef uint32_t MarimayMirBlockId;
typedef uint32_t MarimayMirCallId;
typedef uint32_t MarimayMirRecordId;
typedef uint32_t MarimayMirRecordConstructionId;
typedef uint32_t MarimayMirEnumId;
typedef uint32_t MarimayMirEnumConstructionId;
typedef uint32_t MarimayMirStringId;
typedef uint32_t MarimayMirArrayId;
typedef uint32_t MarimayMirArrayConstructionId;
typedef uint32_t MarimayMirInterfaceId;
typedef uint32_t MarimayMirInterfaceConstructionId;
#define MMAY_MIR_NO_REGISTER UINT32_MAX
#define MMAY_MIR_NO_BLOCK UINT32_MAX
#define MMAY_MIR_NO_CALL UINT32_MAX
#define MMAY_MIR_NO_RECORD UINT32_MAX
#define MMAY_MIR_NO_RECORD_CONSTRUCTION UINT32_MAX
#define MMAY_MIR_NO_ENUM UINT32_MAX
#define MMAY_MIR_NO_ENUM_CONSTRUCTION UINT32_MAX
#define MMAY_MIR_NO_STRING UINT32_MAX
#define MMAY_MIR_NO_ARRAY UINT32_MAX
#define MMAY_MIR_NO_ARRAY_CONSTRUCTION UINT32_MAX
#define MMAY_MIR_NO_INTERFACE UINT32_MAX
#define MMAY_MIR_NO_INTERFACE_CONSTRUCTION UINT32_MAX
typedef struct MarimayMirSpan {
    size_t start;
    size_t end;
} MarimayMirSpan;
typedef enum MarimayMirInstructionKind {
    MMAY_MIR_CONSTANT_I64,
    MMAY_MIR_CONSTANT_INTEGER,
    MMAY_MIR_CONSTANT_BOOL,
    MMAY_MIR_CONSTANT_STRING,
    MMAY_MIR_COPY,
    MMAY_MIR_ADD_CHECKED_I64,
    MMAY_MIR_SUBTRACT_CHECKED_I64,
    MMAY_MIR_MULTIPLY_CHECKED_I64,
    MMAY_MIR_DIVIDE_CHECKED_I64,
    MMAY_MIR_REMAINDER_CHECKED_I64,
    MMAY_MIR_NEGATE_CHECKED_I64,
    MMAY_MIR_NOT_BOOL,
    MMAY_MIR_BITWISE_NOT_I64,
    MMAY_MIR_BITWISE_AND_I64,
    MMAY_MIR_BITWISE_XOR_I64,
    MMAY_MIR_BITWISE_OR_I64,
    MMAY_MIR_SHIFT_LEFT_CHECKED_I64,
    MMAY_MIR_SHIFT_RIGHT_I64,
    MMAY_MIR_EQUAL_I64,
    MMAY_MIR_EQUAL_BOOL,
    MMAY_MIR_EQUAL_STRING,
    MMAY_MIR_LESS_I64,
    MMAY_MIR_LESS_EQUAL_I64,
    MMAY_MIR_RECORD_NEW,
    MMAY_MIR_RECORD_GET,
    MMAY_MIR_RECORD_SET,
    MMAY_MIR_RECORD_IDENTITY_SAME,
    MMAY_MIR_ENUM_NEW,
    MMAY_MIR_ENUM_IS_VARIANT,
    MMAY_MIR_ENUM_REQUIRE_VARIANT,
    MMAY_MIR_ENUM_GET_PAYLOAD,
    MMAY_MIR_CALL,
    MMAY_MIR_CONSTANT_F64,
    MMAY_MIR_ADD_F64,
    MMAY_MIR_SUBTRACT_F64,
    MMAY_MIR_MULTIPLY_F64,
    MMAY_MIR_DIVIDE_F64,
    MMAY_MIR_NEGATE_F64,
    MMAY_MIR_EQUAL_F64,
    MMAY_MIR_LESS_F64,
    MMAY_MIR_LESS_EQUAL_F64,
    MMAY_MIR_ARRAY_NEW,
    MMAY_MIR_ARRAY_LENGTH,
    MMAY_MIR_ARRAY_REQUIRE_INDEX,
    MMAY_MIR_ARRAY_ASSUME_INDEX,
    MMAY_MIR_ARRAY_GET,
    MMAY_MIR_ARRAY_SET,
    MMAY_MIR_ARRAY_GET_IN_BOUNDS,
    MMAY_MIR_ARRAY_SET_IN_BOUNDS,
    MMAY_MIR_ARRAY_PUSH,
    MMAY_MIR_ARRAY_POP,
    MMAY_MIR_ARRAY_TRUNCATE,
    MMAY_MIR_ARRAY_CLEAR,
    MMAY_MIR_ARRAY_RESERVE,
    MMAY_MIR_ARRAY_GENERATION,
    MMAY_MIR_ARRAY_REQUIRE_GENERATION,
    MMAY_MIR_INTERFACE_NEW,
    MMAY_MIR_CALL_INTERFACE,
    MMAY_MIR_INTEGER_POLICY,
    MMAY_MIR_INTEGER_CONVERT,
    MMAY_MIR_FORMAT_F32, MMAY_MIR_FORMAT_F64, MMAY_MIR_FORMAT_BOOL,
    MMAY_MIR_FORMAT_INTEGER, MMAY_MIR_STRING_CONCAT, MMAY_MIR_PARSE_INTEGER,
    MMAY_MIR_IO_PRINTLN,
    MMAY_MIR_TEST_OUTPUT_BEGIN,
    MMAY_MIR_TEST_OUTPUT_END,
    MMAY_MIR_DEFER_REGISTER,
    MMAY_MIR_DEFER_CALL,
    MMAY_MIR_ADD_PROVEN_I64, MMAY_MIR_SUBTRACT_PROVEN_I64,
    MMAY_MIR_MULTIPLY_PROVEN_I64
} MarimayMirInstructionKind;
typedef enum MarimayMirIntegerPolicy {
    MMAY_MIR_INTEGER_CHECKED_OPTION,
    MMAY_MIR_INTEGER_WRAPPING,
    MMAY_MIR_INTEGER_SATURATING
} MarimayMirIntegerPolicy;
typedef enum MarimayMirIntegerOperation {
    MMAY_MIR_POLICY_ADD,
    MMAY_MIR_POLICY_SUBTRACT,
    MMAY_MIR_POLICY_MULTIPLY,
    MMAY_MIR_POLICY_DIVIDE,
    MMAY_MIR_POLICY_REMAINDER,
    MMAY_MIR_POLICY_NEGATE,
    MMAY_MIR_POLICY_SHIFT_LEFT,
    MMAY_MIR_POLICY_SHIFT_RIGHT
} MarimayMirIntegerOperation;
typedef struct MarimayMirInstruction {
    MarimayMirInstructionKind kind;
    MarimayMirRegister target, first, second, third;
    MarimayMirCallId call;
    MarimayMirRecordId record;
    MarimayMirRecordConstructionId record_construction;
    MarimayMirEnumId enum_id;
    MarimayMirEnumConstructionId enum_construction;
    MarimayMirStringId string_constant;
    MarimayMirArrayId array;
    MarimayMirArrayConstructionId array_construction;
    MarimayMirInterfaceId interface_id;
    MarimayMirInterfaceConstructionId interface_construction;
    MarimayMirIntegerPolicy integer_policy;
    MarimayMirIntegerOperation integer_operation;
    MarimayTypeId integer_target_type;
    size_t variant;
    size_t field;
    int64_t integer;
    uint64_t integer_bits;
    uint64_t f64_bits;
    bool boolean;
    MarimayMirSpan span;
} MarimayMirInstruction;
typedef enum MarimayMirTerminatorKind {
    MMAY_MIR_TERMINATOR_UNSET,
    MMAY_MIR_RETURN,
    MMAY_MIR_GOTO,
    MMAY_MIR_BRANCH,
    MMAY_MIR_PANIC,
    MMAY_MIR_CALL_NEVER
} MarimayMirTerminatorKind;
typedef struct MarimayMirTerminator {
    MarimayMirTerminatorKind kind;
    MarimayMirRegister value;
    MarimayMirCallId call;
    MarimayMirBlockId target, otherwise;
    MarimayMirSpan span;
} MarimayMirTerminator;
#endif
