#ifndef MMAY_BYTECODE_COLLECTION_H
#define MMAY_BYTECODE_COLLECTION_H

#include "mmay/bytecode/function.h"

typedef struct MarimayBytecodeCollectionType {
    MarimayBytecodeType key_type;
    uint32_t key_layout;
    MarimayBytecodeType value_type;
    uint32_t value_layout;
    bool is_set;
} MarimayBytecodeCollectionType;

bool mmay_bytecode_add_collection_type(
    MarimayBytecodeFunction *function,
    MarimayBytecodeType key_type,
    uint32_t key_layout,
    MarimayBytecodeType value_type,
    uint32_t value_layout,
    bool is_set,
    uint32_t *result
);
const MarimayBytecodeCollectionType *mmay_bytecode_collection_type(
    const MarimayBytecodeFunction *function,
    uint32_t layout
);

#endif
