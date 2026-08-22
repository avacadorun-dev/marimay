#include "mir_internal.h"

bool mmay_mir_add_interface(
    MarimayMirFunction *function,
    MarimayTypeId type,
    size_t method_count,
    MarimayMirInterfaceId *result
) {
    size_t index = 0;
    MarimayMirInterface layout;
    if (function == NULL || !function->initialized || result == NULL
        || type == MMAY_TYPE_INVALID
        || function->interfaces.length >= function->options.max_interface_layouts
        || function->interfaces.length >= UINT32_MAX) {
        return false;
    }
    for (index = 0; index < function->interfaces.length; index += 1) {
        const MarimayMirInterface *existing = mmay_vector_at_const(
            &function->interfaces,
            index
        );
        if (existing != NULL && existing->type == type) {
            if (existing->method_count != method_count) {
                return false;
            }
            *result = (MarimayMirInterfaceId)index;
            return true;
        }
    }
    layout.type = type;
    layout.method_count = method_count;
    if (!mmay_vector_push(&function->interfaces, &layout)) {
        return false;
    }
    *result = (MarimayMirInterfaceId)(function->interfaces.length - 1);
    return true;
}

bool mmay_mir_add_interface_construction(
    MarimayMirFunction *function,
    MarimayMirInterfaceId interface_id,
    MarimayMirRegister payload,
    MarimayTypeId payload_type,
    const MarimaySymbolId *methods,
    size_t method_count,
    MarimayMirInterfaceConstructionId *result
) {
    const MarimayMirInterface *layout = NULL;
    const MarimayTypeId *actual = NULL;
    MarimayMirInterfaceConstruction construction;
    size_t next_methods = 0;
    if (function == NULL || !function->initialized || result == NULL
        || payload_type == MMAY_TYPE_INVALID
        || (methods == NULL && method_count != 0)
        || function->interface_constructions.length
            >= function->options.max_interface_constructions
        || function->interface_constructions.length >= UINT32_MAX
        || !mmay_size_add(
            function->interface_methods.length,
            method_count,
            &next_methods
        ) || next_methods > function->options.max_interface_methods) {
        return false;
    }
    layout = mmay_vector_at_const(&function->interfaces, interface_id);
    actual = mmay_vector_at_const(&function->register_types, payload);
    if (layout == NULL || layout->method_count != method_count
        || actual == NULL || *actual != payload_type) {
        return false;
    }
    construction.interface_id = interface_id;
    construction.payload = payload;
    construction.payload_type = payload_type;
    construction.method_start = function->interface_methods.length;
    construction.method_count = method_count;
    if (!mmay_vector_reserve(
            &function->interface_constructions,
            function->interface_constructions.length + 1
        ) || !mmay_vector_reserve(
            &function->interface_methods,
            next_methods
        ) || !mmay_vector_append(
            &function->interface_methods,
            methods,
            method_count
        ) || !mmay_vector_push(
            &function->interface_constructions,
            &construction
        )) {
        function->interface_methods.length = construction.method_start;
        return false;
    }
    *result = (MarimayMirInterfaceConstructionId)(
        function->interface_constructions.length - 1
    );
    return true;
}

const MarimayMirInterface *mmay_mir_interface(
    const MarimayMirFunction *function,
    MarimayMirInterfaceId interface_id
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(&function->interfaces, interface_id)
        : NULL;
}

const MarimayMirInterfaceConstruction *mmay_mir_interface_construction(
    const MarimayMirFunction *function,
    MarimayMirInterfaceConstructionId construction_id
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(
            &function->interface_constructions,
            construction_id
        )
        : NULL;
}

const MarimaySymbolId *mmay_mir_interface_method(
    const MarimayMirFunction *function,
    size_t index
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(&function->interface_methods, index)
        : NULL;
}
