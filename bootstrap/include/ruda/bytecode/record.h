#ifndef RUDA_BYTECODE_RECORD_H
#define RUDA_BYTECODE_RECORD_H

#include "ruda/bytecode/function.h"

typedef uint32_t RudaBytecodeRecordId;
typedef uint32_t RudaBytecodeRecordConstructionId;

#define RUDA_BYTECODE_NO_RECORD UINT32_MAX
#define RUDA_BYTECODE_NO_RECORD_CONSTRUCTION UINT32_MAX

typedef struct RudaBytecodeRecordField {
    RudaBytecodeType type;
    uint32_t layout;
    bool is_mutable;
} RudaBytecodeRecordField;

typedef struct RudaBytecodeRecordLayout {
    RudaSymbolId symbol;
    size_t field_start;
    size_t field_count;
} RudaBytecodeRecordLayout;

typedef struct RudaBytecodeRecordArgument {
    uint32_t register_id;
    RudaBytecodeType type;
    uint32_t layout;
} RudaBytecodeRecordArgument;

typedef struct RudaBytecodeRecordConstruction {
    RudaBytecodeRecordId record;
    size_t argument_start;
    size_t argument_count;
} RudaBytecodeRecordConstruction;

bool ruda_bytecode_add_record_layout(
    RudaBytecodeFunction *function,
    RudaSymbolId symbol,
    const RudaBytecodeRecordField *fields,
    size_t field_count,
    RudaBytecodeRecordId *result
);
bool ruda_bytecode_add_record_construction(
    RudaBytecodeFunction *function,
    RudaBytecodeRecordId record,
    const RudaBytecodeRecordArgument *arguments,
    size_t argument_count,
    RudaBytecodeRecordConstructionId *result
);
const RudaBytecodeRecordLayout *ruda_bytecode_record_layout(
    const RudaBytecodeFunction *function,
    RudaBytecodeRecordId record
);
const RudaBytecodeRecordField *ruda_bytecode_record_field(
    const RudaBytecodeFunction *function,
    const RudaBytecodeRecordLayout *layout,
    size_t field
);
const RudaBytecodeRecordConstruction *ruda_bytecode_record_construction(
    const RudaBytecodeFunction *function,
    RudaBytecodeRecordConstructionId construction
);

#endif
