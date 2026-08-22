#ifndef MMAY_BYTECODE_INTERFACE_H
#define MMAY_BYTECODE_INTERFACE_H

#include "mmay/bytecode/function.h"

typedef uint32_t MarimayBytecodeInterfaceId;
typedef uint32_t MarimayBytecodeInterfaceConstructionId;

#define MMAY_BYTECODE_NO_INTERFACE UINT32_MAX
#define MMAY_BYTECODE_NO_INTERFACE_CONSTRUCTION UINT32_MAX

typedef struct MarimayBytecodeInterfaceLayout {
    MarimaySymbolId symbol;
    size_t method_count;
} MarimayBytecodeInterfaceLayout;

typedef struct MarimayBytecodeInterfaceConstruction {
    MarimayBytecodeInterfaceId interface_id;
    uint32_t payload_register;
    uint32_t payload_layout;
    MarimayBytecodeType payload_type;
    size_t method_start;
    size_t method_count;
} MarimayBytecodeInterfaceConstruction;

bool mmay_bytecode_add_interface_layout(
    MarimayBytecodeFunction *function,
    MarimaySymbolId symbol,
    size_t method_count,
    MarimayBytecodeInterfaceId *result
);
bool mmay_bytecode_add_interface_construction(
    MarimayBytecodeFunction *function,
    MarimayBytecodeInterfaceId interface_id,
    uint32_t payload_register,
    MarimayBytecodeType payload_type,
    uint32_t payload_layout,
    const MarimaySymbolId *methods,
    size_t method_count,
    MarimayBytecodeInterfaceConstructionId *result
);
const MarimayBytecodeInterfaceLayout *mmay_bytecode_interface_layout(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeInterfaceId interface_id
);
const MarimayBytecodeInterfaceConstruction *
mmay_bytecode_interface_construction(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeInterfaceConstructionId construction
);

#endif
