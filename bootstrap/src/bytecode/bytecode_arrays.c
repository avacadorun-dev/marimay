#include "ruda/bytecode/bytecode.h"

#include "bytecode_internal.h"

bool ruda_bytecode_add_array_construction(
    RudaBytecodeFunction *function,
    RudaBytecodeType array_type,
    const RudaBytecodeArrayArgument *arguments,
    size_t argument_count,
    RudaBytecodeArrayConstructionId *result
) {
    return ruda_bytecode_add_array_construction_with_layout(
        function,
        array_type,
        RUDA_BYTECODE_NO_LAYOUT,
        arguments,
        argument_count,
        result
    );
}

bool ruda_bytecode_add_array_type(
    RudaBytecodeFunction *function,
    RudaBytecodeType element_type,
    uint32_t element_layout,
    uint32_t *result
) {
    RudaBytecodeArrayType entry;
    size_t index = 0;
    if (function == NULL || !function->initialized || result == NULL
        || !ruda_bytecode_type_is_register(element_type)
        || (ruda_bytecode_type_is_array(element_type)
            ? element_layout >= function->array_types.length
            : element_layout != RUDA_BYTECODE_NO_LAYOUT)
        || function->array_types.length >= UINT32_MAX) {
        return false;
    }
    for (index = 0; index < function->array_types.length; index += 1) {
        const RudaBytecodeArrayType *existing = ruda_vector_at_const(
            &function->array_types, index
        );
        if (existing != NULL && existing->element_type == element_type
            && existing->element_layout == element_layout) {
            *result = (uint32_t)index;
            return true;
        }
    }
    entry.element_type = element_type;
    entry.element_layout = element_layout;
    if (!ruda_vector_push(&function->array_types, &entry)) {
        return false;
    }
    *result = (uint32_t)(function->array_types.length - 1);
    return true;
}

const RudaBytecodeArrayType *ruda_bytecode_array_type(
    const RudaBytecodeFunction *function,
    uint32_t layout
) {
    return function != NULL && function->initialized
        ? ruda_vector_at_const(&function->array_types, layout) : NULL;
}

bool ruda_bytecode_add_array_construction_with_layout(
    RudaBytecodeFunction *function,
    RudaBytecodeType array_type,
    uint32_t array_layout,
    const RudaBytecodeArrayArgument *arguments,
    size_t argument_count,
    RudaBytecodeArrayConstructionId *result
) {
    RudaBytecodeArrayConstruction construction;
    uint32_t element_layout = RUDA_BYTECODE_NO_LAYOUT;
    RudaBytecodeType element_type = ruda_bytecode_function_array_element_type(
        function, array_type, array_layout, &element_layout
    );
    size_t next_argument_count = 0;
    size_t index = 0;
    if (function == NULL || !function->initialized || result == NULL
        || element_type == RUDA_BYTECODE_TYPE_INVALID
        || (arguments == NULL && argument_count != 0)
        || function->array_constructions.length
            >= function->options.max_array_constructions
        || function->array_constructions.length >= UINT32_MAX
        || !ruda_size_add(function->array_arguments.length, argument_count,
            &next_argument_count)
        || next_argument_count > function->options.max_array_arguments) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const RudaBytecodeType *type = arguments[index].register_id
                < function->register_types.length
            ? ruda_vector_at_const(
                &function->register_types, arguments[index].register_id
            )
            : NULL;
        if (type == NULL || arguments[index].type != element_type
            || *type != element_type
            || (array_type == RUDA_BYTECODE_TYPE_ARRAY
                && (arguments[index].layout != element_layout
                    || ruda_bytecode_register_layout(
                        function, arguments[index].register_id
                    ) != element_layout))) {
            return false;
        }
    }
    if (!ruda_vector_reserve(
            &function->array_constructions,
            function->array_constructions.length + 1
        ) || !ruda_vector_reserve(
            &function->array_arguments, next_argument_count
        )) {
        return false;
    }
    construction.array_type = array_type;
    construction.array_layout = array_layout;
    construction.argument_start = function->array_arguments.length;
    construction.argument_count = argument_count;
    if (!ruda_vector_append(
            &function->array_arguments, arguments, argument_count
        ) || !ruda_vector_push(
            &function->array_constructions, &construction
        )) {
        function->array_arguments.length = construction.argument_start;
        return false;
    }
    *result = (RudaBytecodeArrayConstructionId)(
        function->array_constructions.length - 1
    );
    return true;
}

const RudaBytecodeArrayConstruction *ruda_bytecode_array_construction(
    const RudaBytecodeFunction *function,
    RudaBytecodeArrayConstructionId construction
) {
    return function != NULL && function->initialized
        ? ruda_vector_at_const(&function->array_constructions, construction)
        : NULL;
}

const RudaBytecodeArrayArgument *ruda_bytecode_array_argument(
    const RudaBytecodeFunction *function,
    size_t index
) {
    return function != NULL && function->initialized
        ? ruda_vector_at_const(&function->array_arguments, index)
        : NULL;
}
