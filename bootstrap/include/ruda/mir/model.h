#ifndef RUDA_MIR_MODEL_H
#define RUDA_MIR_MODEL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ruda/types/model.h"
typedef uint32_t RudaMirRegister;
typedef uint32_t RudaMirBlockId;
typedef uint32_t RudaMirCallId;
typedef uint32_t RudaMirRecordId;
typedef uint32_t RudaMirRecordConstructionId;
typedef uint32_t RudaMirEnumId;
typedef uint32_t RudaMirEnumConstructionId;
typedef uint32_t RudaMirStringId;
typedef uint32_t RudaMirArrayId;
typedef uint32_t RudaMirArrayConstructionId;
typedef uint32_t RudaMirInterfaceId;
typedef uint32_t RudaMirInterfaceConstructionId;
#define RUDA_MIR_NO_REGISTER UINT32_MAX
#define RUDA_MIR_NO_BLOCK UINT32_MAX
#define RUDA_MIR_NO_CALL UINT32_MAX
#define RUDA_MIR_NO_RECORD UINT32_MAX
#define RUDA_MIR_NO_RECORD_CONSTRUCTION UINT32_MAX
#define RUDA_MIR_NO_ENUM UINT32_MAX
#define RUDA_MIR_NO_ENUM_CONSTRUCTION UINT32_MAX
#define RUDA_MIR_NO_STRING UINT32_MAX
#define RUDA_MIR_NO_ARRAY UINT32_MAX
#define RUDA_MIR_NO_ARRAY_CONSTRUCTION UINT32_MAX
#define RUDA_MIR_NO_INTERFACE UINT32_MAX
#define RUDA_MIR_NO_INTERFACE_CONSTRUCTION UINT32_MAX
typedef struct RudaMirSpan {
    size_t start;
    size_t end;
} RudaMirSpan;

typedef enum RudaMirInstructionKind {
    RUDA_MIR_CONSTANT_I64,
    RUDA_MIR_CONSTANT_INTEGER,
    RUDA_MIR_CONSTANT_BOOL,
    RUDA_MIR_CONSTANT_STRING,
    RUDA_MIR_COPY,
    RUDA_MIR_ADD_CHECKED_I64,
    RUDA_MIR_SUBTRACT_CHECKED_I64,
    RUDA_MIR_MULTIPLY_CHECKED_I64,
    RUDA_MIR_DIVIDE_CHECKED_I64,
    RUDA_MIR_REMAINDER_CHECKED_I64,
    RUDA_MIR_NEGATE_CHECKED_I64,
    RUDA_MIR_NOT_BOOL,
    RUDA_MIR_BITWISE_NOT_I64,
    RUDA_MIR_BITWISE_AND_I64,
    RUDA_MIR_BITWISE_XOR_I64,
    RUDA_MIR_BITWISE_OR_I64,
    RUDA_MIR_SHIFT_LEFT_CHECKED_I64,
    RUDA_MIR_SHIFT_RIGHT_I64,
    RUDA_MIR_EQUAL_I64,
    RUDA_MIR_EQUAL_BOOL,
    RUDA_MIR_EQUAL_STRING,
    RUDA_MIR_LESS_I64,
    RUDA_MIR_LESS_EQUAL_I64,
    RUDA_MIR_RECORD_NEW,
    RUDA_MIR_RECORD_GET,
    RUDA_MIR_RECORD_SET,
    RUDA_MIR_RECORD_IDENTITY_SAME,
    RUDA_MIR_ENUM_NEW,
    RUDA_MIR_ENUM_IS_VARIANT,
    RUDA_MIR_ENUM_REQUIRE_VARIANT,
    RUDA_MIR_ENUM_GET_PAYLOAD,
    RUDA_MIR_CALL,
    RUDA_MIR_CONSTANT_F64,
    RUDA_MIR_ADD_F64,
    RUDA_MIR_SUBTRACT_F64,
    RUDA_MIR_MULTIPLY_F64,
    RUDA_MIR_DIVIDE_F64,
    RUDA_MIR_NEGATE_F64,
    RUDA_MIR_EQUAL_F64,
    RUDA_MIR_LESS_F64,
    RUDA_MIR_LESS_EQUAL_F64,
    RUDA_MIR_ARRAY_NEW,
    RUDA_MIR_ARRAY_LENGTH,
    /*
     * Bounds-check barrier used before evaluating an indexed assignment RHS.
     * On success it copies the array handle into target; on failure it raises
     * G4002 at this instruction's full access span.
     */
    RUDA_MIR_ARRAY_REQUIRE_INDEX,
    RUDA_MIR_ARRAY_GET,
    RUDA_MIR_ARRAY_SET,
    RUDA_MIR_ARRAY_PUSH,
    RUDA_MIR_INTERFACE_NEW,
    RUDA_MIR_CALL_INTERFACE,
    RUDA_MIR_INTEGER_POLICY,
    RUDA_MIR_INTEGER_CONVERT,
    RUDA_MIR_FORMAT_F32,
    RUDA_MIR_FORMAT_F64,
    RUDA_MIR_DEFER_REGISTER,
    RUDA_MIR_DEFER_CALL
} RudaMirInstructionKind;

typedef enum RudaMirIntegerPolicy {
    RUDA_MIR_INTEGER_CHECKED_OPTION,
    RUDA_MIR_INTEGER_WRAPPING,
    RUDA_MIR_INTEGER_SATURATING
} RudaMirIntegerPolicy;
typedef enum RudaMirIntegerOperation {
    RUDA_MIR_POLICY_ADD,
    RUDA_MIR_POLICY_SUBTRACT,
    RUDA_MIR_POLICY_MULTIPLY,
    RUDA_MIR_POLICY_DIVIDE,
    RUDA_MIR_POLICY_REMAINDER,
    RUDA_MIR_POLICY_NEGATE,
    RUDA_MIR_POLICY_SHIFT_LEFT,
    RUDA_MIR_POLICY_SHIFT_RIGHT
} RudaMirIntegerOperation;
typedef struct RudaMirInstruction {
    RudaMirInstructionKind kind;
    RudaMirRegister target;
    RudaMirRegister first;
    RudaMirRegister second;
    RudaMirRegister third;
    RudaMirCallId call;
    RudaMirRecordId record;
    RudaMirRecordConstructionId record_construction;
    RudaMirEnumId enum_id;
    RudaMirEnumConstructionId enum_construction;
    RudaMirStringId string_constant;
    RudaMirArrayId array;
    RudaMirArrayConstructionId array_construction;
    RudaMirInterfaceId interface_id;
    RudaMirInterfaceConstructionId interface_construction;
    RudaMirIntegerPolicy integer_policy;
    RudaMirIntegerOperation integer_operation;
    RudaTypeId integer_target_type;
    size_t variant;
    size_t field;
    int64_t integer;
    uint64_t integer_bits;
    uint64_t f64_bits;
    bool boolean;
    RudaMirSpan span;
} RudaMirInstruction;

typedef enum RudaMirTerminatorKind {
    RUDA_MIR_TERMINATOR_UNSET,
    RUDA_MIR_RETURN,
    RUDA_MIR_GOTO,
    RUDA_MIR_BRANCH,
    RUDA_MIR_PANIC,
    RUDA_MIR_CALL_NEVER
} RudaMirTerminatorKind;

typedef struct RudaMirTerminator {
    RudaMirTerminatorKind kind;
    RudaMirRegister value;
    RudaMirCallId call;
    RudaMirBlockId target;
    RudaMirBlockId otherwise;
    RudaMirSpan span;
} RudaMirTerminator;

#endif
