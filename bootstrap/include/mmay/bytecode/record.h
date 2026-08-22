#ifndef MMAY_BYTECODE_RECORD_H
#define MMAY_BYTECODE_RECORD_H

#include "mmay/bytecode/function.h"

typedef uint32_t MarimayBytecodeRecordId;
typedef uint32_t MarimayBytecodeRecordConstructionId;

#define MMAY_BYTECODE_NO_RECORD UINT32_MAX
#define MMAY_BYTECODE_NO_RECORD_CONSTRUCTION UINT32_MAX

typedef struct MarimayBytecodeRecordField {
    MarimayBytecodeType type;
    uint32_t layout;
    bool is_mutable;
} MarimayBytecodeRecordField;

typedef struct MarimayBytecodeRecordLayout {
    MarimaySymbolId symbol;
    size_t field_start;
    size_t field_count;
} MarimayBytecodeRecordLayout;

typedef struct MarimayBytecodeRecordArgument {
    uint32_t register_id;
    MarimayBytecodeType type;
    uint32_t layout;
} MarimayBytecodeRecordArgument;

typedef struct MarimayBytecodeRecordConstruction {
    MarimayBytecodeRecordId record;
    size_t argument_start;
    size_t argument_count;
} MarimayBytecodeRecordConstruction;

bool mmay_bytecode_add_record_layout(
    MarimayBytecodeFunction *function,
    MarimaySymbolId symbol,
    const MarimayBytecodeRecordField *fields,
    size_t field_count,
    MarimayBytecodeRecordId *result
);
bool mmay_bytecode_add_record_construction(
    MarimayBytecodeFunction *function,
    MarimayBytecodeRecordId record,
    const MarimayBytecodeRecordArgument *arguments,
    size_t argument_count,
    MarimayBytecodeRecordConstructionId *result
);
const MarimayBytecodeRecordLayout *mmay_bytecode_record_layout(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeRecordId record
);
const MarimayBytecodeRecordField *mmay_bytecode_record_field(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeRecordLayout *layout,
    size_t field
);
const MarimayBytecodeRecordConstruction *mmay_bytecode_record_construction(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeRecordConstructionId construction
);

#endif
