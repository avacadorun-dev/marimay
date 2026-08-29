#include "mir_internal.h"
#include "mir_collections_internal.h"

bool mmay_mir_collections_init(
    MarimayMirFunction *function,
    MarimayAllocator allocator
) {
    return function != NULL && mmay_vector_init(
        &function->collections, allocator, sizeof(MarimayMirCollection)
    );
}

void mmay_mir_collections_dispose(MarimayMirFunction *function) {
    if (function != NULL) mmay_vector_dispose(&function->collections);
}

bool mmay_mir_add_collection(
    MarimayMirFunction *function,
    MarimayTypeId type,
    MarimayTypeId key_type,
    MarimayTypeId value_type,
    bool is_set,
    MarimayMirCollectionId *result
) {
    MarimayMirCollection collection;
    size_t index = 0;
    if (function == NULL || !function->initialized || result == NULL
        || type < MMAY_TYPE_FIRST_DYNAMIC || key_type == MMAY_TYPE_INVALID
        || (is_set != (value_type == MMAY_TYPE_INVALID))) {
        return false;
    }
    for (index = 0; index < function->collections.length; index += 1) {
        const MarimayMirCollection *existing = mmay_vector_at_const(
            &function->collections, index
        );
        if (existing != NULL && existing->type == type) {
            if (existing->key_type != key_type
                || existing->value_type != value_type
                || existing->is_set != is_set) {
                return false;
            }
            *result = (MarimayMirCollectionId)index;
            return true;
        }
    }
    if (function->collections.length >= function->options.max_collection_layouts
        || function->collections.length >= UINT32_MAX) {
        return false;
    }
    collection.type = type;
    collection.key_type = key_type;
    collection.value_type = value_type;
    collection.is_set = is_set;
    if (!mmay_vector_push(&function->collections, &collection)) {
        return false;
    }
    *result = (MarimayMirCollectionId)(function->collections.length - 1);
    return true;
}

const MarimayMirCollection *mmay_mir_collection(
    const MarimayMirFunction *function,
    MarimayMirCollectionId collection_id
) {
    return function != NULL && collection_id != MMAY_MIR_NO_COLLECTION
        ? mmay_vector_at_const(&function->collections, collection_id)
        : NULL;
}
