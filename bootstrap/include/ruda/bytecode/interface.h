#ifndef RUDA_BYTECODE_INTERFACE_H
#define RUDA_BYTECODE_INTERFACE_H

#include "ruda/bytecode/function.h"

typedef uint32_t RudaBytecodeInterfaceId;
typedef uint32_t RudaBytecodeInterfaceConstructionId;

#define RUDA_BYTECODE_NO_INTERFACE UINT32_MAX
#define RUDA_BYTECODE_NO_INTERFACE_CONSTRUCTION UINT32_MAX

typedef struct RudaBytecodeInterfaceLayout {
    RudaSymbolId symbol;
    size_t method_count;
} RudaBytecodeInterfaceLayout;

typedef struct RudaBytecodeInterfaceConstruction {
    RudaBytecodeInterfaceId interface_id;
    uint32_t payload_register;
    uint32_t payload_layout;
    RudaBytecodeType payload_type;
    size_t method_start;
    size_t method_count;
} RudaBytecodeInterfaceConstruction;

bool ruda_bytecode_add_interface_layout(
    RudaBytecodeFunction *function,
    RudaSymbolId symbol,
    size_t method_count,
    RudaBytecodeInterfaceId *result
);
bool ruda_bytecode_add_interface_construction(
    RudaBytecodeFunction *function,
    RudaBytecodeInterfaceId interface_id,
    uint32_t payload_register,
    RudaBytecodeType payload_type,
    uint32_t payload_layout,
    const RudaSymbolId *methods,
    size_t method_count,
    RudaBytecodeInterfaceConstructionId *result
);
const RudaBytecodeInterfaceLayout *ruda_bytecode_interface_layout(
    const RudaBytecodeFunction *function,
    RudaBytecodeInterfaceId interface_id
);
const RudaBytecodeInterfaceConstruction *
ruda_bytecode_interface_construction(
    const RudaBytecodeFunction *function,
    RudaBytecodeInterfaceConstructionId construction
);

#endif
