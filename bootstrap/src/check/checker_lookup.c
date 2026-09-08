#include "checker_closure_api.h"
#include "checker_function_values_api.h"
#include "checker_lookup_api.h"
#include "checker_array_types_api.h"
#include "checker_collection_types_api.h"
#include "checker_cfg_api.h"
#include "checker_compound_types_api.h"
#include "checker_core_api.h"
#include "checker_expr_records_api.h"
#include "checker_expr_tuples_api.h"
#include "checker_generic_types_api.h"
#include "package_imports_api.h"






 bool mmay_checker_private_check_exact_type(
    MarimayChecker *checker,
    const MarimayAstNode *node,
    MarimayTypeId *result
) {
    const MarimaySyntaxToken *name = NULL;
    MarimayCheckName declared_name;
    MarimayCheckTypeAliasDeclaration *alias = NULL;
    MarimayCheckRecordDeclaration *record = NULL;
    MarimayCheckEnumDeclaration *enum_declaration = NULL;
    MarimayCheckInterfaceDeclaration *interface_declaration = NULL;
    const MarimayAstNode *type_arguments = NULL;
    bool import_resolved = false;
    const MarimayCheckTypeParameterDeclaration *type_parameter = NULL;
    MarimayTypeId active_type_parameter = MMAY_TYPE_INVALID;
    MarimayVector generic_arguments;
    bool generic_arguments_initialized = false;
    bool array_type_resolved = false;
    bool collection_type_resolved = false;
    bool compound_type_resolved = false;
    memset(&generic_arguments, 0, sizeof(generic_arguments));
    if (node == NULL || result == NULL || node->kind != MMAY_SYNTAX_TYPE) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (!mmay_checker_private_resolve_array_type(
            checker,
            node,
            result,
            &array_type_resolved
        )) {
        return false;
    }
    if (array_type_resolved) {
        return *result != MMAY_TYPE_INVALID;
    }
    if (!mmay_checker_private_resolve_collection_type(
            checker,
            node,
            result,
            &collection_type_resolved
        )) {
        return false;
    }
    if (collection_type_resolved) {
        return *result != MMAY_TYPE_INVALID;
    }
    if (!mmay_checker_private_resolve_compound_type(
            checker,
            node,
            result,
            &compound_type_resolved
        )) {
        return false;
    }
    if (compound_type_resolved) {
        return *result != MMAY_TYPE_INVALID;
    }
    name = mmay_checker_private_check_child_token(node, MMAY_TOKEN_IDENTIFIER, 0);
    if (name == NULL || name->synthetic
        || !mmay_checker_private_check_span_valid(checker, name->start, name->end)) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    type_arguments = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_TYPE_ARGUMENTS,
        0
    );
    declared_name.snapshot = checker->ast->snapshot;
    declared_name.start = name->start;
    declared_name.end = name->end;
    type_parameter = mmay_checker_private_find_type_parameter(
        checker, declared_name
    );
    if (type_parameter != NULL) {
        if (type_arguments != NULL) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "a type parameter cannot receive type arguments",
                node->start,
                node->end
            );
            *result = MMAY_TYPE_INVALID;
            return false;
        }
        (void)type_parameter;
        if (!mmay_checker_private_resolve_type_parameter(
                checker, declared_name, &active_type_parameter
            )) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        *result = active_type_parameter;
        return true;
    }
    if (!mmay_check_import_type(
            checker, node, result, &import_resolved
        )) {
        return false;
    }
    if (import_resolved) {
        return *result != MMAY_TYPE_INVALID;
    }
    if (type_arguments != NULL) {
        record = mmay_checker_private_checker_find_record(checker, declared_name);
        enum_declaration = mmay_checker_private_checker_find_enum(
            checker, declared_name
        );
        if (record == NULL && enum_declaration == NULL
            && mmay_checker_private_check_text_equal(
                checker,
                name->start,
                name->end,
                "Array"
            )) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_ARRAY_TYPE_SPELLING,
                "prelude managed arrays use '[]T'; replace 'Array<T>' with '[]T'",
                node->start,
                node->end
            );
            *result = MMAY_TYPE_INVALID;
            return false;
        }
        if (!mmay_vector_init(
                &generic_arguments,
                checker->result->allocator,
                sizeof(MarimayTypeId)
            )) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        generic_arguments_initialized = true;
        if (!mmay_checker_private_parse_type_arguments(
                checker, type_arguments, &generic_arguments
            )) {
            goto generic_cleanup;
        }
        if (record != NULL || enum_declaration != NULL) {
            size_t expected_count = record != NULL
                ? record->type_parameters.length
                : enum_declaration->type_parameters.length;
            MarimaySymbolId symbol = record != NULL ? record->symbol : enum_declaration->symbol;
            MarimayTypeKind kind = record != NULL
                ? MMAY_TYPE_KIND_NOMINAL : MMAY_TYPE_KIND_NOMINAL;
            if (expected_count == 0 || generic_arguments.length != expected_count) {
                (void)mmay_checker_add_diagnostic(
                    checker,
                    MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                    expected_count == 0
                        ? "type does not accept generic arguments"
                        : "generic type argument count does not match its declaration",
                    node->start,
                    node->end
                );
                goto generic_cleanup;
            }
            if (!mmay_checker_private_intern_generic_nominal(
                    checker, kind, symbol, &generic_arguments, result
                )) {
                mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
                goto generic_cleanup;
            }
            if ((record == NULL && enum_declaration != NULL
                    && enum_declaration->is_builtin)
                || mmay_check_record_occurrence(
                    checker, declared_name, symbol
                )) {
                mmay_vector_dispose(&generic_arguments);
                return true;
            }
            {
                goto generic_cleanup;
            }
        }
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_UNKNOWN_TYPE,
            "generic type name is unknown",
            name->start,
            name->end
        );
generic_cleanup:
        if (generic_arguments_initialized) {
            mmay_vector_dispose(&generic_arguments);
        }
        *result = MMAY_TYPE_INVALID;
        return false;
    }
    if (node->child_count != 1) {
        mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "bool")) {
        *result = MMAY_TYPE_BOOL;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "i8")) {
        *result = MMAY_TYPE_I8;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "i16")) {
        *result = MMAY_TYPE_I16;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "i32")) {
        *result = MMAY_TYPE_I32;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "i64")
        || mmay_checker_private_check_text_equal(checker, name->start, name->end, "int")) {
        *result = MMAY_TYPE_I64;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "u8")
        || mmay_checker_private_check_text_equal(checker, name->start, name->end, "byte")) {
        *result = MMAY_TYPE_U8;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "u16")) {
        *result = MMAY_TYPE_U16;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "u32")) {
        *result = MMAY_TYPE_U32;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "u64")
        || mmay_checker_private_check_text_equal(checker, name->start, name->end, "uint")) {
        *result = MMAY_TYPE_U64;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "f32")) {
        *result = MMAY_TYPE_F32;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "f64")
        || mmay_checker_private_check_text_equal(checker, name->start, name->end, "float")) {
        *result = MMAY_TYPE_F64;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "rune")) {
        *result = MMAY_TYPE_RUNE;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "string")) {
        *result = MMAY_TYPE_STRING;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "void")) {
        *result = MMAY_TYPE_VOID;
    } else if (mmay_checker_private_check_text_equal(checker, name->start, name->end, "never")) {
        *result = MMAY_TYPE_NEVER;
    } else {
        declared_name.snapshot = checker->ast->snapshot;
        declared_name.start = name->start;
        declared_name.end = name->end;
        record = mmay_checker_private_checker_find_record(checker, declared_name);
        if (record != NULL) {
            if (record->type_parameters.length != 0) {
                (void)mmay_checker_add_diagnostic(
                    checker,
                    MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                    "generic record type requires explicit type arguments",
                    node->start,
                    node->end
                );
                *result = MMAY_TYPE_INVALID;
                return false;
            }
            *result = record->type;
            return mmay_check_record_occurrence(
                checker, declared_name, record->symbol
            );
        }
        enum_declaration = mmay_checker_private_checker_find_enum(checker, declared_name);
        if (enum_declaration != NULL) {
            if (enum_declaration->type_parameters.length != 0) {
                (void)mmay_checker_add_diagnostic(
                    checker,
                    MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                    "generic enum type requires explicit type arguments",
                    node->start,
                    node->end
                );
                *result = MMAY_TYPE_INVALID;
                return false;
            }
            *result = enum_declaration->type;
            return enum_declaration->is_builtin
                || mmay_check_record_occurrence(
                    checker, declared_name, enum_declaration->symbol
                );
        }
        interface_declaration = mmay_checker_private_checker_find_interface(
            checker, declared_name
        );
        if (interface_declaration != NULL) {
            *result = interface_declaration->type;
            return mmay_check_record_occurrence(
                checker, declared_name, interface_declaration->symbol
            );
        }
        alias = mmay_checker_private_checker_find_type_alias(checker, declared_name);
        if (alias != NULL) {
            if (!mmay_checker_private_evaluate_type_alias(
                    checker,
                    alias,
                    name->start,
                    name->end
                )) {
                *result = MMAY_TYPE_INVALID;
                return false;
            }
            *result = alias->type;
            return mmay_check_record_occurrence(
                checker, declared_name, alias->symbol
            );
        }
        *result = MMAY_TYPE_INVALID;
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_UNKNOWN_TYPE,
            "unknown type name",
            name->start,
            name->end
        );
        return false;
    }
    return true;
}

 const MarimayCheckLocal *mmay_checker_private_function_find_local(
    const MarimayFunctionChecker *function,
    MarimayCheckName name
) {
    size_t index = function->locals.length;
    while (index != 0) {
        const MarimayCheckLocal *local = mmay_vector_at_const(
            &function->locals,
            index - 1
        );
        if (local != NULL && mmay_checker_private_check_names_equal(
                function->checker,
                local->name,
                name
            )) {
            return local;
        }
        index -= 1;
    }
    return mmay_closure_find_captured_local(function, name);
}

 const MarimayCheckLocal *mmay_checker_private_function_find_local_in_current_scope(
    const MarimayFunctionChecker *function,
    MarimayCheckName name
) {
    size_t index = function->locals.length;
    while (index != 0) {
        const MarimayCheckLocal *local = mmay_vector_at_const(
            &function->locals,
            index - 1
        );
        if (local != NULL && local->scope_depth < function->scope_depth) {
            break;
        }
        if (local != NULL && local->scope_depth == function->scope_depth
            && mmay_checker_private_check_names_equal(
                function->checker,
                local->name,
                name
            )) {
            return local;
        }
        index -= 1;
    }
    return NULL;
}

 MarimayCheckLocal *mmay_checker_private_function_find_local_mutable(
    MarimayFunctionChecker *function,
    MarimayCheckName name
) {
    size_t index = function->locals.length;
    while (index != 0) {
        MarimayCheckLocal *local = mmay_vector_at(&function->locals, index - 1);
        if (local != NULL && mmay_checker_private_check_names_equal(
                function->checker,
                local->name,
                name
            )) {
            return local;
        }
        index -= 1;
    }
    return NULL;
}

 bool mmay_checker_private_initialization_snapshot_init(
    MarimayFunctionChecker *function,
    size_t count,
    MarimayInitializationSnapshot *snapshot
) {
    size_t index = 0;
    if (snapshot == NULL || count > function->locals.length) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->allocator = function->checker->result->allocator;
    snapshot->count = count;
    snapshot->allocation_size = count == 0 ? 1 : count;
    snapshot->states = mmay_allocate(
        &snapshot->allocator,
        snapshot->allocation_size
    );
    if (snapshot->states == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < count; index += 1) {
        const MarimayCheckLocal *local = mmay_vector_at_const(
            &function->locals,
            index
        );
        if (local == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            mmay_deallocate(
                &snapshot->allocator,
                snapshot->states,
                snapshot->allocation_size
            );
            memset(snapshot, 0, sizeof(*snapshot));
            return false;
        }
        snapshot->states[index] = local->initialized ? 1 : 0;
    }
    return true;
}

 bool mmay_checker_private_initialization_snapshot_capture(
    MarimayFunctionChecker *function,
    MarimayInitializationSnapshot *snapshot
) {
    size_t index = 0;
    if (snapshot == NULL || snapshot->states == NULL
        || snapshot->count > function->locals.length) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < snapshot->count; index += 1) {
        const MarimayCheckLocal *local = mmay_vector_at_const(
            &function->locals,
            index
        );
        if (local == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        snapshot->states[index] = local->initialized ? 1 : 0;
    }
    return true;
}

 bool mmay_checker_private_initialization_snapshot_restore(
    MarimayFunctionChecker *function,
    const MarimayInitializationSnapshot *snapshot
) {
    size_t index = 0;
    if (snapshot == NULL || snapshot->states == NULL
        || snapshot->count > function->locals.length) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < snapshot->count; index += 1) {
        MarimayCheckLocal *local = mmay_vector_at(&function->locals, index);
        if (local == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        local->initialized = snapshot->states[index] != 0;
    }
    return true;
}

 void mmay_checker_private_initialization_snapshot_dispose(
    MarimayInitializationSnapshot *snapshot
) {
    if (snapshot == NULL) {
        return;
    }
    mmay_deallocate(
        &snapshot->allocator,
        snapshot->states,
        snapshot->allocation_size
    );
    memset(snapshot, 0, sizeof(*snapshot));
}

bool mmay_function_add_register(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayMirRegister *result
) {
    if (function->mir.register_types.length
        >= function->mir.options.max_registers) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_function_ensure_array_layout(function, type)
        || !mmay_function_ensure_tuple_layout(function, type)
        || !mmay_function_value_layout(function, type)) {
        return false;
    }
    if (!mmay_mir_add_register(&function->mir, type, result)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

 bool mmay_checker_private_function_add_block(
    MarimayFunctionChecker *function,
    MarimayMirBlockId *result
) {
    if (function->mir.blocks.length >= function->mir.options.max_blocks) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_mir_add_block(&function->mir, result)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool mmay_function_append_instruction(
    MarimayFunctionChecker *function,
    const MarimayMirInstruction *instruction
) {
    if (function->mir.instruction_count
        >= function->mir.options.max_instructions) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_mir_append_instruction(
            &function->mir,
            function->block,
            instruction
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool mmay_function_emit_instruction(
    MarimayFunctionChecker *function,
    MarimayMirInstructionKind kind,
    MarimayTypeId type,
    MarimayMirRegister first,
    MarimayMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
) {
    MarimayMirInstruction instruction;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    if (!mmay_function_add_register(function, type, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = first;
    instruction.second = second;
    instruction.integer = integer;
    instruction.boolean = boolean;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = type;
    result->register_id = target;
    result->valid = true;
    return true;
}

 bool mmay_checker_private_function_emit_f64_constant(
    MarimayFunctionChecker *function,
    uint64_t bits,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
) {
    MarimayMirInstruction instruction;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    if (!mmay_function_add_register(function, MMAY_TYPE_F64, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_F64;
    instruction.target = target;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.f64_bits = bits;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = MMAY_TYPE_F64;
    result->register_id = target;
    result->valid = true;
    return true;
}

bool mmay_checker_private_function_record_layout(
    MarimayFunctionChecker *function,
    const MarimayCheckRecordDeclaration *declaration,
    MarimayMirRecordId *result
) {
    MarimayMirRecordField *fields = NULL;
    size_t field_bytes = 0;
    size_t new_field_count = 0;
    size_t index = 0;
    bool layout_exists = false;
    if (function == NULL) {
        return false;
    }
    if (declaration == NULL || result == NULL
        || !declaration->declaration_valid || !declaration->fields_resolved
        || declaration->type == MMAY_TYPE_INVALID
        || !mmay_size_multiply(
            declaration->fields.length,
            sizeof(MarimayMirRecordField),
            &field_bytes
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < function->mir.records.length; index += 1) {
        const MarimayMirRecord *existing = mmay_vector_at_const(
            &function->mir.records,
            index
        );
        if (existing != NULL && existing->type == declaration->type) {
            layout_exists = true;
            break;
        }
    }
    if (!layout_exists
        && (function->mir.records.length
                >= function->mir.options.max_record_layouts
            || !mmay_size_add(
                function->mir.record_fields.length,
                declaration->fields.length,
                &new_field_count
            ) || new_field_count > function->mir.options.max_record_fields)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (field_bytes != 0) {
        fields = mmay_allocate(
            &function->checker->result->allocator,
            field_bytes
        );
        if (fields == NULL) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_ALLOCATION_FAILED
            );
            return false;
        }
    }
    for (index = 0; index < declaration->fields.length; index += 1) {
        const MarimayCheckRecordFieldDeclaration *field = mmay_vector_at_const(
            &declaration->fields,
            index
        );
        if (field == NULL || fields == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            mmay_deallocate(
                &function->checker->result->allocator,
                fields,
                field_bytes
            );
            return false;
        }
        if (!mmay_checker_private_substitute_nominal_member_type(
                function,
                declaration->type,
                &declaration->type_parameters,
                field->type,
                &fields[index].type
            )) {
            mmay_deallocate(
                &function->checker->result->allocator,
                fields,
                field_bytes
            );
            return false;
        }
        fields[index].is_mutable = field->is_mutable;
    }
    if (!mmay_mir_add_record(
            &function->mir,
            declaration->type,
            fields,
            declaration->fields.length,
            result
        )) {
        mmay_checker_fail(
            function->checker,
            layout_exists
                ? MMAY_CHECK_INTERNAL_ERROR
                : MMAY_CHECK_ALLOCATION_FAILED
        );
        mmay_deallocate(
            &function->checker->result->allocator,
            fields,
            field_bytes
        );
        return false;
    }
    mmay_deallocate(
        &function->checker->result->allocator,
        fields,
        field_bytes
    );
    return true;
}

 bool mmay_checker_private_function_enum_layout(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumDeclaration *declaration,
    MarimayMirEnumId *result
) {
    MarimayMirEnumVariant *variants = NULL;
    MarimayMirEnumPayload *payloads = NULL;
    size_t variant_bytes = 0;
    size_t payload_bytes = 0;
    size_t payload_count = 0;
    size_t new_variant_count = 0;
    size_t new_payload_count = 0;
    size_t variant_index = 0;
    size_t payload_cursor = 0;
    bool layout_exists = false;
    bool accepted = false;
    if (function == NULL) {
        return false;
    }
    if (declaration == NULL || result == NULL
        || !declaration->declaration_valid || !declaration->variants_resolved) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (variant_index = 0; variant_index < declaration->variants.length;
        variant_index += 1) {
        const MarimayCheckEnumVariantDeclaration *variant = mmay_vector_at_const(
            &declaration->variants,
            variant_index
        );
        size_t payload_index = 0;
        if (variant == NULL || !variant->payload_initialized) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        for (payload_index = 0; payload_index < variant->payload.length;
            payload_index += 1) {
            const MarimayCheckEnumPayloadItemDeclaration *payload =
                mmay_vector_at_const(&variant->payload, payload_index);
            MarimayTypeId payload_type = MMAY_TYPE_INVALID;
            if (payload == NULL
                || !mmay_checker_private_substitute_nominal_member_type(
                    function,
                    declaration->type,
                    &declaration->type_parameters,
                    payload->type,
                    &payload_type
                )
                || (payload_type != MMAY_TYPE_VOID
                    && !mmay_size_add(payload_count, 1, &payload_count))) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
                return false;
            }
        }
    }
    if (!mmay_size_multiply(
            declaration->variants.length,
            sizeof(MarimayMirEnumVariant),
            &variant_bytes
        ) || !mmay_size_multiply(
            payload_count,
            sizeof(MarimayMirEnumPayload),
            &payload_bytes
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    for (variant_index = 0; variant_index < function->mir.enums.length;
        variant_index += 1) {
        const MarimayMirEnum *existing = mmay_vector_at_const(
            &function->mir.enums,
            variant_index
        );
        if (existing != NULL && existing->type == declaration->type) {
            layout_exists = true;
            break;
        }
    }
    if (!layout_exists
        && (function->mir.enums.length
                >= function->mir.options.max_enum_layouts
            || !mmay_size_add(
                function->mir.enum_variants.length,
                declaration->variants.length,
                &new_variant_count
            ) || new_variant_count > function->mir.options.max_enum_variants
            || !mmay_size_add(
                function->mir.enum_payloads.length,
                payload_count,
                &new_payload_count
            ) || new_payload_count > function->mir.options.max_enum_payloads)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (variant_bytes != 0) {
        variants = mmay_allocate(
            &function->checker->result->allocator,
            variant_bytes
        );
        if (variants == NULL) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    if (payload_bytes != 0) {
        payloads = mmay_allocate(
            &function->checker->result->allocator,
            payload_bytes
        );
        if (payloads == NULL) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    for (variant_index = 0; variant_index < declaration->variants.length;
        variant_index += 1) {
        const MarimayCheckEnumVariantDeclaration *variant = mmay_vector_at_const(
            &declaration->variants,
            variant_index
        );
        size_t payload_index = 0;
        if (variant == NULL || variants == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        switch (variant->kind) {
            case MMAY_CHECKED_ENUM_VARIANT_UNIT:
                variants[variant_index].kind = MMAY_MIR_ENUM_VARIANT_UNIT;
                break;
            case MMAY_CHECKED_ENUM_VARIANT_POSITIONAL:
                variants[variant_index].kind =
                    MMAY_MIR_ENUM_VARIANT_POSITIONAL;
                break;
            case MMAY_CHECKED_ENUM_VARIANT_NAMED:
                variants[variant_index].kind = MMAY_MIR_ENUM_VARIANT_NAMED;
                break;
            default:
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
        }
        variants[variant_index].payload_start = payload_cursor;
        variants[variant_index].payload_count = 0;
        for (payload_index = 0; payload_index < variant->payload.length;
            payload_index += 1) {
            const MarimayCheckEnumPayloadItemDeclaration *payload =
                mmay_vector_at_const(&variant->payload, payload_index);
            MarimayTypeId payload_type = MMAY_TYPE_INVALID;
            if (payload == NULL
                || payload->type == MMAY_TYPE_INVALID
                || (variant->kind == MMAY_CHECKED_ENUM_VARIANT_POSITIONAL
                    && payload->has_name)
                || (variant->kind == MMAY_CHECKED_ENUM_VARIANT_NAMED
                    && !payload->has_name)
                || !mmay_checker_private_substitute_nominal_member_type(
                    function,
                    declaration->type,
                    &declaration->type_parameters,
                    payload->type,
                    &payload_type
                )) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            if (payload_type == MMAY_TYPE_VOID) {
                continue;
            }
            {
                MarimayCheckEnumDeclaration *nested_declaration =
                    mmay_checker_private_checker_find_enum_by_type(
                        function->checker, payload_type
                    );
                MarimayCheckEnumDeclaration nested_concrete;
                MarimayMirEnumId nested_id = MMAY_MIR_NO_ENUM;
                if (nested_declaration != NULL
                    && nested_declaration->type != declaration->type) {
                    nested_concrete = *nested_declaration;
                    nested_concrete.type = payload_type;
                    if (!mmay_checker_private_function_enum_layout(
                            function, &nested_concrete, &nested_id
                        )) {
                        goto cleanup;
                    }
                }
            }
            if (payloads == NULL) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
                goto cleanup;
            }
            payloads[payload_cursor].type = payload_type;
            payload_cursor += 1;
            variants[variant_index].payload_count += 1;
        }
    }
    if (payload_cursor != payload_count || !mmay_mir_add_enum_with_symbol(
            &function->mir,
            declaration->symbol,
            declaration->type,
            variants,
            declaration->variants.length,
            payloads,
            payload_count,
            result
        )) {
        mmay_checker_fail(
            function->checker,
            layout_exists
                ? MMAY_CHECK_INTERNAL_ERROR
                : MMAY_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    accepted = true;

cleanup:
    mmay_deallocate(
        &function->checker->result->allocator,
        payloads,
        payload_bytes
    );
    mmay_deallocate(
        &function->checker->result->allocator,
        variants,
        variant_bytes
    );
    return accepted && !mmay_checker_has_failed(function->checker);
}

 bool mmay_checker_private_function_emit_record_access(
    MarimayFunctionChecker *function,
    MarimayMirInstructionKind kind,
    MarimayTypeId result_type,
    MarimayMirRegister receiver,
    MarimayMirRegister value,
    MarimayMirRecordId record,
    size_t field,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
) {
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    if ((kind != MMAY_MIR_RECORD_GET && kind != MMAY_MIR_RECORD_SET)
        || result == NULL
        || !mmay_function_add_register(function, result_type, &target)) {
        if (result == NULL || (kind != MMAY_MIR_RECORD_GET
                && kind != MMAY_MIR_RECORD_SET)) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = receiver;
    instruction.second = value;
    instruction.record = record;
    instruction.field = field;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

 bool mmay_checker_private_integer_magnitude(
    MarimayChecker *checker,
    const MarimaySyntaxToken *token,
    uint64_t limit,
    uint64_t *result
) {
    const unsigned char *bytes = NULL;
    size_t cursor = 0;
    size_t length = 0;
    unsigned int base = 10;
    uint64_t value = 0;
    if (token == NULL || result == NULL || token->kind != MMAY_TOKEN_INTEGER
        || token->synthetic
        || !mmay_checker_private_check_span_valid(checker, token->start, token->end)) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    bytes = checker->source + token->start;
    length = token->end - token->start;
    if (length >= 2 && bytes[0] == '0') {
        if (bytes[1] == 'b' || bytes[1] == 'B') {
            base = 2;
            cursor = 2;
        } else if (bytes[1] == 'o' || bytes[1] == 'O') {
            base = 8;
            cursor = 2;
        } else if (bytes[1] == 'x' || bytes[1] == 'X') {
            base = 16;
            cursor = 2;
        }
    }
    for (; cursor < length; cursor += 1) {
        unsigned char byte = bytes[cursor];
        unsigned int digit = 0;
        if (byte == '_') {
            continue;
        }
        if (byte >= '0' && byte <= '9') {
            digit = (unsigned int)(byte - '0');
        } else if (byte >= 'a' && byte <= 'f') {
            digit = (unsigned int)(byte - 'a') + 10;
        } else if (byte >= 'A' && byte <= 'F') {
            digit = (unsigned int)(byte - 'A') + 10;
        } else {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (digit >= base || value > (limit - digit) / base) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_INTEGER_LITERAL_RANGE,
                "integer literal is outside the range of int",
                token->start,
                token->end
            );
            return false;
        }
        value = value * base + digit;
    }
    *result = value;
    return true;
}

bool mmay_compile_expression(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);

 bool mmay_checker_private_member_parts(
    const MarimayAstNode *node,
    const MarimayAstNode **receiver,
    const MarimaySyntaxToken **member
);

 bool mmay_checker_private_compile_enum_match(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
);

 bool mmay_checker_private_compile_block(
    MarimayFunctionChecker *function,
    const MarimayAstNode *block
);

 bool mmay_checker_private_function_emit_copy(
    MarimayFunctionChecker *function,
    MarimayMirRegister target,
    MarimayMirRegister source,
    size_t start,
    size_t end
);

 bool mmay_checker_private_function_set_goto(
    MarimayFunctionChecker *function,
    MarimayMirBlockId block,
    MarimayMirBlockId target,
    size_t start,
    size_t end
);

 bool mmay_checker_private_function_set_branch(
    MarimayFunctionChecker *function,
    MarimayMirBlockId block,
    MarimayMirRegister condition,
    MarimayMirBlockId when_true,
    MarimayMirBlockId when_false,
    size_t start,
    size_t end
);

 bool mmay_checker_private_function_set_panic(
    MarimayFunctionChecker *function,
    MarimayMirRegister message,
    size_t start,
    size_t end
);

 bool mmay_checker_private_function_set_never_call(
    MarimayFunctionChecker *function,
    MarimayMirCallId call,
    size_t start,
    size_t end
);

 bool mmay_checker_private_type_is_integer(MarimayTypeId type) {
    return type >= MMAY_TYPE_I8 && type <= MMAY_TYPE_U64;
}

 bool mmay_checker_private_type_is_numeric(MarimayTypeId type) {
    return mmay_checker_private_type_is_integer(type)
        || type == MMAY_TYPE_F32 || type == MMAY_TYPE_F64;
}
