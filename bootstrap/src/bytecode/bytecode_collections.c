#include "mmay/bytecode/bytecode.h"

#include "bytecode_internal.h"

bool mmay_bytecode_add_collection_type(
    MarimayBytecodeFunction *function,
    MarimayBytecodeType key_type,
    uint32_t key_layout,
    MarimayBytecodeType value_type,
    uint32_t value_layout,
    bool is_set,
    uint32_t *result
) {
    MarimayBytecodeCollectionType entry;
    size_t index = 0;
    if (function == NULL || !function->initialized || result == NULL
        || !mmay_bytecode_type_is_register(key_type)
        || !mmay_bytecode_type_layout_is_valid(
            function, key_type, key_layout
        ) || (is_set ? (value_type != MMAY_BYTECODE_TYPE_INVALID
                || value_layout != MMAY_BYTECODE_NO_LAYOUT)
            : (!mmay_bytecode_type_is_register(value_type)
                || !mmay_bytecode_type_layout_is_valid(
                    function, value_type, value_layout
                )))
        || function->collection_types.length
            >= function->options.max_collection_layouts
        || function->collection_types.length >= UINT32_MAX) return false;
    for (index = 0; index < function->collection_types.length; index += 1) {
        const MarimayBytecodeCollectionType *existing = mmay_vector_at_const(
            &function->collection_types, index
        );
        if (existing != NULL && existing->key_type == key_type
            && existing->key_layout == key_layout
            && existing->value_type == value_type
            && existing->value_layout == value_layout
            && existing->is_set == is_set) {
            *result = (uint32_t)index;
            return true;
        }
    }
    entry.key_type = key_type;
    entry.key_layout = key_layout;
    entry.value_type = value_type;
    entry.value_layout = value_layout;
    entry.is_set = is_set;
    if (!mmay_vector_push(&function->collection_types, &entry)) return false;
    *result = (uint32_t)(function->collection_types.length - 1);
    return true;
}

const MarimayBytecodeCollectionType *mmay_bytecode_collection_type(
    const MarimayBytecodeFunction *function,
    uint32_t layout
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(&function->collection_types, layout) : NULL;
}
