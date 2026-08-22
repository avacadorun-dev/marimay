#ifndef MMAY_BYTECODE_ENUM_H
#define MMAY_BYTECODE_ENUM_H

#include "mmay/bytecode/function.h"

typedef uint32_t MarimayBytecodeEnumId;
typedef uint32_t MarimayBytecodeEnumConstructionId;

#define MMAY_BYTECODE_NO_ENUM UINT32_MAX
#define MMAY_BYTECODE_NO_ENUM_CONSTRUCTION UINT32_MAX

typedef struct MarimayBytecodeEnumPayload {
    MarimayBytecodeType type;
    uint32_t layout;
} MarimayBytecodeEnumPayload;

typedef struct MarimayBytecodeEnumVariant {
    size_t payload_start;
    size_t payload_count;
} MarimayBytecodeEnumVariant;

typedef struct MarimayBytecodeEnumLayout {
    MarimaySymbolId symbol;
    size_t variant_start;
    size_t variant_count;
    size_t payload_start;
    size_t payload_count;
} MarimayBytecodeEnumLayout;

typedef struct MarimayBytecodeEnumArgument {
    uint32_t register_id;
    MarimayBytecodeType type;
    uint32_t layout;
} MarimayBytecodeEnumArgument;

typedef struct MarimayBytecodeEnumConstruction {
    MarimayBytecodeEnumId enum_id;
    size_t variant;
    size_t argument_start;
    size_t argument_count;
} MarimayBytecodeEnumConstruction;

bool mmay_bytecode_add_enum_layout(
    MarimayBytecodeFunction *function,
    MarimaySymbolId symbol,
    const MarimayBytecodeEnumVariant *variants,
    size_t variant_count,
    const MarimayBytecodeEnumPayload *payloads,
    size_t payload_count,
    MarimayBytecodeEnumId *result
);
bool mmay_bytecode_add_enum_construction(
    MarimayBytecodeFunction *function,
    MarimayBytecodeEnumId enum_id,
    size_t variant,
    const MarimayBytecodeEnumArgument *arguments,
    size_t argument_count,
    MarimayBytecodeEnumConstructionId *result
);
const MarimayBytecodeEnumLayout *mmay_bytecode_enum_layout(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeEnumId enum_id
);
const MarimayBytecodeEnumConstruction *mmay_bytecode_enum_construction(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeEnumConstructionId construction
);

#endif
