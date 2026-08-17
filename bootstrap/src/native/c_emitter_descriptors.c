#include "c_emitter_internal.h"

#include <stdint.h>

static bool ruda_native_c_function_layout(
    const RudaMirFunction *function,
    RudaTypeId type,
    RudaNativeCManagedLayout *result
) {
    size_t index = 0;
    if (function == NULL || result == NULL || type < RUDA_TYPE_FIRST_DYNAMIC) {
        return false;
    }
    for (index = 0; index < function->arrays.length; index += 1) {
        const RudaMirArray *layout = ruda_vector_at_const(
            &function->arrays, index
        );
        if (layout != NULL && layout->type == type) {
            *result = (RudaNativeCManagedLayout){
                function, RUDA_NATIVE_C_MANAGED_ARRAY, index
            };
            return true;
        }
    }
    for (index = 0; index < function->records.length; index += 1) {
        const RudaMirRecord *layout = ruda_vector_at_const(
            &function->records, index
        );
        if (layout != NULL && layout->type == type) {
            *result = (RudaNativeCManagedLayout){
                function, RUDA_NATIVE_C_MANAGED_RECORD, index
            };
            return true;
        }
    }
    for (index = 0; index < function->enums.length; index += 1) {
        const RudaMirEnum *layout = ruda_vector_at_const(
            &function->enums, index
        );
        if (layout != NULL && layout->type == type) {
            *result = (RudaNativeCManagedLayout){
                function, RUDA_NATIVE_C_MANAGED_ENUM, index
            };
            return true;
        }
    }
    for (index = 0; index < function->interfaces.length; index += 1) {
        const RudaMirInterface *layout = ruda_vector_at_const(
            &function->interfaces, index
        );
        if (layout != NULL && layout->type == type) {
            *result = (RudaNativeCManagedLayout){
                function, RUDA_NATIVE_C_MANAGED_INTERFACE, index
            };
            return true;
        }
    }
    return false;
}

bool ruda_native_c_find_managed_layout(
    const RudaMirFunction *const *functions,
    size_t count,
    RudaTypeId type,
    RudaNativeCManagedLayout *result
) {
    size_t index = 0;
    if (functions == NULL || result == NULL) {
        return false;
    }
    for (index = 0; index < count; index += 1) {
        if (ruda_native_c_function_layout(functions[index], type, result)) {
            return true;
        }
    }
    return false;
}

static bool ruda_native_c_same_record(
    RudaNativeCManagedLayout left,
    RudaNativeCManagedLayout right
) {
    const RudaMirRecord *a = ruda_vector_at_const(
        &left.function->records, left.index
    );
    const RudaMirRecord *b = ruda_vector_at_const(
        &right.function->records, right.index
    );
    size_t index = 0;
    if (a == NULL || b == NULL || a->field_count != b->field_count) {
        return false;
    }
    for (index = 0; index < a->field_count; index += 1) {
        const RudaMirRecordField *af = ruda_vector_at_const(
            &left.function->record_fields, a->field_start + index
        );
        const RudaMirRecordField *bf = ruda_vector_at_const(
            &right.function->record_fields, b->field_start + index
        );
        if (af == NULL || bf == NULL || af->type != bf->type
            || af->is_mutable != bf->is_mutable) {
            return false;
        }
    }
    return true;
}

static bool ruda_native_c_same_enum(
    RudaNativeCManagedLayout left,
    RudaNativeCManagedLayout right
) {
    const RudaMirEnum *a = ruda_vector_at_const(
        &left.function->enums, left.index
    );
    const RudaMirEnum *b = ruda_vector_at_const(
        &right.function->enums, right.index
    );
    size_t variant = 0;
    if (a == NULL || b == NULL || a->variant_count != b->variant_count) {
        return false;
    }
    for (variant = 0; variant < a->variant_count; variant += 1) {
        const RudaMirEnumVariant *av = ruda_vector_at_const(
            &left.function->enum_variants, a->variant_start + variant
        );
        const RudaMirEnumVariant *bv = ruda_vector_at_const(
            &right.function->enum_variants, b->variant_start + variant
        );
        size_t payload = 0;
        if (av == NULL || bv == NULL || av->kind != bv->kind
            || av->payload_count != bv->payload_count) {
            return false;
        }
        for (payload = 0; payload < av->payload_count; payload += 1) {
            const RudaMirEnumPayload *ap = ruda_vector_at_const(
                &left.function->enum_payloads, av->payload_start + payload
            );
            const RudaMirEnumPayload *bp = ruda_vector_at_const(
                &right.function->enum_payloads, bv->payload_start + payload
            );
            if (ap == NULL || bp == NULL || ap->type != bp->type) {
                return false;
            }
        }
    }
    return true;
}

static bool ruda_native_c_same_layout(
    RudaNativeCManagedLayout left,
    RudaNativeCManagedLayout right
) {
    if (left.kind != right.kind) {
        return false;
    }
    if (left.kind == RUDA_NATIVE_C_MANAGED_ARRAY) {
        const RudaMirArray *a = ruda_vector_at_const(
            &left.function->arrays, left.index
        );
        const RudaMirArray *b = ruda_vector_at_const(
            &right.function->arrays, right.index
        );
        return a != NULL && b != NULL && a->element_type == b->element_type;
    }
    if (left.kind == RUDA_NATIVE_C_MANAGED_RECORD) {
        return ruda_native_c_same_record(left, right);
    }
    if (left.kind == RUDA_NATIVE_C_MANAGED_ENUM) {
        return ruda_native_c_same_enum(left, right);
    }
    if (left.kind == RUDA_NATIVE_C_MANAGED_INTERFACE) {
        const RudaMirInterface *a = ruda_vector_at_const(
            &left.function->interfaces, left.index
        );
        const RudaMirInterface *b = ruda_vector_at_const(
            &right.function->interfaces, right.index
        );
        return a != NULL && b != NULL && a->method_count == b->method_count;
    }
    return false;
}

bool ruda_native_c_program_type_supported(
    const RudaMirFunction *const *functions,
    size_t count,
    RudaTypeId type,
    bool result
) {
    RudaNativeCManagedLayout layout;
    if (type == RUDA_TYPE_BOOL || type == RUDA_TYPE_I8
        || type == RUDA_TYPE_I16 || type == RUDA_TYPE_I32
        || type == RUDA_TYPE_I64 || type == RUDA_TYPE_U8
        || type == RUDA_TYPE_U16 || type == RUDA_TYPE_U32
        || type == RUDA_TYPE_U64 || type == RUDA_TYPE_F64
        || type == RUDA_TYPE_STRING) {
        return true;
    }
    if (result && (type == RUDA_TYPE_VOID || type == RUDA_TYPE_NEVER)) {
        return true;
    }
    if (!ruda_native_c_find_managed_layout(functions, count, type, &layout)) {
        return false;
    }
    return layout.kind != RUDA_NATIVE_C_MANAGED_ARRAY
        || ruda_native_c_array_kind(layout.function, type)
            != RUDA_NATIVE_C_ARRAY_UNSUPPORTED;
}

static bool ruda_native_c_layout_fields_supported(
    RudaNativeCManagedLayout layout,
    const RudaMirFunction *const *functions,
    size_t count
) {
    size_t index = 0;
    if (layout.kind == RUDA_NATIVE_C_MANAGED_RECORD) {
        const RudaMirRecord *record = ruda_vector_at_const(
            &layout.function->records, layout.index
        );
        if (record == NULL) return false;
        for (index = 0; index < record->field_count; index += 1) {
            const RudaMirRecordField *field = ruda_vector_at_const(
                &layout.function->record_fields, record->field_start + index
            );
            if (field == NULL || !ruda_native_c_program_type_supported(
                    functions, count, field->type, false
                )) return false;
        }
    } else if (layout.kind == RUDA_NATIVE_C_MANAGED_ENUM) {
        const RudaMirEnum *enumeration = ruda_vector_at_const(
            &layout.function->enums, layout.index
        );
        if (enumeration == NULL) return false;
        for (index = 0; index < enumeration->payload_count; index += 1) {
            const RudaMirEnumPayload *payload = ruda_vector_at_const(
                &layout.function->enum_payloads,
                enumeration->payload_start + index
            );
            if (payload == NULL || !ruda_native_c_program_type_supported(
                    functions, count, payload->type, false
                )) return false;
        }
    }
    return true;
}

bool ruda_native_c_managed_layouts_valid(
    const RudaMirFunction *const *functions,
    size_t count
) {
    size_t function_index = 0;
    if (functions == NULL) return false;
    for (function_index = 0; function_index < count; function_index += 1) {
        const RudaMirFunction *function = functions[function_index];
        RudaNativeCManagedKind kind = RUDA_NATIVE_C_MANAGED_ARRAY;
        if (function == NULL) return false;
        for (kind = RUDA_NATIVE_C_MANAGED_ARRAY;
            kind <= RUDA_NATIVE_C_MANAGED_INTERFACE; kind += 1) {
            const RudaVector *layouts = kind == RUDA_NATIVE_C_MANAGED_ARRAY
                ? &function->arrays : (kind == RUDA_NATIVE_C_MANAGED_RECORD
                    ? &function->records : (kind == RUDA_NATIVE_C_MANAGED_ENUM
                        ? &function->enums : &function->interfaces));
            size_t index = 0;
            for (index = 0; index < layouts->length; index += 1) {
                RudaTypeId type = RUDA_TYPE_INVALID;
                RudaNativeCManagedLayout current = {function, kind, index};
                RudaNativeCManagedLayout canonical;
                if (kind == RUDA_NATIVE_C_MANAGED_ARRAY) {
                    const RudaMirArray *value = ruda_vector_at_const(layouts, index);
                    if (value != NULL) type = value->type;
                } else if (kind == RUDA_NATIVE_C_MANAGED_RECORD) {
                    const RudaMirRecord *value = ruda_vector_at_const(layouts, index);
                    if (value != NULL) type = value->type;
                } else if (kind == RUDA_NATIVE_C_MANAGED_ENUM) {
                    const RudaMirEnum *value = ruda_vector_at_const(layouts, index);
                    if (value != NULL) type = value->type;
                } else {
                    const RudaMirInterface *value = ruda_vector_at_const(layouts, index);
                    if (value != NULL) type = value->type;
                }
                if (type == RUDA_TYPE_INVALID
                    || !ruda_native_c_find_managed_layout(
                        functions, count, type, &canonical
                    ) || !ruda_native_c_same_layout(canonical, current)
                    || !ruda_native_c_layout_fields_supported(
                        current, functions, count
                )) return false;
            }
        }
        {
            size_t construction_index = 0;
            for (construction_index = 0;
                construction_index < function->interface_constructions.length;
                construction_index += 1) {
                const RudaMirInterfaceConstruction *construction =
                    ruda_vector_at_const(
                        &function->interface_constructions, construction_index
                    );
                const RudaMirInterface *interface = construction != NULL
                    ? ruda_vector_at_const(
                        &function->interfaces, construction->interface_id
                    ) : NULL;
                RudaNativeCManagedLayout payload_layout;
                size_t method_index = 0;
                if (construction == NULL || interface == NULL
                    || construction->method_count != interface->method_count
                    || !ruda_native_c_find_managed_layout(
                        functions, count, construction->payload_type,
                        &payload_layout
                    )) return false;
                for (method_index = 0;
                    method_index < construction->method_count;
                    method_index += 1) {
                    const RudaSymbolId *symbol = ruda_vector_at_const(
                        &function->interface_methods,
                        construction->method_start + method_index
                    );
                    const RudaMirFunction *callee = symbol != NULL
                        ? ruda_native_c_find(functions, count, *symbol) : NULL;
                    const RudaTypeId *receiver = callee != NULL
                        ? ruda_vector_at_const(&callee->register_types, 0)
                        : NULL;
                    if (callee == NULL || callee->parameter_count == 0
                        || receiver == NULL
                        || *receiver != construction->payload_type) return false;
                }
            }
        }
    }
    return true;
}

bool ruda_native_c_write_descriptor_name(
    RudaNativeCBuffer *buffer,
    RudaNativeCManagedKind kind,
    RudaTypeId type
) {
    const char *prefix = kind == RUDA_NATIVE_C_MANAGED_RECORD ? "rd"
        : (kind == RUDA_NATIVE_C_MANAGED_ENUM ? "ed"
            : (kind == RUDA_NATIVE_C_MANAGED_INTERFACE ? "id" : NULL));
    return prefix != NULL && ruda_native_c_write(
        buffer, "ruda_g_%s_%u", prefix, (unsigned int)type
    );
}

bool ruda_native_c_write_interface_table_name(
    RudaNativeCBuffer *buffer,
    RudaSymbolId symbol,
    size_t construction
) {
    return ruda_native_c_write(buffer, "ruda_g_it_")
        && ruda_native_c_symbol_hex(buffer, symbol)
        && ruda_native_c_write(buffer, "_%zu", construction);
}

static bool ruda_native_c_emit_field_descriptor(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *const *functions,
    size_t count,
    RudaTypeId type,
    bool is_mutable
) {
    RudaNativeCManagedLayout layout;
    if (type == RUDA_TYPE_STRING) {
        return ruda_native_c_write(
            buffer,
            "{RUDA_MANAGED_CELL_REFERENCE, %s, RUDA_MANAGED_OBJECT_STRING, NULL}",
            is_mutable ? "true" : "false"
        );
    }
    if (type < RUDA_TYPE_FIRST_DYNAMIC) {
        return ruda_native_c_write(
            buffer,
            "{RUDA_MANAGED_CELL_RAW64, %s, RUDA_MANAGED_OBJECT_INVALID, NULL}",
            is_mutable ? "true" : "false"
        );
    }
    if (!ruda_native_c_find_managed_layout(functions, count, type, &layout)) {
        return false;
    }
    if (layout.kind == RUDA_NATIVE_C_MANAGED_ARRAY) {
        return ruda_native_c_write(
            buffer, "{RUDA_MANAGED_CELL_REFERENCE, %s, RUDA_MANAGED_OBJECT_ARRAY, %s}",
            is_mutable ? "true" : "false",
            ruda_native_c_array_descriptor(layout.function, type)
        );
    }
    if (!ruda_native_c_write(
            buffer, "{RUDA_MANAGED_CELL_REFERENCE, %s, %s, &",
            is_mutable ? "true" : "false",
            layout.kind == RUDA_NATIVE_C_MANAGED_RECORD
                ? "RUDA_MANAGED_OBJECT_RECORD"
                : (layout.kind == RUDA_NATIVE_C_MANAGED_ENUM
                    ? "RUDA_MANAGED_OBJECT_ENUM"
                    : "RUDA_MANAGED_OBJECT_INTERFACE")
        ) || !ruda_native_c_write_descriptor_name(buffer, layout.kind, type)) {
        return false;
    }
    return ruda_native_c_write(buffer, "}");
}

static bool ruda_native_c_layout_is_canonical(
    const RudaMirFunction *const *functions,
    size_t count,
    RudaNativeCManagedLayout layout,
    RudaTypeId type
) {
    RudaNativeCManagedLayout canonical;
    return ruda_native_c_find_managed_layout(
            functions, count, type, &canonical
        ) && canonical.function == layout.function
        && canonical.kind == layout.kind && canonical.index == layout.index;
}

static bool ruda_native_c_emit_record_descriptor(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *const *functions,
    size_t count,
    RudaNativeCManagedLayout layout
) {
    const RudaMirRecord *record = ruda_vector_at_const(
        &layout.function->records, layout.index
    );
    size_t index = 0;
    if (record == NULL) return false;
    if (record->field_count != 0) {
        if (!ruda_native_c_write(buffer, "static const RudaManagedFieldDescriptor ")
            || !ruda_native_c_write_descriptor_name(
                buffer, layout.kind, record->type
            ) || !ruda_native_c_write(buffer, "_fields[] = {")) return false;
        for (index = 0; index < record->field_count; index += 1) {
            const RudaMirRecordField *field = ruda_vector_at_const(
                &layout.function->record_fields, record->field_start + index
            );
            if (field == NULL || (index != 0 && !ruda_native_c_write(buffer, ","))
                || !ruda_native_c_emit_field_descriptor(
                    buffer, functions, count, field->type, field->is_mutable
                )) return false;
        }
        if (!ruda_native_c_write(buffer, "};\n")) return false;
    }
    if (!ruda_native_c_write(buffer, "static const RudaManagedRecordDescriptor ")
        || !ruda_native_c_write_descriptor_name(buffer, layout.kind, record->type)
        || !ruda_native_c_write(buffer, " = {%zu, ", record->field_count)) {
        return false;
    }
    if (record->field_count == 0) return ruda_native_c_write(buffer, "NULL};\n");
    return ruda_native_c_write_descriptor_name(buffer, layout.kind, record->type)
        && ruda_native_c_write(buffer, "_fields};\n");
}

static bool ruda_native_c_emit_enum_descriptor(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *const *functions,
    size_t count,
    RudaNativeCManagedLayout layout
) {
    const RudaMirEnum *enumeration = ruda_vector_at_const(
        &layout.function->enums, layout.index
    );
    size_t index = 0;
    if (enumeration == NULL) return false;
    if (enumeration->payload_count != 0) {
        if (!ruda_native_c_write(buffer, "static const RudaManagedFieldDescriptor ")
            || !ruda_native_c_write_descriptor_name(
                buffer, layout.kind, enumeration->type
            ) || !ruda_native_c_write(buffer, "_payloads[] = {")) return false;
        for (index = 0; index < enumeration->payload_count; index += 1) {
            const RudaMirEnumPayload *payload = ruda_vector_at_const(
                &layout.function->enum_payloads,
                enumeration->payload_start + index
            );
            if (payload == NULL || (index != 0 && !ruda_native_c_write(buffer, ","))
                || !ruda_native_c_emit_field_descriptor(
                    buffer, functions, count, payload->type, false
                )) return false;
        }
        if (!ruda_native_c_write(buffer, "};\n")) return false;
    }
    if (enumeration->variant_count != 0
        && (!ruda_native_c_write(buffer,
            "static const RudaManagedEnumVariantDescriptor ")
            || !ruda_native_c_write_descriptor_name(
                buffer, layout.kind, enumeration->type
            ) || !ruda_native_c_write(buffer, "_variants[] = {"))) return false;
    for (index = 0; index < enumeration->variant_count; index += 1) {
        const RudaMirEnumVariant *variant = ruda_vector_at_const(
            &layout.function->enum_variants,
            enumeration->variant_start + index
        );
        if (variant == NULL || (index != 0 && !ruda_native_c_write(buffer, ","))) {
            return false;
        }
        if (variant->payload_count == 0) {
            if (!ruda_native_c_write(buffer, "{0,NULL}")) return false;
        } else if (!ruda_native_c_write(buffer, "{%zu,", variant->payload_count)
            || !ruda_native_c_write_descriptor_name(
                buffer, layout.kind, enumeration->type
            ) || !ruda_native_c_write(
                buffer, "_payloads + %zu}",
                variant->payload_start - enumeration->payload_start
            )) return false;
    }
    if (enumeration->variant_count != 0
        && !ruda_native_c_write(buffer, "};\n")) return false;
    if (!ruda_native_c_write(buffer, "static const RudaManagedEnumDescriptor ")
        || !ruda_native_c_write_descriptor_name(
            buffer, layout.kind, enumeration->type
        ) || !ruda_native_c_write(
            buffer, " = {%zu, ", enumeration->variant_count
        )) return false;
    if (enumeration->variant_count == 0) {
        return ruda_native_c_write(buffer, "NULL};\n");
    }
    return ruda_native_c_write_descriptor_name(
            buffer, layout.kind, enumeration->type
        ) && ruda_native_c_write(buffer, "_variants};\n");
}

static bool ruda_native_c_emit_interface_tables(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *const *functions,
    size_t count
) {
    size_t function_index = 0;
    for (function_index = 0; function_index < count; function_index += 1) {
        const RudaMirFunction *function = functions[function_index];
        size_t construction_index = 0;
        for (construction_index = 0;
            construction_index < function->interface_constructions.length;
            construction_index += 1) {
            const RudaMirInterfaceConstruction *construction =
                ruda_vector_at_const(
                    &function->interface_constructions, construction_index
                );
            size_t method = 0;
            if (construction == NULL
                || !ruda_native_c_write(buffer, "static const uint32_t ")
                || !ruda_native_c_write_interface_table_name(
                    buffer, function->symbol, construction_index
                ) || !ruda_native_c_write(buffer, "[%zu] = {",
                    construction->method_count == 0
                        ? (size_t)1 : construction->method_count)) return false;
            for (method = 0; method < construction->method_count; method += 1) {
                const RudaSymbolId *symbol = ruda_vector_at_const(
                    &function->interface_methods,
                    construction->method_start + method
                );
                size_t target = 0;
                if (symbol == NULL || !ruda_native_c_function_index(
                        functions, count, *symbol, &target
                    ) || (method != 0 && !ruda_native_c_write(buffer, ","))
                    || !ruda_native_c_write(buffer, "%zu", target)) return false;
            }
            if (construction->method_count == 0
                && !ruda_native_c_write(buffer, "0")) return false;
            if (!ruda_native_c_write(buffer, "};\n")) return false;
        }
    }
    return true;
}

bool ruda_native_c_emit_managed_descriptors(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *const *functions,
    size_t count
) {
    size_t function_index = 0;
    RudaNativeCManagedKind kind = RUDA_NATIVE_C_MANAGED_RECORD;
    for (function_index = 0; function_index < count; function_index += 1) {
        const RudaMirFunction *function = functions[function_index];
        size_t index = 0;
        for (index = 0; index < function->arrays.length; index += 1) {
            const RudaMirArray *array = ruda_vector_at_const(
                &function->arrays, index
            );
            RudaNativeCManagedLayout layout = {
                function, RUDA_NATIVE_C_MANAGED_ARRAY, index
            };
            if (array != NULL && ruda_native_c_layout_is_canonical(
                    functions, count, layout, array->type
                ) && ruda_native_c_array_kind(function, array->type)
                    == RUDA_NATIVE_C_ARRAY_GENERIC
                && (!ruda_native_c_write(
                    buffer,
                    "static const RudaManagedArrayDescriptor ruda_g_ad_%u;\n",
                    (unsigned int)array->type
                ))) return false;
        }
    }
    for (function_index = 0; function_index < count; function_index += 1) {
        const RudaMirFunction *function = functions[function_index];
        size_t index = 0;
        for (index = 0; index < function->arrays.length; index += 1) {
            const RudaMirArray *array = ruda_vector_at_const(
                &function->arrays, index
            );
            RudaNativeCManagedLayout layout = {
                function, RUDA_NATIVE_C_MANAGED_ARRAY, index
            };
            if (array != NULL && ruda_native_c_layout_is_canonical(
                    functions, count, layout, array->type
                ) && ruda_native_c_array_kind(function, array->type)
                    == RUDA_NATIVE_C_ARRAY_GENERIC) {
                bool reference = array->element_type >= RUDA_TYPE_FIRST_DYNAMIC;
                if (!ruda_native_c_write(
                        buffer,
                        "static const RudaManagedArrayDescriptor ruda_g_ad_%u = {%s, ",
                        (unsigned int)array->type,
                        reference ? "RUDA_MANAGED_CELL_REFERENCE"
                            : "RUDA_MANAGED_CELL_RAW64"
                    )) return false;
                if (reference && !ruda_native_c_write(
                        buffer, "&ruda_g_ad_%u", (unsigned int)array->element_type
                    )) return false;
                if (!reference && !ruda_native_c_write(buffer, "NULL")) {
                    return false;
                }
                if (!ruda_native_c_write(buffer, "};\n")) return false;
            }
        }
    }
    for (kind = RUDA_NATIVE_C_MANAGED_RECORD;
        kind <= RUDA_NATIVE_C_MANAGED_INTERFACE; kind += 1) {
        for (function_index = 0; function_index < count; function_index += 1) {
            const RudaMirFunction *function = functions[function_index];
            const RudaVector *layouts = kind == RUDA_NATIVE_C_MANAGED_RECORD
                ? &function->records : (kind == RUDA_NATIVE_C_MANAGED_ENUM
                    ? &function->enums : &function->interfaces);
            size_t index = 0;
            for (index = 0; index < layouts->length; index += 1) {
                RudaTypeId type = kind == RUDA_NATIVE_C_MANAGED_RECORD
                    ? ((const RudaMirRecord *)ruda_vector_at_const(layouts, index))->type
                    : (kind == RUDA_NATIVE_C_MANAGED_ENUM
                        ? ((const RudaMirEnum *)ruda_vector_at_const(layouts, index))->type
                        : ((const RudaMirInterface *)ruda_vector_at_const(layouts, index))->type);
                RudaNativeCManagedLayout layout = {function, kind, index};
                const char *name = kind == RUDA_NATIVE_C_MANAGED_RECORD
                    ? "Record" : (kind == RUDA_NATIVE_C_MANAGED_ENUM
                        ? "Enum" : "Interface");
                if (ruda_native_c_layout_is_canonical(
                        functions, count, layout, type
                    ) && (!ruda_native_c_write(
                        buffer, "static const RudaManaged%sDescriptor ", name
                    ) || !ruda_native_c_write_descriptor_name(buffer, kind, type)
                    || !ruda_native_c_write(buffer, ";\n"))) return false;
            }
        }
    }
    for (function_index = 0; function_index < count; function_index += 1) {
        const RudaMirFunction *function = functions[function_index];
        size_t index = 0;
        for (index = 0; index < function->records.length; index += 1) {
            const RudaMirRecord *record = ruda_vector_at_const(
                &function->records, index
            );
            RudaNativeCManagedLayout layout = {
                function, RUDA_NATIVE_C_MANAGED_RECORD, index
            };
            if (record != NULL && ruda_native_c_layout_is_canonical(
                    functions, count, layout, record->type
                ) && !ruda_native_c_emit_record_descriptor(
                    buffer, functions, count, layout
                )) return false;
        }
        for (index = 0; index < function->enums.length; index += 1) {
            const RudaMirEnum *enumeration = ruda_vector_at_const(
                &function->enums, index
            );
            RudaNativeCManagedLayout layout = {
                function, RUDA_NATIVE_C_MANAGED_ENUM, index
            };
            if (enumeration != NULL && ruda_native_c_layout_is_canonical(
                    functions, count, layout, enumeration->type
                ) && !ruda_native_c_emit_enum_descriptor(
                    buffer, functions, count, layout
                )) return false;
        }
        for (index = 0; index < function->interfaces.length; index += 1) {
            const RudaMirInterface *interface = ruda_vector_at_const(
                &function->interfaces, index
            );
            RudaNativeCManagedLayout layout = {
                function, RUDA_NATIVE_C_MANAGED_INTERFACE, index
            };
            if (interface != NULL && ruda_native_c_layout_is_canonical(
                    functions, count, layout, interface->type
                ) && (!ruda_native_c_write(
                    buffer, "static const RudaManagedInterfaceDescriptor "
                ) || !ruda_native_c_write_descriptor_name(
                    buffer, layout.kind, interface->type
                ) || !ruda_native_c_write(
                    buffer, " = {%zu};\n", interface->method_count
                ))) return false;
        }
    }
    if (!ruda_native_c_emit_interface_tables(buffer, functions, count)
        || !ruda_native_c_write(
            buffer, "static void ruda_g_keep_managed(void) {\n"
        )) return false;
    for (function_index = 0; function_index < count; function_index += 1) {
        const RudaMirFunction *function = functions[function_index];
        size_t index = 0;
        for (index = 0; index < function->records.length; index += 1) {
            const RudaMirRecord *value = ruda_vector_at_const(&function->records, index);
            RudaNativeCManagedLayout layout = {function, RUDA_NATIVE_C_MANAGED_RECORD, index};
            if (value != NULL && ruda_native_c_layout_is_canonical(functions, count, layout, value->type)
                && (!ruda_native_c_write(buffer, "  (void)&")
                    || !ruda_native_c_write_descriptor_name(buffer, layout.kind, value->type)
                    || !ruda_native_c_write(buffer, ";\n"))) return false;
        }
        for (index = 0; index < function->enums.length; index += 1) {
            const RudaMirEnum *value = ruda_vector_at_const(&function->enums, index);
            RudaNativeCManagedLayout layout = {function, RUDA_NATIVE_C_MANAGED_ENUM, index};
            if (value != NULL && ruda_native_c_layout_is_canonical(functions, count, layout, value->type)
                && (!ruda_native_c_write(buffer, "  (void)&")
                    || !ruda_native_c_write_descriptor_name(buffer, layout.kind, value->type)
                    || !ruda_native_c_write(buffer, ";\n"))) return false;
        }
        for (index = 0; index < function->interfaces.length; index += 1) {
            const RudaMirInterface *value = ruda_vector_at_const(&function->interfaces, index);
            RudaNativeCManagedLayout layout = {function, RUDA_NATIVE_C_MANAGED_INTERFACE, index};
            if (value != NULL && ruda_native_c_layout_is_canonical(functions, count, layout, value->type)
                && (!ruda_native_c_write(buffer, "  (void)&")
                    || !ruda_native_c_write_descriptor_name(buffer, layout.kind, value->type)
                    || !ruda_native_c_write(buffer, ";\n"))) return false;
        }
        for (index = 0; index < function->interface_constructions.length; index += 1) {
            if (!ruda_native_c_write(buffer, "  (void)")
                || !ruda_native_c_write_interface_table_name(
                    buffer, function->symbol, index
                ) || !ruda_native_c_write(buffer, ";\n")) return false;
        }
    }
    return ruda_native_c_write(buffer, "}\n\n");
}

bool ruda_native_c_emit_managed_parameter_check(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count,
    size_t parameter
) {
    const RudaTypeId *type = ruda_vector_at_const(
        &function->register_types, parameter
    );
    RudaNativeCManagedLayout layout;
    const char *descriptor_type = NULL;
    const char *describe = NULL;
    if (type == NULL || *type < RUDA_TYPE_FIRST_DYNAMIC) return type != NULL;
    if (!ruda_native_c_find_managed_layout(functions, count, *type, &layout)) {
        return false;
    }
    if (layout.kind == RUDA_NATIVE_C_MANAGED_ARRAY) {
        descriptor_type = "RudaManagedArrayDescriptor";
        describe = "ruda_managed_array_describe";
    } else if (layout.kind == RUDA_NATIVE_C_MANAGED_RECORD) {
        descriptor_type = "RudaManagedRecordDescriptor";
        describe = "ruda_managed_record_describe";
    } else if (layout.kind == RUDA_NATIVE_C_MANAGED_ENUM) {
        if (!ruda_native_c_write(buffer,
            "  { const RudaManagedEnumDescriptor *ruda_g_actual = NULL; size_t ruda_g_variant = 0; ruda_g_m = ruda_managed_enum_describe(&ruda_g_x->heap, ruda_g_v_%zu, &ruda_g_actual, &ruda_g_variant); if (ruda_g_m != RUDA_MANAGED_OK || ruda_g_actual != &",
            parameter) || !ruda_native_c_write_descriptor_name(
                buffer, layout.kind, *type
            )) return false;
        return ruda_native_c_write(buffer,
            ") { ruda_g_s = ruda_g_m == RUDA_MANAGED_OK ? 7 : ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } }\n");
    } else {
        if (!ruda_native_c_write(buffer,
            "  { const RudaManagedInterfaceDescriptor *ruda_g_actual = NULL; RudaManagedReference ruda_g_payload; const void *ruda_g_methods = NULL; ruda_g_m = ruda_managed_interface_describe(&ruda_g_x->heap, ruda_g_v_%zu, &ruda_g_actual, &ruda_g_payload, &ruda_g_methods); if (ruda_g_m != RUDA_MANAGED_OK || ruda_g_actual != &",
            parameter) || !ruda_native_c_write_descriptor_name(
                buffer, layout.kind, *type
            )) return false;
        return ruda_native_c_write(buffer,
            ") { ruda_g_s = ruda_g_m == RUDA_MANAGED_OK ? 7 : ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } }\n");
    }
    if (!ruda_native_c_write(buffer,
        "  { const %s *ruda_g_actual = NULL; ruda_g_m = %s(&ruda_g_x->heap, ruda_g_v_%zu, &ruda_g_actual); if (ruda_g_m != RUDA_MANAGED_OK || ruda_g_actual != ",
        descriptor_type, describe, parameter)) return false;
    if (layout.kind == RUDA_NATIVE_C_MANAGED_ARRAY) {
        if (!ruda_native_c_write(buffer, "%s",
            ruda_native_c_array_descriptor(layout.function, *type))) return false;
    } else if (!ruda_native_c_write(buffer, "&")
        || !ruda_native_c_write_descriptor_name(buffer, layout.kind, *type)) {
        return false;
    }
    return ruda_native_c_write(buffer,
        ") { ruda_g_s = ruda_g_m == RUDA_MANAGED_OK ? 7 : ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } }\n");
}
