#ifndef MMAY_MIR_COLLECTIONS_INTERNAL_H
#define MMAY_MIR_COLLECTIONS_INTERNAL_H

#include "mmay/mir/mir.h"

bool mmay_mir_collections_init(
    MarimayMirFunction *function,
    MarimayAllocator allocator
);
void mmay_mir_collections_dispose(MarimayMirFunction *function);
MarimayMirVerifyStatus mmay_mir_verify_collection_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    MarimayTypeId target_type,
    MarimayTypeId first_type,
    MarimayTypeId second_type,
    MarimayTypeId third_type
);
const MarimayMirCollection *mmay_mir_collection(
    const MarimayMirFunction *function,
    MarimayMirCollectionId collection_id
);

#endif
