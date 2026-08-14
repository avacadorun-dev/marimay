#include "c_emitter_internal.h"

#include <string.h>

static RudaTypeId ruda_native_c_register_type(
    const RudaMirFunction *function,
    RudaMirRegister register_id
) {
    const RudaTypeId *type = function != NULL
        ? ruda_vector_at_const(&function->register_types, register_id) : NULL;
    return type != NULL ? *type : RUDA_TYPE_INVALID;
}

static bool ruda_native_c_write_to_cell(
    RudaNativeCBuffer *buffer,
    RudaTypeId type,
    RudaMirRegister register_id
) {
    if (type == RUDA_TYPE_STRING) {
        return ruda_native_c_write(
            buffer,
            "if (ruda_managed_reference_is_invalid(ruda_g_v_%u.managed)) { ruda_g_m = ruda_managed_string_new(&ruda_g_x->heap, ruda_g_v_%u.data, ruda_g_v_%u.length, &ruda_g_v_%u.managed); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } } ruda_g_cell.reference = ruda_g_v_%u.managed",
            register_id, register_id, register_id, register_id, register_id
        );
    }
    if (type == RUDA_TYPE_I64) {
        return ruda_native_c_write(
            buffer,
            "ruda_g_cell.f64_bits = (uint64_t)ruda_g_v_%u",
            register_id
        );
    }
    if (type == RUDA_TYPE_BOOL) {
        return ruda_native_c_write(
            buffer,
            "ruda_g_cell.f64_bits = ruda_g_v_%u ? UINT64_C(1) : UINT64_C(0)",
            register_id
        );
    }
    if (type == RUDA_TYPE_F64) {
        return ruda_native_c_write(
            buffer, "ruda_g_cell.f64_bits = ruda_g_v_%u", register_id
        );
    }
    return type >= RUDA_TYPE_FIRST_DYNAMIC && ruda_native_c_write(
        buffer, "ruda_g_cell.reference = ruda_g_v_%u", register_id
    );
}

static bool ruda_native_c_write_from_cell(
    RudaNativeCBuffer *buffer,
    RudaTypeId type,
    RudaMirRegister target
) {
    if (type == RUDA_TYPE_STRING) {
        return ruda_native_c_write(
            buffer,
            "ruda_g_v_%u.managed = ruda_g_cell.reference; ruda_g_m = ruda_managed_string_bytes(&ruda_g_x->heap, ruda_g_v_%u.managed, &ruda_g_v_%u.data, &ruda_g_v_%u.length); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; }",
            target, target, target, target
        );
    }
    if (type == RUDA_TYPE_I64) {
        return ruda_native_c_write(
            buffer,
            "ruda_g_v_%u = ruda_i64_from_twos_complement(ruda_g_cell.f64_bits)",
            target
        );
    }
    if (type == RUDA_TYPE_BOOL) {
        return ruda_native_c_write(
            buffer, "ruda_g_v_%u = ruda_g_cell.f64_bits != 0", target
        );
    }
    if (type == RUDA_TYPE_F64) {
        return ruda_native_c_write(
            buffer, "ruda_g_v_%u = ruda_g_cell.f64_bits", target
        );
    }
    return type >= RUDA_TYPE_FIRST_DYNAMIC && ruda_native_c_write(
        buffer, "ruda_g_v_%u = ruda_g_cell.reference", target
    );
}

static bool ruda_native_c_emit_cells(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    size_t start,
    size_t count,
    bool record
) {
    size_t index = 0;
    if (!ruda_native_c_write(
            buffer,
            "  { ruda_g_nominal_cells = NULL; ruda_g_nominal_cell_bytes = 0;\n"
            "    if (!ruda_size_multiply(%zu, sizeof(*ruda_g_nominal_cells), &ruda_g_nominal_cell_bytes)) { ruda_g_s = 6; goto ruda_g_cleanup; }\n"
            "    if (ruda_g_nominal_cell_bytes != 0) { ruda_g_nominal_cells = ruda_allocate(&ruda_g_x->allocator, ruda_g_nominal_cell_bytes); if (ruda_g_nominal_cells == NULL) { ruda_g_s = 5; goto ruda_g_cleanup; } }\n",
            count
        )) return false;
    for (index = 0; index < count; index += 1) {
        const RudaMirRecordArgument *record_argument = record
            ? ruda_vector_at_const(&function->record_arguments, start + index)
            : NULL;
        const RudaMirEnumArgument *enum_argument = !record
            ? ruda_vector_at_const(&function->enum_arguments, start + index)
            : NULL;
        RudaTypeId type = record ? record_argument->type : enum_argument->type;
        RudaMirRegister source = record
            ? record_argument->register_id : enum_argument->register_id;
        if ((record && record_argument == NULL) || (!record && enum_argument == NULL)
            || !ruda_native_c_write(
                buffer,
                "    { RudaManagedCell ruda_g_cell; memset(&ruda_g_cell, 0, sizeof(ruda_g_cell)); "
            ) || !ruda_native_c_write_to_cell(buffer, type, source)
            || !ruda_native_c_write(
                buffer,
                "; ruda_g_nominal_cells[%zu] = ruda_g_cell; }\n", index
            )) return false;
    }
    return true;
}

static bool ruda_native_c_emit_record_new(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const RudaMirRecordConstruction *construction = ruda_vector_at_const(
        &function->record_constructions, instruction->record_construction
    );
    const RudaMirRecord *record = construction != NULL
        ? ruda_vector_at_const(&function->records, construction->record) : NULL;
    if (construction == NULL || record == NULL || !ruda_native_c_emit_cells(
            buffer, function, construction->argument_start,
            construction->argument_count, true
        ) || !ruda_native_c_write(
            buffer, "    ruda_g_m = ruda_managed_record_new(&ruda_g_x->heap, &"
        ) || !ruda_native_c_write_descriptor_name(
            buffer, RUDA_NATIVE_C_MANAGED_RECORD, record->type
        ) || !ruda_native_c_write(
            buffer,
            ", ruda_g_nominal_cells, %zu, &ruda_g_v_%u);\n"
            "    ruda_deallocate(&ruda_g_x->allocator, ruda_g_nominal_cells, ruda_g_nominal_cell_bytes); ruda_g_nominal_cells = NULL; ruda_g_nominal_cell_bytes = 0;\n"
            "    if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } }\n",
            construction->argument_count, instruction->target
        )) return false;
    return true;
}

static bool ruda_native_c_emit_enum_new(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const RudaMirEnumConstruction *construction = ruda_vector_at_const(
        &function->enum_constructions, instruction->enum_construction
    );
    const RudaMirEnum *enumeration = construction != NULL
        ? ruda_vector_at_const(&function->enums, construction->enum_id) : NULL;
    if (construction == NULL || enumeration == NULL
        || !ruda_native_c_emit_cells(
            buffer, function, construction->argument_start,
            construction->argument_count, false
        ) || !ruda_native_c_write(
            buffer, "    ruda_g_m = ruda_managed_enum_new(&ruda_g_x->heap, &"
        ) || !ruda_native_c_write_descriptor_name(
            buffer, RUDA_NATIVE_C_MANAGED_ENUM, enumeration->type
        ) || !ruda_native_c_write(
            buffer,
            ", %zu, ruda_g_nominal_cells, %zu, &ruda_g_v_%u);\n"
            "    ruda_deallocate(&ruda_g_x->allocator, ruda_g_nominal_cells, ruda_g_nominal_cell_bytes); ruda_g_nominal_cells = NULL; ruda_g_nominal_cell_bytes = 0;\n"
            "    if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } }\n",
            construction->variant, construction->argument_count,
            instruction->target
        )) return false;
    return true;
}

static bool ruda_native_c_emit_interface_new(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const RudaMirInterfaceConstruction *construction = ruda_vector_at_const(
        &function->interface_constructions,
        instruction->interface_construction
    );
    const RudaMirInterface *interface = construction != NULL
        ? ruda_vector_at_const(
            &function->interfaces, construction->interface_id
        ) : NULL;
    if (construction == NULL || interface == NULL
        || !ruda_native_c_write(
            buffer,
            "  ruda_g_m = ruda_managed_interface_new(&ruda_g_x->heap, &"
        ) || !ruda_native_c_write_descriptor_name(
            buffer, RUDA_NATIVE_C_MANAGED_INTERFACE, interface->type
        ) || !ruda_native_c_write(
            buffer, ", ruda_g_v_%u, ", construction->payload
        ) || !ruda_native_c_write_interface_table_name(
            buffer, function->symbol, instruction->interface_construction
        ) || !ruda_native_c_write(
            buffer,
            ", &ruda_g_v_%u); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; }\n",
            instruction->target
        )) return false;
    return true;
}

static bool ruda_native_c_method_possible(
    const RudaMirFunction *const *functions,
    size_t count,
    RudaTypeId interface_type,
    uint32_t slot,
    size_t target
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
            const RudaMirInterface *interface = construction != NULL
                ? ruda_vector_at_const(
                    &function->interfaces, construction->interface_id
                ) : NULL;
            const RudaSymbolId *method = construction != NULL
                && slot < construction->method_count
                ? ruda_vector_at_const(
                    &function->interface_methods,
                    construction->method_start + slot
                ) : NULL;
            size_t method_index = 0;
            if (interface != NULL && interface->type == interface_type
                && method != NULL && ruda_native_c_function_index(
                    functions, count, *method, &method_index
                ) && method_index == target) return true;
        }
    }
    return false;
}

static bool ruda_native_c_emit_interface_call(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count,
    const RudaMirInstruction *instruction
) {
    const RudaMirCall *call = ruda_native_c_call(function, instruction->call);
    const RudaMirCallArgument *receiver = call != NULL
        ? ruda_native_c_call_argument(function, call->argument_start) : NULL;
    RudaNativeCManagedLayout layout;
    size_t target = 0;
    if (call == NULL || receiver == NULL || !ruda_native_c_find_managed_layout(
            functions, count, receiver->type, &layout
        ) || layout.kind != RUDA_NATIVE_C_MANAGED_INTERFACE
        || !ruda_native_c_write(
            buffer,
            "  { const RudaManagedInterfaceDescriptor *ruda_g_actual = NULL; RudaManagedReference ruda_g_payload = ruda_managed_invalid_reference(); const void *ruda_g_table = NULL; uint32_t ruda_g_method = 0; ruda_g_m = ruda_managed_interface_describe(&ruda_g_x->heap, ruda_g_v_%u, &ruda_g_actual, &ruda_g_payload, &ruda_g_table); if (ruda_g_m == RUDA_MANAGED_OK && ruda_g_actual != &",
            receiver->register_id
        ) || !ruda_native_c_write_descriptor_name(
            buffer, layout.kind, receiver->type
        ) || !ruda_native_c_write(
            buffer,
            ") ruda_g_m = RUDA_MANAGED_TYPE_MISMATCH; if (ruda_g_m != RUDA_MANAGED_OK || ruda_g_table == NULL) { ruda_g_s = ruda_g_m == RUDA_MANAGED_OK ? 7 : ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ruda_g_method = ((const uint32_t *)ruda_g_table)[%u]; switch (ruda_g_method) {\n",
            call->interface_slot
        )) return false;
    for (target = 0; target < count; target += 1) {
        size_t argument_index = 1;
        if (!ruda_native_c_method_possible(
                functions, count, receiver->type, call->interface_slot, target
            )) continue;
        if (!ruda_native_c_write(buffer, "    case %zu: ruda_g_s = ", target)
            || !ruda_native_c_symbol(buffer, functions[target]->symbol)
            || !ruda_native_c_write(buffer, "(ruda_g_x, ruda_g_p, ruda_g_payload")) {
            return false;
        }
        for (argument_index = 1;
            argument_index < call->argument_count; argument_index += 1) {
            const RudaMirCallArgument *argument = ruda_native_c_call_argument(
                function, call->argument_start + argument_index
            );
            if (argument == NULL || !ruda_native_c_write(
                    buffer, ", ruda_g_v_%u", argument->register_id
                )) return false;
        }
        if (!ruda_native_c_write(
            buffer, ", &ruda_g_v_%u, ruda_g_start, ruda_g_end); break;\n",
            instruction->target)) return false;
    }
    if (!ruda_native_c_write(
            buffer, "    default: ruda_g_s = 7; break; } }\n"
        ) || !ruda_native_c_emit_propagate_call(
            buffer, function->symbol, instruction->span, false
        )) return false;
    return true;
}

bool ruda_native_c_emit_managed_instruction(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count,
    const RudaMirInstruction *instruction
) {
    RudaTypeId type = ruda_native_c_register_type(
        function, instruction->target
    );
    switch (instruction->kind) {
        case RUDA_MIR_RECORD_NEW:
            return ruda_native_c_emit_record_new(buffer, function, instruction);
        case RUDA_MIR_RECORD_GET:
            if (!ruda_native_c_write(
                buffer,
                "  { RudaManagedCell ruda_g_cell; ruda_g_m = ruda_managed_record_get(&ruda_g_x->heap, ruda_g_v_%u, %zu, &ruda_g_cell); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ",
                instruction->first, instruction->field
            ) || !ruda_native_c_write_from_cell(
                buffer, type, instruction->target
            ) || !ruda_native_c_write(buffer, "; }\n")) return false;
            return true;
        case RUDA_MIR_ENUM_GET_PAYLOAD:
            if (!ruda_native_c_write(
                buffer,
                "  { RudaManagedCell ruda_g_cell; ruda_g_m = ruda_managed_enum_get_payload(&ruda_g_x->heap, ruda_g_v_%u, %zu, %zu, &ruda_g_cell); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ",
                instruction->first, instruction->variant, instruction->field
            ) || !ruda_native_c_write_from_cell(
                buffer, type, instruction->target
            ) || !ruda_native_c_write(buffer, "; }\n")) return false;
            return true;
        case RUDA_MIR_RECORD_SET:
            return ruda_native_c_write(
                buffer,
                "  { RudaManagedCell ruda_g_cell; memset(&ruda_g_cell, 0, sizeof(ruda_g_cell)); "
            ) && ruda_native_c_write_to_cell(
                buffer, ruda_native_c_register_type(function, instruction->second),
                instruction->second
            ) && ruda_native_c_write(
                buffer,
                "; ruda_g_m = ruda_managed_record_set(&ruda_g_x->heap, ruda_g_v_%u, %zu, ruda_g_cell); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ruda_g_v_%u = ruda_g_v_%u; }\n",
                instruction->first, instruction->field, instruction->target,
                instruction->second
            );
        case RUDA_MIR_RECORD_IDENTITY_SAME:
            return ruda_native_c_write(
                buffer,
                "  ruda_g_v_%u = ruda_managed_reference_equal(ruda_g_v_%u, ruda_g_v_%u);\n",
                instruction->target, instruction->first, instruction->second
            );
        case RUDA_MIR_ENUM_NEW:
            return ruda_native_c_emit_enum_new(buffer, function, instruction);
        case RUDA_MIR_ENUM_IS_VARIANT:
        case RUDA_MIR_ENUM_REQUIRE_VARIANT:
            return ruda_native_c_write(
                buffer,
                instruction->kind == RUDA_MIR_ENUM_IS_VARIANT
                    ? "  { bool ruda_g_match = false; ruda_g_m = ruda_managed_enum_is_variant(&ruda_g_x->heap, ruda_g_v_%u, %zu, &ruda_g_match); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ruda_g_v_%u = ruda_g_match; }\n"
                    : "  { bool ruda_g_match = false; ruda_g_m = ruda_managed_enum_is_variant(&ruda_g_x->heap, ruda_g_v_%u, %zu, &ruda_g_match); if (ruda_g_m != RUDA_MANAGED_OK || !ruda_g_match) { ruda_g_s = ruda_g_m == RUDA_MANAGED_OK ? 7 : ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ruda_g_v_%u = ruda_g_v_%u; }\n",
                instruction->first, instruction->variant, instruction->target,
                instruction->first
            );
        case RUDA_MIR_INTERFACE_NEW:
            return ruda_native_c_emit_interface_new(buffer, function, instruction);
        case RUDA_MIR_CALL_INTERFACE:
            return ruda_native_c_emit_interface_call(
                buffer, function, functions, count, instruction
            );
        default:
            return false;
    }
}
