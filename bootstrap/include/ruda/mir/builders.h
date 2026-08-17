#ifndef RUDA_MIR_BUILDERS_H
#define RUDA_MIR_BUILDERS_H

#include "ruda/mir/function.h"
#include "ruda/mir/nominal.h"

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
bool ruda_mir_set_call_interface_slot(
    RudaMirFunction *function,
    RudaMirCallId call,
    uint32_t slot
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
bool ruda_mir_add_enum_with_symbol(
    RudaMirFunction *function,
    RudaSymbolId symbol,
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
bool ruda_mir_add_interface(
    RudaMirFunction *function,
    RudaTypeId type,
    size_t method_count,
    RudaMirInterfaceId *result
);
bool ruda_mir_add_interface_construction(
    RudaMirFunction *function,
    RudaMirInterfaceId interface_id,
    RudaMirRegister payload,
    RudaTypeId payload_type,
    const RudaSymbolId *methods,
    size_t method_count,
    RudaMirInterfaceConstructionId *result
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

#endif
