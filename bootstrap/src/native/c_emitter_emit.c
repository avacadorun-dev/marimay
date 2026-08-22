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

static const char *ruda_native_c_integer_operation(RudaMirInstructionKind kind) {
    switch (kind) {
        case RUDA_MIR_ADD_CHECKED_I64: return "ruda_checked_integer_add";
        case RUDA_MIR_SUBTRACT_CHECKED_I64:
            return "ruda_checked_integer_subtract";
        case RUDA_MIR_MULTIPLY_CHECKED_I64:
            return "ruda_checked_integer_multiply";
        case RUDA_MIR_DIVIDE_CHECKED_I64:
            return "ruda_checked_integer_divide";
        case RUDA_MIR_REMAINDER_CHECKED_I64:
            return "ruda_checked_integer_remainder";
        case RUDA_MIR_SHIFT_LEFT_CHECKED_I64:
            return "ruda_checked_integer_shift_left";
        case RUDA_MIR_SHIFT_RIGHT_I64:
            return "ruda_checked_integer_shift_right";
        case RUDA_MIR_NEGATE_CHECKED_I64:
            return "ruda_checked_integer_negate";
        default: return NULL;
    }
}

static bool ruda_native_c_emit_generic_integer(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    RudaTypeId type = ruda_native_c_register_type(function, instruction->target);
    const char *kind = ruda_native_c_integer_kind_name(type);
    const char *convert = ruda_native_c_integer_conversion_name(type);
    const char *operation = ruda_native_c_integer_operation(instruction->kind);
    if (kind == NULL || convert == NULL || operation == NULL) {
        return false;
    }
    if (instruction->kind == RUDA_MIR_NEGATE_CHECKED_I64) {
        return ruda_native_c_write(
            buffer,
            "  { RudaIntegerValue ruda_g_left = {%s, (uint64_t)ruda_g_v_%u}; RudaIntegerValue ruda_g_out_value = {%s, 0}; ruda_g_s = (int)%s(ruda_g_left, &ruda_g_out_value); if (ruda_g_s == 0 && !%s(ruda_g_out_value, &ruda_g_v_%u)) ruda_g_s = 1; if (ruda_g_s != 0) { *ruda_g_start = %zu; *ruda_g_end = %zu; goto ruda_g_cleanup; } }\n",
            kind, instruction->first, kind, operation, convert,
            instruction->target, instruction->span.start, instruction->span.end
        );
    }
    return ruda_native_c_write(
        buffer,
        "  { RudaIntegerValue ruda_g_left = {%s, (uint64_t)ruda_g_v_%u}; RudaIntegerValue ruda_g_right = {%s, (uint64_t)ruda_g_v_%u}; RudaIntegerValue ruda_g_out_value = {%s, 0}; ruda_g_s = (int)%s(ruda_g_left, ruda_g_right, &ruda_g_out_value); if (ruda_g_s == 0 && !%s(ruda_g_out_value, &ruda_g_v_%u)) ruda_g_s = 1; if (ruda_g_s != 0) { *ruda_g_start = %zu; *ruda_g_end = %zu; goto ruda_g_cleanup; } }\n",
        kind, instruction->first, kind, instruction->second, kind, operation,
        convert, instruction->target, instruction->span.start,
        instruction->span.end
    );
}

static bool ruda_native_c_emit_generic_bitwise(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    const char *operator_text
) {
    RudaTypeId type = ruda_native_c_register_type(function, instruction->target);
    const char *kind = ruda_native_c_integer_kind_name(type);
    const char *convert = ruda_native_c_integer_conversion_name(type);
    if (kind == NULL || convert == NULL || operator_text == NULL) {
        return false;
    }
    if (instruction->second == RUDA_MIR_NO_REGISTER) {
        return ruda_native_c_write(
            buffer,
            "  { RudaIntegerValue ruda_g_left = {%s, (uint64_t)ruda_g_v_%u}; RudaIntegerValue ruda_g_out_value = {%s, 0}; ruda_g_out_value.bits = (%s ruda_g_left.bits) & ((ruda_integer_bit_width(%s) == 64U) ? UINT64_MAX : (UINT64_C(1) << ruda_integer_bit_width(%s)) - UINT64_C(1)); if (!%s(ruda_g_out_value, &ruda_g_v_%u)) { ruda_g_s = 1; goto ruda_g_cleanup; } }\n",
            kind, instruction->first, kind, operator_text, kind, kind,
            convert, instruction->target
        );
    }
    return ruda_native_c_write(
        buffer,
        "  { RudaIntegerValue ruda_g_left = {%s, (uint64_t)ruda_g_v_%u}; RudaIntegerValue ruda_g_right = {%s, (uint64_t)ruda_g_v_%u}; RudaIntegerValue ruda_g_out_value = {%s, 0}; ruda_g_out_value.bits = (ruda_g_left.bits %s ruda_g_right.bits) & ((ruda_integer_bit_width(%s) == 64U) ? UINT64_MAX : (UINT64_C(1) << ruda_integer_bit_width(%s)) - UINT64_C(1)); if (!%s(ruda_g_out_value, &ruda_g_v_%u)) { ruda_g_s = 1; goto ruda_g_cleanup; } }\n",
        kind, instruction->first, kind, instruction->second, kind,
        operator_text, kind, kind, convert, instruction->target
    );
}

bool ruda_native_c_emit_signature(
    RudaNativeCBuffer *buffer, const RudaMirFunction *function, bool prototype
) {
    size_t index = 0;
    if (!ruda_native_c_write(buffer, "static int ") || !ruda_native_c_symbol(buffer, function->symbol)
        || !ruda_native_c_write(buffer, "(ruda_g_execution *ruda_g_x, ruda_g_panic *ruda_g_p")) return false;
    for (index = 0; index < function->parameter_count; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(&function->register_types, index);
        if (!ruda_native_c_write(buffer, ", %s ruda_g_v_%zu", ruda_native_c_type(function, *type), index)) return false;
    }
    if (function->result_type != RUDA_TYPE_VOID
        && function->result_type != RUDA_TYPE_NEVER
        && !ruda_native_c_write(buffer, ", %s *ruda_g_out", ruda_native_c_type(function, function->result_type))) return false;
    return ruda_native_c_write(buffer,
        ", size_t *ruda_g_start, size_t *ruda_g_end)%s", prototype ? ";\n" : " {\n"
    );
}

bool ruda_native_c_emit_call_arguments(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirCall *call
) {
    size_t argument_index = 0;
    for (argument_index = 0;
        argument_index < call->argument_count;
        argument_index += 1) {
        const RudaMirCallArgument *argument = ruda_native_c_call_argument(
            function,
            call->argument_start + argument_index
        );
        if (!ruda_native_c_write(
                buffer,
                ", ruda_g_v_%u",
                argument->register_id
            )) {
            return false;
        }
    }
    return true;
}

bool ruda_native_c_emit_propagate_call(
    RudaNativeCBuffer *buffer,
    RudaSymbolId symbol,
    RudaMirSpan span,
    bool returns_never
) {
    if (!ruda_native_c_write(buffer, "  if (ruda_g_s == 4) {\n    if (!ruda_g_append_frame(ruda_g_p, ")
        || !ruda_native_c_symbol_data(buffer, symbol)
        || !ruda_native_c_write(
            buffer,
            ", %zu, %zu)) ruda_g_s = 5;\n    goto ruda_g_cleanup;\n  }\n",
            span.start,
            span.end
        )) {
        return false;
    }
    if (returns_never) {
        return ruda_native_c_write(
            buffer,
            "  if (ruda_g_s == 0) { *ruda_g_start = %zu; *ruda_g_end = %zu; ruda_g_s = 5; }\n  goto ruda_g_cleanup;\n",
            span.start,
            span.end
        );
    }
    return ruda_native_c_write(
        buffer,
        "  if (ruda_g_s != 0) goto ruda_g_cleanup;\n"
    );
}

bool ruda_native_c_emit_function(
    RudaNativeCBuffer *buffer, const RudaMirFunction *function,
    const RudaMirFunction *const *functions, size_t count
) {
    size_t index = 0;
    size_t managed_count = ruda_native_c_managed_register_count(function);
    if (!ruda_native_c_emit_signature(buffer, function, false)
        || !ruda_native_c_write(
            buffer,
            "  int ruda_g_s = 0;\n  (void)ruda_g_p;\n  (void)ruda_g_x;\n"
        )) return false;
    if (function->calls.length != 0 && !ruda_native_c_write(
            buffer,
            "  uint32_t ruda_g_defers[%zu] = {0};\n  size_t ruda_g_defer_count = 0;\n  ruda_g_frame ruda_g_cleanup_frames[RUDA_G_MAX_FRAMES];\n  ruda_g_suppressed_panic ruda_g_cleanup_suppressed[RUDA_G_MAX_SUPPRESSED];\n  ruda_g_panic ruda_g_cleanup_p = {0};\n  ruda_g_cleanup_p.frames = ruda_g_cleanup_frames;\n  ruda_g_cleanup_p.frame_capacity = RUDA_G_MAX_FRAMES;\n  ruda_g_cleanup_p.suppressed = ruda_g_cleanup_suppressed;\n  ruda_g_cleanup_p.suppressed_capacity = RUDA_G_MAX_SUPPRESSED;\n  (void)ruda_g_defers;\n  (void)ruda_g_defer_count;\n  (void)ruda_g_cleanup_p;\n",
            function->calls.length
        )) {
        return false;
    }
    if (!ruda_native_c_write(
            buffer, "  (void)ruda_g_start;\n  (void)ruda_g_end;\n"
        )) return false;
    if (function->result_type != RUDA_TYPE_VOID
        && function->result_type != RUDA_TYPE_NEVER
        && !ruda_native_c_write(buffer, "  (void)ruda_g_out;\n")) {
        return false;
    }
    for (index = 0; index < function->parameter_count; index += 1) {
        if (!ruda_native_c_write(buffer, "  (void)ruda_g_v_%zu;\n", index)) {
            return false;
        }
    }
    for (index = function->parameter_count; index < function->register_types.length; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(&function->register_types, index);
        if (type != NULL && *type == RUDA_TYPE_VOID) {
            continue;
        }
        if (!ruda_native_c_write(
                buffer,
                *type >= RUDA_TYPE_FIRST_DYNAMIC
                    ? "  %s ruda_g_v_%zu = ruda_managed_invalid_reference();\n"
                    : (*type == RUDA_TYPE_STRING
                        ? "  %s ruda_g_v_%zu = {0};\n"
                        : (*type == RUDA_TYPE_BOOL
                            ? "  %s ruda_g_v_%zu = false;\n"
                            : "  %s ruda_g_v_%zu = 0;\n")),
                ruda_native_c_type(function, *type),
                index
            )) return false;
        if (*type == RUDA_TYPE_STRING && !ruda_native_c_write(
                buffer,
                "  ruda_g_v_%zu.managed = ruda_managed_invalid_reference();\n",
                index
            )) return false;
        if (!ruda_native_c_write(buffer, "  (void)ruda_g_v_%zu;\n", index)) {
            return false;
        }
    }
    if (managed_count != 0) {
        size_t managed_index = 0;
        if (!ruda_native_c_write(
                buffer,
                "  RudaManagedStatus ruda_g_m;\n"
                "  RudaManagedRootFrame ruda_g_root = {0};\n"
                "  RudaManagedReference **ruda_g_locations = NULL;\n"
                "  size_t ruda_g_location_bytes = 0;\n"
                "  RudaManagedCell *ruda_g_nominal_cells = NULL;\n"
                "  size_t ruda_g_nominal_cell_bytes = 0;\n"
                "  bool ruda_g_root_linked = false;\n"
                "  if (!ruda_size_multiply(%zu, sizeof(*ruda_g_locations), &ruda_g_location_bytes)) { ruda_g_s = 6; goto ruda_g_cleanup; }\n"
                "  ruda_g_locations = ruda_allocate(&ruda_g_x->allocator, ruda_g_location_bytes);\n"
                "  if (ruda_g_locations == NULL) { ruda_g_s = 5; goto ruda_g_cleanup; }\n",
                managed_count
            )) return false;
        for (index = 0; index < function->register_types.length; index += 1) {
            const RudaTypeId *type = ruda_vector_at_const(
                &function->register_types,
                index
            );
            if (type != NULL
                && (*type == RUDA_TYPE_STRING
                    || *type >= RUDA_TYPE_FIRST_DYNAMIC)) {
                if (!ruda_native_c_write(
                        buffer,
                        *type == RUDA_TYPE_STRING
                            ? "  ruda_g_locations[%zu] = &ruda_g_v_%zu.managed;\n"
                            : "  ruda_g_locations[%zu] = &ruda_g_v_%zu;\n",
                        managed_index,
                        index
                    )) return false;
                managed_index += 1;
            }
        }
        if (!ruda_native_c_write(
                buffer,
                "  ruda_g_m = ruda_managed_root_push(&ruda_g_x->heap, &ruda_g_root, ruda_g_locations, %zu);\n"
                "  if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; }\n"
                "  ruda_g_root_linked = true;\n",
                managed_count
            )) return false;
        for (index = 0; index < function->parameter_count; index += 1) {
            const RudaTypeId *type = ruda_vector_at_const(
                &function->register_types,
                index
            );
            if (type != NULL && *type >= RUDA_TYPE_FIRST_DYNAMIC
                && !ruda_native_c_emit_managed_parameter_check(
                    buffer, function, functions, count, index
                )) return false;
        }
    }
    if (!ruda_native_c_write(buffer, "  goto ruda_g_b_0;\n")) return false;
    for (index = 0; index < function->blocks.length; index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(&function->blocks, index);
        size_t instruction_index = 0;
        if (!ruda_native_c_write(buffer, "ruda_g_b_%zu:\n", index)) return false;
        for (instruction_index = 0; instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *i = ruda_vector_at_const(&block->instructions, instruction_index);
            const char *helper = NULL;
            switch (i->kind) {
                case RUDA_MIR_CONSTANT_I64:
                    if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_i64_from_twos_complement(UINT64_C(0x%016llx));\n", i->target, (unsigned long long)(uint64_t)i->integer)) return false;
                    break;
                case RUDA_MIR_CONSTANT_INTEGER: {
                    RudaTypeId type = ruda_native_c_register_type(
                        function, i->target
                    );
                    const char *kind = ruda_native_c_integer_kind_name(type);
                    const char *convert = ruda_native_c_integer_conversion_name(type);
                    if (kind == NULL || convert == NULL) return false;
                    if (!ruda_native_c_write(
                            buffer,
                            "  { RudaIntegerValue ruda_g_integer_value = {%s, UINT64_C(0x%016llx)}; if (!%s(ruda_g_integer_value, &ruda_g_v_%u)) { ruda_g_s = 1; *ruda_g_start = %zu; *ruda_g_end = %zu; goto ruda_g_cleanup; } }\n",
                            kind,
                            (unsigned long long)i->integer_bits,
                            convert,
                            i->target,
                            i->span.start,
                            i->span.end
                        )) return false;
                    break;
                }
                case RUDA_MIR_CONSTANT_F64:
                    if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = UINT64_C(0x%016llx);\n", i->target, (unsigned long long)i->f64_bits)) return false;
                    break;
                case RUDA_MIR_CONSTANT_BOOL: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = %s;\n", i->target, i->boolean ? "true" : "false")) return false; break;
                case RUDA_MIR_CONSTANT_STRING: {
                    const RudaMirStringConstant *constant = ruda_vector_at_const(
                        &function->string_constants,
                        i->string_constant
                    );
                    if (constant == NULL
                        || !ruda_native_c_write(
                            buffer,
                            "  ruda_g_v_%u.data = ",
                            i->target
                        )
                        || !ruda_native_c_string_data(
                            buffer,
                            function->symbol,
                            i->string_constant
                        )
                        || !ruda_native_c_write(
                            buffer,
                            "; ruda_g_v_%u.length = %zu; ruda_g_v_%u.managed = ruda_managed_invalid_reference();\n",
                            i->target,
                            constant->value.length,
                            i->target
                        )) {
                        return false;
                    }
                    break;
                }
                case RUDA_MIR_COPY: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_g_v_%u;\n", i->target, i->first)) return false; break;
                case RUDA_MIR_NOT_BOOL: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = !ruda_g_v_%u;\n", i->target, i->first)) return false; break;
                case RUDA_MIR_EQUAL_I64: case RUDA_MIR_EQUAL_BOOL: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_g_v_%u == ruda_g_v_%u;\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_EQUAL_STRING: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_g_string_equal(ruda_g_v_%u, ruda_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_LESS_I64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_g_v_%u < ruda_g_v_%u;\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_LESS_EQUAL_I64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_g_v_%u <= ruda_g_v_%u;\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_EQUAL_F64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_f64_equal(ruda_g_v_%u, ruda_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_LESS_F64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_f64_less(ruda_g_v_%u, ruda_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_LESS_EQUAL_F64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_f64_less_equal(ruda_g_v_%u, ruda_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_ADD_F64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_f64_add(ruda_g_v_%u, ruda_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_SUBTRACT_F64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_f64_subtract(ruda_g_v_%u, ruda_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_MULTIPLY_F64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_f64_multiply(ruda_g_v_%u, ruda_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_DIVIDE_F64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_f64_divide(ruda_g_v_%u, ruda_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_NEGATE_F64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_f64_negate(ruda_g_v_%u);\n", i->target, i->first)) return false; break;
                case RUDA_MIR_ARRAY_NEW: {
                    const RudaMirArrayConstruction *construction =
                        ruda_native_c_array_construction(
                            function,
                            i->array_construction
                        );
                    const RudaMirArray *array = construction != NULL
                        ? ruda_vector_at_const(
                            &function->arrays,
                            construction->array
                        )
                        : NULL;
                    size_t argument_index = 0;
                    if (construction == NULL || array == NULL
                        || !ruda_native_c_write(
                            buffer,
                            "  { RudaManagedCell *ruda_g_cells = NULL; size_t ruda_g_cell_bytes = 0;\n"
                            "    if (!ruda_size_multiply(%zu, sizeof(*ruda_g_cells), &ruda_g_cell_bytes)) { ruda_g_s = 6; goto ruda_g_cleanup; }\n"
                            "    if (ruda_g_cell_bytes != 0) { ruda_g_cells = ruda_allocate(&ruda_g_x->allocator, ruda_g_cell_bytes); if (ruda_g_cells == NULL) { ruda_g_s = 5; goto ruda_g_cleanup; } }\n",
                            construction->argument_count
                        )) return false;
                    for (argument_index = 0;
                        argument_index < construction->argument_count;
                        argument_index += 1) {
                        const RudaMirArrayArgument *argument =
                            ruda_native_c_array_argument(
                                function,
                                construction->argument_start + argument_index
                            );
                        if (argument == NULL || !ruda_native_c_write(
                                buffer,
                                "    { RudaManagedCell ruda_g_cell; memset(&ruda_g_cell, 0, sizeof(ruda_g_cell)); "
                            ) || !ruda_native_c_write_to_managed_cell(
                                buffer,
                                array->element_type,
                                argument->register_id
                            ) || !ruda_native_c_write(
                                buffer,
                                "; ruda_g_cells[%zu] = ruda_g_cell; }\n",
                                argument_index
                            )) return false;
                    }
                    if (!ruda_native_c_write(
                            buffer,
                            "    ruda_g_m = ruda_managed_array_new(&ruda_g_x->heap, %s, ruda_g_cells, %zu, &ruda_g_v_%u);\n"
                            "    ruda_deallocate(&ruda_g_x->allocator, ruda_g_cells, ruda_g_cell_bytes);\n"
                            "    if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; }\n"
                            "  }\n",
                            ruda_native_c_array_descriptor(function, array->type),
                            construction->argument_count,
                            i->target
                        )) return false;
                    break;
                }
                case RUDA_MIR_ARRAY_PUSH: {
                    const RudaMirArray *array = ruda_vector_at_const(
                        &function->arrays, i->array
                    );
                    if (array == NULL || !ruda_native_c_write(
                            buffer,
                            "  { RudaManagedCell ruda_g_cell; memset(&ruda_g_cell, 0, sizeof(ruda_g_cell)); "
                        ) || !ruda_native_c_write_to_managed_cell(
                            buffer, array->element_type, i->second
                        ) || !ruda_native_c_write(
                            buffer,
                            "; ruda_g_m = ruda_managed_array_push(&ruda_g_x->heap, ruda_g_v_%u, ruda_g_cell); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ruda_g_v_%u = ruda_g_v_%u; }\n",
                            i->first, i->target, i->first
                        )) return false;
                    break;
                }
                case RUDA_MIR_ARRAY_POP:
                case RUDA_MIR_ARRAY_TRUNCATE:
                case RUDA_MIR_ARRAY_CLEAR:
                case RUDA_MIR_ARRAY_RESERVE:
                case RUDA_MIR_ARRAY_GENERATION:
                case RUDA_MIR_ARRAY_REQUIRE_GENERATION:
                    if (!ruda_native_c_emit_array_mutation_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case RUDA_MIR_ARRAY_LENGTH:
                    if (!ruda_native_c_write(
                            buffer,
                            "  { size_t ruda_g_length = 0; const RudaManagedArrayDescriptor *ruda_g_actual = NULL; ruda_g_m = ruda_managed_array_describe(&ruda_g_x->heap, ruda_g_v_%u, &ruda_g_actual); if (ruda_g_m == RUDA_MANAGED_OK && ruda_g_actual != %s) ruda_g_m = RUDA_MANAGED_TYPE_MISMATCH; if (ruda_g_m == RUDA_MANAGED_OK) ruda_g_m = ruda_managed_array_length(&ruda_g_x->heap, ruda_g_v_%u, &ruda_g_length); if (ruda_g_m != RUDA_MANAGED_OK || ruda_g_length > (size_t)INT64_MAX) { ruda_g_s = ruda_g_m == RUDA_MANAGED_OK ? 6 : ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ruda_g_v_%u = (int64_t)ruda_g_length; }\n",
                            i->first,
                            ruda_native_c_array_descriptor(
                                function,
                                ((const RudaMirArray *)ruda_vector_at_const(
                                    &function->arrays,
                                    i->array
                                ))->type
                            ),
                            i->first,
                            i->target
                        )) return false;
                    break;
                case RUDA_MIR_ARRAY_REQUIRE_INDEX:
                    if (!ruda_native_c_write(
                            buffer,
                            "  { size_t ruda_g_length = 0; ruda_g_m = ruda_g_array_check(&ruda_g_x->heap, ruda_g_v_%u, %s, ruda_g_v_%u, &ruda_g_length); if (ruda_g_m == RUDA_MANAGED_BOUNDS) { ruda_g_s = ruda_g_begin_bounds(ruda_g_p, ruda_g_v_%u, ruda_g_length, ",
                            i->first,
                            ruda_native_c_array_descriptor(
                                function,
                                ((const RudaMirArray *)ruda_vector_at_const(
                                    &function->arrays,
                                    i->array
                                ))->type
                            ),
                            i->second,
                            i->second
                        ) || !ruda_native_c_symbol_data(buffer, function->symbol)
                        || !ruda_native_c_write(
                            buffer,
                            ", %zu, %zu); goto ruda_g_cleanup; } if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ruda_g_v_%u = ruda_g_v_%u; }\n",
                            i->span.start,
                            i->span.end,
                            i->target,
                            i->first
                        )) return false;
                    break;
                case RUDA_MIR_ARRAY_ASSUME_INDEX:
                    if (!ruda_native_c_write(
                            buffer,
                            "  ruda_g_v_%u = ruda_g_v_%u;\n",
                            i->target, i->first
                        )) return false;
                    break;
                case RUDA_MIR_ARRAY_GET: {
                    const RudaMirArray *array = ruda_vector_at_const(
                        &function->arrays,
                        i->array
                    );
                    if (array == NULL || !ruda_native_c_write(
                            buffer,
                            "  { RudaManagedCell ruda_g_cell; size_t ruda_g_length = 0; ruda_g_m = ruda_g_array_check(&ruda_g_x->heap, ruda_g_v_%u, %s, ruda_g_v_%u, &ruda_g_length); if (ruda_g_m == RUDA_MANAGED_OK) ruda_g_m = ruda_managed_array_get(&ruda_g_x->heap, ruda_g_v_%u, ruda_g_v_%u, &ruda_g_cell, &ruda_g_length); if (ruda_g_m == RUDA_MANAGED_BOUNDS) { ruda_g_s = ruda_g_begin_bounds(ruda_g_p, ruda_g_v_%u, ruda_g_length, ",
                            i->first,
                            ruda_native_c_array_descriptor(function, array->type),
                            i->second,
                            i->first,
                            i->second,
                            i->second
                        ) || !ruda_native_c_symbol_data(buffer, function->symbol)
                        || !ruda_native_c_write(
                            buffer,
                            ", %zu, %zu); goto ruda_g_cleanup; } if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ",
                            i->span.start,
                            i->span.end
                        ) || !ruda_native_c_write_from_managed_cell(
                            buffer, array->element_type, i->target
                        ) || !ruda_native_c_write(buffer, "; }\n")) return false;
                    break;
                }
                case RUDA_MIR_ARRAY_SET: {
                    const RudaMirArray *array = ruda_vector_at_const(
                        &function->arrays,
                        i->array
                    );
                    if (array == NULL || !ruda_native_c_write(
                            buffer,
                            "  { RudaManagedCell ruda_g_cell; size_t ruda_g_length = 0; memset(&ruda_g_cell, 0, sizeof(ruda_g_cell)); "
                        ) || !ruda_native_c_write_to_managed_cell(
                            buffer, array->element_type, i->third
                        ) || !ruda_native_c_write(
                            buffer,
                            "; ruda_g_m = ruda_g_array_check(&ruda_g_x->heap, ruda_g_v_%u, %s, ruda_g_v_%u, &ruda_g_length); if (ruda_g_m == RUDA_MANAGED_OK) ruda_g_m = ruda_managed_array_set(&ruda_g_x->heap, ruda_g_v_%u, ruda_g_v_%u, ruda_g_cell, &ruda_g_length); if (ruda_g_m == RUDA_MANAGED_BOUNDS) { ruda_g_s = ruda_g_begin_bounds(ruda_g_p, ruda_g_v_%u, ruda_g_length, ",
                            i->first,
                            ruda_native_c_array_descriptor(function, array->type),
                            i->second,
                            i->first,
                            i->second,
                            i->second
                        ) || !ruda_native_c_symbol_data(buffer, function->symbol)
                        || !ruda_native_c_write(
                            buffer,
                            ", %zu, %zu); goto ruda_g_cleanup; } if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ruda_g_v_%u = ruda_g_v_%u; }\n",
                            i->span.start,
                            i->span.end,
                            i->target,
                            i->third
                        )) return false;
                    break;
                }
                case RUDA_MIR_ARRAY_GET_IN_BOUNDS:
                case RUDA_MIR_ARRAY_SET_IN_BOUNDS:
                    if (!ruda_native_c_emit_range_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case RUDA_MIR_BITWISE_NOT_I64:
                    if (ruda_native_c_register_type(function, i->target)
                            != RUDA_TYPE_I64) {
                        if (!ruda_native_c_emit_generic_bitwise(
                                buffer, function, i, "~"
                            )) return false;
                    } else if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_i64_bitwise_not(ruda_g_v_%u);\n", i->target, i->first)) return false;
                    break;
                case RUDA_MIR_BITWISE_AND_I64: case RUDA_MIR_BITWISE_XOR_I64: case RUDA_MIR_BITWISE_OR_I64: {
                    const char *name = i->kind == RUDA_MIR_BITWISE_AND_I64 ? "and" : i->kind == RUDA_MIR_BITWISE_XOR_I64 ? "xor" : "or";
                    const char *operator_text = i->kind == RUDA_MIR_BITWISE_AND_I64
                        ? "&" : i->kind == RUDA_MIR_BITWISE_XOR_I64 ? "^" : "|";
                    if (ruda_native_c_register_type(function, i->target)
                            != RUDA_TYPE_I64) {
                        if (!ruda_native_c_emit_generic_bitwise(
                                buffer, function, i, operator_text
                            )) return false;
                    } else if (!ruda_native_c_write(
                            buffer,
                            "  ruda_g_v_%u = ruda_i64_bitwise_%s(ruda_g_v_%u, ruda_g_v_%u);\n",
                            i->target,
                            name,
                            i->first,
                            i->second
                        )) {
                        return false;
                    }
                    break;
                }
                case RUDA_MIR_ADD_CHECKED_I64: helper = "ruda_checked_i64_add"; break;
                case RUDA_MIR_SUBTRACT_CHECKED_I64: helper = "ruda_checked_i64_subtract"; break;
                case RUDA_MIR_MULTIPLY_CHECKED_I64: helper = "ruda_checked_i64_multiply"; break;
                case RUDA_MIR_DIVIDE_CHECKED_I64: helper = "ruda_checked_i64_divide"; break;
                case RUDA_MIR_REMAINDER_CHECKED_I64: helper = "ruda_checked_i64_remainder"; break;
                case RUDA_MIR_SHIFT_LEFT_CHECKED_I64: helper = "ruda_checked_i64_shift_left"; break;
                case RUDA_MIR_SHIFT_RIGHT_I64: helper = "ruda_checked_i64_shift_right"; break;
                case RUDA_MIR_NEGATE_CHECKED_I64: helper = "ruda_checked_i64_negate"; break;
                case RUDA_MIR_CALL: {
                    const RudaMirCall *call = ruda_native_c_call(function, i->call);
                    const RudaMirFunction *callee = ruda_native_c_find(functions, count, call->callee);
                    if (!ruda_native_c_write(buffer, "  ruda_g_s = ")
                        || !ruda_native_c_symbol(buffer, callee->symbol)
                        || !ruda_native_c_write(buffer, "(ruda_g_x, ruda_g_p")
                        || !ruda_native_c_emit_call_arguments(buffer, function, call)
                        || !(call->result_type == RUDA_TYPE_VOID
                            ? ruda_native_c_write(
                                buffer,
                                ", ruda_g_start, ruda_g_end);\n"
                            )
                            : ruda_native_c_write(
                                buffer,
                                ", &ruda_g_v_%u, ruda_g_start, ruda_g_end);\n",
                                i->target
                            ))
                        || !ruda_native_c_emit_propagate_call(
                            buffer,
                            function->symbol,
                            i->span,
                            false
                        )) return false;
                    break;
                }
                case RUDA_MIR_RECORD_NEW: case RUDA_MIR_RECORD_GET:
                case RUDA_MIR_RECORD_SET:
                case RUDA_MIR_RECORD_IDENTITY_SAME:
                case RUDA_MIR_ENUM_NEW: case RUDA_MIR_ENUM_IS_VARIANT:
                case RUDA_MIR_ENUM_REQUIRE_VARIANT:
                case RUDA_MIR_ENUM_GET_PAYLOAD:
                case RUDA_MIR_INTERFACE_NEW:
                case RUDA_MIR_CALL_INTERFACE:
                    if (!ruda_native_c_emit_managed_instruction(
                            buffer, function, functions, count, i
                        )) return false;
                    break;
                case RUDA_MIR_INTEGER_POLICY:
                case RUDA_MIR_INTEGER_CONVERT:
                    if (!ruda_native_c_emit_numeric_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case RUDA_MIR_FORMAT_F32:
                case RUDA_MIR_FORMAT_F64:
                    if (!ruda_native_c_emit_format_instruction(buffer, i)) {
                        return false;
                    }
                    break;
                case RUDA_MIR_IO_PRINTLN:
                    if (!ruda_native_c_emit_io_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case RUDA_MIR_TEST_OUTPUT_BEGIN:
                case RUDA_MIR_TEST_OUTPUT_END:
                    if (!ruda_native_c_emit_test_output_instruction(
                            buffer, function, i
                        )) return false;
                    break;
                case RUDA_MIR_DEFER_REGISTER:
                case RUDA_MIR_DEFER_CALL:
                    if (!ruda_native_c_emit_defer_instruction(
                            buffer, function, functions, count, i
                        )) {
                        return false;
                    }
                    break;
                default: return false;
            }
            if (helper != NULL
                && ruda_native_c_register_type(function, i->target)
                    != RUDA_TYPE_I64) {
                if (!ruda_native_c_emit_generic_integer(buffer, function, i)) {
                    return false;
                }
                helper = NULL;
            }
            if (helper != NULL) {
                if (i->kind == RUDA_MIR_NEGATE_CHECKED_I64) {
                    if (!ruda_native_c_write(buffer, "  ruda_g_s = (int)%s(ruda_g_v_%u, &ruda_g_v_%u);\n", helper, i->first, i->target)) return false;
                } else if (!ruda_native_c_write(buffer, "  ruda_g_s = (int)%s(ruda_g_v_%u, ruda_g_v_%u, &ruda_g_v_%u);\n", helper, i->first, i->second, i->target)) return false;
                if (!ruda_native_c_write(buffer, "  if (ruda_g_s != 0) { *ruda_g_start = %zu; *ruda_g_end = %zu; goto ruda_g_cleanup; }\n", i->span.start, i->span.end)) return false;
            }
        }
        if (block->terminator.kind == RUDA_MIR_GOTO) {
            if (!ruda_native_c_write(buffer, "  goto ruda_g_b_%u;\n", block->terminator.target)) return false;
        } else if (block->terminator.kind == RUDA_MIR_BRANCH) {
            if (!ruda_native_c_write(buffer, "  if (ruda_g_v_%u) goto ruda_g_b_%u; else goto ruda_g_b_%u;\n", block->terminator.value, block->terminator.target, block->terminator.otherwise)) return false;
        } else if (block->terminator.kind == RUDA_MIR_PANIC) {
            if (!ruda_native_c_write(
                    buffer,
                    "  ruda_g_s = ruda_g_begin_panic(ruda_g_p, ruda_g_v_%u, ",
                    block->terminator.value
                )
                || !ruda_native_c_symbol_data(buffer, function->symbol)
                || !ruda_native_c_write(
                    buffer,
                    ", %zu, %zu); goto ruda_g_cleanup;\n",
                    block->terminator.span.start,
                    block->terminator.span.end
                )) return false;
        } else if (block->terminator.kind == RUDA_MIR_CALL_NEVER) {
            const RudaMirCall *call = ruda_native_c_call(
                function,
                block->terminator.call
            );
            const RudaMirFunction *callee = ruda_native_c_find(
                functions,
                count,
                call->callee
            );
            if (!ruda_native_c_write(buffer, "  ruda_g_s = ")
                || !ruda_native_c_symbol(buffer, callee->symbol)
                || !ruda_native_c_write(buffer, "(ruda_g_x, ruda_g_p")
                || !ruda_native_c_emit_call_arguments(buffer, function, call)
                || !ruda_native_c_write(
                    buffer,
                    ", ruda_g_start, ruda_g_end);\n"
                )
                || !ruda_native_c_emit_propagate_call(
                    buffer,
                    function->symbol,
                    block->terminator.span,
                    true
                )) return false;
        } else if (function->result_type == RUDA_TYPE_VOID) {
            if (!ruda_native_c_write(buffer, "  ruda_g_s = 0; goto ruda_g_cleanup;\n")) return false;
        } else if (!ruda_native_c_write(buffer, "  *ruda_g_out = ruda_g_v_%u; ruda_g_s = 0; goto ruda_g_cleanup;\n", block->terminator.value)) return false;
    }
    if (!ruda_native_c_write(buffer, "ruda_g_cleanup:\n")) {
        return false;
    }
    if (!ruda_native_c_emit_defer_cleanup(
            buffer, function, functions, count
        )) {
        return false;
    }
    if (managed_count != 0 && !ruda_native_c_write(
            buffer,
            "  if (ruda_g_root_linked && ruda_managed_root_pop(&ruda_g_x->heap, &ruda_g_root) != RUDA_MANAGED_OK && ruda_g_s == 0) ruda_g_s = 7;\n"
            "  ruda_deallocate(&ruda_g_x->allocator, ruda_g_nominal_cells, ruda_g_nominal_cell_bytes);\n"
            "  ruda_deallocate(&ruda_g_x->allocator, ruda_g_locations, ruda_g_location_bytes);\n"
        )) {
        return false;
    }
    return ruda_native_c_write(buffer, "  return ruda_g_s;\n}\n\n");
}
