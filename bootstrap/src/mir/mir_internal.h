#ifndef MMAY_MIR_INTERNAL_H
#define MMAY_MIR_INTERNAL_H
#include "mmay/mir/mir.h"
#include <stddef.h>
#include <stdint.h>
enum {
    MMAY_MIR_MAX_DATAFLOW_CELLS = 16000000
};

typedef enum MarimayMirManagedLayoutKind {
    MMAY_MIR_MANAGED_LAYOUT_RECORD,
    MMAY_MIR_MANAGED_LAYOUT_ENUM,
    MMAY_MIR_MANAGED_LAYOUT_ARRAY,
    MMAY_MIR_MANAGED_LAYOUT_COLLECTION,
    MMAY_MIR_MANAGED_LAYOUT_INTERFACE
} MarimayMirManagedLayoutKind;

typedef struct MarimayMirManagedLayoutInfo {
    MarimayMirManagedLayoutKind kind;
    uint32_t layout;
} MarimayMirManagedLayoutInfo;

extern const uint64_t mmay_mir_checked_index_top;
extern const uint64_t mmay_mir_checked_index_none;
typedef struct MarimayMirManagedRecord {
    MarimayTypeId type;
    size_t field_count;
    size_t allocation_size;
    MarimayMirValue fields[];
} MarimayMirManagedRecord;
typedef struct MarimayMirManagedEnum {
    MarimayTypeId type;
    size_t variant;
    size_t variant_count;
    size_t selected_payload_count;
    size_t layout_payload_count;
    size_t allocation_size;
    MarimayMirValue payload[];
} MarimayMirManagedEnum;
typedef struct MarimayMirManagedString {
    size_t allocation_size;
    size_t length;
    unsigned char bytes[];
} MarimayMirManagedString;
typedef struct MarimayMirManagedArray {
    MarimayTypeId type;
    MarimayTypeId element_type;
    size_t allocation_size;
    size_t length;
    size_t capacity;
    uint64_t mutation_generation;
    MarimayMirValue elements[];
} MarimayMirManagedArray;
typedef struct MarimayMirManagedInterface {
    MarimayTypeId type;
    MarimayMirValue payload;
    size_t method_count;
    size_t allocation_size;
    MarimaySymbolId methods[];
} MarimayMirManagedInterface;
MarimayMirVerifyStatus mmay_mir_verify_defer_control_flow(
    const MarimayMirFunction *function,
    size_t *work
);
MarimayTypeId mmay_mir_register_type(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id
);
bool mmay_mir_register_is(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id,
    MarimayTypeId type
);
const MarimayMirCall *mmay_mir_call(
    const MarimayMirFunction *function,
    MarimayMirCallId call_id
);
const MarimayMirCallArgument *mmay_mir_call_argument(
    const MarimayMirFunction *function,
    size_t index
);
const MarimayMirRecord *mmay_mir_record(
    const MarimayMirFunction *function,
    MarimayMirRecordId record_id
);
const MarimayMirRecordField *mmay_mir_record_field(
    const MarimayMirFunction *function,
    const MarimayMirRecord *record,
    size_t field
);
const MarimayMirRecordConstruction *mmay_mir_record_construction(
    const MarimayMirFunction *function,
    MarimayMirRecordConstructionId construction_id
);
const MarimayMirRecordArgument *mmay_mir_record_argument(
    const MarimayMirFunction *function,
    size_t index
);
const MarimayMirEnum *mmay_mir_enum(
    const MarimayMirFunction *function,
    MarimayMirEnumId enum_id
);
const MarimayMirEnumVariant *mmay_mir_enum_variant(
    const MarimayMirFunction *function,
    const MarimayMirEnum *enum_layout,
    size_t variant
);
const MarimayMirEnumPayload *mmay_mir_enum_payload(
    const MarimayMirFunction *function,
    const MarimayMirEnumVariant *variant,
    size_t payload
);
const MarimayMirEnumConstruction *mmay_mir_enum_construction(
    const MarimayMirFunction *function,
    MarimayMirEnumConstructionId construction_id
);
const MarimayMirEnumArgument *mmay_mir_enum_argument(
    const MarimayMirFunction *function,
    size_t index
);
const MarimayMirStringConstant *mmay_mir_string_constant(
    const MarimayMirFunction *function,
    MarimayMirStringId string_id
);
const MarimayMirArray *mmay_mir_array(
    const MarimayMirFunction *function,
    MarimayMirArrayId array_id
);
const MarimayMirArrayConstruction *mmay_mir_array_construction(
    const MarimayMirFunction *function,
    MarimayMirArrayConstructionId construction_id
);
const MarimayMirArrayArgument *mmay_mir_array_argument(
    const MarimayMirFunction *function,
    size_t index
);
const MarimayMirInterface *mmay_mir_interface(
    const MarimayMirFunction *function,
    MarimayMirInterfaceId interface_id
);
const MarimayMirInterfaceConstruction *mmay_mir_interface_construction(
    const MarimayMirFunction *function,
    MarimayMirInterfaceConstructionId construction_id
);
const MarimaySymbolId *mmay_mir_interface_method(
    const MarimayMirFunction *function,
    size_t index
);
MarimayMirVerifyStatus mmay_mir_validate_instruction_types(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMap *managed_types
);
MarimayMirVerifyStatus mmay_mir_verify_numeric_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
);
MarimayMirVerifyStatus mmay_mir_verify_io_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
);
MarimayMirVerifyStatus mmay_mir_verify_format_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
);
MarimayMirVerifyStatus mmay_mir_verify_parse_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
);
MarimayMirInterpretStatus mmay_mir_execute_format_instruction(
    const MarimayMirInstruction *instruction, const MarimayMirValue *source,
    MarimayMirValue *target, MarimayMirInterpretOptions options);
MarimayMirInterpretStatus mmay_mir_execute_string_concat_instruction(
    const MarimayMirValue *left, const MarimayMirValue *right,
    MarimayMirValue *target, MarimayMirInterpretOptions options);
MarimayMirInterpretStatus mmay_mir_execute_io_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *source,
    MarimayMirValue *target,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
);
MarimayMirInterpretStatus mmay_mir_execute_parse_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *text,
    const MarimayMirValue *radix,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
);
MarimayMirInterpretStatus mmay_mir_execute_test_output_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *source,
    const MarimayMirValue *message,
    MarimayMirValue *target,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
);
bool mmay_mir_instruction_uses_first(MarimayMirInstructionKind kind);
bool mmay_mir_instruction_uses_second(MarimayMirInstructionKind kind);
bool mmay_mir_instruction_uses_third(MarimayMirInstructionKind kind);
bool mmay_mir_assigned_register(
    const unsigned char *assigned,
    size_t register_count,
    MarimayMirRegister register_id
);
void mmay_mir_transfer_assignments(
    const MarimayMirBlock *block,
    unsigned char *assigned,
    size_t register_count
);
void mmay_mir_transfer_checked_index_instruction(
    const MarimayMirInstruction *instruction,
    uint64_t *checked,
    size_t register_count
);
void mmay_mir_transfer_checked_indices(
    const MarimayMirBlock *block,
    uint64_t *checked,
    size_t register_count
);
uint64_t mmay_mir_meet_checked_index(uint64_t left, uint64_t right);
bool mmay_mir_consume_verify_work(
    size_t *used,
    size_t amount,
    size_t limit
);
MarimayMirVerifyStatus mmay_mir_verify_managed_layouts(
    const MarimayMirFunction *function,
    MarimayMap *managed_types,
    size_t *work
);
MarimayMirVerifyStatus mmay_mir_verify_interface_layouts(
    const MarimayMirFunction *function,
    MarimayMap *managed_types,
    size_t *work
);
MarimayMirVerifyStatus mmay_mir_verify_range_proofs(
    const MarimayMirFunction *function,
    size_t *work
);
MarimayMirVerifyStatus mmay_mir_verify_integer_range_proofs(
    const MarimayMirFunction *function,
    size_t *work
);
MarimayMirRangeOptimizeStatus mmay_mir_optimize_integer_ranges(
    MarimayMirFunction *function
);
typedef struct MarimayMirRangeAnalysis {
    const MarimayMirFunction *function;
    unsigned char *incoming_nonnegative;
    unsigned char *scratch;
    size_t incoming_bytes;
    size_t scratch_bytes;
} MarimayMirRangeAnalysis;
MarimayMirVerifyStatus mmay_mir_range_analysis_init(
    const MarimayMirFunction *function,
    size_t *work,
    MarimayMirRangeAnalysis *analysis
);
void mmay_mir_range_analysis_dispose(MarimayMirRangeAnalysis *analysis);
bool mmay_mir_range_assumption_valid(
    MarimayMirRangeAnalysis *analysis,
    const MarimayMirFunction *function,
    MarimayMirBlockId block_id,
    size_t instruction_index,
    const MarimayMirInstruction *instruction
);
const MarimayMirManagedEnum *mmay_mir_execution_enum_value(
    const MarimayMirExecution *execution,
    MarimayMirValue value
);
const MarimayMirManagedString *mmay_mir_execution_string_value(
    const MarimayMirExecution *execution,
    MarimayMirValue value
);
MarimayMirManagedArray *mmay_mir_execution_array_value_mutable(
    MarimayMirExecution *execution,
    MarimayMirValue value
);
MarimayMirManagedArray **mmay_mir_execution_array_slot(
    MarimayMirExecution *execution,
    MarimayMirValue value
);
const MarimayMirManagedArray *mmay_mir_execution_array_value(
    const MarimayMirExecution *execution,
    MarimayMirValue value
);
const MarimayMirManagedInterface *mmay_mir_execution_interface_value(
    const MarimayMirExecution *execution,
    MarimayMirValue value
);
MarimayMirManagedRecord *mmay_mir_execution_record(
    MarimayMirExecution *execution,
    MarimayMirReference reference
);
bool mmay_mir_managed_record_matches_layout(
    const MarimayMirFunction *function,
    const MarimayMirRecord *layout,
    const MarimayMirManagedRecord *record
);
bool mmay_mir_type_is_managed(MarimayTypeId type);
bool mmay_mir_managed_array_matches_layout(
    const MarimayMirArray *layout,
    const MarimayMirManagedArray *value
);
bool mmay_mir_managed_enum_matches_layout(
    const MarimayMirFunction *function,
    const MarimayMirEnum *layout,
    const MarimayMirManagedEnum *value
);
MarimayMirInterpretStatus mmay_mir_execution_new_record(
    MarimayMirExecution *execution,
    MarimayTypeId type,
    const MarimayMirValue *fields,
    const MarimayMirRecordField *layout,
    size_t field_count,
    MarimayMirReference *result
);
MarimayMirInterpretStatus mmay_mir_execution_new_string(
    MarimayMirExecution *execution,
    const unsigned char *bytes,
    size_t length,
    MarimayMirReference *result
);
MarimayMirInterpretStatus mmay_mir_execution_new_enum(
    MarimayMirExecution *execution,
    const MarimayMirFunction *function,
    const MarimayMirEnum *enum_layout,
    size_t selected_variant,
    const MarimayMirValue *payload_values,
    size_t payload_value_count,
    MarimayMirReference *result
);
MarimayMirInterpretStatus mmay_mir_execute_numeric_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *first,
    const MarimayMirValue *second,
    MarimayMirValue *target,
    MarimayMirInterpretOptions options
);
MarimayMirInterpretStatus mmay_mir_execution_array_preflight(
    MarimayMirExecution *execution,
    size_t length,
    size_t *new_total,
    size_t *element_bytes,
    size_t *allocation_size
);
MarimayMirInterpretStatus mmay_mir_execution_new_array(
    MarimayMirExecution *execution,
    const MarimayMirArray *layout,
    const MarimayMirValue *elements,
    size_t length,
    MarimayMirReference *result
);
MarimayMirInterpretStatus mmay_mir_execution_array_push(
    MarimayMirExecution *execution,
    const MarimayMirArray *layout,
    MarimayMirValue array,
    MarimayMirValue element
);
MarimayMirInterpretStatus mmay_mir_execute_array_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *first,
    const MarimayMirValue *second,
    const MarimayMirValue *third,
    MarimayMirValue *target,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
);
MarimayMirInterpretStatus mmay_mir_execution_new_interface(
    MarimayMirExecution *execution,
    MarimayTypeId type,
    MarimayMirValue payload,
    const MarimaySymbolId *methods,
    size_t method_count,
    MarimayMirReference *result
);
MarimayMirInterpretStatus mmay_mir_invoke_call(
    const MarimayMirFunction *function, MarimayMirCallId call_id,
    const MarimayMirValue *registers, size_t register_count,
    const MarimayAllocator *allocator, MarimayMirInterpretOptions options,
    const MarimaySymbolId *selected_callee,
    const MarimayMirValue *selected_receiver, MarimayMirValue *result
);
MarimayMirInterpretStatus mmay_mir_invoke_deferred_call(
    const MarimayMirFunction *function, MarimayMirCallId call_id,
    const MarimayMirValue *registers, size_t register_count,
    const MarimayAllocator *allocator, MarimayMirInterpretOptions options
);
bool mmay_mir_panic_append_suppressed(
    MarimayMirPanic *primary, MarimayMirPanic *secondary, MarimayAllocator allocator
);
#endif
