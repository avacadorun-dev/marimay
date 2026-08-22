#ifndef MMAY_MIR_BUILDERS_H
#define MMAY_MIR_BUILDERS_H

#include "mmay/mir/function.h"
#include "mmay/mir/nominal.h"

bool mmay_mir_function_init(
    MarimayMirFunction *function,
    MarimayAllocator allocator,
    MarimaySymbolId symbol,
    const MarimayTypeId *parameter_types,
    size_t parameter_count,
    MarimayTypeId result_type,
    MarimayMirFunctionOptions options
);
void mmay_mir_function_dispose(MarimayMirFunction *function);
bool mmay_mir_add_register(
    MarimayMirFunction *function,
    MarimayTypeId type,
    MarimayMirRegister *result
);
bool mmay_mir_add_block(MarimayMirFunction *function, MarimayMirBlockId *result);
bool mmay_mir_add_call(
    MarimayMirFunction *function,
    MarimaySymbolId callee,
    MarimayTypeId result_type,
    const MarimayMirCallArgument *arguments,
    size_t argument_count,
    MarimayMirCallId *result
);
bool mmay_mir_set_call_interface_slot(
    MarimayMirFunction *function,
    MarimayMirCallId call,
    uint32_t slot
);
bool mmay_mir_add_record(
    MarimayMirFunction *function,
    MarimayTypeId type,
    const MarimayMirRecordField *fields,
    size_t field_count,
    MarimayMirRecordId *result
);
bool mmay_mir_add_record_construction(
    MarimayMirFunction *function,
    MarimayMirRecordId record,
    const MarimayMirRecordArgument *arguments,
    size_t argument_count,
    MarimayMirRecordConstructionId *result
);
bool mmay_mir_add_enum(
    MarimayMirFunction *function,
    MarimayTypeId type,
    const MarimayMirEnumVariant *variants,
    size_t variant_count,
    const MarimayMirEnumPayload *payloads,
    size_t payload_count,
    MarimayMirEnumId *result
);
bool mmay_mir_add_enum_with_symbol(
    MarimayMirFunction *function,
    MarimaySymbolId symbol,
    MarimayTypeId type,
    const MarimayMirEnumVariant *variants,
    size_t variant_count,
    const MarimayMirEnumPayload *payloads,
    size_t payload_count,
    MarimayMirEnumId *result
);
bool mmay_mir_add_enum_construction(
    MarimayMirFunction *function,
    MarimayMirEnumId enum_id,
    size_t variant,
    const MarimayMirEnumArgument *arguments,
    size_t argument_count,
    MarimayMirEnumConstructionId *result
);
bool mmay_mir_add_string_constant(
    MarimayMirFunction *function,
    const unsigned char *bytes,
    size_t length,
    MarimayMirStringId *result
);
bool mmay_mir_add_array(
    MarimayMirFunction *function,
    MarimayTypeId type,
    MarimayTypeId element_type,
    MarimayMirArrayId *result
);
bool mmay_mir_add_array_construction(
    MarimayMirFunction *function,
    MarimayMirArrayId array,
    const MarimayMirArrayArgument *arguments,
    size_t argument_count,
    MarimayMirArrayConstructionId *result
);
bool mmay_mir_add_interface(
    MarimayMirFunction *function,
    MarimayTypeId type,
    size_t method_count,
    MarimayMirInterfaceId *result
);
bool mmay_mir_add_interface_construction(
    MarimayMirFunction *function,
    MarimayMirInterfaceId interface_id,
    MarimayMirRegister payload,
    MarimayTypeId payload_type,
    const MarimaySymbolId *methods,
    size_t method_count,
    MarimayMirInterfaceConstructionId *result
);
bool mmay_mir_append_instruction(
    MarimayMirFunction *function,
    MarimayMirBlockId block,
    const MarimayMirInstruction *instruction
);
bool mmay_mir_set_terminator(
    MarimayMirFunction *function,
    MarimayMirBlockId block,
    MarimayMirTerminator terminator
);

#endif
