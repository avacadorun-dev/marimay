#ifndef RUDA_MIR_MIR_H
#define RUDA_MIR_MIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/vector.h"
#include "ruda/types/type.h"

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
/*
 * Context-bound handle into a RudaMirExecution object graph.  Execution
 * provenance rejects foreign and stale handles; slot stores a kind-tagged
 * arena index and is never a host pointer.
 */
typedef struct RudaMirReference {
    uint64_t execution_provenance;
    uint32_t slot;
} RudaMirReference;

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
#define RUDA_MIR_INVALID_REFERENCE \
    ((RudaMirReference){UINT64_MAX, UINT32_MAX})

typedef struct RudaMirSpan {
    size_t start;
    size_t end;
} RudaMirSpan;

typedef enum RudaMirInstructionKind {
    RUDA_MIR_CONSTANT_I64,
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
    RUDA_MIR_ARRAY_SET
} RudaMirInstructionKind;

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
    size_t variant;
    size_t field;
    int64_t integer;
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
    size_t argument_start;
    size_t argument_count;
} RudaMirCall;

typedef struct RudaMirRecordField {
    RudaTypeId type;
    bool is_mutable;
} RudaMirRecordField;

typedef struct RudaMirRecord {
    RudaTypeId type;
    size_t field_start;
    size_t field_count;
} RudaMirRecord;

typedef struct RudaMirRecordArgument {
    RudaMirRegister register_id;
    RudaTypeId type;
} RudaMirRecordArgument;

typedef struct RudaMirRecordConstruction {
    RudaMirRecordId record;
    size_t argument_start;
    size_t argument_count;
} RudaMirRecordConstruction;

typedef enum RudaMirEnumVariantKind {
    RUDA_MIR_ENUM_VARIANT_UNIT,
    RUDA_MIR_ENUM_VARIANT_POSITIONAL,
    RUDA_MIR_ENUM_VARIANT_NAMED
} RudaMirEnumVariantKind;

typedef struct RudaMirEnumPayload {
    RudaTypeId type;
} RudaMirEnumPayload;

typedef struct RudaMirEnumVariant {
    RudaMirEnumVariantKind kind;
    size_t payload_start;
    size_t payload_count;
} RudaMirEnumVariant;

typedef struct RudaMirEnum {
    RudaTypeId type;
    size_t variant_start;
    size_t variant_count;
    size_t payload_start;
    size_t payload_count;
} RudaMirEnum;

typedef struct RudaMirEnumArgument {
    RudaMirRegister register_id;
    RudaTypeId type;
} RudaMirEnumArgument;

typedef struct RudaMirEnumConstruction {
    RudaMirEnumId enum_id;
    size_t variant;
    size_t argument_start;
    size_t argument_count;
} RudaMirEnumConstruction;

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
    size_t parameter_count;
    size_t instruction_count;
    size_t string_bytes;
    RudaMirFunctionOptions options;
    bool initialized;
} RudaMirFunction;

typedef enum RudaMirVerifyStatus {
    RUDA_MIR_VERIFY_OK,
    RUDA_MIR_VERIFY_INVALID_ARGUMENT,
    RUDA_MIR_VERIFY_ALLOCATION_FAILED,
    RUDA_MIR_VERIFY_LIMIT_REACHED,
    RUDA_MIR_VERIFY_INVALID_CONTROL_FLOW,
    RUDA_MIR_VERIFY_INVALID_REGISTER,
    RUDA_MIR_VERIFY_UNINITIALIZED_REGISTER,
    RUDA_MIR_VERIFY_TYPE_MISMATCH
} RudaMirVerifyStatus;

typedef struct RudaMirValue {
    RudaTypeId type;
    union {
        int64_t i64;
        uint64_t f64_bits;
        bool boolean;
        RudaMirReference reference;
    } as;
} RudaMirValue;

typedef struct RudaMirExecutionOptions {
    size_t max_objects;
    size_t max_fields_per_object;
    size_t max_total_fields;
    size_t max_strings;
    size_t max_total_string_bytes;
    size_t max_arrays;
    size_t max_array_length;
    size_t max_total_array_elements;
} RudaMirExecutionOptions;

typedef struct RudaMirExecution {
    RudaAllocator allocator;
    /* Oracle-owned record descriptors, retained until explicit disposal. */
    RudaVector objects;
    /* Oracle-owned immutable enum descriptors, indexed separately by type. */
    RudaVector enums;
    /* Oracle-owned immutable UTF-8 strings. */
    RudaVector strings;
    /* Oracle-owned mutable arrays, indexed separately by exact type. */
    RudaVector arrays;
    RudaMirExecutionOptions options;
    size_t total_fields;
    size_t total_string_bytes;
    size_t total_array_elements;
    uint64_t provenance;
    bool initialized;
} RudaMirExecution;

typedef enum RudaMirInterpretStatus {
    RUDA_MIR_INTERPRET_OK,
    RUDA_MIR_INTERPRET_INVALID_ARGUMENT,
    RUDA_MIR_INTERPRET_ALLOCATION_FAILED,
    RUDA_MIR_INTERPRET_INVALID_MIR,
    RUDA_MIR_INTERPRET_STEP_LIMIT,
    RUDA_MIR_INTERPRET_OVERFLOW,
    RUDA_MIR_INTERPRET_DIVISION_BY_ZERO,
    RUDA_MIR_INTERPRET_INVALID_SHIFT,
    RUDA_MIR_INTERPRET_PANIC,
    RUDA_MIR_INTERPRET_INVALID_REFERENCE,
    RUDA_MIR_INTERPRET_OBJECT_LIMIT,
    RUDA_MIR_INTERPRET_UNSUPPORTED_HOST
} RudaMirInterpretStatus;

typedef struct RudaMirPanic {
    /* Stable explicit-source-panic code from the runtime diagnostic contract. */
    const char *code;
    RudaOwnedBytes message;
    RudaSymbolId symbol;
    RudaMirSpan span;
} RudaMirPanic;

typedef RudaMirInterpretStatus (*RudaMirCallHandler)(
    void *context,
    RudaSymbolId callee,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
);

typedef struct RudaMirInterpretOptions {
    size_t max_steps;
    size_t *remaining_shared_steps;
    void *call_context;
    RudaMirCallHandler call;
    RudaMirExecution *execution;
    /* Optional caller-owned zero-initialized output; prior content is disposed. */
    RudaMirPanic *panic;
} RudaMirInterpretOptions;

RudaMirFunctionOptions ruda_mir_function_default_options(void);
RudaMirInterpretOptions ruda_mir_interpret_default_options(void);
RudaMirExecutionOptions ruda_mir_execution_default_options(void);
bool ruda_mir_reference_is_invalid(RudaMirReference reference);
bool ruda_mir_reference_equal(
    RudaMirReference left,
    RudaMirReference right
);

bool ruda_mir_execution_init(
    RudaMirExecution *execution,
    RudaAllocator allocator,
    RudaMirExecutionOptions options
);
/* Invalidates every RudaMirReference owned by this execution. */
void ruda_mir_execution_dispose(RudaMirExecution *execution);
/* Internal oracle inspection; no corresponding safe-source tag operation. */
bool ruda_mir_execution_inspect_enum(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t *variant,
    size_t *payload_count
);
bool ruda_mir_execution_enum_payload(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t index,
    RudaMirValue *result
);
bool ruda_mir_execution_string_bytes(
    const RudaMirExecution *execution,
    RudaMirValue value,
    const unsigned char **bytes,
    size_t *length
);
bool ruda_mir_execution_array_length(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t *length
);
bool ruda_mir_execution_array_element(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t index,
    RudaMirValue *result
);
/* Internal oracle inspection by allocation order. */
bool ruda_mir_execution_array_at(
    const RudaMirExecution *execution,
    size_t ordinal,
    RudaMirValue *result
);
/* Internal boundary validation for context-bound managed handles. */
bool ruda_mir_execution_owns_reference(
    const RudaMirExecution *execution,
    RudaMirValue value
);

bool ruda_mir_function_init(
    RudaMirFunction *function,
    RudaAllocator allocator,
    RudaSymbolId symbol,
    const RudaTypeId *parameter_types,
    size_t parameter_count,
    RudaTypeId result_type,
    RudaMirFunctionOptions options
);
void ruda_mir_function_dispose(RudaMirFunction *function);
bool ruda_mir_add_register(
    RudaMirFunction *function,
    RudaTypeId type,
    RudaMirRegister *result
);
bool ruda_mir_add_block(RudaMirFunction *function, RudaMirBlockId *result);
bool ruda_mir_add_call(
    RudaMirFunction *function,
    RudaSymbolId callee,
    RudaTypeId result_type,
    const RudaMirCallArgument *arguments,
    size_t argument_count,
    RudaMirCallId *result
);
bool ruda_mir_add_record(
    RudaMirFunction *function,
    RudaTypeId type,
    const RudaMirRecordField *fields,
    size_t field_count,
    RudaMirRecordId *result
);
bool ruda_mir_add_record_construction(
    RudaMirFunction *function,
    RudaMirRecordId record,
    const RudaMirRecordArgument *arguments,
    size_t argument_count,
    RudaMirRecordConstructionId *result
);
bool ruda_mir_add_enum(
    RudaMirFunction *function,
    RudaTypeId type,
    const RudaMirEnumVariant *variants,
    size_t variant_count,
    const RudaMirEnumPayload *payloads,
    size_t payload_count,
    RudaMirEnumId *result
);
bool ruda_mir_add_enum_construction(
    RudaMirFunction *function,
    RudaMirEnumId enum_id,
    size_t variant,
    const RudaMirEnumArgument *arguments,
    size_t argument_count,
    RudaMirEnumConstructionId *result
);
bool ruda_mir_add_string_constant(
    RudaMirFunction *function,
    const unsigned char *bytes,
    size_t length,
    RudaMirStringId *result
);
bool ruda_mir_add_array(
    RudaMirFunction *function,
    RudaTypeId type,
    RudaTypeId element_type,
    RudaMirArrayId *result
);
bool ruda_mir_add_array_construction(
    RudaMirFunction *function,
    RudaMirArrayId array,
    const RudaMirArrayArgument *arguments,
    size_t argument_count,
    RudaMirArrayConstructionId *result
);
bool ruda_mir_append_instruction(
    RudaMirFunction *function,
    RudaMirBlockId block,
    const RudaMirInstruction *instruction
);
bool ruda_mir_set_terminator(
    RudaMirFunction *function,
    RudaMirBlockId block,
    RudaMirTerminator terminator
);

RudaMirVerifyStatus ruda_mir_verify(const RudaMirFunction *function);
RudaMirInterpretStatus ruda_mir_interpret(
    const RudaMirFunction *function,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaMirInterpretOptions options,
    RudaMirValue *result
);
void ruda_mir_panic_dispose(RudaMirPanic *panic);

#endif
