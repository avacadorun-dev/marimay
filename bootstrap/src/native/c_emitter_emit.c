#include "c_emitter_internal.h"

#include <string.h>

enum {
    /* Keep large cold/error state out of hot callers without forcing an
       already pressure-heavy record leaf into the outlined frame. */
    MMAY_NATIVE_C_OUTLINE_REGISTER_LIMIT = 32,
    MMAY_NATIVE_C_INLINE_PRESSURE_LIMIT = 32
};

static MarimayTypeId mmay_native_c_register_type(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id
) {
    const MarimayTypeId *type = function != NULL
        ? mmay_vector_at_const(&function->register_types, register_id) : NULL;
    return type != NULL ? *type : MMAY_TYPE_INVALID;
}

static const char *mmay_native_c_integer_operation(MarimayMirInstructionKind kind) {
    switch (kind) {
        case MMAY_MIR_ADD_CHECKED_I64: return "mmay_checked_integer_add";
        case MMAY_MIR_SUBTRACT_CHECKED_I64:
            return "mmay_checked_integer_subtract";
        case MMAY_MIR_MULTIPLY_CHECKED_I64:
            return "mmay_checked_integer_multiply";
        case MMAY_MIR_DIVIDE_CHECKED_I64:
            return "mmay_checked_integer_divide";
        case MMAY_MIR_REMAINDER_CHECKED_I64:
            return "mmay_checked_integer_remainder";
        case MMAY_MIR_SHIFT_LEFT_CHECKED_I64:
            return "mmay_checked_integer_shift_left";
        case MMAY_MIR_SHIFT_RIGHT_I64:
            return "mmay_checked_integer_shift_right";
        case MMAY_MIR_NEGATE_CHECKED_I64:
            return "mmay_checked_integer_negate";
        default: return NULL;
    }
}

static bool mmay_native_c_emit_generic_integer(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    MarimayTypeId type = mmay_native_c_register_type(function, instruction->target);
    const char *kind = mmay_native_c_integer_kind_name(type);
    const char *convert = mmay_native_c_integer_conversion_name(type);
    const char *operation = mmay_native_c_integer_operation(instruction->kind);
    if (kind == NULL || convert == NULL || operation == NULL) {
        return false;
    }
    if (instruction->kind == MMAY_MIR_NEGATE_CHECKED_I64) {
        return mmay_native_c_write(
            buffer,
            "  { MarimayIntegerValue mmay_g_left = {%s, (uint64_t)mmay_g_v_%u}; MarimayIntegerValue mmay_g_out_value = {%s, 0}; mmay_g_s = (int)%s(mmay_g_left, &mmay_g_out_value); if (mmay_g_s == 0 && !%s(mmay_g_out_value, &mmay_g_v_%u)) mmay_g_s = 1; if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; } }\n",
            kind, instruction->first, kind, operation, convert,
            instruction->target, instruction->span.start, instruction->span.end
        );
    }
    return mmay_native_c_write(
        buffer,
        "  { MarimayIntegerValue mmay_g_left = {%s, (uint64_t)mmay_g_v_%u}; MarimayIntegerValue mmay_g_right = {%s, (uint64_t)mmay_g_v_%u}; MarimayIntegerValue mmay_g_out_value = {%s, 0}; mmay_g_s = (int)%s(mmay_g_left, mmay_g_right, &mmay_g_out_value); if (mmay_g_s == 0 && !%s(mmay_g_out_value, &mmay_g_v_%u)) mmay_g_s = 1; if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; } }\n",
        kind, instruction->first, kind, instruction->second, kind, operation,
        convert, instruction->target, instruction->span.start,
        instruction->span.end
    );
}

static bool mmay_native_c_emit_generic_bitwise(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const char *operator_text
) {
    MarimayTypeId type = mmay_native_c_register_type(function, instruction->target);
    const char *kind = mmay_native_c_integer_kind_name(type);
    const char *convert = mmay_native_c_integer_conversion_name(type);
    if (kind == NULL || convert == NULL || operator_text == NULL) {
        return false;
    }
    if (instruction->second == MMAY_MIR_NO_REGISTER) {
        return mmay_native_c_write(
            buffer,
            "  { MarimayIntegerValue mmay_g_left = {%s, (uint64_t)mmay_g_v_%u}; MarimayIntegerValue mmay_g_out_value = {%s, 0}; mmay_g_out_value.bits = (%s mmay_g_left.bits) & ((mmay_integer_bit_width(%s) == 64U) ? UINT64_MAX : (UINT64_C(1) << mmay_integer_bit_width(%s)) - UINT64_C(1)); if (!%s(mmay_g_out_value, &mmay_g_v_%u)) { mmay_g_s = 1; goto mmay_g_cleanup; } }\n",
            kind, instruction->first, kind, operator_text, kind, kind,
            convert, instruction->target
        );
    }
    return mmay_native_c_write(
        buffer,
        "  { MarimayIntegerValue mmay_g_left = {%s, (uint64_t)mmay_g_v_%u}; MarimayIntegerValue mmay_g_right = {%s, (uint64_t)mmay_g_v_%u}; MarimayIntegerValue mmay_g_out_value = {%s, 0}; mmay_g_out_value.bits = (mmay_g_left.bits %s mmay_g_right.bits) & ((mmay_integer_bit_width(%s) == 64U) ? UINT64_MAX : (UINT64_C(1) << mmay_integer_bit_width(%s)) - UINT64_C(1)); if (!%s(mmay_g_out_value, &mmay_g_v_%u)) { mmay_g_s = 1; goto mmay_g_cleanup; } }\n",
        kind, instruction->first, kind, instruction->second, kind,
        operator_text, kind, kind, convert, instruction->target
    );
}

static bool mmay_native_c_inline_record_leaf(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count
) {
    const MarimayTypeId *receiver = NULL;
    MarimayNativeCManagedLayout receiver_layout;
    if (function == NULL || function->calls.length != 0
        || function->parameter_count == 0) {
        return false;
    }
    receiver = mmay_vector_at_const(&function->register_types, 0);
    return receiver != NULL && *receiver >= MMAY_TYPE_FIRST_DYNAMIC
        && mmay_native_c_find_managed_layout(
            functions, count, *receiver, &receiver_layout
        ) && receiver_layout.kind == MMAY_NATIVE_C_MANAGED_RECORD;
}

static bool mmay_native_c_outline_large_function(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count
) {
    size_t call_index = 0;
    if (function == NULL || function->calls.length == 0
        || function->register_types.length
            < MMAY_NATIVE_C_OUTLINE_REGISTER_LIMIT) {
        return false;
    }
    for (call_index = 0; call_index < function->calls.length;
        call_index += 1) {
        const MarimayMirCall *call = mmay_vector_at_const(
            &function->calls, call_index
        );
        const MarimayMirFunction *callee = call != NULL
            ? mmay_native_c_find(functions, count, call->callee) : NULL;
        if (callee == NULL) return false;
        if (callee->register_types.length > MMAY_NATIVE_C_INLINE_PRESSURE_LIMIT
            && mmay_native_c_inline_record_leaf(
                callee, functions, count
            )) {
            return false;
        }
    }
    return true;
}

bool mmay_native_c_emit_signature(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    bool prototype
) {
    size_t index = 0;
    bool inline_record_leaf = mmay_native_c_inline_record_leaf(
        function, functions, count
    );
    bool outline_large_function = mmay_native_c_outline_large_function(
        function, functions, count
    );
    if (!mmay_native_c_write(
            buffer,
            inline_record_leaf
                ? "static MMAY_G_ALWAYS_INLINE int "
                : outline_large_function
                    ? "static MMAY_G_NOINLINE int " : "static int "
        )
        || !mmay_native_c_symbol(buffer, function->symbol)
        || !mmay_native_c_write(buffer, "(mmay_g_execution *mmay_g_x, mmay_g_panic *mmay_g_p")) return false;
    for (index = 0; index < function->parameter_count; index += 1) {
        const MarimayTypeId *type = mmay_vector_at_const(&function->register_types, index);
        MarimayNativeCManagedLayout layout;
        if (!mmay_native_c_write(buffer, ", %s mmay_g_v_%zu", mmay_native_c_type(function, *type), index)) return false;
        if (*type >= MMAY_TYPE_FIRST_DYNAMIC
            && mmay_native_c_find_managed_layout(
                functions, count, *type, &layout
            ) && layout.kind == MMAY_NATIVE_C_MANAGED_RECORD) {
            const MarimayMirRecord *record = mmay_vector_at_const(
                &layout.function->records, layout.index
            );
            size_t field_index = 0;
            if (record == NULL || !mmay_native_c_write(
                    buffer, ", MarimayManagedRecord *mmay_g_ro_%zu", index
                )) return false;
            for (field_index = 0; field_index < record->field_count;
                field_index += 1) {
                const MarimayMirRecordField *field = mmay_vector_at_const(
                    &layout.function->record_fields,
                    record->field_start + field_index
                );
                MarimayNativeCManagedLayout field_layout;
                if (field != NULL && mmay_native_c_find_managed_layout(
                        functions, count, field->type, &field_layout
                    ) && field_layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY
                    && !mmay_native_c_write(
                        buffer,
                        ", MarimayManagedArray *mmay_g_ro_%zu_f_%zu",
                        index, field_index
                    )) return false;
            }
        }
    }
    if (function->result_type != MMAY_TYPE_VOID
        && function->result_type != MMAY_TYPE_NEVER
        && !mmay_native_c_write(buffer, ", %s *mmay_g_out", mmay_native_c_type(function, function->result_type))) return false;
    return mmay_native_c_write(buffer,
        ", size_t *mmay_g_start, size_t *mmay_g_end)%s", prototype ? ";\n" : " {\n"
    );
}

bool mmay_native_c_emit_call_arguments(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirCall *call
) {
    const MarimayMirFunction *callee = call != NULL
        ? mmay_native_c_find(functions, count, call->callee) : NULL;
    size_t argument_index = 0;
    if (callee == NULL) return false;
    for (argument_index = 0;
        argument_index < call->argument_count;
        argument_index += 1) {
        const MarimayMirCallArgument *argument = mmay_native_c_call_argument(
            function,
            call->argument_start + argument_index
        );
        const MarimayTypeId *callee_type = mmay_vector_at_const(
            &callee->register_types, argument_index
        );
        MarimayNativeCManagedLayout callee_layout;
        MarimayNativeCManagedLayout caller_layout;
        bool record_parameter = callee_type != NULL
            && *callee_type >= MMAY_TYPE_FIRST_DYNAMIC
            && mmay_native_c_find_managed_layout(
                functions, count, *callee_type, &callee_layout
            ) && callee_layout.kind == MMAY_NATIVE_C_MANAGED_RECORD;
        bool caller_record_view = argument != NULL
            && mmay_native_c_find_managed_layout(
                functions, count, argument->type, &caller_layout
            ) && caller_layout.kind == MMAY_NATIVE_C_MANAGED_RECORD;
        if (argument == NULL || !mmay_native_c_write(
                buffer,
                ", mmay_g_v_%u",
                argument->register_id
            )) {
            return false;
        }
        if (record_parameter && !mmay_native_c_write(
                buffer,
                caller_record_view ? ", mmay_g_rv_%u.object" : ", NULL",
                argument->register_id
            )) return false;
        if (record_parameter) {
            const MarimayMirRecord *record = mmay_vector_at_const(
                &callee_layout.function->records, callee_layout.index
            );
            size_t field_index = 0;
            if (record == NULL) return false;
            for (field_index = 0; field_index < record->field_count;
                field_index += 1) {
                const MarimayMirRecordField *field = mmay_vector_at_const(
                    &callee_layout.function->record_fields,
                    record->field_start + field_index
                );
                MarimayNativeCManagedLayout field_layout;
                if (field != NULL && mmay_native_c_find_managed_layout(
                        functions, count, field->type, &field_layout
                    ) && field_layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY
                    && !mmay_native_c_write(
                        buffer,
                        caller_record_view
                            ? ", mmay_g_rv_%u_f_%zu.object" : ", NULL",
                        argument->register_id, field_index
                    )) return false;
            }
        }
    }
    return true;
}

static bool mmay_native_c_record_view_is_bound_at(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id,
    size_t depth
) {
    size_t block_index = 0;
    bool found = false;
    if (function == NULL || depth >= function->register_types.length) {
        return false;
    }
    if (register_id < function->parameter_count) return true;
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
            if (instruction != NULL && instruction->target == register_id) {
                if (instruction->kind == MMAY_MIR_CALL) {
                    found = true;
                } else if (instruction->kind == MMAY_MIR_COPY
                    && mmay_native_c_record_view_is_bound_at(
                        function, instruction->first, depth + 1
                    )) {
                    found = true;
                } else {
                    return false;
                }
            }
        }
    }
    return found;
}

static bool mmay_native_c_record_view_is_bound(
    const MarimayMirFunction *function,
    MarimayMirRegister register_id
) {
    return mmay_native_c_record_view_is_bound_at(function, register_id, 0);
}

static bool mmay_native_c_emit_call_record_hints(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    const MarimayMirCall *call
) {
    const MarimayMirFunction *callee = call != NULL
        ? mmay_native_c_find(functions, count, call->callee) : NULL;
    size_t argument_index = 0;
    if (callee == NULL) return false;
    for (argument_index = 0; argument_index < call->argument_count;
        argument_index += 1) {
        const MarimayMirCallArgument *argument = mmay_native_c_call_argument(
            function, call->argument_start + argument_index
        );
        const MarimayTypeId *parameter = mmay_vector_at_const(
            &callee->register_types, argument_index
        );
        MarimayNativeCManagedLayout layout;
        const MarimayMirRecord *record = NULL;
        size_t field_index = 0;
        bool stable_hints = false;
        if (argument == NULL || parameter == NULL) return false;
        if (*parameter < MMAY_TYPE_FIRST_DYNAMIC
            || !mmay_native_c_find_managed_layout(
                functions, count, *parameter, &layout
            ) || layout.kind != MMAY_NATIVE_C_MANAGED_RECORD) continue;
        record = mmay_vector_at_const(&layout.function->records, layout.index);
        if (record == NULL) return false;
        stable_hints = mmay_native_c_record_view_is_bound(
            function, argument->register_id
        );
        for (field_index = 0; stable_hints
            && field_index < record->field_count; field_index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &layout.function->record_fields,
                record->field_start + field_index
            );
            MarimayNativeCManagedLayout field_layout;
            if (field != NULL && field->is_mutable
                && mmay_native_c_find_managed_layout(
                    functions, count, field->type, &field_layout
                ) && field_layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY) {
                stable_hints = false;
            }
        }
        if (stable_hints) continue;
        if (!mmay_native_c_write(
                buffer,
                "  if (mmay_g_rv_%u.object == NULL || !mmay_managed_generated_reference_equal(mmay_g_rv_%u.reference, mmay_g_v_%u)) { mmay_g_m = mmay_managed_record_generated_bind(&mmay_g_x->heap, mmay_g_v_%u, &",
                argument->register_id, argument->register_id,
                argument->register_id, argument->register_id
            ) || !mmay_native_c_write_descriptor_name(
                buffer, MMAY_NATIVE_C_MANAGED_RECORD, *parameter
            ) || !mmay_native_c_write(
                buffer, ", &mmay_g_rv_%u); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }",
                argument->register_id
            )) return false;
        for (field_index = 0; field_index < record->field_count;
            field_index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &layout.function->record_fields,
                record->field_start + field_index
            );
            MarimayNativeCManagedLayout field_layout;
            if (field != NULL && mmay_native_c_find_managed_layout(
                    functions, count, field->type, &field_layout
                ) && field_layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY
                && !mmay_native_c_write(
                    buffer, " mmay_g_rv_%u_f_%zu.object = NULL;",
                    argument->register_id, field_index
                )) return false;
        }
        if (!mmay_native_c_write(buffer, " }\n")) return false;
        for (field_index = 0; field_index < record->field_count;
            field_index += 1) {
            const MarimayMirRecordField *field = mmay_vector_at_const(
                &layout.function->record_fields,
                record->field_start + field_index
            );
            MarimayNativeCManagedLayout field_layout;
            if (field != NULL && mmay_native_c_find_managed_layout(
                    functions, count, field->type, &field_layout
                ) && field_layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY
                ) {
                if (field->is_mutable) {
                    if (!mmay_native_c_write(
                            buffer,
                            "  mmay_g_m = mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_rv_%u.object->fields[%zu].reference, %s, &mmay_g_rv_%u_f_%zu); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n",
                            argument->register_id, field_index,
                            mmay_native_c_array_descriptor(
                                field_layout.function, field->type
                            ),
                            argument->register_id, field_index
                        )) return false;
                } else if (!mmay_native_c_write(
                        buffer,
                        "  if (mmay_g_rv_%u_f_%zu.object == NULL) { mmay_g_m = mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_rv_%u.object->fields[%zu].reference, %s, &mmay_g_rv_%u_f_%zu); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } }\n",
                        argument->register_id, field_index,
                        argument->register_id, field_index,
                        mmay_native_c_array_descriptor(
                            field_layout.function, field->type
                        ),
                        argument->register_id, field_index
                    )) return false;
            }
        }
    }
    return true;
}

static bool mmay_native_c_emit_record_register_bind(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayMirRegister register_id
) {
    const MarimayTypeId *type = mmay_vector_at_const(
        &function->register_types, register_id
    );
    MarimayNativeCManagedLayout layout;
    const MarimayMirRecord *record = NULL;
    size_t field_index = 0;
    if (type == NULL) return false;
    if (*type < MMAY_TYPE_FIRST_DYNAMIC
        || !mmay_native_c_find_managed_layout(
            functions, count, *type, &layout
        ) || layout.kind != MMAY_NATIVE_C_MANAGED_RECORD) return true;
    record = mmay_vector_at_const(&layout.function->records, layout.index);
    if (record == NULL || !mmay_native_c_write(
            buffer,
            "  mmay_g_m = mmay_managed_record_generated_bind(&mmay_g_x->heap, mmay_g_v_%u, &",
            register_id
        ) || !mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_RECORD, *type
        ) || !mmay_native_c_write(
            buffer,
            ", &mmay_g_rv_%u); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n",
            register_id
        )) return false;
    for (field_index = 0; field_index < record->field_count;
        field_index += 1) {
        const MarimayMirRecordField *field = mmay_vector_at_const(
            &layout.function->record_fields, record->field_start + field_index
        );
        MarimayNativeCManagedLayout field_layout;
        if (field != NULL && mmay_native_c_find_managed_layout(
                functions, count, field->type, &field_layout
            ) && field_layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY
            && !mmay_native_c_write(
                buffer,
                "  mmay_g_m = mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_rv_%u.object->fields[%zu].reference, %s, &mmay_g_rv_%u_f_%zu); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n",
                register_id, field_index,
                mmay_native_c_array_descriptor(
                    field_layout.function, field->type
                ),
                register_id, field_index
            )) return false;
    }
    return true;
}

bool mmay_native_c_emit_propagate_call(
    MarimayNativeCBuffer *buffer,
    MarimaySymbolId symbol,
    MarimayMirSpan span,
    bool returns_never
) {
    if (!mmay_native_c_write(buffer, "  if (mmay_g_s == 4) {\n    if (!mmay_g_append_frame(mmay_g_p, ")
        || !mmay_native_c_symbol_data(buffer, symbol)
        || !mmay_native_c_write(
            buffer,
            ", %zu, %zu)) mmay_g_s = 5;\n    goto mmay_g_cleanup;\n  }\n",
            span.start,
            span.end
        )) {
        return false;
    }
    if (returns_never) {
        return mmay_native_c_write(
            buffer,
            "  if (mmay_g_s == 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; mmay_g_s = 5; }\n  goto mmay_g_cleanup;\n",
            span.start,
            span.end
        );
    }
    return mmay_native_c_write(
        buffer,
        "  if (mmay_g_s != 0) goto mmay_g_cleanup;\n"
    );
}

bool mmay_native_c_emit_function(
    MarimayNativeCBuffer *buffer, const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions, size_t count,
    const unsigned char *root_registers, size_t root_count
) {
    size_t index = 0;
    size_t managed_count = root_count;
    bool release_rootless = managed_count != 0
        && !mmay_native_c_function_requires_roots(
            function, functions, count, 0
        );
    if (!mmay_native_c_emit_signature(
            buffer, function, functions, count, false
        )
        || !mmay_native_c_write(
            buffer,
            "  int mmay_g_s = 0;\n"
            "  MarimayManagedStatus mmay_g_m;\n"
            "  (void)mmay_g_m;\n"
            "  (void)mmay_g_p;\n  (void)mmay_g_x;\n"
        )) return false;
    if (mmay_native_c_batch_scalar_mutation(function)
        && !mmay_native_c_write(
            buffer, "  bool mmay_g_array_mutated = false;\n"
        )) return false;
    if (mmay_native_c_function_has_defer(function) && !mmay_native_c_write(
            buffer,
            "  uint32_t mmay_g_defers[%zu] = {0};\n  size_t mmay_g_defer_count = 0;\n  mmay_g_frame mmay_g_cleanup_frames[MMAY_G_MAX_FRAMES];\n  mmay_g_suppressed_panic mmay_g_cleanup_suppressed[MMAY_G_MAX_SUPPRESSED];\n  mmay_g_panic mmay_g_cleanup_p = {0};\n  mmay_g_cleanup_p.frames = mmay_g_cleanup_frames;\n  mmay_g_cleanup_p.frame_capacity = MMAY_G_MAX_FRAMES;\n  mmay_g_cleanup_p.suppressed = mmay_g_cleanup_suppressed;\n  mmay_g_cleanup_p.suppressed_capacity = MMAY_G_MAX_SUPPRESSED;\n  (void)mmay_g_defers;\n  (void)mmay_g_defer_count;\n  (void)mmay_g_cleanup_p;\n",
            function->calls.length
        )) {
        return false;
    }
    if (!mmay_native_c_write(
            buffer, "  (void)mmay_g_start;\n  (void)mmay_g_end;\n"
        )) return false;
    if (function->result_type != MMAY_TYPE_VOID
        && function->result_type != MMAY_TYPE_NEVER
        && !mmay_native_c_write(buffer, "  (void)mmay_g_out;\n")) {
        return false;
    }
    for (index = 0; index < function->parameter_count; index += 1) {
        MarimayNativeCManagedLayout parameter_layout;
        const MarimayTypeId *parameter_type = mmay_vector_at_const(
            &function->register_types, index
        );
        if (!mmay_native_c_write(buffer, "  (void)mmay_g_v_%zu;\n", index)) {
            return false;
        }
        if (parameter_type != NULL
            && *parameter_type >= MMAY_TYPE_FIRST_DYNAMIC
            && mmay_native_c_find_managed_layout(
                functions, count, *parameter_type, &parameter_layout
            ) && parameter_layout.kind == MMAY_NATIVE_C_MANAGED_RECORD) {
            const MarimayMirRecord *record = mmay_vector_at_const(
                &parameter_layout.function->records,
                parameter_layout.index
            );
            size_t field_index = 0;
            if (record == NULL || !mmay_native_c_write(
                    buffer, "  (void)mmay_g_ro_%zu;\n", index
                )) return false;
            for (field_index = 0; field_index < record->field_count;
                field_index += 1) {
                const MarimayMirRecordField *field = mmay_vector_at_const(
                    &parameter_layout.function->record_fields,
                    record->field_start + field_index
                );
                MarimayNativeCManagedLayout field_layout;
                if (field != NULL && mmay_native_c_find_managed_layout(
                        functions, count, field->type, &field_layout
                    ) && field_layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY
                    && !mmay_native_c_write(
                        buffer, "  (void)mmay_g_ro_%zu_f_%zu;\n",
                        index, field_index
                    )) return false;
            }
        }
    }
    for (index = function->parameter_count; index < function->register_types.length; index += 1) {
        const MarimayTypeId *type = mmay_vector_at_const(&function->register_types, index);
        if (type != NULL && *type == MMAY_TYPE_VOID) {
            continue;
        }
        if (!mmay_native_c_write(
                buffer,
                *type >= MMAY_TYPE_FIRST_DYNAMIC
                    ? "  %s mmay_g_v_%zu = mmay_managed_invalid_reference_generated();\n"
                    : (*type == MMAY_TYPE_STRING
                        ? "  %s mmay_g_v_%zu = {0};\n"
                        : (*type == MMAY_TYPE_BOOL
                            ? "  %s mmay_g_v_%zu = false;\n"
                            : "  %s mmay_g_v_%zu = 0;\n")),
                mmay_native_c_type(function, *type),
                index
            )) return false;
        if (*type == MMAY_TYPE_STRING && !mmay_native_c_write(
                buffer,
                "  mmay_g_v_%zu.managed = mmay_managed_invalid_reference_generated();\n",
                index
            )) return false;
        if (!mmay_native_c_write(buffer, "  (void)mmay_g_v_%zu;\n", index)) {
            return false;
        }
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        if (mmay_native_c_register_array(
                function, (MarimayMirRegister)index
            ) != NULL && !mmay_native_c_write(
                buffer,
                "  MarimayManagedArrayGeneratedView mmay_g_av_%zu = {0};\n"
                "  (void)mmay_g_av_%zu;\n",
                index, index
            )) {
            return false;
        }
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const MarimayTypeId *type = mmay_vector_at_const(
            &function->register_types, index
        );
        MarimayNativeCManagedLayout layout;
        bool record_parameter = type != NULL
            && *type >= MMAY_TYPE_FIRST_DYNAMIC
            && mmay_native_c_find_managed_layout(
                functions, count, *type, &layout
            ) && layout.kind == MMAY_NATIVE_C_MANAGED_RECORD;
        if (record_parameter && !mmay_native_c_write(
                buffer,
                "  MarimayManagedRecordGeneratedView mmay_g_rv_%zu = {0};\n"
                "  (void)mmay_g_rv_%zu;\n",
                index, index
            )) {
            return false;
        }
        if (record_parameter) {
            const MarimayMirRecord *record = mmay_vector_at_const(
                &layout.function->records, layout.index
            );
            size_t field_index = 0;
            if (record == NULL) return false;
            for (field_index = 0; field_index < record->field_count;
                field_index += 1) {
                const MarimayMirRecordField *field = mmay_vector_at_const(
                    &layout.function->record_fields,
                    record->field_start + field_index
                );
                MarimayNativeCManagedLayout field_layout;
                if (field != NULL && mmay_native_c_find_managed_layout(
                        functions, count, field->type, &field_layout
                    ) && field_layout.kind == MMAY_NATIVE_C_MANAGED_ARRAY
                    && !mmay_native_c_write(
                        buffer,
                        "  MarimayManagedArrayGeneratedView mmay_g_rv_%zu_f_%zu = {0};\n"
                        "  (void)mmay_g_rv_%zu_f_%zu;\n",
                        index, field_index, index, field_index
                    )) {
                    return false;
                }
            }
        }
    }
    if (managed_count != 0) {
        size_t managed_index = 0;
        bool inline_root_locations = managed_count <= 64;
        if (!mmay_native_c_write(
                buffer,
                "  MarimayManagedRootFrame mmay_g_root = {0};\n"
                "  MarimayManagedReference **mmay_g_locations = NULL;\n"
            )) return false;
        if (inline_root_locations && !mmay_native_c_write(
                buffer,
                "  MarimayManagedReference *mmay_g_location_storage[%zu] = {0};\n"
                "  mmay_g_locations = mmay_g_location_storage;\n",
                managed_count
            )) return false;
        if (!mmay_native_c_write(
                buffer,
                "  size_t mmay_g_location_bytes = 0;\n"
                "  MarimayManagedCell *mmay_g_nominal_cells = NULL;\n"
                "  size_t mmay_g_nominal_cell_bytes = 0;\n"
                "  bool mmay_g_root_linked = false;\n"
                "  (void)mmay_g_root;\n"
                "  (void)mmay_g_locations;\n"
                "  (void)mmay_g_location_bytes;\n"
                "  (void)mmay_g_root_linked;\n"
            )) return false;
        if (release_rootless && !mmay_native_c_write(
                buffer, "#if MMAY_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
            )) return false;
        if (inline_root_locations) {
            if (!mmay_native_c_write(
                    buffer,
                    "#if MMAY_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
                    "  if (!mmay_size_multiply(%zu, sizeof(*mmay_g_locations), &mmay_g_location_bytes)) { mmay_g_s = 6; goto mmay_g_cleanup; }\n"
                    "  mmay_g_locations = mmay_allocate(&mmay_g_x->allocator, mmay_g_location_bytes);\n"
                    "  if (mmay_g_locations == NULL) { mmay_g_s = 5; goto mmay_g_cleanup; }\n"
                    "#endif\n",
                    managed_count
                )) return false;
        } else if (!mmay_native_c_write(
                buffer,
                "  if (!mmay_size_multiply(%zu, sizeof(*mmay_g_locations), &mmay_g_location_bytes)) { mmay_g_s = 6; goto mmay_g_cleanup; }\n"
                "  mmay_g_locations = mmay_allocate(&mmay_g_x->allocator, mmay_g_location_bytes);\n"
                "  if (mmay_g_locations == NULL) { mmay_g_s = 5; goto mmay_g_cleanup; }\n",
                managed_count
            )) return false;
        for (index = 0; index < function->register_types.length; index += 1) {
            const MarimayTypeId *type = mmay_vector_at_const(
                &function->register_types,
                index
            );
            if (type != NULL && root_registers != NULL
                && root_registers[index] != 0
                && (*type == MMAY_TYPE_STRING
                    || *type >= MMAY_TYPE_FIRST_DYNAMIC)) {
                if (!mmay_native_c_write(
                        buffer,
                        *type == MMAY_TYPE_STRING
                            ? "  mmay_g_locations[%zu] = &mmay_g_v_%zu.managed;\n"
                            : "  mmay_g_locations[%zu] = &mmay_g_v_%zu;\n",
                        managed_index,
                        index
                    )) return false;
                managed_index += 1;
            }
        }
        if (release_rootless) {
            if (!mmay_native_c_write(
                    buffer,
                    "  mmay_g_m = mmay_managed_root_push(&mmay_g_x->heap, &mmay_g_root, mmay_g_locations, %zu);\n"
                    "  if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n"
                    "  mmay_g_root_linked = true;\n",
                    managed_count
                )) return false;
        } else if (!mmay_native_c_write(
                buffer,
                "#if MMAY_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
                "  mmay_g_m = mmay_managed_root_push(&mmay_g_x->heap, &mmay_g_root, mmay_g_locations, %zu);\n"
                "  if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n"
                "#else\n"
                "  mmay_managed_root_push_generated(&mmay_g_x->heap, &mmay_g_root, mmay_g_locations, %zu);\n"
                "#endif\n"
                "  mmay_g_root_linked = true;\n",
                managed_count, managed_count
            )) return false;
        if (release_rootless && !mmay_native_c_write(buffer, "#endif\n")) {
            return false;
        }
    }
    for (index = 0; index < function->parameter_count; index += 1) {
        const MarimayTypeId *type = mmay_vector_at_const(
            &function->register_types,
            index
        );
        if (type != NULL && *type >= MMAY_TYPE_FIRST_DYNAMIC
            && !mmay_native_c_emit_managed_parameter_check(
                buffer, function, functions, count, index
            )) return false;
    }
    for (index = 0; index < function->parameter_count; index += 1) {
        if (mmay_native_c_array_children_parameter(
                function, functions, count, (MarimayMirRegister)index
            ) && !mmay_native_c_write(
                buffer,
                "  mmay_g_m = mmay_managed_array_generated_cache_children(&mmay_g_x->heap, mmay_g_v_%zu, &mmay_g_av_%zu); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n",
                index, index
            )) return false;
    }
    if (!mmay_native_c_write(buffer, "  goto mmay_g_b_0;\n")) return false;
    for (index = 0; index < function->blocks.length; index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(&function->blocks, index);
        size_t instruction_index = 0;
        if (!mmay_native_c_write(buffer, "mmay_g_b_%zu:\n", index)) return false;
        for (instruction_index = 0; instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *i = mmay_vector_at_const(&block->instructions, instruction_index);
            const char *helper = NULL;
            switch (i->kind) {
                case MMAY_MIR_CONSTANT_I64:
                    if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_i64_from_twos_complement_fast(UINT64_C(0x%016llx));\n", i->target, (unsigned long long)(uint64_t)i->integer)) return false;
                    break;
                case MMAY_MIR_CONSTANT_INTEGER: {
                    MarimayTypeId type = mmay_native_c_register_type(
                        function, i->target
                    );
                    const char *kind = mmay_native_c_integer_kind_name(type);
                    const char *convert = mmay_native_c_integer_conversion_name(type);
                    if (kind == NULL || convert == NULL) return false;
                    if (!mmay_native_c_write(
                            buffer,
                            "  { MarimayIntegerValue mmay_g_integer_value = {%s, UINT64_C(0x%016llx)}; if (!%s(mmay_g_integer_value, &mmay_g_v_%u)) { mmay_g_s = 1; *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; } }\n",
                            kind,
                            (unsigned long long)i->integer_bits,
                            convert,
                            i->target,
                            i->span.start,
                            i->span.end
                        )) return false;
                    break;
                }
                case MMAY_MIR_CONSTANT_F64:
                    if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = UINT64_C(0x%016llx);\n", i->target, (unsigned long long)i->f64_bits)) return false;
                    break;
                case MMAY_MIR_CONSTANT_BOOL: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = %s;\n", i->target, i->boolean ? "true" : "false")) return false; break;
                case MMAY_MIR_CONSTANT_STRING: {
                    const MarimayMirStringConstant *constant = mmay_vector_at_const(
                        &function->string_constants,
                        i->string_constant
                    );
                    if (constant == NULL
                        || !mmay_native_c_write(
                            buffer,
                            "  mmay_g_v_%u.data = ",
                            i->target
                        )
                        || !mmay_native_c_string_data(
                            buffer,
                            function->symbol,
                            i->string_constant
                        )
                        || !mmay_native_c_write(
                            buffer,
                            "; mmay_g_v_%u.length = %zu; mmay_g_v_%u.managed = mmay_managed_invalid_reference_generated();\n",
                            i->target,
                            constant->value.length,
                            i->target
                        )) {
                        return false;
                    }
                    break;
                }
                case MMAY_MIR_COPY:
                    {
                        MarimayNativeCManagedLayout copied_layout;
                        const MarimayMirRecord *copied_record = NULL;
                        size_t field_index = 0;
                        bool copies_record_view = false;
                    if (!mmay_native_c_write(
                            buffer, "  mmay_g_v_%u = mmay_g_v_%u;\n",
                            i->target, i->first
                        )) return false;
                    if (mmay_native_c_register_array(function, i->target)
                            != NULL && !mmay_native_c_write(
                                buffer,
                                "  mmay_g_av_%u = mmay_g_av_%u;\n",
                                i->target, i->first
                            )) return false;
                    copies_record_view = mmay_native_c_find_managed_layout(
                            functions, count,
                            mmay_native_c_register_type(function, i->target),
                            &copied_layout
                        ) && copied_layout.kind
                            == MMAY_NATIVE_C_MANAGED_RECORD;
                    if (copies_record_view && !mmay_native_c_write(
                            buffer,
                            "  mmay_g_rv_%u = mmay_g_rv_%u;\n",
                            i->target, i->first
                        )) return false;
                    if (copies_record_view) {
                        copied_record = mmay_vector_at_const(
                            &copied_layout.function->records,
                            copied_layout.index
                        );
                        if (copied_record == NULL) return false;
                        for (field_index = 0;
                            field_index < copied_record->field_count;
                            field_index += 1) {
                            const MarimayMirRecordField *field =
                                mmay_vector_at_const(
                                    &copied_layout.function->record_fields,
                                    copied_record->field_start + field_index
                                );
                            MarimayNativeCManagedLayout field_layout;
                            if (field != NULL
                                && mmay_native_c_find_managed_layout(
                                    functions, count, field->type,
                                    &field_layout
                                ) && field_layout.kind
                                    == MMAY_NATIVE_C_MANAGED_ARRAY
                                && !mmay_native_c_write(
                                    buffer,
                                    "  mmay_g_rv_%u_f_%zu = mmay_g_rv_%u_f_%zu;\n",
                                    i->target, field_index, i->first,
                                    field_index
                                )) return false;
                        }
                    }
                    break;
                    }
                case MMAY_MIR_NOT_BOOL: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = !mmay_g_v_%u;\n", i->target, i->first)) return false; break;
                case MMAY_MIR_EQUAL_I64: case MMAY_MIR_EQUAL_BOOL: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_g_v_%u == mmay_g_v_%u;\n", i->target, i->first, i->second)) return false; break;
                case MMAY_MIR_EQUAL_STRING: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_g_string_equal(mmay_g_v_%u, mmay_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case MMAY_MIR_LESS_I64: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_g_v_%u < mmay_g_v_%u;\n", i->target, i->first, i->second)) return false; break;
                case MMAY_MIR_LESS_EQUAL_I64: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_g_v_%u <= mmay_g_v_%u;\n", i->target, i->first, i->second)) return false; break;
                case MMAY_MIR_EQUAL_F64: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_f64_equal(mmay_g_v_%u, mmay_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case MMAY_MIR_LESS_F64: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_f64_less(mmay_g_v_%u, mmay_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case MMAY_MIR_LESS_EQUAL_F64: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_f64_less_equal(mmay_g_v_%u, mmay_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case MMAY_MIR_ADD_F64: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_f64_add(mmay_g_v_%u, mmay_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case MMAY_MIR_SUBTRACT_F64: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_f64_subtract(mmay_g_v_%u, mmay_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case MMAY_MIR_MULTIPLY_F64: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_f64_multiply(mmay_g_v_%u, mmay_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case MMAY_MIR_DIVIDE_F64: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_f64_divide(mmay_g_v_%u, mmay_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case MMAY_MIR_NEGATE_F64: if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_f64_negate(mmay_g_v_%u);\n", i->target, i->first)) return false; break;
                case MMAY_MIR_ARRAY_NEW: {
                    const MarimayMirArrayConstruction *construction =
                        mmay_native_c_array_construction(
                            function,
                            i->array_construction
                        );
                    const MarimayMirArray *array = construction != NULL
                        ? mmay_vector_at_const(
                            &function->arrays,
                            construction->array
                        )
                        : NULL;
                    size_t argument_index = 0;
                    if (construction == NULL || array == NULL
                        || !mmay_native_c_write(
                            buffer,
                            "  { MarimayManagedCell *mmay_g_cells = NULL; size_t mmay_g_cell_bytes = 0;\n"
                            "    if (!mmay_size_multiply(%zu, sizeof(*mmay_g_cells), &mmay_g_cell_bytes)) { mmay_g_s = 6; goto mmay_g_cleanup; }\n"
                            "    if (mmay_g_cell_bytes != 0) { mmay_g_cells = mmay_allocate(&mmay_g_x->allocator, mmay_g_cell_bytes); if (mmay_g_cells == NULL) { mmay_g_s = 5; goto mmay_g_cleanup; } }\n",
                            construction->argument_count
                        )) return false;
                    for (argument_index = 0;
                        argument_index < construction->argument_count;
                        argument_index += 1) {
                        const MarimayMirArrayArgument *argument =
                            mmay_native_c_array_argument(
                                function,
                                construction->argument_start + argument_index
                            );
                        if (argument == NULL || !mmay_native_c_write(
                                buffer,
                                "    { MarimayManagedCell mmay_g_cell; memset(&mmay_g_cell, 0, sizeof(mmay_g_cell)); "
                            ) || !mmay_native_c_write_to_managed_cell(
                                buffer,
                                array->element_type,
                                argument->register_id
                            ) || !mmay_native_c_write(
                                buffer,
                                "; mmay_g_cells[%zu] = mmay_g_cell; }\n",
                                argument_index
                            )) return false;
                    }
                    if (!mmay_native_c_write(
                            buffer,
                            "    mmay_g_m = mmay_managed_array_new(&mmay_g_x->heap, %s, mmay_g_cells, %zu, &mmay_g_v_%u);\n"
                            "    mmay_deallocate(&mmay_g_x->allocator, mmay_g_cells, mmay_g_cell_bytes);\n"
                            "    if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n"
                            "    mmay_g_av_%u = (MarimayManagedArrayGeneratedView){0};",
                            mmay_native_c_array_descriptor(function, array->type),
                            construction->argument_count,
                            i->target,
                            i->target
                        ) || (mmay_native_c_array_view_known(
                                function, i->target
                            ) && !mmay_native_c_write(
                                buffer,
                                " mmay_g_av_%u.object = mmay_managed_array_generated_child_unchecked(&mmay_g_x->heap, mmay_g_v_%u);",
                                i->target, i->target
                            )) || !mmay_native_c_write(
                                buffer, "\n  }\n"
                            )) return false;
                    break;
                }
                case MMAY_MIR_ARRAY_PUSH: {
                    const MarimayMirArray *array = mmay_vector_at_const(
                        &function->arrays, i->array
                    );
                    MarimayMirRegister fill_count = MMAY_MIR_NO_REGISTER;
                    MarimayMirRegister fill_index = MMAY_MIR_NO_REGISTER;
                    uint64_t fill_bits = 0;
                    bool raw64 = array != NULL
                        && (array->element_type == MMAY_TYPE_I64
                            || array->element_type == MMAY_TYPE_U64
                            || array->element_type == MMAY_TYPE_F64);
                    bool fill = raw64 && mmay_native_c_array_fill_plan(
                        function, i, &fill_count, &fill_index, &fill_bits,
                        NULL
                    );
                    bool untracked = raw64
                        && mmay_native_c_array_generation_unobserved(
                            function, i->first
                        );
                    if (array == NULL || !mmay_native_c_write(
                            buffer,
                            "  { MarimayManagedCell mmay_g_cell; memset(&mmay_g_cell, 0, sizeof(mmay_g_cell)); "
                        ) || (raw64
                            ? !mmay_native_c_write(
                                buffer,
                                "mmay_g_cell.f64_bits = (uint64_t)mmay_g_v_%u",
                                i->second
                            )
                            : !mmay_native_c_write_to_managed_cell(
                                buffer, array->element_type, i->second
                            )) || !mmay_native_c_write(buffer, ";")
                        || (!mmay_native_c_array_view_known(
                                function, i->first
                            ) && !mmay_native_c_write(
                                buffer,
                                " if (mmay_g_av_%u.object == NULL) { mmay_g_m = mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_v_%u, %s, &mmay_g_av_%u); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } }",
                                i->first, i->first,
                                mmay_native_c_array_descriptor(
                                    function, array->type
                                ),
                                i->first
                            ))) return false;
                    if (fill) {
                        if (!mmay_native_c_write(
                            buffer,
                            " if ((uint64_t)mmay_g_v_%u > (uint64_t)SIZE_MAX) { mmay_g_s = 6; goto mmay_g_cleanup; } mmay_g_m = mmay_managed_array_fill_raw64_pushes(&mmay_g_x->heap, mmay_g_v_%u, (size_t)mmay_g_v_%u, UINT64_C(0x%016llx)); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } mmay_g_v_%u = mmay_i64_from_twos_complement_fast((uint64_t)mmay_g_v_%u - UINT64_C(1)); mmay_g_v_%u = mmay_g_v_%u; mmay_g_av_%u = mmay_g_av_%u; }\n",
                            fill_count,
                            i->first,
                            fill_count,
                            (unsigned long long)fill_bits,
                            fill_index,
                            fill_count,
                            i->target, i->first,
                            i->target, i->first
                        )) return false;
                    } else if (raw64) {
                        if (!mmay_native_c_write(
                                buffer,
                                " mmay_g_m = %s(&mmay_g_x->heap, mmay_g_v_%u, &mmay_g_av_%u, mmay_g_cell.f64_bits); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } mmay_g_v_%u = mmay_g_v_%u; mmay_g_av_%u = mmay_g_av_%u; }\n",
                                untracked
                                    ? "mmay_managed_array_generated_push_raw64_untracked"
                                    : "mmay_managed_array_generated_push_raw64",
                                i->first, i->first, i->target, i->first,
                                i->target, i->first
                            )) return false;
                    } else if (!mmay_native_c_write(
                            buffer,
                            " { MarimayManagedArrayView mmay_g_checked = mmay_managed_array_generated_expand(&mmay_g_av_%u, mmay_g_v_%u); mmay_g_m = mmay_managed_array_view_push(&mmay_g_x->heap, &mmay_g_checked, mmay_g_cell); } if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } mmay_g_v_%u = mmay_g_v_%u; mmay_g_av_%u = mmay_g_av_%u; }\n",
                            i->first, i->first, i->target, i->first,
                            i->target, i->first
                        )) return false;
                    break;
                }
                case MMAY_MIR_ARRAY_POP:
                case MMAY_MIR_ARRAY_TRUNCATE:
                case MMAY_MIR_ARRAY_CLEAR:
                case MMAY_MIR_ARRAY_RESERVE:
                case MMAY_MIR_ARRAY_GENERATION:
                case MMAY_MIR_ARRAY_REQUIRE_GENERATION:
                    if (!mmay_native_c_emit_array_mutation_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case MMAY_MIR_ARRAY_LENGTH:
                case MMAY_MIR_ARRAY_REQUIRE_INDEX:
                case MMAY_MIR_ARRAY_ASSUME_INDEX:
                case MMAY_MIR_ARRAY_GET:
                case MMAY_MIR_ARRAY_SET:
                case MMAY_MIR_ARRAY_GET_IN_BOUNDS:
                case MMAY_MIR_ARRAY_SET_IN_BOUNDS:
                    if (!mmay_native_c_emit_range_instruction(
                            buffer, function, functions, count, block,
                            instruction_index, i
                        )) return false;
                    break;
                case MMAY_MIR_COLLECTION_NEW:
                case MMAY_MIR_COLLECTION_LENGTH:
                case MMAY_MIR_COLLECTION_CONTAINS:
                case MMAY_MIR_COLLECTION_GET:
                case MMAY_MIR_COLLECTION_PUT:
                case MMAY_MIR_COLLECTION_ADD:
                case MMAY_MIR_COLLECTION_REMOVE:
                case MMAY_MIR_COLLECTION_CLEAR:
                case MMAY_MIR_COLLECTION_GENERATION:
                case MMAY_MIR_COLLECTION_REQUIRE_GENERATION:
                case MMAY_MIR_COLLECTION_ENTRY_KEY:
                case MMAY_MIR_COLLECTION_ENTRY_VALUE:
                case MMAY_MIR_COLLECTION_GET_OPTION:
                case MMAY_MIR_COLLECTION_REMOVE_OPTION:
                case MMAY_MIR_COLLECTION_PUT_RESULT:
                case MMAY_MIR_COLLECTION_ADD_RESULT:
                    if (!mmay_native_c_emit_collection_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case MMAY_MIR_BITWISE_NOT_I64:
                    if (mmay_native_c_register_type(function, i->target)
                            != MMAY_TYPE_I64) {
                        if (!mmay_native_c_emit_generic_bitwise(
                                buffer, function, i, "~"
                            )) return false;
                    } else if (!mmay_native_c_write(buffer, "  mmay_g_v_%u = mmay_i64_bitwise_not(mmay_g_v_%u);\n", i->target, i->first)) return false;
                    break;
                case MMAY_MIR_BITWISE_AND_I64: case MMAY_MIR_BITWISE_XOR_I64: case MMAY_MIR_BITWISE_OR_I64: {
                    const char *name = i->kind == MMAY_MIR_BITWISE_AND_I64 ? "and" : i->kind == MMAY_MIR_BITWISE_XOR_I64 ? "xor" : "or";
                    const char *operator_text = i->kind == MMAY_MIR_BITWISE_AND_I64
                        ? "&" : i->kind == MMAY_MIR_BITWISE_XOR_I64 ? "^" : "|";
                    if (mmay_native_c_register_type(function, i->target)
                            != MMAY_TYPE_I64) {
                        if (!mmay_native_c_emit_generic_bitwise(
                                buffer, function, i, operator_text
                            )) return false;
                    } else if (!mmay_native_c_write(
                            buffer,
                            "  mmay_g_v_%u = mmay_i64_bitwise_%s(mmay_g_v_%u, mmay_g_v_%u);\n",
                            i->target,
                            name,
                            i->first,
                            i->second
                        )) {
                        return false;
                    }
                    break;
                }
                case MMAY_MIR_ADD_CHECKED_I64:
                    if (mmay_native_c_register_type(function, i->target)
                            == MMAY_TYPE_I64
                        && mmay_native_c_add_one_proven(
                            function, functions, count, i
                        )) {
                        if (!mmay_native_c_write(
                                buffer,
                                "  mmay_g_v_%u = mmay_i64_add_proven_fast(mmay_g_v_%u, mmay_g_v_%u);\n",
                                i->target, i->first, i->second
                            )) return false;
                    } else {
                        helper = "mmay_checked_i64_add_fast";
                    }
                    break;
                case MMAY_MIR_SUBTRACT_CHECKED_I64:
                    if (mmay_native_c_register_type(function, i->target)
                            == MMAY_TYPE_I64
                        && mmay_native_c_subtract_one_proven(
                            function, functions, count, i
                        )) {
                        if (!mmay_native_c_write(
                                buffer,
                                "  mmay_g_v_%u = mmay_i64_subtract_proven_fast(mmay_g_v_%u, mmay_g_v_%u);\n",
                                i->target, i->first, i->second
                            )) return false;
                    } else {
                        helper = "mmay_checked_i64_subtract_fast";
                    }
                    break;
                case MMAY_MIR_MULTIPLY_CHECKED_I64: helper = "mmay_checked_i64_multiply_fast"; break;
                case MMAY_MIR_ADD_PROVEN_I64:
                    if (!mmay_native_c_write(
                            buffer,
                            "  mmay_g_v_%u = mmay_i64_add_proven_fast(mmay_g_v_%u, mmay_g_v_%u);\n",
                            i->target, i->first, i->second
                        )) return false;
                    break;
                case MMAY_MIR_SUBTRACT_PROVEN_I64:
                    if (!mmay_native_c_write(
                            buffer,
                            "  mmay_g_v_%u = mmay_i64_subtract_proven_fast(mmay_g_v_%u, mmay_g_v_%u);\n",
                            i->target, i->first, i->second
                        )) return false;
                    break;
                case MMAY_MIR_MULTIPLY_PROVEN_I64:
                    if (!mmay_native_c_write(
                            buffer,
                            "  mmay_g_v_%u = mmay_i64_multiply_proven_fast(mmay_g_v_%u, mmay_g_v_%u);\n",
                            i->target, i->first, i->second
                        )) return false;
                    break;
                case MMAY_MIR_DIVIDE_CHECKED_I64:
                case MMAY_MIR_REMAINDER_CHECKED_I64:
                    if (mmay_native_c_register_type(function, i->target)
                            == MMAY_TYPE_I64
                        && mmay_native_c_divisor_proven(function, i)) {
                        if (!mmay_native_c_write(
                                buffer,
                                "  mmay_g_v_%u = mmay_g_v_%u %s mmay_g_v_%u;\n",
                                i->target, i->first,
                                i->kind == MMAY_MIR_DIVIDE_CHECKED_I64
                                    ? "/" : "%",
                                i->second
                            )) return false;
                    } else {
                        helper = i->kind == MMAY_MIR_DIVIDE_CHECKED_I64
                            ? "mmay_checked_i64_divide_fast"
                            : "mmay_checked_i64_remainder_fast";
                    }
                    break;
                case MMAY_MIR_SHIFT_LEFT_CHECKED_I64: helper = "mmay_checked_i64_shift_left"; break;
                case MMAY_MIR_SHIFT_RIGHT_I64: helper = "mmay_checked_i64_shift_right"; break;
                case MMAY_MIR_NEGATE_CHECKED_I64:
                    if (mmay_native_c_register_type(function, i->target)
                            == MMAY_TYPE_I64
                        && mmay_native_c_negate_proven(function, i)) {
                        if (!mmay_native_c_write(
                                buffer,
                                "  mmay_g_v_%u = mmay_i64_from_twos_complement_fast(UINT64_C(0) - (uint64_t)mmay_g_v_%u);\n",
                                i->target, i->first
                            )) return false;
                    } else {
                        helper = "mmay_checked_i64_negate_fast";
                    }
                    break;
                case MMAY_MIR_CALL: {
                    const MarimayMirCall *call = mmay_native_c_call(function, i->call);
                    const MarimayMirFunction *callee = mmay_native_c_find(functions, count, call->callee);
                    if (!mmay_native_c_emit_call_record_hints(
                            buffer, function, functions, count, call
                        ) || !mmay_native_c_write(buffer, "  mmay_g_s = ")
                        || !mmay_native_c_symbol(buffer, callee->symbol)
                        || !mmay_native_c_write(buffer, "(mmay_g_x, mmay_g_p")
                        || !mmay_native_c_emit_call_arguments(
                            buffer, function, functions, count, call
                        )
                        || !(call->result_type == MMAY_TYPE_VOID
                            ? mmay_native_c_write(
                                buffer,
                                ", mmay_g_start, mmay_g_end);\n"
                            )
                            : mmay_native_c_write(
                                buffer,
                                ", &mmay_g_v_%u, mmay_g_start, mmay_g_end);\n",
                                i->target
                            ))
                        || !mmay_native_c_emit_propagate_call(
                            buffer,
                            function->symbol,
                            i->span,
                            false
                        ) || (call->result_type != MMAY_TYPE_VOID
                            && call->result_type != MMAY_TYPE_NEVER
                            && !mmay_native_c_emit_record_register_bind(
                                buffer, function, functions, count, i->target
                            ))) return false;
                    if (mmay_native_c_register_array(function, i->target)
                            != NULL && !mmay_native_c_write(
                                buffer,
                                "  mmay_g_av_%u = (MarimayManagedArrayGeneratedView){0};\n",
                                i->target
                            )) return false;
                    break;
                }
                case MMAY_MIR_RECORD_NEW: case MMAY_MIR_RECORD_GET:
                case MMAY_MIR_RECORD_SET:
                case MMAY_MIR_RECORD_IDENTITY_SAME:
                case MMAY_MIR_ENUM_NEW: case MMAY_MIR_ENUM_IS_VARIANT:
                case MMAY_MIR_ENUM_REQUIRE_VARIANT:
                case MMAY_MIR_ENUM_GET_PAYLOAD:
                case MMAY_MIR_INTERFACE_NEW:
                case MMAY_MIR_CALL_INTERFACE:
                    if (!mmay_native_c_emit_managed_instruction(
                            buffer, function, functions, count, i
                        )) return false;
                    break;
                case MMAY_MIR_INTEGER_POLICY:
                case MMAY_MIR_INTEGER_CONVERT:
                    if (!mmay_native_c_emit_numeric_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case MMAY_MIR_FORMAT_F32:
                case MMAY_MIR_FORMAT_F64:
                case MMAY_MIR_FORMAT_BOOL:
                case MMAY_MIR_FORMAT_INTEGER:
                case MMAY_MIR_STRING_CONCAT:
                case MMAY_MIR_PARSE_INTEGER:
                    if (!mmay_native_c_emit_format_instruction(
                            buffer, function, i
                        )) {
                        return false;
                    }
                    break;
                case MMAY_MIR_TEXT_UNARY:
                case MMAY_MIR_TEXT_BINARY:
                    if (!mmay_native_c_emit_text_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case MMAY_MIR_PATH:
                case MMAY_MIR_FS_UNARY:
                case MMAY_MIR_FS_BINARY:
                    if (!mmay_native_c_emit_fs_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case MMAY_MIR_ENV_READ:
                case MMAY_MIR_PROCESS_CONTEXT:
                    if (!mmay_native_c_emit_context_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case MMAY_MIR_IO_PRINTLN:
                case MMAY_MIR_IO_WRITE:
                case MMAY_MIR_IO_FLUSH:
                case MMAY_MIR_IO_READ_LINE:
                case MMAY_MIR_IO_READ_BYTES:
                case MMAY_MIR_IO_WRITE_BYTES:
                    if (!mmay_native_c_emit_io_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case MMAY_MIR_TEST_ASSERT:
                case MMAY_MIR_TEST_OUTPUT_BEGIN:
                case MMAY_MIR_TEST_OUTPUT_END:
                    if (!mmay_native_c_emit_test_output_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case MMAY_MIR_DEFER_REGISTER:
                case MMAY_MIR_DEFER_CALL:
                    if (!mmay_native_c_emit_defer_instruction(
                            buffer, function, functions, count, i
                        )) {
                        return false;
                    }
                    break;
                default: return false;
            }
            if (helper != NULL
                && mmay_native_c_register_type(function, i->target)
                    != MMAY_TYPE_I64) {
                if (!mmay_native_c_emit_generic_integer(buffer, function, i)) {
                    return false;
                }
                helper = NULL;
            }
            if (helper != NULL) {
                if (i->kind == MMAY_MIR_NEGATE_CHECKED_I64) {
                    if (!mmay_native_c_write(buffer, "  mmay_g_s = (int)%s(mmay_g_v_%u, &mmay_g_v_%u);\n", helper, i->first, i->target)) return false;
                } else if (!mmay_native_c_write(buffer, "  mmay_g_s = (int)%s(mmay_g_v_%u, mmay_g_v_%u, &mmay_g_v_%u);\n", helper, i->first, i->second, i->target)) return false;
                if (!mmay_native_c_write(buffer, "  if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; }\n", i->span.start, i->span.end)) return false;
            }
        }
        if (block->terminator.kind == MMAY_MIR_GOTO) {
            if (!mmay_native_c_write(buffer, "  goto mmay_g_b_%u;\n", block->terminator.target)) return false;
        } else if (block->terminator.kind == MMAY_MIR_BRANCH) {
            if (!mmay_native_c_write(buffer, "  if (mmay_g_v_%u) goto mmay_g_b_%u; else goto mmay_g_b_%u;\n", block->terminator.value, block->terminator.target, block->terminator.otherwise)) return false;
        } else if (block->terminator.kind == MMAY_MIR_PANIC) {
            if (!mmay_native_c_write(
                    buffer,
                    "  mmay_g_s = mmay_g_begin_panic(mmay_g_p, mmay_g_v_%u, ",
                    block->terminator.value
                )
                || !mmay_native_c_symbol_data(buffer, function->symbol)
                || !mmay_native_c_write(
                    buffer,
                    ", %zu, %zu); goto mmay_g_cleanup;\n",
                    block->terminator.span.start,
                    block->terminator.span.end
                )) return false;
        } else if (block->terminator.kind == MMAY_MIR_CALL_NEVER) {
            const MarimayMirCall *call = mmay_native_c_call(
                function,
                block->terminator.call
            );
            const MarimayMirFunction *callee = mmay_native_c_find(
                functions,
                count,
                call->callee
            );
            if (!mmay_native_c_emit_call_record_hints(
                    buffer, function, functions, count, call
                ) || !mmay_native_c_write(buffer, "  mmay_g_s = ")
                || !mmay_native_c_symbol(buffer, callee->symbol)
                || !mmay_native_c_write(buffer, "(mmay_g_x, mmay_g_p")
                || !mmay_native_c_emit_call_arguments(
                    buffer, function, functions, count, call
                )
                || !mmay_native_c_write(
                    buffer,
                    ", mmay_g_start, mmay_g_end);\n"
                )
                || !mmay_native_c_emit_propagate_call(
                    buffer,
                    function->symbol,
                    block->terminator.span,
                    true
                )) return false;
        } else if (function->result_type == MMAY_TYPE_VOID) {
            if (!mmay_native_c_write(buffer, "  mmay_g_s = 0; goto mmay_g_cleanup;\n")) return false;
        } else if (!mmay_native_c_write(buffer, "  *mmay_g_out = mmay_g_v_%u; mmay_g_s = 0; goto mmay_g_cleanup;\n", block->terminator.value)) return false;
    }
    if (!mmay_native_c_write(buffer, "mmay_g_cleanup:\n")) {
        return false;
    }
    if (!mmay_native_c_emit_defer_cleanup(
            buffer, function, functions, count
        )) {
        return false;
    }
    if (managed_count != 0) {
        bool inline_root_locations = managed_count <= 64;
        if (release_rootless && !mmay_native_c_write(
                buffer, "#if MMAY_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
            )) return false;
        if (release_rootless) {
            if (!mmay_native_c_write(
                    buffer,
                    "  if (mmay_g_root_linked && mmay_managed_root_pop(&mmay_g_x->heap, &mmay_g_root) != MMAY_MANAGED_OK && mmay_g_s == 0) mmay_g_s = 7;\n"
                )) return false;
        } else if (!mmay_native_c_write(
                buffer,
                "#if MMAY_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
                "  if (mmay_g_root_linked && mmay_managed_root_pop(&mmay_g_x->heap, &mmay_g_root) != MMAY_MANAGED_OK && mmay_g_s == 0) mmay_g_s = 7;\n"
                "#else\n"
                "  if (mmay_g_root_linked && !mmay_managed_root_pop_generated(&mmay_g_x->heap, &mmay_g_root) && mmay_g_s == 0) mmay_g_s = 7;\n"
                "#endif\n"
            )) return false;
        if (inline_root_locations) {
            if (!mmay_native_c_write(
                    buffer,
                    "#if MMAY_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
                    "  mmay_deallocate(&mmay_g_x->allocator, mmay_g_locations, mmay_g_location_bytes);\n"
                    "#endif\n"
                )) return false;
        } else if (!mmay_native_c_write(
                buffer,
                "  mmay_deallocate(&mmay_g_x->allocator, mmay_g_locations, mmay_g_location_bytes);\n"
            )) return false;
        if (release_rootless && !mmay_native_c_write(buffer, "#endif\n")) {
            return false;
        }
        if (!mmay_native_c_write(
                buffer,
                "  if (mmay_g_nominal_cells != NULL) mmay_deallocate(&mmay_g_x->allocator, mmay_g_nominal_cells, mmay_g_nominal_cell_bytes);\n"
            )) return false;
    }
    return mmay_native_c_write(buffer, "  return mmay_g_s;\n}\n\n");
}
