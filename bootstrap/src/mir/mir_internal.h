#ifndef RUDA_MIR_INTERNAL_H
#define RUDA_MIR_INTERNAL_H
#include "ruda/mir/mir.h"
#include <stddef.h>
#include <stdint.h>
enum {
    RUDA_MIR_MAX_DATAFLOW_CELLS = 16000000
};

typedef enum RudaMirManagedLayoutKind {
    RUDA_MIR_MANAGED_LAYOUT_RECORD,
    RUDA_MIR_MANAGED_LAYOUT_ENUM,
    RUDA_MIR_MANAGED_LAYOUT_ARRAY,
    RUDA_MIR_MANAGED_LAYOUT_INTERFACE
} RudaMirManagedLayoutKind;

typedef struct RudaMirManagedLayoutInfo {
    RudaMirManagedLayoutKind kind;
    uint32_t layout;
} RudaMirManagedLayoutInfo;

extern const uint64_t ruda_mir_checked_index_top;
extern const uint64_t ruda_mir_checked_index_none;
typedef struct RudaMirManagedRecord {
    RudaTypeId type;
    size_t field_count;
    size_t allocation_size;
    RudaMirValue fields[];
} RudaMirManagedRecord;
typedef struct RudaMirManagedEnum {
    RudaTypeId type;
    size_t variant;
    size_t variant_count;
    size_t selected_payload_count;
    size_t layout_payload_count;
    size_t allocation_size;
    RudaMirValue payload[];
} RudaMirManagedEnum;
typedef struct RudaMirManagedString {
    size_t allocation_size;
    size_t length;
    unsigned char bytes[];
} RudaMirManagedString;
typedef struct RudaMirManagedArray {
    RudaTypeId type;
    RudaTypeId element_type;
    size_t allocation_size;
    size_t length;
    RudaMirValue elements[];
} RudaMirManagedArray;
typedef struct RudaMirManagedInterface {
    RudaTypeId type;
    RudaMirValue payload;
    size_t method_count;
    size_t allocation_size;
    RudaSymbolId methods[];
} RudaMirManagedInterface;
RudaTypeId ruda_mir_register_type(
    const RudaMirFunction *function,
    RudaMirRegister register_id
);
bool ruda_mir_register_is(
    const RudaMirFunction *function,
    RudaMirRegister register_id,
    RudaTypeId type
);
const RudaMirCall *ruda_mir_call(
    const RudaMirFunction *function,
    RudaMirCallId call_id
);
const RudaMirCallArgument *ruda_mir_call_argument(
    const RudaMirFunction *function,
    size_t index
);
const RudaMirRecord *ruda_mir_record(
    const RudaMirFunction *function,
    RudaMirRecordId record_id
);
const RudaMirRecordField *ruda_mir_record_field(
    const RudaMirFunction *function,
    const RudaMirRecord *record,
    size_t field
);
const RudaMirRecordConstruction *ruda_mir_record_construction(
    const RudaMirFunction *function,
    RudaMirRecordConstructionId construction_id
);
const RudaMirRecordArgument *ruda_mir_record_argument(
    const RudaMirFunction *function,
    size_t index
);
const RudaMirEnum *ruda_mir_enum(
    const RudaMirFunction *function,
    RudaMirEnumId enum_id
);
const RudaMirEnumVariant *ruda_mir_enum_variant(
    const RudaMirFunction *function,
    const RudaMirEnum *enum_layout,
    size_t variant
);
const RudaMirEnumPayload *ruda_mir_enum_payload(
    const RudaMirFunction *function,
    const RudaMirEnumVariant *variant,
    size_t payload
);
const RudaMirEnumConstruction *ruda_mir_enum_construction(
    const RudaMirFunction *function,
    RudaMirEnumConstructionId construction_id
);
const RudaMirEnumArgument *ruda_mir_enum_argument(
    const RudaMirFunction *function,
    size_t index
);
const RudaMirStringConstant *ruda_mir_string_constant(
    const RudaMirFunction *function,
    RudaMirStringId string_id
);
const RudaMirArray *ruda_mir_array(
    const RudaMirFunction *function,
    RudaMirArrayId array_id
);
const RudaMirArrayConstruction *ruda_mir_array_construction(
    const RudaMirFunction *function,
    RudaMirArrayConstructionId construction_id
);
const RudaMirArrayArgument *ruda_mir_array_argument(
    const RudaMirFunction *function,
    size_t index
);
const RudaMirInterface *ruda_mir_interface(
    const RudaMirFunction *function,
    RudaMirInterfaceId interface_id
);
const RudaMirInterfaceConstruction *ruda_mir_interface_construction(
    const RudaMirFunction *function,
    RudaMirInterfaceConstructionId construction_id
);
const RudaSymbolId *ruda_mir_interface_method(
    const RudaMirFunction *function,
    size_t index
);
RudaMirVerifyStatus ruda_mir_validate_instruction_types(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    const RudaMap *managed_types
);
bool ruda_mir_instruction_uses_first(RudaMirInstructionKind kind);
bool ruda_mir_instruction_uses_second(RudaMirInstructionKind kind);
bool ruda_mir_instruction_uses_third(RudaMirInstructionKind kind);
bool ruda_mir_assigned_register(
    const unsigned char *assigned,
    size_t register_count,
    RudaMirRegister register_id
);
void ruda_mir_transfer_assignments(
    const RudaMirBlock *block,
    unsigned char *assigned,
    size_t register_count
);
void ruda_mir_transfer_checked_index_instruction(
    const RudaMirInstruction *instruction,
    uint64_t *checked,
    size_t register_count
);
void ruda_mir_transfer_checked_indices(
    const RudaMirBlock *block,
    uint64_t *checked,
    size_t register_count
);
uint64_t ruda_mir_meet_checked_index(uint64_t left, uint64_t right);
bool ruda_mir_consume_verify_work(
    size_t *used,
    size_t amount,
    size_t limit
);
RudaMirVerifyStatus ruda_mir_verify_managed_layouts(
    const RudaMirFunction *function,
    RudaMap *managed_types,
    size_t *work
);
RudaMirVerifyStatus ruda_mir_verify_interface_layouts(
    const RudaMirFunction *function,
    RudaMap *managed_types,
    size_t *work
);
const RudaMirManagedEnum *ruda_mir_execution_enum_value(
    const RudaMirExecution *execution,
    RudaMirValue value
);
const RudaMirManagedString *ruda_mir_execution_string_value(
    const RudaMirExecution *execution,
    RudaMirValue value
);
RudaMirManagedArray *ruda_mir_execution_array_value_mutable(
    RudaMirExecution *execution,
    RudaMirValue value
);
const RudaMirManagedArray *ruda_mir_execution_array_value(
    const RudaMirExecution *execution,
    RudaMirValue value
);
const RudaMirManagedInterface *ruda_mir_execution_interface_value(
    const RudaMirExecution *execution,
    RudaMirValue value
);
RudaMirManagedRecord *ruda_mir_execution_record(
    RudaMirExecution *execution,
    RudaMirReference reference
);
bool ruda_mir_managed_record_matches_layout(
    const RudaMirFunction *function,
    const RudaMirRecord *layout,
    const RudaMirManagedRecord *record
);
bool ruda_mir_type_is_managed(RudaTypeId type);
bool ruda_mir_managed_array_matches_layout(
    const RudaMirArray *layout,
    const RudaMirManagedArray *value
);
bool ruda_mir_managed_enum_matches_layout(
    const RudaMirFunction *function,
    const RudaMirEnum *layout,
    const RudaMirManagedEnum *value
);
RudaMirInterpretStatus ruda_mir_execution_new_record(
    RudaMirExecution *execution,
    RudaTypeId type,
    const RudaMirValue *fields,
    const RudaMirRecordField *layout,
    size_t field_count,
    RudaMirReference *result
);
RudaMirInterpretStatus ruda_mir_execution_new_string(
    RudaMirExecution *execution,
    const unsigned char *bytes,
    size_t length,
    RudaMirReference *result
);
RudaMirInterpretStatus ruda_mir_execution_new_enum(
    RudaMirExecution *execution,
    const RudaMirFunction *function,
    const RudaMirEnum *enum_layout,
    size_t selected_variant,
    const RudaMirValue *payload_values,
    size_t payload_value_count,
    RudaMirReference *result
);
RudaMirInterpretStatus ruda_mir_execution_array_preflight(
    RudaMirExecution *execution,
    size_t length,
    size_t *new_total,
    size_t *element_bytes,
    size_t *allocation_size
);
RudaMirInterpretStatus ruda_mir_execution_new_array(
    RudaMirExecution *execution,
    const RudaMirArray *layout,
    const RudaMirValue *elements,
    size_t length,
    RudaMirReference *result
);
RudaMirInterpretStatus ruda_mir_execution_new_interface(
    RudaMirExecution *execution,
    RudaTypeId type,
    RudaMirValue payload,
    const RudaSymbolId *methods,
    size_t method_count,
    RudaMirReference *result
);
#endif
