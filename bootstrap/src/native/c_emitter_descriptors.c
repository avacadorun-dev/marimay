#include "c_emitter_internal.h"

#include <stdint.h>

const MarimayMirArray *mmay_native_c_register_array(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id
) {
    const MarimayTypeId *type = function != NULL
        ? mmay_vector_at_const(&function->register_types, register_id) : NULL;
    return type != NULL ? mmay_native_c_array(function, *type) : NULL;
}

static bool mmay_native_c_function_layout(
    const MarimayMirFunction *function,
    MarimayTypeId type,
    MarimayNativeCManagedLayout *result
) {
    size_t index = 0;
    if (function == NULL || result == NULL || type < MMAY_TYPE_FIRST_DYNAMIC) {
        return false;
    }
    for (index = 0; index < function->arrays.length; index += 1) {
        const MarimayMirArray *layout = mmay_vector_at_const(
            &function->arrays, index
        );
        if (layout != NULL && layout->type == type) {
            *result = (MarimayNativeCManagedLayout){
                function, MMAY_NATIVE_C_MANAGED_ARRAY, index
            };
            return true;
        }
    }
    for (index = 0; index < function->collections.length; index += 1) {
        const MarimayMirCollection *layout = mmay_vector_at_const(
            &function->collections, index
        );
        if (layout != NULL && layout->type == type) {
            *result = (MarimayNativeCManagedLayout){
                function, MMAY_NATIVE_C_MANAGED_COLLECTION, index
            };
            return true;
        }
    }
    for (index = 0; index < function->records.length; index += 1) {
        const MarimayMirRecord *layout = mmay_vector_at_const(
            &function->records, index
        );
        if (layout != NULL && layout->type == type) {
            *result = (MarimayNativeCManagedLayout){
                function, MMAY_NATIVE_C_MANAGED_RECORD, index
            };
            return true;
        }
    }
    for (index = 0; index < function->enums.length; index += 1) {
        const MarimayMirEnum *layout = mmay_vector_at_const(
            &function->enums, index
        );
        if (layout != NULL && layout->type == type) {
            *result = (MarimayNativeCManagedLayout){
                function, MMAY_NATIVE_C_MANAGED_ENUM, index
            };
            return true;
        }
    }
    for (index = 0; index < function->interfaces.length; index += 1) {
        const MarimayMirInterface *layout = mmay_vector_at_const(
            &function->interfaces, index
        );
        if (layout != NULL && layout->type == type) {
            *result = (MarimayNativeCManagedLayout){
                function, MMAY_NATIVE_C_MANAGED_INTERFACE, index
            };
            return true;
        }
    }
    return false;
}

bool mmay_native_c_find_managed_layout(
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayTypeId type,
    MarimayNativeCManagedLayout *result
) {
    size_t index = 0;
    if (functions == NULL || result == NULL) {
        return false;
    }
    for (index = 0; index < count; index += 1) {
        if (mmay_native_c_function_layout(functions[index], type, result)) {
            return true;
        }
    }
    return false;
}

static bool mmay_native_c_same_record(
    MarimayNativeCManagedLayout left,
    MarimayNativeCManagedLayout right
) {
    const MarimayMirRecord *a = mmay_vector_at_const(
        &left.function->records, left.index
    );
    const MarimayMirRecord *b = mmay_vector_at_const(
        &right.function->records, right.index
    );
    size_t index = 0;
    if (a == NULL || b == NULL || a->field_count != b->field_count) {
        return false;
    }
    for (index = 0; index < a->field_count; index += 1) {
        const MarimayMirRecordField *af = mmay_vector_at_const(
            &left.function->record_fields, a->field_start + index
        );
        const MarimayMirRecordField *bf = mmay_vector_at_const(
            &right.function->record_fields, b->field_start + index
        );
        if (af == NULL || bf == NULL || af->type != bf->type
            || af->is_mutable != bf->is_mutable) {
            return false;
        }
    }
    return true;
}

static bool mmay_native_c_same_enum(
    MarimayNativeCManagedLayout left,
    MarimayNativeCManagedLayout right
) {
    const MarimayMirEnum *a = mmay_vector_at_const(
        &left.function->enums, left.index
    );
    const MarimayMirEnum *b = mmay_vector_at_const(
        &right.function->enums, right.index
    );
    size_t variant = 0;
    if (a == NULL || b == NULL || a->variant_count != b->variant_count) {
        return false;
    }
    for (variant = 0; variant < a->variant_count; variant += 1) {
        const MarimayMirEnumVariant *av = mmay_vector_at_const(
            &left.function->enum_variants, a->variant_start + variant
        );
        const MarimayMirEnumVariant *bv = mmay_vector_at_const(
            &right.function->enum_variants, b->variant_start + variant
        );
        size_t payload = 0;
        if (av == NULL || bv == NULL || av->kind != bv->kind
            || av->payload_count != bv->payload_count) {
            return false;
        }
        for (payload = 0; payload < av->payload_count; payload += 1) {
            const MarimayMirEnumPayload *ap = mmay_vector_at_const(
                &left.function->enum_payloads, av->payload_start + payload
            );
            const MarimayMirEnumPayload *bp = mmay_vector_at_const(
                &right.function->enum_payloads, bv->payload_start + payload
            );
            if (ap == NULL || bp == NULL || ap->type != bp->type) {
                return false;
            }
        }
    }
    return true;
}

static bool mmay_native_c_same_layout(
    MarimayNativeCManagedLayout left,
    MarimayNativeCManagedLayout right
) {
    if (left.kind != right.kind) {
        return false;
    }
    if (left.kind == MMAY_NATIVE_C_MANAGED_ARRAY) {
        const MarimayMirArray *a = mmay_vector_at_const(
            &left.function->arrays, left.index
        );
        const MarimayMirArray *b = mmay_vector_at_const(
            &right.function->arrays, right.index
        );
        return a != NULL && b != NULL && a->element_type == b->element_type;
    }
    if (left.kind == MMAY_NATIVE_C_MANAGED_COLLECTION) {
        const MarimayMirCollection *a = mmay_vector_at_const(
            &left.function->collections, left.index
        );
        const MarimayMirCollection *b = mmay_vector_at_const(
            &right.function->collections, right.index
        );
        return a != NULL && b != NULL && a->key_type == b->key_type
            && a->value_type == b->value_type && a->is_set == b->is_set;
    }
    if (left.kind == MMAY_NATIVE_C_MANAGED_RECORD) {
        return mmay_native_c_same_record(left, right);
    }
    if (left.kind == MMAY_NATIVE_C_MANAGED_ENUM) {
        return mmay_native_c_same_enum(left, right);
    }
    if (left.kind == MMAY_NATIVE_C_MANAGED_INTERFACE) {
        const MarimayMirInterface *a = mmay_vector_at_const(
            &left.function->interfaces, left.index
        );
        const MarimayMirInterface *b = mmay_vector_at_const(
            &right.function->interfaces, right.index
        );
        return a != NULL && b != NULL && a->method_count == b->method_count;
    }
    return false;
}

bool mmay_native_c_program_type_supported(
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayTypeId type,
    bool result
) {
    MarimayNativeCManagedLayout layout;
    if (type == MMAY_TYPE_BOOL || type == MMAY_TYPE_I8
        || type == MMAY_TYPE_I16 || type == MMAY_TYPE_I32
        || type == MMAY_TYPE_I64 || type == MMAY_TYPE_U8
        || type == MMAY_TYPE_U16 || type == MMAY_TYPE_U32
        || type == MMAY_TYPE_RUNE
        || type == MMAY_TYPE_U64 || type == MMAY_TYPE_F32
        || type == MMAY_TYPE_F64
        || type == MMAY_TYPE_STRING) {
        return true;
    }
    if (type == MMAY_TYPE_VOID
        || (result && type == MMAY_TYPE_NEVER)) {
        return true;
    }
    if (!mmay_native_c_find_managed_layout(functions, count, type, &layout)) {
        return false;
    }
    return layout.kind != MMAY_NATIVE_C_MANAGED_ARRAY
        || mmay_native_c_array_kind(layout.function, type)
            != MMAY_NATIVE_C_ARRAY_UNSUPPORTED;
}

static bool mmay_native_c_layout_fields_supported(
    MarimayNativeCManagedLayout layout,
    const MarimayMirFunction *const *functions,
    size_t count
) {
    size_t index = 0;
    if (layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY) {
        const MarimayMirArray *array = mmay_vector_at_const(
            &layout.function->arrays, layout.index
        );
        return array != NULL && mmay_native_c_program_type_supported(
            functions, count, array->element_type, false
        );
    }
    if (layout.kind == MMAY_NATIVE_C_MANAGED_COLLECTION) {
        const MarimayMirCollection *collection = mmay_vector_at_const(
            &layout.function->collections, layout.index
        );
        return collection != NULL
            && mmay_native_c_program_type_supported(
                functions, count, collection->key_type, false
            ) && (collection->is_set
                || mmay_native_c_program_type_supported(
                    functions, count, collection->value_type, false
                ));
    }
    if (layout.kind == MMAY_NATIVE_C_MANAGED_RECORD) {
        const MarimayMirRecord *record = mmay_vector_at_const(
            &layout.function->records, layout.index
        );
        if (record == NULL) return false;
        for (index = 0; index < record->field_count; index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &layout.function->record_fields, record->field_start + index
            );
            if (field == NULL || !mmay_native_c_program_type_supported(
                    functions, count, field->type, false
                )) return false;
        }
    } else if (layout.kind == MMAY_NATIVE_C_MANAGED_ENUM) {
        const MarimayMirEnum *enumeration = mmay_vector_at_const(
            &layout.function->enums, layout.index
        );
        if (enumeration == NULL) return false;
        for (index = 0; index < enumeration->payload_count; index += 1) {
            const MarimayMirEnumPayload *payload = mmay_vector_at_const(
                &layout.function->enum_payloads,
                enumeration->payload_start + index
            );
            if (payload == NULL || !mmay_native_c_program_type_supported(
                    functions, count, payload->type, false
                )) return false;
        }
    }
    return true;
}

bool mmay_native_c_managed_layouts_valid(
    const MarimayMirFunction *const *functions,
    size_t count
) {
    size_t function_index = 0;
    if (functions == NULL) return false;
    for (function_index = 0; function_index < count; function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        MarimayNativeCManagedKind kind = MMAY_NATIVE_C_MANAGED_ARRAY;
        if (function == NULL) return false;
        for (kind = MMAY_NATIVE_C_MANAGED_ARRAY;
            kind <= MMAY_NATIVE_C_MANAGED_INTERFACE; kind += 1) {
            const MarimayVector *layouts = kind == MMAY_NATIVE_C_MANAGED_ARRAY
                ? &function->arrays : (kind == MMAY_NATIVE_C_MANAGED_COLLECTION
                    ? &function->collections : (kind == MMAY_NATIVE_C_MANAGED_RECORD
                    ? &function->records : (kind == MMAY_NATIVE_C_MANAGED_ENUM
                        ? &function->enums : &function->interfaces)));
            size_t index = 0;
            for (index = 0; index < layouts->length; index += 1) {
                MarimayTypeId type = MMAY_TYPE_INVALID;
                MarimayNativeCManagedLayout current = {function, kind, index};
                MarimayNativeCManagedLayout canonical;
                if (kind == MMAY_NATIVE_C_MANAGED_ARRAY) {
                    const MarimayMirArray *value = mmay_vector_at_const(layouts, index);
                    if (value != NULL) type = value->type;
                } else if (kind == MMAY_NATIVE_C_MANAGED_COLLECTION) {
                    const MarimayMirCollection *value =
                        mmay_vector_at_const(layouts, index);
                    if (value != NULL) type = value->type;
                } else if (kind == MMAY_NATIVE_C_MANAGED_RECORD) {
                    const MarimayMirRecord *value = mmay_vector_at_const(layouts, index);
                    if (value != NULL) type = value->type;
                } else if (kind == MMAY_NATIVE_C_MANAGED_ENUM) {
                    const MarimayMirEnum *value = mmay_vector_at_const(layouts, index);
                    if (value != NULL) type = value->type;
                } else {
                    const MarimayMirInterface *value = mmay_vector_at_const(layouts, index);
                    if (value != NULL) type = value->type;
                }
                if (type == MMAY_TYPE_INVALID
                    || !mmay_native_c_find_managed_layout(
                        functions, count, type, &canonical
                    ) || !mmay_native_c_same_layout(canonical, current)
                    || !mmay_native_c_layout_fields_supported(
                        current, functions, count
                )) return false;
            }
        }
        {
            size_t construction_index = 0;
            for (construction_index = 0;
                construction_index < function->interface_constructions.length;
                construction_index += 1) {
                const MarimayMirInterfaceConstruction *construction =
                    mmay_vector_at_const(
                        &function->interface_constructions, construction_index
                    );
                const MarimayMirInterface *interface = construction != NULL
                    ? mmay_vector_at_const(
                        &function->interfaces, construction->interface_id
                    ) : NULL;
                MarimayNativeCManagedLayout payload_layout;
                size_t method_index = 0;
                if (construction == NULL || interface == NULL
                    || construction->method_count != interface->method_count
                    || !mmay_native_c_find_managed_layout(
                        functions, count, construction->payload_type,
                        &payload_layout
                    )) return false;
                for (method_index = 0;
                    method_index < construction->method_count;
                    method_index += 1) {
                    const MarimaySymbolId *symbol = mmay_vector_at_const(
                        &function->interface_methods,
                        construction->method_start + method_index
                    );
                    const MarimayMirFunction *callee = symbol != NULL
                        ? mmay_native_c_find(functions, count, *symbol) : NULL;
                    const MarimayTypeId *receiver = callee != NULL
                        ? mmay_vector_at_const(&callee->register_types, 0)
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

bool mmay_native_c_write_descriptor_name(
    MarimayNativeCBuffer *buffer,
    MarimayNativeCManagedKind kind,
    MarimayTypeId type
) {
    const char *prefix = kind == MMAY_NATIVE_C_MANAGED_COLLECTION ? "md"
        : (kind == MMAY_NATIVE_C_MANAGED_RECORD ? "rd"
        : (kind == MMAY_NATIVE_C_MANAGED_ENUM ? "ed"
            : (kind == MMAY_NATIVE_C_MANAGED_INTERFACE ? "id" : NULL)));
    return prefix != NULL && mmay_native_c_write(
        buffer, "mmay_g_%s_%u", prefix, (unsigned int)type
    );
}

bool mmay_native_c_write_interface_table_name(
    MarimayNativeCBuffer *buffer,
    MarimaySymbolId symbol,
    size_t construction
) {
    return mmay_native_c_write(buffer, "mmay_g_it_")
        && mmay_native_c_symbol_hex(buffer, symbol)
        && mmay_native_c_write(buffer, "_%zu", construction);
}

static bool mmay_native_c_emit_field_descriptor(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayTypeId type,
    bool is_mutable
) {
    MarimayNativeCManagedLayout layout;
    if (type == MMAY_TYPE_STRING) {
        return mmay_native_c_write(
            buffer,
            "{MMAY_MANAGED_CELL_REFERENCE, %s, MMAY_MANAGED_OBJECT_STRING, NULL}",
            is_mutable ? "true" : "false"
        );
    }
    if (type < MMAY_TYPE_FIRST_DYNAMIC) {
        return mmay_native_c_write(
            buffer,
            "{MMAY_MANAGED_CELL_RAW64, %s, MMAY_MANAGED_OBJECT_INVALID, NULL}",
            is_mutable ? "true" : "false"
        );
    }
    if (!mmay_native_c_find_managed_layout(functions, count, type, &layout)) {
        return false;
    }
    if (layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY) {
        return mmay_native_c_write(
            buffer, "{MMAY_MANAGED_CELL_REFERENCE, %s, MMAY_MANAGED_OBJECT_ARRAY, %s}",
            is_mutable ? "true" : "false",
            mmay_native_c_array_descriptor(layout.function, type)
        );
    }
    if (layout.kind == MMAY_NATIVE_C_MANAGED_COLLECTION) {
        if (!mmay_native_c_write(
                buffer,
                "{MMAY_MANAGED_CELL_REFERENCE, %s, MMAY_MANAGED_OBJECT_MAP, &",
                is_mutable ? "true" : "false"
            ) || !mmay_native_c_write_descriptor_name(
                buffer, layout.kind, type
            )) return false;
        return mmay_native_c_write(buffer, "}");
    }
    if (!mmay_native_c_write(
            buffer, "{MMAY_MANAGED_CELL_REFERENCE, %s, %s, &",
            is_mutable ? "true" : "false",
            layout.kind == MMAY_NATIVE_C_MANAGED_RECORD
                ? "MMAY_MANAGED_OBJECT_RECORD"
                : (layout.kind == MMAY_NATIVE_C_MANAGED_ENUM
                    ? "MMAY_MANAGED_OBJECT_ENUM"
                    : "MMAY_MANAGED_OBJECT_INTERFACE")
        ) || !mmay_native_c_write_descriptor_name(buffer, layout.kind, type)) {
        return false;
    }
    return mmay_native_c_write(buffer, "}");
}

static bool mmay_native_c_layout_is_canonical(
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayNativeCManagedLayout layout,
    MarimayTypeId type
) {
    MarimayNativeCManagedLayout canonical;
    return mmay_native_c_find_managed_layout(
            functions, count, type, &canonical
        ) && canonical.function == layout.function
        && canonical.kind == layout.kind && canonical.index == layout.index;
}

static bool mmay_native_c_enum_has_immediate_variant(
    const MarimayMirFunction *function, const MarimayMirEnum *enumeration
) {
    size_t index = 0;
    if (function == NULL || enumeration == NULL
        || enumeration->variant_count != 2) return false;
    for (index = 0; index < enumeration->variant_count; index += 1) {
        const MarimayMirEnumVariant *variant = mmay_vector_at_const(
            &function->enum_variants, enumeration->variant_start + index
        );
        const MarimayMirEnumPayload *payload = variant != NULL
                && variant->payload_count == 1
            ? mmay_vector_at_const(
                &function->enum_payloads, variant->payload_start
            ) : NULL;
        if (variant != NULL && (variant->payload_count == 0
            || (variant->payload_count == 1 && payload != NULL
                && payload->type > MMAY_TYPE_INVALID
                && payload->type < MMAY_TYPE_FIRST_DYNAMIC
                && payload->type != MMAY_TYPE_STRING
                && payload->type != MMAY_TYPE_VOID
                && payload->type != MMAY_TYPE_NEVER))) return true;
    }
    return false;
}

bool mmay_native_c_immediate_enum_id(
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayTypeId type,
    uint32_t *result
) {
    size_t function_index = 0;
    uint32_t next = 0;
    if (functions == NULL || result == NULL) return false;
    for (function_index = 0; function_index < count; function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        size_t index = 0;
        for (index = 0; function != NULL && index < function->enums.length;
            index += 1) {
            const MarimayMirEnum *enumeration = mmay_vector_at_const(
                &function->enums, index
            );
            MarimayNativeCManagedLayout layout = {
                function, MMAY_NATIVE_C_MANAGED_ENUM, index
            };
            if (enumeration != NULL
                && mmay_native_c_layout_is_canonical(
                    functions, count, layout, enumeration->type
                ) && mmay_native_c_enum_has_immediate_variant(
                    function, enumeration
                )) {
                if (next == UINT32_C(0x3fffffff)) {
                    return false;
                }
                next += UINT32_C(1);
                if (enumeration->type == type) {
                    *result = next;
                    return true;
                }
            }
        }
    }
    return false;
}

static bool mmay_native_c_emit_immediate_enum_registration(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count
) {
    size_t function_index = 0;
    uint32_t expected = 0;
    if (!mmay_native_c_write(
            buffer,
            "static MarimayManagedStatus mmay_g_register_immediate_enums(MarimayManagedHeap *mmay_g_heap) { MarimayManagedStatus mmay_g_status = MMAY_MANAGED_OK; uint32_t mmay_g_id = 0; (void)mmay_g_heap; (void)mmay_g_status; (void)mmay_g_id;\n"
        )) return false;
    for (function_index = 0; function_index < count; function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        size_t index = 0;
        for (index = 0; function != NULL && index < function->enums.length;
            index += 1) {
            const MarimayMirEnum *enumeration = mmay_vector_at_const(
                &function->enums, index
            );
            MarimayNativeCManagedLayout layout = {
                function, MMAY_NATIVE_C_MANAGED_ENUM, index
            };
            if (enumeration != NULL
                && mmay_native_c_layout_is_canonical(
                    functions, count, layout, enumeration->type
                ) && mmay_native_c_enum_has_immediate_variant(
                    function, enumeration
                )) {
                expected += UINT32_C(1);
                if (!mmay_native_c_write(
                        buffer,
                        "  mmay_g_status = mmay_managed_enum_register_immediate_descriptor(mmay_g_heap, &"
                    ) || !mmay_native_c_write_descriptor_name(
                        buffer, layout.kind, enumeration->type
                    ) || !mmay_native_c_write(
                        buffer,
                        ", &mmay_g_id); if (mmay_g_status != MMAY_MANAGED_OK || mmay_g_id != UINT32_C(%u)) return mmay_g_status == MMAY_MANAGED_OK ? MMAY_MANAGED_TYPE_MISMATCH : mmay_g_status;\n",
                        (unsigned int)expected
                    )) return false;
            }
        }
    }
    return mmay_native_c_write(buffer, "  return MMAY_MANAGED_OK; }\n\n");
}

static bool mmay_native_c_emit_array_element_descriptor(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirArray *array
) {
    MarimayNativeCManagedLayout layout;
    if (array == NULL) return false;
    if (array->element_type == MMAY_TYPE_STRING) {
        return mmay_native_c_write(
            buffer,
            "MMAY_MANAGED_CELL_REFERENCE, MMAY_MANAGED_ARRAY_STORAGE_CELL, sizeof(MarimayManagedCell), MMAY_MANAGED_OBJECT_STRING, NULL"
        );
    }
    if (array->element_type < MMAY_TYPE_FIRST_DYNAMIC) {
        const char *storage = NULL;
        const char *stride = NULL;
        if (array->element_type == MMAY_TYPE_BOOL
            || array->element_type == MMAY_TYPE_I8
            || array->element_type == MMAY_TYPE_U8) {
            storage = "MMAY_MANAGED_ARRAY_STORAGE_RAW8";
            stride = "sizeof(uint8_t)";
        } else if (array->element_type == MMAY_TYPE_I16
            || array->element_type == MMAY_TYPE_U16) {
            storage = "MMAY_MANAGED_ARRAY_STORAGE_RAW16";
            stride = "sizeof(uint16_t)";
        } else if (array->element_type == MMAY_TYPE_I32
            || array->element_type == MMAY_TYPE_U32
            || array->element_type == MMAY_TYPE_RUNE
            || array->element_type == MMAY_TYPE_F32) {
            storage = "MMAY_MANAGED_ARRAY_STORAGE_RAW32";
            stride = "sizeof(uint32_t)";
        } else if (array->element_type == MMAY_TYPE_I64
            || array->element_type == MMAY_TYPE_U64
            || array->element_type == MMAY_TYPE_F64) {
            storage = "MMAY_MANAGED_ARRAY_STORAGE_RAW64";
            stride = "sizeof(uint64_t)";
        }
        return storage != NULL && stride != NULL && mmay_native_c_write(
            buffer,
            "MMAY_MANAGED_CELL_RAW64, %s, %s, MMAY_MANAGED_OBJECT_INVALID, NULL",
            storage, stride
        );
    }
    if (!mmay_native_c_find_managed_layout(
            functions, count, array->element_type, &layout
        )) return false;
    if (layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY) {
        return mmay_native_c_write(
            buffer,
            "MMAY_MANAGED_CELL_REFERENCE, MMAY_MANAGED_ARRAY_STORAGE_CELL, sizeof(MarimayManagedCell), MMAY_MANAGED_OBJECT_ARRAY, %s",
            mmay_native_c_array_descriptor(
                layout.function, array->element_type
            )
        );
    }
    if (layout.kind == MMAY_NATIVE_C_MANAGED_COLLECTION) {
        if (!mmay_native_c_write(
                buffer,
                "MMAY_MANAGED_CELL_REFERENCE, MMAY_MANAGED_ARRAY_STORAGE_CELL, sizeof(MarimayManagedCell), MMAY_MANAGED_OBJECT_MAP, &"
            ) || !mmay_native_c_write_descriptor_name(
                buffer, layout.kind, array->element_type
            )) return false;
        return true;
    }
    if (!mmay_native_c_write(
        buffer,
        "MMAY_MANAGED_CELL_REFERENCE, MMAY_MANAGED_ARRAY_STORAGE_CELL, sizeof(MarimayManagedCell), %s, &",
            layout.kind == MMAY_NATIVE_C_MANAGED_RECORD
                ? "MMAY_MANAGED_OBJECT_RECORD"
                : (layout.kind == MMAY_NATIVE_C_MANAGED_ENUM
                    ? "MMAY_MANAGED_OBJECT_ENUM"
                    : "MMAY_MANAGED_OBJECT_INTERFACE")
        ) || !mmay_native_c_write_descriptor_name(
            buffer, layout.kind, array->element_type
        )) return false;
    return true;
}

static bool mmay_native_c_emit_nominal_forward_declarations(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count
) {
    MarimayNativeCManagedKind kind = MMAY_NATIVE_C_MANAGED_RECORD;
    size_t function_index = 0;
    for (kind = MMAY_NATIVE_C_MANAGED_RECORD;
        kind <= MMAY_NATIVE_C_MANAGED_INTERFACE; kind += 1) {
        for (function_index = 0; function_index < count; function_index += 1) {
            const MarimayMirFunction *function = functions[function_index];
            const MarimayVector *layouts = kind == MMAY_NATIVE_C_MANAGED_RECORD
                ? &function->records : (kind == MMAY_NATIVE_C_MANAGED_ENUM
                    ? &function->enums : &function->interfaces);
            size_t index = 0;
            for (index = 0; index < layouts->length; index += 1) {
                MarimayTypeId type = kind == MMAY_NATIVE_C_MANAGED_RECORD
                    ? ((const MarimayMirRecord *)mmay_vector_at_const(
                        layouts, index))->type
                    : (kind == MMAY_NATIVE_C_MANAGED_ENUM
                        ? ((const MarimayMirEnum *)mmay_vector_at_const(
                            layouts, index))->type
                        : ((const MarimayMirInterface *)mmay_vector_at_const(
                            layouts, index))->type);
                MarimayNativeCManagedLayout layout = {function, kind, index};
                const char *name = kind == MMAY_NATIVE_C_MANAGED_RECORD
                    ? "Record" : (kind == MMAY_NATIVE_C_MANAGED_ENUM
                        ? "Enum" : "Interface");
                if (mmay_native_c_layout_is_canonical(
                        functions, count, layout, type
                    ) && (!mmay_native_c_write(
                        buffer, "static const MarimayManaged%sDescriptor ", name
                    ) || !mmay_native_c_write_descriptor_name(
                        buffer, kind, type
                    ) || !mmay_native_c_write(buffer, ";\n"))) return false;
            }
        }
    }
    return true;
}

static bool mmay_native_c_emit_record_descriptor(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayNativeCManagedLayout layout
) {
    const MarimayMirRecord *record = mmay_vector_at_const(
        &layout.function->records, layout.index
    );
    size_t index = 0;
    if (record == NULL) return false;
    if (record->field_count != 0) {
        if (!mmay_native_c_write(buffer, "static const MarimayManagedFieldDescriptor ")
            || !mmay_native_c_write_descriptor_name(
                buffer, layout.kind, record->type
            ) || !mmay_native_c_write(buffer, "_fields[] = {")) return false;
        for (index = 0; index < record->field_count; index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &layout.function->record_fields, record->field_start + index
            );
            if (field == NULL || (index != 0 && !mmay_native_c_write(buffer, ","))
                || !mmay_native_c_emit_field_descriptor(
                    buffer, functions, count, field->type, field->is_mutable
                )) return false;
        }
        if (!mmay_native_c_write(buffer, "};\n")) return false;
    }
    if (!mmay_native_c_write(buffer, "static const MarimayManagedRecordDescriptor ")
        || !mmay_native_c_write_descriptor_name(buffer, layout.kind, record->type)
        || !mmay_native_c_write(buffer, " = {%zu, ", record->field_count)) {
        return false;
    }
    if (record->field_count == 0) return mmay_native_c_write(buffer, "NULL};\n");
    return mmay_native_c_write_descriptor_name(buffer, layout.kind, record->type)
        && mmay_native_c_write(buffer, "_fields};\n");
}

static bool mmay_native_c_emit_enum_descriptor(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayNativeCManagedLayout layout
) {
    const MarimayMirEnum *enumeration = mmay_vector_at_const(
        &layout.function->enums, layout.index
    );
    size_t index = 0;
    if (enumeration == NULL) return false;
    if (enumeration->payload_count != 0) {
        if (!mmay_native_c_write(buffer, "static const MarimayManagedFieldDescriptor ")
            || !mmay_native_c_write_descriptor_name(
                buffer, layout.kind, enumeration->type
            ) || !mmay_native_c_write(buffer, "_payloads[] = {")) return false;
        for (index = 0; index < enumeration->payload_count; index += 1) {
            const MarimayMirEnumPayload *payload = mmay_vector_at_const(
                &layout.function->enum_payloads,
                enumeration->payload_start + index
            );
            if (payload == NULL || (index != 0 && !mmay_native_c_write(buffer, ","))
                || !mmay_native_c_emit_field_descriptor(
                    buffer, functions, count, payload->type, false
                )) return false;
        }
        if (!mmay_native_c_write(buffer, "};\n")) return false;
    }
    if (enumeration->variant_count != 0
        && (!mmay_native_c_write(buffer,
            "static const MarimayManagedEnumVariantDescriptor ")
            || !mmay_native_c_write_descriptor_name(
                buffer, layout.kind, enumeration->type
            ) || !mmay_native_c_write(buffer, "_variants[] = {"))) return false;
    for (index = 0; index < enumeration->variant_count; index += 1) {
        const MarimayMirEnumVariant *variant = mmay_vector_at_const(
            &layout.function->enum_variants,
            enumeration->variant_start + index
        );
        if (variant == NULL || (index != 0 && !mmay_native_c_write(buffer, ","))) {
            return false;
        }
        if (variant->payload_count == 0) {
            if (!mmay_native_c_write(buffer, "{0,NULL}")) return false;
        } else if (!mmay_native_c_write(buffer, "{%zu,", variant->payload_count)
            || !mmay_native_c_write_descriptor_name(
                buffer, layout.kind, enumeration->type
            ) || !mmay_native_c_write(
                buffer, "_payloads + %zu}",
                variant->payload_start - enumeration->payload_start
            )) return false;
    }
    if (enumeration->variant_count != 0
        && !mmay_native_c_write(buffer, "};\n")) return false;
    if (!mmay_native_c_write(buffer, "static const MarimayManagedEnumDescriptor ")
        || !mmay_native_c_write_descriptor_name(
            buffer, layout.kind, enumeration->type
        ) || !mmay_native_c_write(
            buffer, " = {%zu, ", enumeration->variant_count
        )) return false;
    if (enumeration->variant_count == 0) {
        return mmay_native_c_write(buffer, "NULL};\n");
    }
    return mmay_native_c_write_descriptor_name(
            buffer, layout.kind, enumeration->type
        ) && mmay_native_c_write(buffer, "_variants};\n");
}

static bool mmay_native_c_emit_interface_tables(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count
) {
    size_t function_index = 0;
    for (function_index = 0; function_index < count; function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        size_t construction_index = 0;
        for (construction_index = 0;
            construction_index < function->interface_constructions.length;
            construction_index += 1) {
            const MarimayMirInterfaceConstruction *construction =
                mmay_vector_at_const(
                    &function->interface_constructions, construction_index
                );
            size_t method = 0;
            if (construction == NULL
                || !mmay_native_c_write(buffer, "static const uint32_t ")
                || !mmay_native_c_write_interface_table_name(
                    buffer, function->symbol, construction_index
                ) || !mmay_native_c_write(buffer, "[%zu] = {",
                    construction->method_count == 0
                        ? (size_t)1 : construction->method_count)) return false;
            for (method = 0; method < construction->method_count; method += 1) {
                const MarimaySymbolId *symbol = mmay_vector_at_const(
                    &function->interface_methods,
                    construction->method_start + method
                );
                size_t target = 0;
                if (symbol == NULL || !mmay_native_c_function_index(
                        functions, count, *symbol, &target
                    ) || (method != 0 && !mmay_native_c_write(buffer, ","))
                    || !mmay_native_c_write(buffer, "%zu", target)) return false;
            }
            if (construction->method_count == 0
                && !mmay_native_c_write(buffer, "0")) return false;
            if (!mmay_native_c_write(buffer, "};\n")) return false;
        }
    }
    return true;
}

bool mmay_native_c_emit_managed_descriptors(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count
) {
    size_t function_index = 0;
    for (function_index = 0; function_index < count; function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        size_t index = 0;
        for (index = 0; index < function->arrays.length; index += 1) {
            const MarimayMirArray *array = mmay_vector_at_const(
                &function->arrays, index
            );
            MarimayNativeCManagedLayout layout = {
                function, MMAY_NATIVE_C_MANAGED_ARRAY, index
            };
            if (array != NULL && mmay_native_c_layout_is_canonical(
                    functions, count, layout, array->type
                ) && mmay_native_c_array_kind(function, array->type)
                    == MMAY_NATIVE_C_ARRAY_GENERIC
                && (!mmay_native_c_write(
                    buffer,
                    "static const MarimayManagedArrayDescriptor mmay_g_ad_%u;\n",
                    (unsigned int)array->type
                ))) return false;
        }
    }
    if (!mmay_native_c_emit_nominal_forward_declarations(
            buffer, functions, count
        ) || !mmay_native_c_emit_collection_descriptor_declarations(
            buffer, functions, count
        )) return false;
    for (function_index = 0; function_index < count; function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        size_t index = 0;
        for (index = 0; index < function->arrays.length; index += 1) {
            const MarimayMirArray *array = mmay_vector_at_const(
                &function->arrays, index
            );
            MarimayNativeCManagedLayout layout = {
                function, MMAY_NATIVE_C_MANAGED_ARRAY, index
            };
            if (array != NULL && mmay_native_c_layout_is_canonical(
                    functions, count, layout, array->type
                ) && mmay_native_c_array_kind(function, array->type)
                    == MMAY_NATIVE_C_ARRAY_GENERIC) {
                if (!mmay_native_c_write(
                        buffer,
                        "static const MarimayManagedArrayDescriptor mmay_g_ad_%u = {",
                        (unsigned int)array->type
                    ) || !mmay_native_c_emit_array_element_descriptor(
                        buffer, functions, count, array
                    )) return false;
                if (!mmay_native_c_write(buffer, "};\n")) return false;
            }
        }
    }
    if (!mmay_native_c_emit_collection_descriptors(
            buffer, functions, count
        )) return false;
    for (function_index = 0; function_index < count; function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        size_t index = 0;
        for (index = 0; index < function->records.length; index += 1) {
            const MarimayMirRecord *record = mmay_vector_at_const(
                &function->records, index
            );
            MarimayNativeCManagedLayout layout = {
                function, MMAY_NATIVE_C_MANAGED_RECORD, index
            };
            if (record != NULL && mmay_native_c_layout_is_canonical(
                    functions, count, layout, record->type
                ) && !mmay_native_c_emit_record_descriptor(
                    buffer, functions, count, layout
                )) return false;
        }
        for (index = 0; index < function->enums.length; index += 1) {
            const MarimayMirEnum *enumeration = mmay_vector_at_const(
                &function->enums, index
            );
            MarimayNativeCManagedLayout layout = {
                function, MMAY_NATIVE_C_MANAGED_ENUM, index
            };
            if (enumeration != NULL && mmay_native_c_layout_is_canonical(
                    functions, count, layout, enumeration->type
                ) && !mmay_native_c_emit_enum_descriptor(
                    buffer, functions, count, layout
                )) return false;
        }
        for (index = 0; index < function->interfaces.length; index += 1) {
            const MarimayMirInterface *interface = mmay_vector_at_const(
                &function->interfaces, index
            );
            MarimayNativeCManagedLayout layout = {
                function, MMAY_NATIVE_C_MANAGED_INTERFACE, index
            };
            if (interface != NULL && mmay_native_c_layout_is_canonical(
                    functions, count, layout, interface->type
                ) && (!mmay_native_c_write(
                    buffer, "static const MarimayManagedInterfaceDescriptor "
                ) || !mmay_native_c_write_descriptor_name(
                    buffer, layout.kind, interface->type
                ) || !mmay_native_c_write(
                    buffer, " = {%zu};\n", interface->method_count
                ))) return false;
        }
    }
    if (!mmay_native_c_emit_interface_tables(buffer, functions, count)
        || !mmay_native_c_emit_immediate_enum_registration(
            buffer, functions, count
        )
        || !mmay_native_c_write(
            buffer, "static void mmay_g_keep_managed(void) {\n"
        )) return false;
    for (function_index = 0; function_index < count; function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        size_t index = 0;
        for (index = 0; index < function->collections.length; index += 1) {
            const MarimayMirCollection *value = mmay_vector_at_const(
                &function->collections, index
            );
            MarimayNativeCManagedLayout layout = {
                function, MMAY_NATIVE_C_MANAGED_COLLECTION, index
            };
            if (value != NULL && mmay_native_c_layout_is_canonical(
                    functions, count, layout, value->type
                ) && (!mmay_native_c_write(buffer, "  (void)&")
                    || !mmay_native_c_write_descriptor_name(
                        buffer, layout.kind, value->type
                    ) || !mmay_native_c_write(buffer, ";\n"))) return false;
        }
        for (index = 0; index < function->records.length; index += 1) {
            const MarimayMirRecord *value = mmay_vector_at_const(&function->records, index);
            MarimayNativeCManagedLayout layout = {function, MMAY_NATIVE_C_MANAGED_RECORD, index};
            if (value != NULL && mmay_native_c_layout_is_canonical(functions, count, layout, value->type)
                && (!mmay_native_c_write(buffer, "  (void)&")
                    || !mmay_native_c_write_descriptor_name(buffer, layout.kind, value->type)
                    || !mmay_native_c_write(buffer, ";\n"))) return false;
        }
        for (index = 0; index < function->enums.length; index += 1) {
            const MarimayMirEnum *value = mmay_vector_at_const(&function->enums, index);
            MarimayNativeCManagedLayout layout = {function, MMAY_NATIVE_C_MANAGED_ENUM, index};
            if (value != NULL && mmay_native_c_layout_is_canonical(functions, count, layout, value->type)
                && (!mmay_native_c_write(buffer, "  (void)&")
                    || !mmay_native_c_write_descriptor_name(buffer, layout.kind, value->type)
                    || !mmay_native_c_write(buffer, ";\n"))) return false;
        }
        for (index = 0; index < function->interfaces.length; index += 1) {
            const MarimayMirInterface *value = mmay_vector_at_const(&function->interfaces, index);
            MarimayNativeCManagedLayout layout = {function, MMAY_NATIVE_C_MANAGED_INTERFACE, index};
            if (value != NULL && mmay_native_c_layout_is_canonical(functions, count, layout, value->type)
                && (!mmay_native_c_write(buffer, "  (void)&")
                    || !mmay_native_c_write_descriptor_name(buffer, layout.kind, value->type)
                    || !mmay_native_c_write(buffer, ";\n"))) return false;
        }
        for (index = 0; index < function->interface_constructions.length; index += 1) {
            if (!mmay_native_c_write(buffer, "  (void)")
                || !mmay_native_c_write_interface_table_name(
                    buffer, function->symbol, index
                ) || !mmay_native_c_write(buffer, ";\n")) return false;
        }
    }
    return mmay_native_c_write(buffer, "}\n\n");
}

static bool mmay_native_c_record_array_field_used(
    const MarimayMirFunction *function,
    MarimayMirRegister parameter,
    size_t field
) {
    size_t block_index = 0;
    if (function == NULL) return false;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) return false;
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && instruction->kind == MMAY_MIR_RECORD_GET
                && instruction->first == parameter
                && instruction->field == field
                && mmay_native_c_register_array(
                    function, instruction->target
                ) != NULL) {
                return true;
            }
        }
    }
    return false;
}

bool mmay_native_c_emit_managed_parameter_check(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    size_t parameter
) {
    const MarimayTypeId *type = mmay_vector_at_const(
        &function->register_types, parameter
    );
    MarimayNativeCManagedLayout layout;
    const char *descriptor_type = NULL;
    const char *describe = NULL;
    if (type == NULL || *type < MMAY_TYPE_FIRST_DYNAMIC) return type != NULL;
    if (!mmay_native_c_find_managed_layout(functions, count, *type, &layout)) {
        return false;
    }
    if (layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY) {
        return mmay_native_c_write(
            buffer,
            "  mmay_g_m = mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_v_%zu, %s, &mmay_g_av_%zu); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n",
            parameter,
            mmay_native_c_array_descriptor(layout.function, *type),
            parameter
        );
    } else if (layout.kind == MMAY_NATIVE_C_MANAGED_COLLECTION) {
        if (!mmay_native_c_write(
                buffer,
                "  { MarimayManagedSlot *mmay_g_slot = mmay_managed_generated_resolve(&mmay_g_x->heap, mmay_g_v_%zu); if (mmay_g_slot == NULL || mmay_g_slot->kind != MMAY_MANAGED_OBJECT_MAP || mmay_g_slot->as.map == NULL || mmay_g_slot->as.map->descriptor != &",
                parameter
            ) || !mmay_native_c_write_descriptor_name(
                buffer, layout.kind, *type
            )) return false;
        return mmay_native_c_write(
            buffer,
            ") { mmay_g_s = 7; goto mmay_g_cleanup; } }\n"
        );
    } else if (layout.kind == MMAY_NATIVE_C_MANAGED_RECORD) {
        const MarimayMirRecord *record = mmay_vector_at_const(
            &layout.function->records, layout.index
        );
        size_t field_index = 0;
        if (!mmay_native_c_write(
                buffer,
                "  if (mmay_g_ro_%zu != NULL) { mmay_g_rv_%zu.reference = mmay_g_v_%zu; mmay_g_rv_%zu.object = mmay_g_ro_%zu; mmay_g_m = mmay_g_ro_%zu->descriptor == &",
                parameter, parameter, parameter, parameter, parameter,
                parameter
            ) || !mmay_native_c_write_descriptor_name(
                buffer, layout.kind, *type
            )) {
            return false;
        }
        if (!mmay_native_c_write(
                buffer,
                " ? MMAY_MANAGED_OK : MMAY_MANAGED_TYPE_MISMATCH; } else { mmay_g_m = mmay_managed_record_generated_bind(&mmay_g_x->heap, mmay_g_v_%zu, &",
                parameter
            ) || !mmay_native_c_write_descriptor_name(
                buffer, layout.kind, *type
            ) || !mmay_native_c_write(
                buffer,
                ", &mmay_g_rv_%zu); } if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n",
                parameter
            ) || record == NULL) {
            return false;
        }
        for (field_index = 0; field_index < record->field_count;
            field_index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &layout.function->record_fields,
                record->field_start + field_index
            );
            MarimayNativeCManagedLayout field_layout;
            if (field != NULL
                && mmay_native_c_record_array_field_used(
                    function, (MarimayMirRegister)parameter, field_index
                ) && mmay_native_c_find_managed_layout(
                    functions, count, field->type, &field_layout
                ) && field_layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY
                && !mmay_native_c_write(
                    buffer,
                    "  if (mmay_g_ro_%zu_f_%zu != NULL) { mmay_g_rv_%zu_f_%zu.object = mmay_g_ro_%zu_f_%zu; mmay_g_m = mmay_g_ro_%zu_f_%zu->descriptor == %s ? MMAY_MANAGED_OK : MMAY_MANAGED_TYPE_MISMATCH; } else { mmay_g_m = mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_rv_%zu.object->fields[%zu].reference, %s, &mmay_g_rv_%zu_f_%zu); } if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n",
                    parameter, field_index,
                    parameter, field_index,
                    parameter, field_index,
                    parameter, field_index,
                    mmay_native_c_array_descriptor(
                        field_layout.function, field->type
                    ),
                    parameter, field_index,
                    mmay_native_c_array_descriptor(
                        field_layout.function, field->type
                    ),
                    parameter, field_index
                )) {
                return false;
            }
        }
        return true;
    } else if (layout.kind == MMAY_NATIVE_C_MANAGED_ENUM) {
        if (!mmay_native_c_write(buffer,
            "  { const MarimayManagedEnumDescriptor *mmay_g_actual = NULL; size_t mmay_g_variant = 0; mmay_g_m = mmay_managed_enum_describe(&mmay_g_x->heap, mmay_g_v_%zu, &mmay_g_actual, &mmay_g_variant); if (mmay_g_m != MMAY_MANAGED_OK || mmay_g_actual != &",
            parameter) || !mmay_native_c_write_descriptor_name(
                buffer, layout.kind, *type
            )) return false;
        return mmay_native_c_write(buffer,
            ") { mmay_g_s = mmay_g_m == MMAY_MANAGED_OK ? 7 : mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } }\n");
    } else {
        if (!mmay_native_c_write(buffer,
            "  { const MarimayManagedInterfaceDescriptor *mmay_g_actual = NULL; MarimayManagedReference mmay_g_payload; const void *mmay_g_methods = NULL; mmay_g_m = mmay_managed_interface_describe(&mmay_g_x->heap, mmay_g_v_%zu, &mmay_g_actual, &mmay_g_payload, &mmay_g_methods); if (mmay_g_m != MMAY_MANAGED_OK || mmay_g_actual != &",
            parameter) || !mmay_native_c_write_descriptor_name(
                buffer, layout.kind, *type
            )) return false;
        return mmay_native_c_write(buffer,
            ") { mmay_g_s = mmay_g_m == MMAY_MANAGED_OK ? 7 : mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } }\n");
    }
    if (!mmay_native_c_write(buffer,
        "  { const %s *mmay_g_actual = NULL; mmay_g_m = %s(&mmay_g_x->heap, mmay_g_v_%zu, &mmay_g_actual); if (mmay_g_m != MMAY_MANAGED_OK || mmay_g_actual != ",
        descriptor_type, describe, parameter)) return false;
    if (layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY) {
        if (!mmay_native_c_write(buffer, "%s",
            mmay_native_c_array_descriptor(layout.function, *type))) return false;
    } else if (!mmay_native_c_write(buffer, "&")
        || !mmay_native_c_write_descriptor_name(buffer, layout.kind, *type)) {
        return false;
    }
    return mmay_native_c_write(buffer,
        ") { mmay_g_s = mmay_g_m == MMAY_MANAGED_OK ? 7 : mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } }\n");
}
