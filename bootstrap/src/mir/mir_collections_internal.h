#ifndef MMAY_MIR_COLLECTIONS_INTERNAL_H
#define MMAY_MIR_COLLECTIONS_INTERNAL_H

#include "mmay/mir/mir.h"

bool mmay_mir_collections_init(
    MarimayMirFunction *function,
    MarimayAllocator allocator
);
void mmay_mir_collections_dispose(MarimayMirFunction *function);
const MarimayMirCollection *mmay_mir_collection(
    const MarimayMirFunction *function,
    MarimayMirCollectionId collection_id
);

#endif
