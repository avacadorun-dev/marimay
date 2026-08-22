#include "c_emitter_internal.h"

#include <string.h>

static MarimayTypeId mmay_native_c_register_type(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id
) {
    const MarimayTypeId *type = function != NULL
        ? mmay_vector_at_const(&function->register_types, register_id) : NULL;
    return type != NULL ? *type : MMAY_TYPE_INVALID;
}

static bool mmay_native_c_record_parameter_view(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayMirRegister register_id
) {
    MarimayTypeId type = mmay_native_c_register_type(function, register_id);
    MarimayNativeCManagedLayout layout;
    return register_id < function->parameter_count
        && type >= MMAY_TYPE_FIRST_DYNAMIC
        && mmay_native_c_find_managed_layout(
            functions, count, type, &layout
        ) && layout.kind == MMAY_NATIVE_C_MANAGED_RECORD;
}

static bool mmay_native_c_record_view(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayMirRegister register_id,
    MarimayNativeCManagedLayout *layout
) {
    MarimayTypeId type = mmay_native_c_register_type(function, register_id);
    return layout != NULL && type >= MMAY_TYPE_FIRST_DYNAMIC
        && mmay_native_c_find_managed_layout(
            functions, count, type, layout
        ) && layout->kind == MMAY_NATIVE_C_MANAGED_RECORD;
}

static const MarimayMirRecordField *mmay_native_c_record_parameter_field(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayMirRegister register_id,
    size_t field_index
) {
    MarimayTypeId type = mmay_native_c_register_type(function, register_id);
    MarimayNativeCManagedLayout layout;
    const MarimayMirRecord *record = NULL;
    if (register_id >= function->parameter_count
        || !mmay_native_c_find_managed_layout(
            functions, count, type, &layout
        ) || layout.kind != MMAY_NATIVE_C_MANAGED_RECORD) {
        return NULL;
    }
    record = mmay_vector_at_const(&layout.function->records, layout.index);
    return record != NULL && field_index < record->field_count
        ? mmay_vector_at_const(
            &layout.function->record_fields,
            record->field_start + field_index
        ) : NULL;
}

static bool mmay_native_c_emit_record_view_get(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirInstruction *instruction,
    MarimayTypeId target_type,
    const MarimayNativeCManagedLayout *source_layout
) {
    MarimayNativeCManagedLayout target_layout;
    bool array_target = mmay_native_c_register_array(
        function, instruction->target
    ) != NULL;
    bool record_target = mmay_native_c_record_view(
        function, functions, count, instruction->target, &target_layout
    );
    if (source_layout == NULL || !mmay_native_c_write(
            buffer,
            "  { if (mmay_g_rv_%u.object == NULL) { mmay_g_m = mmay_managed_record_generated_bind(&mmay_g_x->heap, mmay_g_v_%u, &",
            instruction->first, instruction->first
        ) || !mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_RECORD,
            mmay_native_c_register_type(function, instruction->first)
        ) || !mmay_native_c_write(
            buffer,
            ", &mmay_g_rv_%u); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } } MarimayManagedCell mmay_g_cell = mmay_g_rv_%u.object->fields[%zu]; ",
            instruction->first, instruction->first, instruction->field
        ) || !mmay_native_c_write_from_managed_cell(
            buffer, target_type, instruction->target
        )) return false;
    if (array_target && !mmay_native_c_write(
            buffer,
            "; mmay_g_av_%u.object = mmay_managed_array_generated_child_unchecked(&mmay_g_x->heap, mmay_g_cell.reference)",
            instruction->target
        )) return false;
    if (record_target && !mmay_native_c_write(
            buffer,
            "; mmay_g_rv_%u.reference = mmay_g_cell.reference; mmay_g_rv_%u.object = mmay_managed_record_generated_child_unchecked(&mmay_g_x->heap, mmay_g_cell.reference)",
            instruction->target, instruction->target
        )) return false;
    return mmay_native_c_write(buffer, "; }\n");
}

static bool mmay_native_c_emit_record_null_hints(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    size_t parameter
) {
    const MarimayTypeId *type = function != NULL
        ? mmay_vector_at_const(&function->register_types, parameter) : NULL;
    MarimayNativeCManagedLayout layout;
    const MarimayMirRecord *record = NULL;
    size_t field_index = 0;
    if (function == NULL || parameter >= function->parameter_count
        || type == NULL || *type < MMAY_TYPE_FIRST_DYNAMIC
        || !mmay_native_c_find_managed_layout(
            functions, count, *type, &layout
        ) || layout.kind != MMAY_NATIVE_C_MANAGED_RECORD) {
        return true;
    }
    record = mmay_vector_at_const(&layout.function->records, layout.index);
    if (record == NULL || !mmay_native_c_write(buffer, ", NULL")) return false;
    for (field_index = 0; field_index < record->field_count;
        field_index += 1) {
        const MarimayMirRecordField *field = mmay_vector_at_const(
            &layout.function->record_fields, record->field_start + field_index
        );
        MarimayNativeCManagedLayout field_layout;
        if (field != NULL && mmay_native_c_find_managed_layout(
                functions, count, field->type, &field_layout
            ) && field_layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY
            && !mmay_native_c_write(buffer, ", NULL")) return false;
    }
    return true;
}

bool mmay_native_c_enum_construction_immediate(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirEnumConstruction *construction = function != NULL
        && instruction != NULL && instruction->kind == MMAY_MIR_ENUM_NEW
        ? mmay_vector_at_const(
            &function->enum_constructions, instruction->enum_construction
        ) : NULL;
    const MarimayMirEnum *enumeration = construction != NULL
        ? mmay_vector_at_const(&function->enums, construction->enum_id) : NULL;
    const MarimayMirEnumVariant *variant = enumeration != NULL
        && construction->variant < enumeration->variant_count
        ? mmay_vector_at_const(
            &function->enum_variants,
            enumeration->variant_start + construction->variant
        ) : NULL;
    const MarimayMirEnumPayload *payload = variant != NULL
        && variant->payload_count == 1
        ? mmay_vector_at_const(
            &function->enum_payloads, variant->payload_start
        ) : NULL;
    if (enumeration == NULL || enumeration->variant_count != 2
        || variant == NULL
        || construction->argument_count != variant->payload_count) {
        return false;
    }
    if (variant->payload_count == 0) {
        return true;
    }
    return variant->payload_count == 1 && payload != NULL
        && payload->type > MMAY_TYPE_INVALID
        && payload->type < MMAY_TYPE_FIRST_DYNAMIC
        && payload->type != MMAY_TYPE_STRING
        && payload->type != MMAY_TYPE_VOID
        && payload->type != MMAY_TYPE_NEVER;
}

bool mmay_native_c_write_to_managed_cell(
    MarimayNativeCBuffer *buffer,
    MarimayTypeId type,
    MarimayMirRegister register_id
) {
    if (type == MMAY_TYPE_STRING) {
        return mmay_native_c_write(
            buffer,
            "if (mmay_managed_reference_is_invalid(mmay_g_v_%u.managed)) { mmay_g_m = mmay_managed_string_new(&mmay_g_x->heap, mmay_g_v_%u.data, mmay_g_v_%u.length, &mmay_g_v_%u.managed); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } } mmay_g_cell.reference = mmay_g_v_%u.managed",
            register_id, register_id, register_id, register_id, register_id
        );
    }
    if (type >= MMAY_TYPE_I8 && type <= MMAY_TYPE_U64) {
        if (type == MMAY_TYPE_I64 || type == MMAY_TYPE_U64) {
            return mmay_native_c_write(
                buffer,
                "mmay_g_cell.f64_bits = (uint64_t)mmay_g_v_%u",
                register_id
            );
        }
        const char *kind = mmay_native_c_integer_kind_name(type);
        return kind != NULL && mmay_native_c_write(
            buffer,
            "{ MarimayIntegerValue mmay_g_cell_value = {%s, (uint64_t)mmay_g_v_%u}; unsigned int mmay_g_cell_width = mmay_integer_bit_width(%s); mmay_g_cell.f64_bits = mmay_g_cell_value.bits & (mmay_g_cell_width == 64U ? UINT64_MAX : (UINT64_C(1) << mmay_g_cell_width) - UINT64_C(1)); }",
            kind, register_id, kind
        );
    }
    if (type == MMAY_TYPE_BOOL) {
        return mmay_native_c_write(
            buffer,
            "mmay_g_cell.f64_bits = mmay_g_v_%u ? UINT64_C(1) : UINT64_C(0)",
            register_id
        );
    }
    if (type == MMAY_TYPE_F64) {
        return mmay_native_c_write(
            buffer, "mmay_g_cell.f64_bits = mmay_g_v_%u", register_id
        );
    }
    if (type == MMAY_TYPE_F32) {
        return mmay_native_c_write(
            buffer, "mmay_g_cell.f64_bits = mmay_g_v_%u", register_id
        );
    }
    return type >= MMAY_TYPE_FIRST_DYNAMIC && mmay_native_c_write(
        buffer, "mmay_g_cell.reference = mmay_g_v_%u", register_id
    );
}

bool mmay_native_c_write_from_managed_cell(
    MarimayNativeCBuffer *buffer,
    MarimayTypeId type,
    MarimayMirRegister target
) {
    if (type == MMAY_TYPE_STRING) {
        return mmay_native_c_write(
            buffer,
            "mmay_g_v_%u.managed = mmay_g_cell.reference; mmay_g_m = mmay_managed_string_bytes(&mmay_g_x->heap, mmay_g_v_%u.managed, &mmay_g_v_%u.data, &mmay_g_v_%u.length); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }",
            target, target, target, target
        );
    }
    if (type >= MMAY_TYPE_I8 && type <= MMAY_TYPE_U64) {
        if (type == MMAY_TYPE_I64) {
            return mmay_native_c_write(
                buffer,
                "mmay_g_v_%u = mmay_i64_from_twos_complement_fast(mmay_g_cell.f64_bits)",
                target
            );
        }
        if (type == MMAY_TYPE_U64) {
            return mmay_native_c_write(
                buffer, "mmay_g_v_%u = mmay_g_cell.f64_bits", target
            );
        }
        const char *kind = mmay_native_c_integer_kind_name(type);
        const char *convert = mmay_native_c_integer_conversion_name(type);
        return kind != NULL && convert != NULL && mmay_native_c_write(
            buffer,
            "{ MarimayIntegerValue mmay_g_cell_value = {%s, mmay_g_cell.f64_bits}; if (!%s(mmay_g_cell_value, &mmay_g_v_%u)) { mmay_g_s = 7; goto mmay_g_cleanup; } }",
            kind, convert, target
        );
    }
    if (type == MMAY_TYPE_BOOL) {
        return mmay_native_c_write(
            buffer, "mmay_g_v_%u = mmay_g_cell.f64_bits != 0", target
        );
    }
    if (type == MMAY_TYPE_F64) {
        return mmay_native_c_write(
            buffer, "mmay_g_v_%u = mmay_g_cell.f64_bits", target
        );
    }
    if (type == MMAY_TYPE_F32) {
        return mmay_native_c_write(
            buffer, "mmay_g_v_%u = (uint32_t)mmay_g_cell.f64_bits", target
        );
    }
    return type >= MMAY_TYPE_FIRST_DYNAMIC && mmay_native_c_write(
        buffer, "mmay_g_v_%u = mmay_g_cell.reference", target
    );
}

static bool mmay_native_c_emit_cells(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    size_t start,
    size_t count,
    bool record
) {
    size_t index = 0;
    if (count <= 64) {
        if (count == 0) {
            if (!mmay_native_c_write(
                    buffer, "  {\n"
                )) return false;
        } else if (!mmay_native_c_write(
                buffer,
                "  { MarimayManagedCell mmay_g_inline_nominal_cells[%zu];\n",
                count
            )) return false;
    } else if (!mmay_native_c_write(
            buffer,
            "  { mmay_g_nominal_cells = NULL; mmay_g_nominal_cell_bytes = 0;\n"
            "    if (!mmay_size_multiply(%zu, sizeof(*mmay_g_nominal_cells), &mmay_g_nominal_cell_bytes)) { mmay_g_s = 6; goto mmay_g_cleanup; }\n"
            "    if (mmay_g_nominal_cell_bytes != 0) { mmay_g_nominal_cells = mmay_allocate(&mmay_g_x->allocator, mmay_g_nominal_cell_bytes); if (mmay_g_nominal_cells == NULL) { mmay_g_s = 5; goto mmay_g_cleanup; } }\n",
            count
        )) return false;
    for (index = 0; index < count; index += 1) {
        const MarimayMirRecordArgument *record_argument = record
            ? mmay_vector_at_const(&function->record_arguments, start + index)
            : NULL;
        const MarimayMirEnumArgument *enum_argument = !record
            ? mmay_vector_at_const(&function->enum_arguments, start + index)
            : NULL;
        MarimayTypeId type = record ? record_argument->type : enum_argument->type;
        MarimayMirRegister source = record
            ? record_argument->register_id : enum_argument->register_id;
        if ((record && record_argument == NULL) || (!record && enum_argument == NULL)
            || !mmay_native_c_write(
                buffer,
                "    { MarimayManagedCell mmay_g_cell; memset(&mmay_g_cell, 0, sizeof(mmay_g_cell)); "
            ) || !mmay_native_c_write_to_managed_cell(buffer, type, source)
            || !mmay_native_c_write(
                buffer,
                count <= 64
                    ? "; mmay_g_inline_nominal_cells[%zu] = mmay_g_cell; }\n"
                    : "; mmay_g_nominal_cells[%zu] = mmay_g_cell; }\n",
                index
            )) return false;
    }
    return true;
}

static bool mmay_native_c_emit_record_new(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirRecordConstruction *construction = mmay_vector_at_const(
        &function->record_constructions, instruction->record_construction
    );
    const MarimayMirRecord *record = construction != NULL
        ? mmay_vector_at_const(&function->records, construction->record) : NULL;
    if (construction == NULL || record == NULL || !mmay_native_c_emit_cells(
            buffer, function, construction->argument_start,
            construction->argument_count, true
        ) || !mmay_native_c_write(
            buffer, "    mmay_g_m = mmay_managed_record_new(&mmay_g_x->heap, &"
        ) || !mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_RECORD, record->type
        ) || !mmay_native_c_write(
            buffer,
            construction->argument_count == 0
                ? ", NULL, %zu, &mmay_g_v_%u);\n"
                : construction->argument_count <= 64
                    ? ", mmay_g_inline_nominal_cells, %zu, &mmay_g_v_%u);\n"
                    : ", mmay_g_nominal_cells, %zu, &mmay_g_v_%u);\n",
            construction->argument_count, instruction->target
        ) || (construction->argument_count > 64 && !mmay_native_c_write(
            buffer,
            "    mmay_deallocate(&mmay_g_x->allocator, mmay_g_nominal_cells, mmay_g_nominal_cell_bytes); mmay_g_nominal_cells = NULL; mmay_g_nominal_cell_bytes = 0;\n"
        )) || !mmay_native_c_write(
            buffer,
            "    if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } }\n"
        )) return false;
    return true;
}

static bool mmay_native_c_emit_enum_new(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirEnumConstruction *construction = mmay_vector_at_const(
        &function->enum_constructions, instruction->enum_construction
    );
    const MarimayMirEnum *enumeration = construction != NULL
        ? mmay_vector_at_const(&function->enums, construction->enum_id) : NULL;
    bool immediate = mmay_native_c_enum_construction_immediate(
        function, instruction
    );
    uint32_t immediate_id = 0;
    if (construction == NULL || enumeration == NULL
        || !mmay_native_c_emit_cells(
            buffer, function, construction->argument_start,
            construction->argument_count, false
        )) return false;
    if (immediate) {
        return mmay_native_c_immediate_enum_id(
                functions, count, enumeration->type, &immediate_id
            ) && mmay_native_c_write(
            buffer,
            construction->argument_count == 0
                ? "    mmay_managed_enum_immediate_new_generated(&mmay_g_x->heap, UINT32_C(%u), %zu, UINT64_C(0), &mmay_g_v_%u); }\n"
                : "    mmay_managed_enum_immediate_new_generated(&mmay_g_x->heap, UINT32_C(%u), %zu, mmay_g_inline_nominal_cells[0].f64_bits, &mmay_g_v_%u); }\n",
            (unsigned int)immediate_id, construction->variant,
            instruction->target
        );
    }
    if (!mmay_native_c_write(
            buffer, "    mmay_g_m = mmay_managed_enum_new(&mmay_g_x->heap, &"
        ) || !mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_ENUM, enumeration->type
        ) || !mmay_native_c_write(
            buffer,
            construction->argument_count == 0
                ? ", %zu, NULL, %zu, &mmay_g_v_%u);\n"
                : construction->argument_count <= 64
                    ? ", %zu, mmay_g_inline_nominal_cells, %zu, &mmay_g_v_%u);\n"
                    : ", %zu, mmay_g_nominal_cells, %zu, &mmay_g_v_%u);\n",
            construction->variant, construction->argument_count,
            instruction->target
        ) || (construction->argument_count > 64 && !mmay_native_c_write(
            buffer,
            "    mmay_deallocate(&mmay_g_x->allocator, mmay_g_nominal_cells, mmay_g_nominal_cell_bytes); mmay_g_nominal_cells = NULL; mmay_g_nominal_cell_bytes = 0;\n"
        )) || !mmay_native_c_write(
            buffer,
            "    if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } }\n"
        )) return false;
    return true;
}

static bool mmay_native_c_emit_interface_new(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirInterfaceConstruction *construction = mmay_vector_at_const(
        &function->interface_constructions,
        instruction->interface_construction
    );
    const MarimayMirInterface *interface = construction != NULL
        ? mmay_vector_at_const(
            &function->interfaces, construction->interface_id
        ) : NULL;
    if (construction == NULL || interface == NULL
        || !mmay_native_c_write(
            buffer,
            "  mmay_g_m = mmay_managed_interface_new(&mmay_g_x->heap, &"
        ) || !mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_INTERFACE, interface->type
        ) || !mmay_native_c_write(
            buffer, ", mmay_g_v_%u, ", construction->payload
        ) || !mmay_native_c_write_interface_table_name(
            buffer, function->symbol, instruction->interface_construction
        ) || !mmay_native_c_write(
            buffer,
            ", &mmay_g_v_%u); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n",
            instruction->target
        )) return false;
    return true;
}

bool mmay_native_c_method_possible(
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayTypeId interface_type,
    uint32_t slot,
    size_t target
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
            const MarimayMirInterface *interface = construction != NULL
                ? mmay_vector_at_const(
                    &function->interfaces, construction->interface_id
                ) : NULL;
            const MarimaySymbolId *method = construction != NULL
                && slot < construction->method_count
                ? mmay_vector_at_const(
                    &function->interface_methods,
                    construction->method_start + slot
                ) : NULL;
            size_t method_index = 0;
            if (interface != NULL && interface->type == interface_type
                && method != NULL && mmay_native_c_function_index(
                    functions, count, *method, &method_index
                ) && method_index == target) return true;
        }
    }
    return false;
}

static bool mmay_native_c_emit_interface_call(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirCall *call = mmay_native_c_call(function, instruction->call);
    const MarimayMirCallArgument *receiver = call != NULL
        ? mmay_native_c_call_argument(function, call->argument_start) : NULL;
    MarimayNativeCManagedLayout layout;
    size_t target = 0;
    if (call == NULL || receiver == NULL || !mmay_native_c_find_managed_layout(
            functions, count, receiver->type, &layout
        ) || layout.kind != MMAY_NATIVE_C_MANAGED_INTERFACE
        || !mmay_native_c_write(
            buffer,
            "  { const MarimayManagedInterfaceDescriptor *mmay_g_actual = NULL; MarimayManagedReference mmay_g_payload = mmay_managed_invalid_reference_generated(); const void *mmay_g_table = NULL; uint32_t mmay_g_method = 0; mmay_g_m = mmay_managed_interface_describe(&mmay_g_x->heap, mmay_g_v_%u, &mmay_g_actual, &mmay_g_payload, &mmay_g_table); if (mmay_g_m == MMAY_MANAGED_OK && mmay_g_actual != &",
            receiver->register_id
        ) || !mmay_native_c_write_descriptor_name(
            buffer, layout.kind, receiver->type
        ) || !mmay_native_c_write(
            buffer,
            ") mmay_g_m = MMAY_MANAGED_TYPE_MISMATCH; if (mmay_g_m != MMAY_MANAGED_OK || mmay_g_table == NULL) { mmay_g_s = mmay_g_m == MMAY_MANAGED_OK ? 7 : mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } mmay_g_method = ((const uint32_t *)mmay_g_table)[%u]; switch (mmay_g_method) {\n",
            call->interface_slot
        )) return false;
    for (target = 0; target < count; target += 1) {
        size_t argument_index = 1;
        if (!mmay_native_c_method_possible(
                functions, count, receiver->type, call->interface_slot, target
            )) continue;
        if (!mmay_native_c_write(buffer, "    case %zu: mmay_g_s = ", target)
            || !mmay_native_c_symbol(buffer, functions[target]->symbol)
            || !mmay_native_c_write(buffer, "(mmay_g_x, mmay_g_p, mmay_g_payload")
            || !mmay_native_c_emit_record_null_hints(
                buffer, functions[target], functions, count, 0
            )) {
            return false;
        }
        for (argument_index = 1;
            argument_index < call->argument_count; argument_index += 1) {
            const MarimayMirCallArgument *argument = mmay_native_c_call_argument(
                function, call->argument_start + argument_index
            );
            if (argument == NULL || !mmay_native_c_write(
                    buffer, ", mmay_g_v_%u", argument->register_id
                )) return false;
            if (!mmay_native_c_emit_record_null_hints(
                    buffer, functions[target], functions, count,
                    argument_index
                )) return false;
        }
        if (!(call->result_type == MMAY_TYPE_VOID
                ? mmay_native_c_write(
                    buffer,
                    ", mmay_g_start, mmay_g_end); break;\n"
                )
                : mmay_native_c_write(
                    buffer,
                    ", &mmay_g_v_%u, mmay_g_start, mmay_g_end); break;\n",
                    instruction->target
                ))) return false;
    }
    if (!mmay_native_c_write(
            buffer, "    default: mmay_g_s = 7; break; } }\n"
        ) || !mmay_native_c_emit_propagate_call(
            buffer, function->symbol, instruction->span, false
        )) return false;
    if (mmay_native_c_register_array(function, instruction->target) != NULL
        && !mmay_native_c_write(
            buffer,
            "  mmay_g_av_%u = (MarimayManagedArrayGeneratedView){0};\n",
            instruction->target
        )) {
        return false;
    }
    return true;
}

bool mmay_native_c_emit_managed_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirInstruction *instruction
) {
    MarimayTypeId type = mmay_native_c_register_type(
        function, instruction->target
    );
    switch (instruction->kind) {
        case MMAY_MIR_RECORD_NEW:
            return mmay_native_c_emit_record_new(buffer, function, instruction);
        case MMAY_MIR_RECORD_GET:
            if (mmay_native_c_record_parameter_view(
                    function, functions, count, instruction->first
                )) {
                bool array_target = mmay_native_c_register_array(
                    function, instruction->target
                ) != NULL;
                const MarimayMirArray *array = array_target
                    ? mmay_native_c_register_array(
                        function, instruction->target
                    ) : NULL;
                const MarimayMirRecordField *field = array_target
                    ? mmay_native_c_record_parameter_field(
                        function, functions, count, instruction->first,
                        instruction->field
                    ) : NULL;
                if (array_target && (array == NULL || field == NULL)) {
                    return false;
                }
                if (array_target && field->is_mutable
                    && !mmay_native_c_write(
                            buffer,
                            "  mmay_g_m = mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_rv_%u.object->fields[%zu].reference, %s, &mmay_g_rv_%u_f_%zu); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n",
                            instruction->first, instruction->field,
                            mmay_native_c_array_descriptor(
                                function, array->type
                            ),
                            instruction->first, instruction->field
                        )) {
                    return false;
                }
                if (!mmay_native_c_write(
                        buffer,
                        "  { MarimayManagedCell mmay_g_cell = mmay_g_rv_%u.object->fields[%zu]; ",
                        instruction->first, instruction->field
                    ) || !mmay_native_c_write_from_managed_cell(
                        buffer, type, instruction->target
                    ) || (array_target && !mmay_native_c_write(
                            buffer,
                            "; mmay_g_av_%u = mmay_g_rv_%u_f_%zu",
                            instruction->target, instruction->first,
                            instruction->field
                    )) || !mmay_native_c_write(buffer, "; }\n")) {
                    return false;
                }
                return true;
            }
            {
                MarimayNativeCManagedLayout source_layout;
                if (mmay_native_c_record_view(
                        function, functions, count, instruction->first,
                        &source_layout
                    )) {
                    return mmay_native_c_emit_record_view_get(
                        buffer, function, functions, count, instruction,
                        type, &source_layout
                    );
                }
            }
            if (!mmay_native_c_write(
                buffer,
                "  { MarimayManagedCell mmay_g_cell; mmay_g_m = mmay_managed_record_get(&mmay_g_x->heap, mmay_g_v_%u, %zu, &mmay_g_cell); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } ",
                instruction->first, instruction->field
            ) || !mmay_native_c_write_from_managed_cell(
                buffer, type, instruction->target
            ) || (mmay_native_c_register_array(function, instruction->target)
                != NULL && !mmay_native_c_write(
                    buffer,
                    "; mmay_g_av_%u = (MarimayManagedArrayGeneratedView){0}",
                    instruction->target
                )) || !mmay_native_c_write(buffer, "; }\n")) return false;
            return true;
        case MMAY_MIR_ENUM_GET_PAYLOAD:
            if (!mmay_native_c_write(
                buffer,
                "  { MarimayManagedCell mmay_g_cell; size_t mmay_g_immediate_variant = 0; uint64_t mmay_g_immediate_payload = 0; if (mmay_managed_enum_immediate_decode_fast(&mmay_g_x->heap, mmay_g_v_%u, &mmay_g_immediate_variant, &mmay_g_immediate_payload)) { if (mmay_g_immediate_variant != %zu || %zu != 0) { mmay_g_s = 7; goto mmay_g_cleanup; } mmay_g_cell.f64_bits = mmay_g_immediate_payload; } else { mmay_g_m = mmay_managed_enum_get_payload(&mmay_g_x->heap, mmay_g_v_%u, %zu, %zu, &mmay_g_cell); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } } ",
                instruction->first, instruction->variant, instruction->field,
                instruction->first, instruction->variant, instruction->field
            ) || !mmay_native_c_write_from_managed_cell(
                buffer, type, instruction->target
            ) || (mmay_native_c_register_array(function, instruction->target)
                != NULL && !mmay_native_c_write(
                    buffer,
                    "; mmay_g_av_%u = (MarimayManagedArrayGeneratedView){0}",
                    instruction->target
                )) || !mmay_native_c_write(buffer, "; }\n")) return false;
            return true;
        case MMAY_MIR_RECORD_SET:
            if (mmay_native_c_record_parameter_view(
                    function, functions, count, instruction->first
                ) && mmay_native_c_register_type(
                    function, instruction->second
                ) < MMAY_TYPE_FIRST_DYNAMIC
                && mmay_native_c_register_type(
                    function, instruction->second
                ) != MMAY_TYPE_STRING) {
                return mmay_native_c_write(
                    buffer,
                    "  { MarimayManagedCell mmay_g_cell; memset(&mmay_g_cell, 0, sizeof(mmay_g_cell)); "
                ) && mmay_native_c_write_to_managed_cell(
                    buffer,
                    mmay_native_c_register_type(function, instruction->second),
                    instruction->second
                ) && mmay_native_c_write(
                    buffer,
                    "; mmay_g_rv_%u.object->fields[%zu] = mmay_g_cell; mmay_g_v_%u = mmay_g_v_%u; }\n",
                    instruction->first, instruction->field,
                    instruction->target, instruction->second
                );
            }
            return mmay_native_c_write(
                buffer,
                "  { MarimayManagedCell mmay_g_cell; memset(&mmay_g_cell, 0, sizeof(mmay_g_cell)); "
            ) && mmay_native_c_write_to_managed_cell(
                buffer, mmay_native_c_register_type(function, instruction->second),
                instruction->second
            ) && mmay_native_c_write(
                buffer,
                "; mmay_g_m = mmay_managed_record_set(&mmay_g_x->heap, mmay_g_v_%u, %zu, mmay_g_cell); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } mmay_g_v_%u = mmay_g_v_%u; }\n",
                instruction->first, instruction->field, instruction->target,
                instruction->second
            );
        case MMAY_MIR_RECORD_IDENTITY_SAME:
            return mmay_native_c_write(
                buffer,
                "  mmay_g_v_%u = mmay_managed_reference_equal(mmay_g_v_%u, mmay_g_v_%u);\n",
                instruction->target, instruction->first, instruction->second
            );
        case MMAY_MIR_ENUM_NEW:
            return mmay_native_c_emit_enum_new(
                buffer, function, functions, count, instruction
            );
        case MMAY_MIR_ENUM_IS_VARIANT:
        case MMAY_MIR_ENUM_REQUIRE_VARIANT:
            return mmay_native_c_write(
                buffer,
                instruction->kind == MMAY_MIR_ENUM_IS_VARIANT
                    ? "  { bool mmay_g_match = false; size_t mmay_g_immediate_variant = 0; if (mmay_managed_enum_immediate_decode_fast(&mmay_g_x->heap, mmay_g_v_%u, &mmay_g_immediate_variant, NULL)) mmay_g_match = mmay_g_immediate_variant == %zu; else { mmay_g_m = mmay_managed_enum_is_variant(&mmay_g_x->heap, mmay_g_v_%u, %zu, &mmay_g_match); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } } mmay_g_v_%u = mmay_g_match; }\n"
                    : "  { bool mmay_g_match = false; size_t mmay_g_immediate_variant = 0; if (mmay_managed_enum_immediate_decode_fast(&mmay_g_x->heap, mmay_g_v_%u, &mmay_g_immediate_variant, NULL)) mmay_g_match = mmay_g_immediate_variant == %zu; else { mmay_g_m = mmay_managed_enum_is_variant(&mmay_g_x->heap, mmay_g_v_%u, %zu, &mmay_g_match); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } } if (!mmay_g_match) { mmay_g_s = 7; goto mmay_g_cleanup; } mmay_g_v_%u = mmay_g_v_%u; }\n",
                instruction->first, instruction->variant,
                instruction->first, instruction->variant,
                instruction->target, instruction->first
            );
        case MMAY_MIR_INTERFACE_NEW:
            return mmay_native_c_emit_interface_new(buffer, function, instruction);
        case MMAY_MIR_CALL_INTERFACE:
            return mmay_native_c_emit_interface_call(
                buffer, function, functions, count, instruction
            );
        default:
            return false;
    }
}
