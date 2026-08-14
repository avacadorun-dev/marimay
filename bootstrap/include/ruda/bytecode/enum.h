#ifndef RUDA_BYTECODE_ENUM_H
#define RUDA_BYTECODE_ENUM_H

#include "ruda/bytecode/function.h"

typedef uint32_t RudaBytecodeEnumId;
typedef uint32_t RudaBytecodeEnumConstructionId;

#define RUDA_BYTECODE_NO_ENUM UINT32_MAX
#define RUDA_BYTECODE_NO_ENUM_CONSTRUCTION UINT32_MAX

typedef struct RudaBytecodeEnumPayload {
    RudaBytecodeType type;
    uint32_t layout;
} RudaBytecodeEnumPayload;

typedef struct RudaBytecodeEnumVariant {
    size_t payload_start;
    size_t payload_count;
} RudaBytecodeEnumVariant;

typedef struct RudaBytecodeEnumLayout {
    RudaSymbolId symbol;
    size_t variant_start;
    size_t variant_count;
    size_t payload_start;
    size_t payload_count;
} RudaBytecodeEnumLayout;

typedef struct RudaBytecodeEnumArgument {
    uint32_t register_id;
    RudaBytecodeType type;
    uint32_t layout;
} RudaBytecodeEnumArgument;

typedef struct RudaBytecodeEnumConstruction {
    RudaBytecodeEnumId enum_id;
    size_t variant;
    size_t argument_start;
    size_t argument_count;
} RudaBytecodeEnumConstruction;

bool ruda_bytecode_add_enum_layout(
    RudaBytecodeFunction *function,
    RudaSymbolId symbol,
    const RudaBytecodeEnumVariant *variants,
    size_t variant_count,
    const RudaBytecodeEnumPayload *payloads,
    size_t payload_count,
    RudaBytecodeEnumId *result
);
bool ruda_bytecode_add_enum_construction(
    RudaBytecodeFunction *function,
    RudaBytecodeEnumId enum_id,
    size_t variant,
    const RudaBytecodeEnumArgument *arguments,
    size_t argument_count,
    RudaBytecodeEnumConstructionId *result
);
const RudaBytecodeEnumLayout *ruda_bytecode_enum_layout(
    const RudaBytecodeFunction *function,
    RudaBytecodeEnumId enum_id
);
const RudaBytecodeEnumConstruction *ruda_bytecode_enum_construction(
    const RudaBytecodeFunction *function,
    RudaBytecodeEnumConstructionId construction
);

#endif
