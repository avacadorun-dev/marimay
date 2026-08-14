#include "mir_internal.h"

bool ruda_mir_add_interface(
    RudaMirFunction *function,
    RudaTypeId type,
    size_t method_count,
    RudaMirInterfaceId *result
) {
    size_t index = 0;
    RudaMirInterface layout;
    if (function == NULL || !function->initialized || result == NULL
        || type == RUDA_TYPE_INVALID
        || function->interfaces.length >= function->options.max_interface_layouts
        || function->interfaces.length >= UINT32_MAX) {
        return false;
    }
    for (index = 0; index < function->interfaces.length; index += 1) {
        const RudaMirInterface *existing = ruda_vector_at_const(
            &function->interfaces,
            index
        );
        if (existing != NULL && existing->type == type) {
            if (existing->method_count != method_count) {
                return false;
            }
            *result = (RudaMirInterfaceId)index;
            return true;
        }
    }
    layout.type = type;
    layout.method_count = method_count;
    if (!ruda_vector_push(&function->interfaces, &layout)) {
        return false;
    }
    *result = (RudaMirInterfaceId)(function->interfaces.length - 1);
    return true;
}

bool ruda_mir_add_interface_construction(
    RudaMirFunction *function,
    RudaMirInterfaceId interface_id,
    RudaMirRegister payload,
    RudaTypeId payload_type,
    const RudaSymbolId *methods,
    size_t method_count,
    RudaMirInterfaceConstructionId *result
) {
    const RudaMirInterface *layout = NULL;
    const RudaTypeId *actual = NULL;
    RudaMirInterfaceConstruction construction;
    size_t next_methods = 0;
    if (function == NULL || !function->initialized || result == NULL
        || payload_type == RUDA_TYPE_INVALID
        || (methods == NULL && method_count != 0)
        || function->interface_constructions.length
            >= function->options.max_interface_constructions
        || function->interface_constructions.length >= UINT32_MAX
        || !ruda_size_add(
            function->interface_methods.length,
            method_count,
            &next_methods
        ) || next_methods > function->options.max_interface_methods) {
        return false;
    }
    layout = ruda_vector_at_const(&function->interfaces, interface_id);
    actual = ruda_vector_at_const(&function->register_types, payload);
    if (layout == NULL || layout->method_count != method_count
        || actual == NULL || *actual != payload_type) {
        return false;
    }
    construction.interface_id = interface_id;
    construction.payload = payload;
    construction.payload_type = payload_type;
    construction.method_start = function->interface_methods.length;
    construction.method_count = method_count;
    if (!ruda_vector_reserve(
            &function->interface_constructions,
            function->interface_constructions.length + 1
        ) || !ruda_vector_reserve(
            &function->interface_methods,
            next_methods
        ) || !ruda_vector_append(
            &function->interface_methods,
            methods,
            method_count
        ) || !ruda_vector_push(
            &function->interface_constructions,
            &construction
        )) {
        function->interface_methods.length = construction.method_start;
        return false;
    }
    *result = (RudaMirInterfaceConstructionId)(
        function->interface_constructions.length - 1
    );
    return true;
}

const RudaMirInterface *ruda_mir_interface(
    const RudaMirFunction *function,
    RudaMirInterfaceId interface_id
) {
    return function != NULL && function->initialized
        ? ruda_vector_at_const(&function->interfaces, interface_id)
        : NULL;
}

const RudaMirInterfaceConstruction *ruda_mir_interface_construction(
    const RudaMirFunction *function,
    RudaMirInterfaceConstructionId construction_id
) {
    return function != NULL && function->initialized
        ? ruda_vector_at_const(
            &function->interface_constructions,
            construction_id
        )
        : NULL;
}

const RudaSymbolId *ruda_mir_interface_method(
    const RudaMirFunction *function,
    size_t index
) {
    return function != NULL && function->initialized
        ? ruda_vector_at_const(&function->interface_methods, index)
        : NULL;
}
