#include "c_emitter_internal.h"

#include <limits.h>

static bool mmay_native_c_collection_canonical(
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirFunction *function,
    size_t index,
    MarimayTypeId type
) {
    MarimayNativeCManagedLayout layout;
    return mmay_native_c_find_managed_layout(
            functions, count, type, &layout
        ) && layout.function == function
        && layout.kind == MMAY_NATIVE_C_MANAGED_COLLECTION
        && layout.index == index;
}

static bool mmay_native_c_collection_field(
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
            "{MMAY_MANAGED_CELL_REFERENCE,%s,MMAY_MANAGED_OBJECT_STRING,NULL}",
            is_mutable ? "true" : "false"
        );
    }
    if (type < MMAY_TYPE_FIRST_DYNAMIC) {
        return mmay_native_c_write(
            buffer,
            "{MMAY_MANAGED_CELL_RAW64,%s,MMAY_MANAGED_OBJECT_INVALID,NULL}",
            is_mutable ? "true" : "false"
        );
    }
    if (!mmay_native_c_find_managed_layout(functions, count, type, &layout)) {
        return false;
    }
    if (layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY) {
        return mmay_native_c_write(
            buffer,
            "{MMAY_MANAGED_CELL_REFERENCE,%s,MMAY_MANAGED_OBJECT_ARRAY,%s}",
            is_mutable ? "true" : "false",
            mmay_native_c_array_descriptor(layout.function, type)
        );
    }
    if (!mmay_native_c_write(
            buffer,
            "{MMAY_MANAGED_CELL_REFERENCE,%s,%s,&",
            is_mutable ? "true" : "false",
            layout.kind == MMAY_NATIVE_C_MANAGED_COLLECTION
                ? "MMAY_MANAGED_OBJECT_MAP"
                : (layout.kind == MMAY_NATIVE_C_MANAGED_RECORD
                    ? "MMAY_MANAGED_OBJECT_RECORD"
                    : (layout.kind == MMAY_NATIVE_C_MANAGED_ENUM
                        ? "MMAY_MANAGED_OBJECT_ENUM"
                        : "MMAY_MANAGED_OBJECT_INTERFACE"))
        ) || !mmay_native_c_write_descriptor_name(
            buffer, layout.kind, type
        )) return false;
    return mmay_native_c_write(buffer, "}");
}

bool mmay_native_c_emit_collection_descriptor_declarations(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count
) {
    size_t function_index = 0;
    for (function_index = 0; function_index < count; function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        size_t index = 0;
        for (index = 0; function != NULL
                && index < function->collections.length; index += 1) {
            const MarimayMirCollection *collection = mmay_vector_at_const(
                &function->collections, index
            );
            if (collection != NULL && mmay_native_c_collection_canonical(
                    functions, count, function, index, collection->type
                ) && (!mmay_native_c_write(
                    buffer, "extern const MarimayManagedMapDescriptor "
                ) || !mmay_native_c_write_descriptor_name(
                    buffer, MMAY_NATIVE_C_MANAGED_COLLECTION, collection->type
                ) || !mmay_native_c_write(buffer, ";\n"))) return false;
        }
    }
    return true;
}

bool mmay_native_c_emit_collection_descriptors(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *const *functions,
    size_t count
) {
    size_t function_index = 0;
    for (function_index = 0; function_index < count; function_index += 1) {
        const MarimayMirFunction *function = functions[function_index];
        size_t index = 0;
        for (index = 0; function != NULL
                && index < function->collections.length; index += 1) {
            const MarimayMirCollection *collection = mmay_vector_at_const(
                &function->collections, index
            );
            if (collection == NULL || !mmay_native_c_collection_canonical(
                    functions, count, function, index, collection->type
                )) continue;
            if (!mmay_native_c_write(
                    buffer, "const MarimayManagedMapDescriptor "
                ) || !mmay_native_c_write_descriptor_name(
                    buffer, MMAY_NATIVE_C_MANAGED_COLLECTION, collection->type
                ) || !mmay_native_c_write(buffer, " = {")
                || !mmay_native_c_collection_field(
                    buffer, functions, count, collection->key_type, false
                ) || !mmay_native_c_write(buffer, ",")
                || !(collection->is_set
                    ? mmay_native_c_write(
                        buffer,
                        "{MMAY_MANAGED_CELL_INVALID,false,MMAY_MANAGED_OBJECT_INVALID,NULL}"
                    ) : mmay_native_c_collection_field(
                        buffer, functions, count, collection->value_type, false
                    )) || !mmay_native_c_write(
                    buffer, ",%s};\n", collection->is_set ? "true" : "false"
                )) return false;
        }
    }
    return true;
}

bool mmay_native_c_emit_collection_runtime(MarimayNativeCBuffer *buffer) {
    return mmay_native_c_write(
        buffer,
        "static int mmay_g_begin_collection_iterator_mutation(mmay_g_panic *p, bool is_set, const unsigned char *symbol, size_t start, size_t end) { static const unsigned char map_message[] = \"map iterator invalidated by mutation\", set_message[] = \"set iterator invalidated by mutation\"; const unsigned char *message = is_set ? set_message : map_message; size_t length = is_set ? sizeof(set_message) - 1U : sizeof(map_message) - 1U; if (p == NULL) return 4; memcpy(p->message_storage, message, length); p->code = \"G4005\"; p->message.data = p->message_storage; p->message.length = length; p->symbol = symbol; p->start = start; p->end = end; p->frame_count = 0; p->suppressed_count = 0; return mmay_g_append_frame(p, symbol, start, end) ? 4 : 5; }\n"
    ) && mmay_native_c_write(
        buffer,
        "static int mmay_g_collection_option(mmay_g_execution *x, MarimayManagedReference map, MarimayManagedCell key, bool remove, bool reference_value, const MarimayManagedEnumDescriptor *option, uint32_t immediate_id, MarimayManagedReference *out) {\n"
        "  MarimayManagedCell value = {0}; MarimayManagedRootFrame roots = {0};\n"
        "  MarimayManagedReference *locations[1] = {&value.reference};\n"
        "  MarimayManagedStatus status, pop_status; bool found = false;\n"
        "  value.reference = mmay_managed_invalid_reference();\n"
        "  status = remove ? mmay_managed_map_remove(&x->heap, map, key, &value, &found) : mmay_managed_map_get(&x->heap, map, key, &value, &found);\n"
        "  if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status);\n"
        "  if (!found || !reference_value) {\n"
        "    mmay_managed_enum_immediate_new_generated(&x->heap, immediate_id, found ? 0U : 1U, found ? value.f64_bits : UINT64_C(0), out);\n"
        "    return 0;\n"
        "  }\n"
        "  status = mmay_managed_root_push(&x->heap, &roots, locations, 1);\n"
        "  if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status);\n"
        "  status = mmay_managed_enum_new(&x->heap, option, 0, &value, 1, out);\n"
        "  pop_status = mmay_managed_root_pop(&x->heap, &roots);\n"
        "  return mmay_g_managed_status(status == MMAY_MANAGED_OK ? pop_status : status);\n"
        "}\n"
    ) && mmay_native_c_write(
        buffer,
        "static int mmay_g_collection_update_result(mmay_g_execution *x, MarimayManagedReference collection, MarimayManagedCell key, MarimayManagedCell value, bool is_set, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error, uint32_t immediate_id, MarimayManagedReference *out) {\n"
        "  MarimayManagedCell payload = {0}, error_cell = {0};\n"
        "  MarimayManagedReference *locations[1] = {&error_cell.reference};\n"
        "  MarimayManagedRootFrame roots = {0}; MarimayManagedStatus status, pop_status;\n"
        "  bool inserted = false; error_cell.reference = mmay_managed_invalid_reference();\n"
        "  status = is_set ? mmay_managed_set_add(&x->heap, collection, key, &inserted) : mmay_managed_map_put(&x->heap, collection, key, value, &inserted);\n"
        "  if (status == MMAY_MANAGED_OK) {\n"
        "    mmay_managed_enum_immediate_new_generated(&x->heap, immediate_id, 0, inserted ? UINT64_C(1) : UINT64_C(0), out);\n"
        "    return 0;\n"
        "  }\n"
        "  if (status != MMAY_MANAGED_LENGTH_LIMIT) return mmay_g_managed_status(status);\n"
        "  status = mmay_managed_root_push(&x->heap, &roots, locations, 1);\n"
        "  payload.f64_bits = (uint64_t)INT64_MAX;\n"
        "  if (status == MMAY_MANAGED_OK) status = mmay_managed_enum_new(&x->heap, error, 0, &payload, 1, &error_cell.reference);\n"
        "  if (status == MMAY_MANAGED_OK) status = mmay_managed_enum_new(&x->heap, result, 1, &error_cell, 1, out);\n"
        "  if (roots.linked) { pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; }\n"
        "  return mmay_g_managed_status(status);\n"
        "}\n"
    );
}

static const MarimayMirCollection *mmay_native_c_instruction_collection(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    return function != NULL && instruction != NULL
        ? mmay_vector_at_const(
            &function->collections, instruction->collection
        ) : NULL;
}

static bool mmay_native_c_collection_key(
    MarimayNativeCBuffer *buffer,
    MarimayTypeId type,
    MarimayMirRegister source
) {
    return mmay_native_c_write(buffer, "memset(&mmay_g_cell,0,sizeof(mmay_g_cell)); ")
        && mmay_native_c_write_to_managed_cell(buffer, type, source)
        && mmay_native_c_write(buffer, "; mmay_g_key = mmay_g_cell; ");
}

static bool mmay_native_c_collection_value(
    MarimayNativeCBuffer *buffer,
    MarimayTypeId type,
    MarimayMirRegister source
) {
    return mmay_native_c_write(buffer, "memset(&mmay_g_cell,0,sizeof(mmay_g_cell)); ")
        && mmay_native_c_write_to_managed_cell(buffer, type, source)
        && mmay_native_c_write(buffer, "; mmay_g_value = mmay_g_cell; ");
}

static bool mmay_native_c_collection_status(
    MarimayNativeCBuffer *buffer
) {
    return mmay_native_c_write(
        buffer,
        "if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }"
    );
}

static bool mmay_native_c_collection_result_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirInstruction *instruction,
    const MarimayMirCollection *collection
) {
    const MarimayMirEnum *result = mmay_vector_at_const(
        &function->enums, instruction->enum_id
    );
    const bool option = instruction->kind == MMAY_MIR_COLLECTION_GET_OPTION
        || instruction->kind == MMAY_MIR_COLLECTION_REMOVE_OPTION;
    const MarimayMirEnum *error = option ? NULL : mmay_vector_at_const(
        &function->enums, instruction->option_enum_id
    );
    uint32_t immediate_id = 0;
    /* Verified Option<V> has an empty None variant; the collection Result
     * has a raw bool Ok variant. Both descriptors are registered at entry. */
    if (result == NULL || (!option && error == NULL)
        || !mmay_native_c_immediate_enum_id(
            functions, count, result->type, &immediate_id
        )) return false;
    if (!mmay_native_c_write(
            buffer,
            "  { MarimayManagedCell mmay_g_cell = {0}, mmay_g_key = {0}, mmay_g_value = {0}; (void)mmay_g_value; "
        ) || !mmay_native_c_collection_key(
            buffer, collection->key_type, instruction->second
        )) return false;
    if (!option && !collection->is_set
        && !mmay_native_c_collection_value(
            buffer, collection->value_type, instruction->third
        )) return false;
    if (option) {
        if (!mmay_native_c_write(
                buffer,
                "mmay_g_s = mmay_g_collection_option(mmay_g_x, mmay_g_v_%u, mmay_g_key, %s, %s, &",
                instruction->first,
                instruction->kind == MMAY_MIR_COLLECTION_REMOVE_OPTION
                    ? "true" : "false",
                collection->value_type == MMAY_TYPE_STRING
                    || collection->value_type >= MMAY_TYPE_FIRST_DYNAMIC
                    ? "true" : "false"
            )) return false;
    } else if (!mmay_native_c_write(
            buffer,
            "mmay_g_s = mmay_g_collection_update_result(mmay_g_x, mmay_g_v_%u, mmay_g_key, mmay_g_value, %s, &",
            instruction->first, collection->is_set ? "true" : "false"
        )) return false;
    if (!mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_ENUM, result->type
        )) return false;
    if (!option && (!mmay_native_c_write(buffer, ", &")
        || !mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_ENUM, error->type
        ))) return false;
    return mmay_native_c_write(
        buffer,
        ", UINT32_C(%u), &mmay_g_v_%u); if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; } }\n",
        (unsigned int)immediate_id, instruction->target,
        instruction->span.start, instruction->span.end
    );
}

bool mmay_native_c_emit_collection_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirCollection *collection =
        mmay_native_c_instruction_collection(function, instruction);
    if (buffer == NULL || function == NULL || instruction == NULL
        || collection == NULL) return false;
    if (instruction->kind >= MMAY_MIR_COLLECTION_GET_OPTION
        && instruction->kind <= MMAY_MIR_COLLECTION_ADD_RESULT) {
        return mmay_native_c_collection_result_instruction(
            buffer, function, functions, count, instruction, collection
        );
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_NEW) {
        return mmay_native_c_write(buffer, "  mmay_g_m = %s(&mmay_g_x->heap, &",
                collection->is_set ? "mmay_managed_set_new" : "mmay_managed_map_new")
            && mmay_native_c_write_descriptor_name(
                buffer, MMAY_NATIVE_C_MANAGED_COLLECTION, collection->type
            ) && mmay_native_c_write(
                buffer, ", &mmay_g_v_%u); ", instruction->target
            ) && mmay_native_c_collection_status(buffer)
            && mmay_native_c_write(buffer, "\n");
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_LENGTH) {
        return mmay_native_c_write(
            buffer,
            "  { size_t mmay_g_length = 0; mmay_g_m = %s(&mmay_g_x->heap, mmay_g_v_%u, &mmay_g_length); ",
            collection->is_set ? "mmay_managed_set_length" : "mmay_managed_map_length",
            instruction->first
        ) && mmay_native_c_collection_status(buffer)
            && mmay_native_c_write(
                buffer,
                " if (mmay_g_length > (size_t)INT64_MAX) { mmay_g_s = 7; goto mmay_g_cleanup; } mmay_g_v_%u = (int64_t)mmay_g_length; }\n",
                instruction->target
            );
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_CLEAR) {
        return mmay_native_c_write(
            buffer, "  mmay_g_m = %s(&mmay_g_x->heap, mmay_g_v_%u); ",
            collection->is_set ? "mmay_managed_set_clear" : "mmay_managed_map_clear",
            instruction->first
        ) && mmay_native_c_collection_status(buffer)
            && mmay_native_c_write(
                buffer, " mmay_g_v_%u = mmay_g_v_%u;\n",
                instruction->target, instruction->first
            );
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_GENERATION) {
        return mmay_native_c_write(
            buffer, "  mmay_g_m = %s(&mmay_g_x->heap, mmay_g_v_%u, &mmay_g_v_%u); ",
            collection->is_set ? "mmay_managed_set_mutation_generation"
                : "mmay_managed_map_mutation_generation",
            instruction->first, instruction->target
        ) && mmay_native_c_collection_status(buffer)
            && mmay_native_c_write(buffer, "\n");
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_REQUIRE_GENERATION) {
        if (!mmay_native_c_write(
                buffer, "  mmay_g_m = %s(&mmay_g_x->heap, mmay_g_v_%u, mmay_g_v_%u); if (mmay_g_m == MMAY_MANAGED_ITERATOR_INVALIDATED) { mmay_g_s = mmay_g_begin_collection_iterator_mutation(mmay_g_p, %s, ",
                collection->is_set ? "mmay_managed_set_check_mutation_generation"
                    : "mmay_managed_map_check_mutation_generation",
                instruction->first, instruction->second,
                collection->is_set ? "true" : "false"
            ) || !mmay_native_c_symbol_data(buffer, function->symbol)
            || !mmay_native_c_write(
                buffer,
                ", %zu, %zu); goto mmay_g_cleanup; } ",
                instruction->span.start, instruction->span.end
            ) || !mmay_native_c_collection_status(buffer)) return false;
        return mmay_native_c_write(
            buffer, " mmay_g_v_%u = mmay_g_v_%u;\n",
            instruction->target, instruction->first
        );
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_NEXT) {
        if (!mmay_native_c_write(
                buffer,
                "  { size_t mmay_g_next = 0; if (mmay_g_v_%u < 0 || (uint64_t)mmay_g_v_%u > SIZE_MAX) { mmay_g_s = 7; goto mmay_g_cleanup; } mmay_g_m = mmay_managed_collection_next(&mmay_g_x->heap, mmay_g_v_%u, %s, (size_t)mmay_g_v_%u, &mmay_g_next); ",
                instruction->second, instruction->second, instruction->first,
                collection->is_set ? "true" : "false", instruction->second
            ) || !mmay_native_c_collection_status(buffer)) return false;
        return mmay_native_c_write(
            buffer,
            " if (mmay_g_next > INT64_MAX) { mmay_g_s = 6; goto mmay_g_cleanup; } mmay_g_v_%u = (int64_t)mmay_g_next; }\n",
            instruction->target
        );
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_ENTRY_KEY
        || instruction->kind == MMAY_MIR_COLLECTION_ENTRY_VALUE
        || instruction->kind == MMAY_MIR_COLLECTION_CURSOR_KEY
        || instruction->kind == MMAY_MIR_COLLECTION_CURSOR_VALUE) {
        bool key = instruction->kind == MMAY_MIR_COLLECTION_ENTRY_KEY
            || instruction->kind == MMAY_MIR_COLLECTION_CURSOR_KEY;
        bool cursor = instruction->kind == MMAY_MIR_COLLECTION_CURSOR_KEY
            || instruction->kind == MMAY_MIR_COLLECTION_CURSOR_VALUE;
        MarimayTypeId output_type = key
            ? collection->key_type : collection->value_type;
        if (!mmay_native_c_write(
                buffer,
                "  { MarimayManagedCell mmay_g_cell = {0}, mmay_g_key = {0}, mmay_g_value = {0}; (void)mmay_g_value; if (mmay_g_v_%u < 0 || (uint64_t)mmay_g_v_%u > SIZE_MAX) { mmay_g_s = 7; goto mmay_g_cleanup; } ",
                instruction->second, instruction->second
            )) return false;
        if (cursor) {
            if (!mmay_native_c_write(
                    buffer,
                    "mmay_g_m = mmay_managed_collection_cursor_entry(&mmay_g_x->heap, mmay_g_v_%u, %s, (size_t)mmay_g_v_%u, &mmay_g_key, &mmay_g_value); ",
                    instruction->first, collection->is_set ? "true" : "false",
                    instruction->second
                )) return false;
        } else if (!mmay_native_c_write(
                buffer,
                "mmay_g_m = %s(&mmay_g_x->heap, mmay_g_v_%u, (size_t)mmay_g_v_%u, &mmay_g_key%s); ",
                collection->is_set ? "mmay_managed_set_entry" : "mmay_managed_map_entry",
                instruction->first, instruction->second,
                collection->is_set ? "" : ", &mmay_g_value"
            )) return false;
        if (!mmay_native_c_collection_status(buffer)
            || !mmay_native_c_write(
                buffer, " mmay_g_cell = %s; ", key ? "mmay_g_key" : "mmay_g_value"
            ) || !mmay_native_c_write_from_managed_cell(
                buffer, output_type, instruction->target
            )) return false;
        return mmay_native_c_write(buffer, "; }\n");
    }
    if (instruction->kind == MMAY_MIR_COLLECTION_CONTAINS
        || instruction->kind == MMAY_MIR_COLLECTION_GET
        || instruction->kind == MMAY_MIR_COLLECTION_PUT
        || instruction->kind == MMAY_MIR_COLLECTION_ADD
        || instruction->kind == MMAY_MIR_COLLECTION_REMOVE) {
        if (!mmay_native_c_write(
                buffer,
                "  { MarimayManagedCell mmay_g_cell = {0}, mmay_g_key = {0}, mmay_g_value = {0}; bool mmay_g_flag = false; (void)mmay_g_value; "
            ) || !mmay_native_c_collection_key(
                buffer, collection->key_type, instruction->second
            )) return false;
        if (instruction->kind == MMAY_MIR_COLLECTION_PUT
            && !mmay_native_c_collection_value(
                buffer, collection->value_type, instruction->third
            )) return false;
        if (instruction->kind == MMAY_MIR_COLLECTION_CONTAINS) {
            if (!mmay_native_c_write(
                    buffer, "mmay_g_m = %s(&mmay_g_x->heap, mmay_g_v_%u, mmay_g_key, &mmay_g_flag); ",
                    collection->is_set ? "mmay_managed_set_contains"
                        : "mmay_managed_map_contains",
                    instruction->first
                ) || !mmay_native_c_collection_status(buffer)
                || !mmay_native_c_write(
                    buffer, " mmay_g_v_%u = mmay_g_flag; }\n",
                    instruction->target
                )) return false;
        } else if (instruction->kind == MMAY_MIR_COLLECTION_GET) {
            if (!mmay_native_c_write(
                    buffer,
                    "mmay_g_m = mmay_managed_map_get(&mmay_g_x->heap, mmay_g_v_%u, mmay_g_key, &mmay_g_value, &mmay_g_flag); ",
                    instruction->first
                ) || !mmay_native_c_collection_status(buffer)
                || !mmay_native_c_write(
                    buffer, " if (!mmay_g_flag) { mmay_g_s = 7; goto mmay_g_cleanup; } mmay_g_cell = mmay_g_value; "
                ) || !mmay_native_c_write_from_managed_cell(
                    buffer, collection->value_type, instruction->target
                ) || !mmay_native_c_write(buffer, "; }\n")) return false;
        } else if (instruction->kind == MMAY_MIR_COLLECTION_PUT
            || instruction->kind == MMAY_MIR_COLLECTION_ADD) {
            if (!mmay_native_c_write(
                    buffer, "mmay_g_m = %s(&mmay_g_x->heap, mmay_g_v_%u, mmay_g_key%s, &mmay_g_flag); ",
                    instruction->kind == MMAY_MIR_COLLECTION_ADD
                        ? "mmay_managed_set_add" : "mmay_managed_map_put",
                    instruction->first,
                    instruction->kind == MMAY_MIR_COLLECTION_PUT
                        ? ", mmay_g_value" : ""
                ) || !mmay_native_c_collection_status(buffer)
                || !mmay_native_c_write(
                    buffer, " mmay_g_v_%u = mmay_g_v_%u; }\n",
                    instruction->target, instruction->first
                )) return false;
        } else if (!mmay_native_c_write(
                buffer, "mmay_g_m = %s(&mmay_g_x->heap, mmay_g_v_%u, mmay_g_key%s, &mmay_g_flag); ",
                collection->is_set ? "mmay_managed_set_remove"
                    : "mmay_managed_map_remove",
                instruction->first, collection->is_set ? "" : ", &mmay_g_value"
            ) || !mmay_native_c_collection_status(buffer)
            || !mmay_native_c_write(
                buffer, " mmay_g_v_%u = mmay_g_flag; }\n",
                instruction->target
            )) return false;
        return true;
    }
    return false;
}
