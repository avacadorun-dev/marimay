#include "mmay/bytecode/bytecode.h"

#include "bytecode_internal.h"

bool mmay_bytecode_type_requires_layout(MarimayBytecodeType type) {
    return type == MMAY_BYTECODE_TYPE_ARRAY
        || type == MMAY_BYTECODE_TYPE_RECORD
        || type == MMAY_BYTECODE_TYPE_ENUM
        || type == MMAY_BYTECODE_TYPE_INTERFACE
        || type == MMAY_BYTECODE_TYPE_MAP || type == MMAY_BYTECODE_TYPE_SET;
}

bool mmay_bytecode_type_layout_is_valid(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeType type,
    uint32_t layout
) {
    if (function == NULL) {
        return false;
    }
    if (type == MMAY_BYTECODE_TYPE_RECORD) {
        return layout < function->record_layouts.length;
    }
    if (type == MMAY_BYTECODE_TYPE_ENUM) {
        return layout < function->enum_layouts.length;
    }
    if (type == MMAY_BYTECODE_TYPE_INTERFACE) {
        return layout < function->interface_layouts.length;
    }
    if (type == MMAY_BYTECODE_TYPE_ARRAY) {
        return layout < function->array_types.length;
    }
    if (type == MMAY_BYTECODE_TYPE_MAP || type == MMAY_BYTECODE_TYPE_SET) {
        const MarimayBytecodeCollectionType *collection =
            mmay_bytecode_collection_type(function, layout);
        return collection != NULL
            && collection->is_set == (type == MMAY_BYTECODE_TYPE_SET);
    }
    return layout == MMAY_BYTECODE_NO_LAYOUT;
}

bool mmay_bytecode_set_register_layout(
    MarimayBytecodeFunction *function,
    uint32_t register_id,
    uint32_t layout
) {
    uint32_t *stored = NULL;
    if (function == NULL || !function->initialized
        || register_id >= function->register_layouts.length) {
        return false;
    }
    stored = mmay_vector_at(&function->register_layouts, register_id);
    if (stored == NULL) {
        return false;
    }
    *stored = layout;
    return true;
}

uint32_t mmay_bytecode_register_layout(
    const MarimayBytecodeFunction *function,
    uint32_t register_id
) {
    const uint32_t *stored = function != NULL && function->initialized
            && register_id < function->register_layouts.length
        ? mmay_vector_at_const(&function->register_layouts, register_id)
        : NULL;
    return stored != NULL ? *stored : MMAY_BYTECODE_NO_LAYOUT;
}

bool mmay_bytecode_add_record_layout(
    MarimayBytecodeFunction *function,
    MarimaySymbolId symbol,
    const MarimayBytecodeRecordField *fields,
    size_t field_count,
    MarimayBytecodeRecordId *result
) {
    MarimayBytecodeRecordLayout layout;
    size_t next_fields = 0;
    if (function == NULL || !function->initialized || result == NULL
        || (fields == NULL && field_count != 0)
        || function->record_layouts.length >= function->options.max_record_layouts
        || function->record_layouts.length >= UINT32_MAX
        || !mmay_size_add(function->record_fields.length, field_count, &next_fields)
        || next_fields > function->options.max_record_fields) {
        return false;
    }
    if (!mmay_vector_reserve(
            &function->record_layouts, function->record_layouts.length + 1
        ) || !mmay_vector_reserve(&function->record_fields, next_fields)) {
        return false;
    }
    layout.symbol = symbol;
    layout.field_start = function->record_fields.length;
    layout.field_count = field_count;
    if (!mmay_vector_append(&function->record_fields, fields, field_count)
        || !mmay_vector_push(&function->record_layouts, &layout)) {
        function->record_fields.length = layout.field_start;
        return false;
    }
    *result = (MarimayBytecodeRecordId)(function->record_layouts.length - 1);
    return true;
}

bool mmay_bytecode_add_record_construction(
    MarimayBytecodeFunction *function,
    MarimayBytecodeRecordId record,
    const MarimayBytecodeRecordArgument *arguments,
    size_t argument_count,
    MarimayBytecodeRecordConstructionId *result
) {
    const MarimayBytecodeRecordLayout *layout = mmay_bytecode_record_layout(
        function,
        record
    );
    MarimayBytecodeRecordConstruction construction;
    size_t next_arguments = 0;
    size_t index = 0;
    if (function == NULL || !function->initialized || result == NULL
        || layout == NULL || layout->field_count != argument_count
        || (arguments == NULL && argument_count != 0)
        || function->record_constructions.length
            >= function->options.max_record_constructions
        || !mmay_size_add(
            function->record_arguments.length,
            argument_count,
            &next_arguments
        ) || next_arguments > function->options.max_record_arguments) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const MarimayBytecodeRecordField *field = mmay_bytecode_record_field(
            function,
            layout,
            index
        );
        const MarimayBytecodeType *actual = arguments[index].register_id
                < function->register_types.length
            ? mmay_vector_at_const(
                &function->register_types,
                arguments[index].register_id
            )
            : NULL;
        if (field == NULL || arguments[index].type != field->type
            || arguments[index].layout != field->layout || actual == NULL
            || *actual != arguments[index].type
            || mmay_bytecode_register_layout(
                function,
                arguments[index].register_id
            ) != arguments[index].layout) {
            return false;
        }
    }
    if (!mmay_vector_reserve(
            &function->record_constructions,
            function->record_constructions.length + 1
        ) || !mmay_vector_reserve(
            &function->record_arguments, next_arguments
        )) {
        return false;
    }
    construction.record = record;
    construction.argument_start = function->record_arguments.length;
    construction.argument_count = argument_count;
    if (!mmay_vector_append(
            &function->record_arguments, arguments, argument_count
        ) || !mmay_vector_push(&function->record_constructions, &construction)) {
        function->record_arguments.length = construction.argument_start;
        return false;
    }
    *result = (MarimayBytecodeRecordConstructionId)(
        function->record_constructions.length - 1
    );
    return true;
}

const MarimayBytecodeRecordLayout *mmay_bytecode_record_layout(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeRecordId record
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(&function->record_layouts, record)
        : NULL;
}

const MarimayBytecodeRecordField *mmay_bytecode_record_field(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeRecordLayout *layout,
    size_t field
) {
    return function != NULL && function->initialized && layout != NULL
            && field < layout->field_count
        ? mmay_vector_at_const(
            &function->record_fields,
            layout->field_start + field
        )
        : NULL;
}

const MarimayBytecodeRecordConstruction *mmay_bytecode_record_construction(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeRecordConstructionId construction
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(&function->record_constructions, construction)
        : NULL;
}

bool mmay_bytecode_add_enum_layout(
    MarimayBytecodeFunction *function,
    MarimaySymbolId symbol,
    const MarimayBytecodeEnumVariant *variants,
    size_t variant_count,
    const MarimayBytecodeEnumPayload *payloads,
    size_t payload_count,
    MarimayBytecodeEnumId *result
) {
    MarimayBytecodeEnumLayout layout;
    size_t next_variants = 0;
    size_t next_payloads = 0;
    size_t index = 0;
    if (function == NULL || !function->initialized || result == NULL
        || (variants == NULL && variant_count != 0)
        || (payloads == NULL && payload_count != 0)
        || function->enum_layouts.length >= function->options.max_enum_layouts
        || !mmay_size_add(
            function->enum_variants.length, variant_count, &next_variants
        ) || next_variants > function->options.max_enum_variants
        || !mmay_size_add(
            function->enum_payloads.length, payload_count, &next_payloads
        ) || next_payloads > function->options.max_enum_payloads) {
        return false;
    }
    if (!mmay_vector_reserve(
            &function->enum_layouts, function->enum_layouts.length + 1
        ) || !mmay_vector_reserve(&function->enum_variants, next_variants)
        || !mmay_vector_reserve(&function->enum_payloads, next_payloads)) {
        return false;
    }
    layout.symbol = symbol;
    layout.variant_start = function->enum_variants.length;
    layout.variant_count = variant_count;
    layout.payload_start = function->enum_payloads.length;
    layout.payload_count = payload_count;
    if (!mmay_vector_append(&function->enum_payloads, payloads, payload_count)
        || !mmay_vector_append(&function->enum_variants, variants, variant_count)
        || !mmay_vector_push(&function->enum_layouts, &layout)) {
        function->enum_variants.length = layout.variant_start;
        function->enum_payloads.length = layout.payload_start;
        return false;
    }
    (void)index;
    *result = (MarimayBytecodeEnumId)(function->enum_layouts.length - 1);
    return true;
}

bool mmay_bytecode_add_enum_construction(
    MarimayBytecodeFunction *function,
    MarimayBytecodeEnumId enum_id,
    size_t variant,
    const MarimayBytecodeEnumArgument *arguments,
    size_t argument_count,
    MarimayBytecodeEnumConstructionId *result
) {
    const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
        function,
        enum_id
    );
    const MarimayBytecodeEnumVariant *selected = NULL;
    MarimayBytecodeEnumConstruction construction;
    size_t next_arguments = 0;
    size_t index = 0;
    if (function == NULL || !function->initialized || result == NULL
        || layout == NULL || variant >= layout->variant_count
        || (arguments == NULL && argument_count != 0)
        || function->enum_constructions.length
            >= function->options.max_enum_constructions
        || !mmay_size_add(
            function->enum_arguments.length, argument_count, &next_arguments
        ) || next_arguments > function->options.max_enum_arguments) {
        return false;
    }
    selected = mmay_vector_at_const(
        &function->enum_variants,
        layout->variant_start + variant
    );
    if (selected == NULL || selected->payload_count != argument_count) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const MarimayBytecodeEnumPayload *payload = mmay_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + selected->payload_start + index
        );
        const MarimayBytecodeType *actual = arguments[index].register_id
                < function->register_types.length
            ? mmay_vector_at_const(
                &function->register_types,
                arguments[index].register_id
            )
            : NULL;
        if (payload == NULL || actual == NULL
            || arguments[index].type != payload->type
            || arguments[index].layout != payload->layout
            || *actual != arguments[index].type
            || mmay_bytecode_register_layout(
                function,
                arguments[index].register_id
            ) != arguments[index].layout) {
            return false;
        }
    }
    if (!mmay_vector_reserve(
            &function->enum_constructions,
            function->enum_constructions.length + 1
        ) || !mmay_vector_reserve(&function->enum_arguments, next_arguments)) {
        return false;
    }
    construction.enum_id = enum_id;
    construction.variant = variant;
    construction.argument_start = function->enum_arguments.length;
    construction.argument_count = argument_count;
    if (!mmay_vector_append(
            &function->enum_arguments, arguments, argument_count
        ) || !mmay_vector_push(&function->enum_constructions, &construction)) {
        function->enum_arguments.length = construction.argument_start;
        return false;
    }
    *result = (MarimayBytecodeEnumConstructionId)(
        function->enum_constructions.length - 1
    );
    return true;
}

const MarimayBytecodeEnumLayout *mmay_bytecode_enum_layout(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeEnumId enum_id
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(&function->enum_layouts, enum_id)
        : NULL;
}

const MarimayBytecodeEnumConstruction *mmay_bytecode_enum_construction(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeEnumConstructionId construction
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(&function->enum_constructions, construction)
        : NULL;
}

bool mmay_bytecode_add_interface_layout(
    MarimayBytecodeFunction *function,
    MarimaySymbolId symbol,
    size_t method_count,
    MarimayBytecodeInterfaceId *result
) {
    MarimayBytecodeInterfaceLayout layout;
    if (function == NULL || !function->initialized || result == NULL
        || function->interface_layouts.length
            >= function->options.max_interface_layouts
        || function->interface_layouts.length >= UINT32_MAX) {
        return false;
    }
    layout.symbol = symbol;
    layout.method_count = method_count;
    if (!mmay_vector_push(&function->interface_layouts, &layout)) {
        return false;
    }
    *result = (MarimayBytecodeInterfaceId)(function->interface_layouts.length - 1);
    return true;
}

bool mmay_bytecode_add_interface_construction(
    MarimayBytecodeFunction *function,
    MarimayBytecodeInterfaceId interface_id,
    uint32_t payload_register,
    MarimayBytecodeType payload_type,
    uint32_t payload_layout,
    const MarimaySymbolId *methods,
    size_t method_count,
    MarimayBytecodeInterfaceConstructionId *result
) {
    const MarimayBytecodeInterfaceLayout *layout = mmay_bytecode_interface_layout(
        function,
        interface_id
    );
    MarimayBytecodeInterfaceConstruction construction;
    size_t next_methods = 0;
    if (function == NULL || !function->initialized || result == NULL
        || layout == NULL || layout->method_count != method_count
        || (methods == NULL && method_count != 0)
        || payload_register >= function->register_types.length
        || function->interface_constructions.length
            >= function->options.max_interface_constructions
        || !mmay_size_add(
            function->interface_methods.length, method_count, &next_methods
        ) || next_methods > function->options.max_interface_methods) {
        return false;
    }
    {
        const MarimayBytecodeType *actual = mmay_vector_at_const(
            &function->register_types,
            payload_register
        );
        if (actual == NULL || *actual != payload_type
            || mmay_bytecode_register_layout(function, payload_register)
                != payload_layout
            || !mmay_bytecode_type_layout_is_valid(
                function,
                payload_type,
                payload_layout
            )) {
            return false;
        }
    }
    if (!mmay_vector_reserve(
            &function->interface_constructions,
            function->interface_constructions.length + 1
        ) || !mmay_vector_reserve(
            &function->interface_methods, next_methods
        )) {
        return false;
    }
    construction.interface_id = interface_id;
    construction.payload_register = payload_register;
    construction.payload_layout = payload_layout;
    construction.payload_type = payload_type;
    construction.method_start = function->interface_methods.length;
    construction.method_count = method_count;
    if (!mmay_vector_append(
            &function->interface_methods, methods, method_count
        ) || !mmay_vector_push(
            &function->interface_constructions, &construction
        )) {
        function->interface_methods.length = construction.method_start;
        return false;
    }
    *result = (MarimayBytecodeInterfaceConstructionId)(
        function->interface_constructions.length - 1
    );
    return true;
}

const MarimayBytecodeInterfaceLayout *mmay_bytecode_interface_layout(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeInterfaceId interface_id
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(&function->interface_layouts, interface_id)
        : NULL;
}

const MarimayBytecodeInterfaceConstruction *mmay_bytecode_interface_construction(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeInterfaceConstructionId construction
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(&function->interface_constructions, construction)
        : NULL;
}
