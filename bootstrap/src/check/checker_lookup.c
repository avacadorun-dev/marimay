#include "checker_lookup_api.h"
#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_expr_records_api.h"
#include "package_imports_api.h"






 bool ruda_checker_private_check_exact_type(
    RudaChecker *checker,
    const RudaAstNode *node,
    RudaTypeId *result
) {
    const RudaSyntaxToken *name = NULL;
    RudaCheckName declared_name;
    RudaCheckTypeAliasDeclaration *alias = NULL;
    RudaCheckRecordDeclaration *record = NULL;
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    RudaCheckInterfaceDeclaration *interface_declaration = NULL;
    const RudaAstNode *type_arguments = NULL;
    const RudaAstNode *argument_type = NULL;
    RudaTypeId element_type = RUDA_TYPE_INVALID;
    size_t argument_count = 0;
    size_t index = 0;
    bool import_resolved = false;
    if (node == NULL || result == NULL || node->kind != RUDA_SYNTAX_TYPE) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    name = ruda_checker_private_check_child_token(node, RUDA_TOKEN_IDENTIFIER, 0);
    if (name == NULL || name->synthetic
        || !ruda_checker_private_check_span_valid(checker, name->start, name->end)) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    type_arguments = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_TYPE_ARGUMENTS,
        0
    );
    if (ruda_checker_private_check_text_equal(
            checker,
            name->start,
            name->end,
            "Array"
        )) {
        if (type_arguments != NULL) {
            for (index = 0; index < type_arguments->child_count; index += 1) {
                const RudaAstElement *element = ruda_ast_child(
                    type_arguments,
                    index
                );
                if (element == NULL) {
                    ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
                    return false;
                }
                if (element->kind == RUDA_AST_ELEMENT_NODE) {
                    if (element->as.node == NULL
                        || element->as.node->kind != RUDA_SYNTAX_TYPE
                        || argument_count == SIZE_MAX) {
                        ruda_checker_fail(
                            checker,
                            RUDA_CHECK_INVALID_ARGUMENT
                        );
                        return false;
                    }
                    argument_type = element->as.node;
                    argument_count += 1;
                } else if (element->as.token.synthetic) {
                    ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
                    return false;
                }
            }
        }
        if (type_arguments == NULL || argument_count != 1
            || argument_type == NULL) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "Array requires exactly one type argument",
                node->start,
                node->end
            );
            *result = RUDA_TYPE_INVALID;
            return false;
        }
        if (!ruda_checker_private_check_exact_type(checker, argument_type, &element_type)) {
            *result = RUDA_TYPE_INVALID;
            return false;
        }
        if (!ruda_type_intern_array(
                &checker->result->types,
                element_type,
                result
            )) {
            ruda_checker_fail(
                checker,
                checker->result->types.descriptors.length
                        >= checker->options.type_options.max_types
                            - RUDA_TYPE_FIRST_DYNAMIC
                    ? RUDA_CHECK_LIMIT_REACHED
                    : RUDA_CHECK_ALLOCATION_FAILED
            );
            return false;
        }
        return true;
    }
    if (!ruda_check_import_type(
            checker, node, result, &import_resolved
        )) {
        return false;
    }
    if (import_resolved) {
        return *result != RUDA_TYPE_INVALID;
    }
    if (node->child_count != 1) {
        ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "bool")) {
        *result = RUDA_TYPE_BOOL;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "i8")) {
        *result = RUDA_TYPE_I8;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "i16")) {
        *result = RUDA_TYPE_I16;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "i32")) {
        *result = RUDA_TYPE_I32;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "i64")
        || ruda_checker_private_check_text_equal(checker, name->start, name->end, "int")) {
        *result = RUDA_TYPE_I64;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "u8")
        || ruda_checker_private_check_text_equal(checker, name->start, name->end, "byte")) {
        *result = RUDA_TYPE_U8;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "u16")) {
        *result = RUDA_TYPE_U16;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "u32")) {
        *result = RUDA_TYPE_U32;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "u64")
        || ruda_checker_private_check_text_equal(checker, name->start, name->end, "uint")) {
        *result = RUDA_TYPE_U64;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "f32")) {
        *result = RUDA_TYPE_F32;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "f64")
        || ruda_checker_private_check_text_equal(checker, name->start, name->end, "float")) {
        *result = RUDA_TYPE_F64;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "rune")) {
        *result = RUDA_TYPE_RUNE;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "string")) {
        *result = RUDA_TYPE_STRING;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "void")) {
        *result = RUDA_TYPE_VOID;
    } else if (ruda_checker_private_check_text_equal(checker, name->start, name->end, "never")) {
        *result = RUDA_TYPE_NEVER;
    } else {
        declared_name.snapshot = checker->ast->snapshot;
        declared_name.start = name->start;
        declared_name.end = name->end;
        record = ruda_checker_private_checker_find_record(checker, declared_name);
        if (record != NULL) {
            *result = record->type;
            return ruda_check_record_occurrence(
                checker, declared_name, record->symbol
            );
        }
        enum_declaration = ruda_checker_private_checker_find_enum(checker, declared_name);
        if (enum_declaration != NULL) {
            *result = enum_declaration->type;
            return ruda_check_record_occurrence(
                checker, declared_name, enum_declaration->symbol
            );
        }
        interface_declaration = ruda_checker_private_checker_find_interface(
            checker, declared_name
        );
        if (interface_declaration != NULL) {
            *result = interface_declaration->type;
            return ruda_check_record_occurrence(
                checker, declared_name, interface_declaration->symbol
            );
        }
        alias = ruda_checker_private_checker_find_type_alias(checker, declared_name);
        if (alias != NULL) {
            if (!ruda_checker_private_evaluate_type_alias(
                    checker,
                    alias,
                    name->start,
                    name->end
                )) {
                *result = RUDA_TYPE_INVALID;
                return false;
            }
            *result = alias->type;
            return ruda_check_record_occurrence(
                checker, declared_name, alias->symbol
            );
        }
        *result = RUDA_TYPE_INVALID;
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_UNKNOWN_TYPE,
            "unknown type name",
            name->start,
            name->end
        );
        return false;
    }
    return true;
}

 const RudaCheckLocal *ruda_checker_private_function_find_local(
    const RudaFunctionChecker *function,
    RudaCheckName name
) {
    size_t index = function->locals.length;
    while (index != 0) {
        const RudaCheckLocal *local = ruda_vector_at_const(
            &function->locals,
            index - 1
        );
        if (local != NULL && ruda_checker_private_check_names_equal(
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

 const RudaCheckLocal *ruda_checker_private_function_find_local_in_current_scope(
    const RudaFunctionChecker *function,
    RudaCheckName name
) {
    size_t index = function->locals.length;
    while (index != 0) {
        const RudaCheckLocal *local = ruda_vector_at_const(
            &function->locals,
            index - 1
        );
        if (local != NULL && local->scope_depth < function->scope_depth) {
            break;
        }
        if (local != NULL && local->scope_depth == function->scope_depth
            && ruda_checker_private_check_names_equal(
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

 RudaCheckLocal *ruda_checker_private_function_find_local_mutable(
    RudaFunctionChecker *function,
    RudaCheckName name
) {
    size_t index = function->locals.length;
    while (index != 0) {
        RudaCheckLocal *local = ruda_vector_at(&function->locals, index - 1);
        if (local != NULL && ruda_checker_private_check_names_equal(
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

 bool ruda_checker_private_initialization_snapshot_init(
    RudaFunctionChecker *function,
    size_t count,
    RudaInitializationSnapshot *snapshot
) {
    size_t index = 0;
    if (snapshot == NULL || count > function->locals.length) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->allocator = function->checker->result->allocator;
    snapshot->count = count;
    snapshot->allocation_size = count == 0 ? 1 : count;
    snapshot->states = ruda_allocate(
        &snapshot->allocator,
        snapshot->allocation_size
    );
    if (snapshot->states == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < count; index += 1) {
        const RudaCheckLocal *local = ruda_vector_at_const(
            &function->locals,
            index
        );
        if (local == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            ruda_deallocate(
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

 bool ruda_checker_private_initialization_snapshot_capture(
    RudaFunctionChecker *function,
    RudaInitializationSnapshot *snapshot
) {
    size_t index = 0;
    if (snapshot == NULL || snapshot->states == NULL
        || snapshot->count > function->locals.length) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < snapshot->count; index += 1) {
        const RudaCheckLocal *local = ruda_vector_at_const(
            &function->locals,
            index
        );
        if (local == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        snapshot->states[index] = local->initialized ? 1 : 0;
    }
    return true;
}

 bool ruda_checker_private_initialization_snapshot_restore(
    RudaFunctionChecker *function,
    const RudaInitializationSnapshot *snapshot
) {
    size_t index = 0;
    if (snapshot == NULL || snapshot->states == NULL
        || snapshot->count > function->locals.length) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < snapshot->count; index += 1) {
        RudaCheckLocal *local = ruda_vector_at(&function->locals, index);
        if (local == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        local->initialized = snapshot->states[index] != 0;
    }
    return true;
}

 void ruda_checker_private_initialization_snapshot_dispose(
    RudaInitializationSnapshot *snapshot
) {
    if (snapshot == NULL) {
        return;
    }
    ruda_deallocate(
        &snapshot->allocator,
        snapshot->states,
        snapshot->allocation_size
    );
    memset(snapshot, 0, sizeof(*snapshot));
}

bool ruda_function_add_register(
    RudaFunctionChecker *function,
    RudaTypeId type,
    RudaMirRegister *result
) {
    if (function->mir.register_types.length
        >= function->mir.options.max_registers) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_function_ensure_array_layout(function, type)) {
        return false;
    }
    if (!ruda_mir_add_register(&function->mir, type, result)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

 bool ruda_checker_private_function_add_block(
    RudaFunctionChecker *function,
    RudaMirBlockId *result
) {
    if (function->mir.blocks.length >= function->mir.options.max_blocks) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_mir_add_block(&function->mir, result)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool ruda_function_append_instruction(
    RudaFunctionChecker *function,
    const RudaMirInstruction *instruction
) {
    if (function->mir.instruction_count
        >= function->mir.options.max_instructions) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_mir_append_instruction(
            &function->mir,
            function->block,
            instruction
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool ruda_function_emit_instruction(
    RudaFunctionChecker *function,
    RudaMirInstructionKind kind,
    RudaTypeId type,
    RudaMirRegister first,
    RudaMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirInstruction instruction;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    if (!ruda_function_add_register(function, type, &target)) {
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
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = type;
    result->register_id = target;
    result->valid = true;
    return true;
}

 bool ruda_checker_private_function_emit_f64_constant(
    RudaFunctionChecker *function,
    uint64_t bits,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirInstruction instruction;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    if (!ruda_function_add_register(function, RUDA_TYPE_F64, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_F64;
    instruction.target = target;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.f64_bits = bits;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = RUDA_TYPE_F64;
    result->register_id = target;
    result->valid = true;
    return true;
}

 bool ruda_checker_private_function_record_layout(
    RudaFunctionChecker *function,
    const RudaCheckRecordDeclaration *declaration,
    RudaMirRecordId *result
) {
    RudaMirRecordField *fields = NULL;
    size_t field_bytes = 0;
    size_t new_field_count = 0;
    size_t index = 0;
    bool layout_exists = false;
    if (function == NULL) {
        return false;
    }
    if (declaration == NULL || result == NULL
        || !declaration->declaration_valid || !declaration->fields_resolved
        || declaration->type == RUDA_TYPE_INVALID
        || !ruda_size_multiply(
            declaration->fields.length,
            sizeof(RudaMirRecordField),
            &field_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < function->mir.records.length; index += 1) {
        const RudaMirRecord *existing = ruda_vector_at_const(
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
            || !ruda_size_add(
                function->mir.record_fields.length,
                declaration->fields.length,
                &new_field_count
            ) || new_field_count > function->mir.options.max_record_fields)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (field_bytes != 0) {
        fields = ruda_allocate(
            &function->checker->result->allocator,
            field_bytes
        );
        if (fields == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            return false;
        }
    }
    for (index = 0; index < declaration->fields.length; index += 1) {
        const RudaCheckRecordFieldDeclaration *field = ruda_vector_at_const(
            &declaration->fields,
            index
        );
        if (field == NULL || fields == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            ruda_deallocate(
                &function->checker->result->allocator,
                fields,
                field_bytes
            );
            return false;
        }
        fields[index].type = field->type;
        fields[index].is_mutable = field->is_mutable;
    }
    if (!ruda_mir_add_record(
            &function->mir,
            declaration->type,
            fields,
            declaration->fields.length,
            result
        )) {
        ruda_checker_fail(
            function->checker,
            layout_exists
                ? RUDA_CHECK_INTERNAL_ERROR
                : RUDA_CHECK_ALLOCATION_FAILED
        );
        ruda_deallocate(
            &function->checker->result->allocator,
            fields,
            field_bytes
        );
        return false;
    }
    ruda_deallocate(
        &function->checker->result->allocator,
        fields,
        field_bytes
    );
    return true;
}

 bool ruda_checker_private_function_enum_layout(
    RudaFunctionChecker *function,
    const RudaCheckEnumDeclaration *declaration,
    RudaMirEnumId *result
) {
    RudaMirEnumVariant *variants = NULL;
    RudaMirEnumPayload *payloads = NULL;
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
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (variant_index = 0; variant_index < declaration->variants.length;
        variant_index += 1) {
        const RudaCheckEnumVariantDeclaration *variant = ruda_vector_at_const(
            &declaration->variants,
            variant_index
        );
        if (variant == NULL || !variant->payload_initialized
            || !ruda_size_add(
                payload_count,
                variant->payload.length,
                &payload_count
            )) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
    }
    if (!ruda_size_multiply(
            declaration->variants.length,
            sizeof(RudaMirEnumVariant),
            &variant_bytes
        ) || !ruda_size_multiply(
            payload_count,
            sizeof(RudaMirEnumPayload),
            &payload_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    for (variant_index = 0; variant_index < function->mir.enums.length;
        variant_index += 1) {
        const RudaMirEnum *existing = ruda_vector_at_const(
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
            || !ruda_size_add(
                function->mir.enum_variants.length,
                declaration->variants.length,
                &new_variant_count
            ) || new_variant_count > function->mir.options.max_enum_variants
            || !ruda_size_add(
                function->mir.enum_payloads.length,
                payload_count,
                &new_payload_count
            ) || new_payload_count > function->mir.options.max_enum_payloads)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (variant_bytes != 0) {
        variants = ruda_allocate(
            &function->checker->result->allocator,
            variant_bytes
        );
        if (variants == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    if (payload_bytes != 0) {
        payloads = ruda_allocate(
            &function->checker->result->allocator,
            payload_bytes
        );
        if (payloads == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    for (variant_index = 0; variant_index < declaration->variants.length;
        variant_index += 1) {
        const RudaCheckEnumVariantDeclaration *variant = ruda_vector_at_const(
            &declaration->variants,
            variant_index
        );
        size_t payload_index = 0;
        if (variant == NULL || variants == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        switch (variant->kind) {
            case RUDA_CHECKED_ENUM_VARIANT_UNIT:
                variants[variant_index].kind = RUDA_MIR_ENUM_VARIANT_UNIT;
                break;
            case RUDA_CHECKED_ENUM_VARIANT_POSITIONAL:
                variants[variant_index].kind =
                    RUDA_MIR_ENUM_VARIANT_POSITIONAL;
                break;
            case RUDA_CHECKED_ENUM_VARIANT_NAMED:
                variants[variant_index].kind = RUDA_MIR_ENUM_VARIANT_NAMED;
                break;
            default:
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
        }
        variants[variant_index].payload_start = payload_cursor;
        variants[variant_index].payload_count = variant->payload.length;
        for (payload_index = 0; payload_index < variant->payload.length;
            payload_index += 1) {
            const RudaCheckEnumPayloadItemDeclaration *payload =
                ruda_vector_at_const(&variant->payload, payload_index);
            if (payload == NULL || payloads == NULL
                || payload->type == RUDA_TYPE_INVALID
                || (variant->kind == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL
                    && payload->has_name)
                || (variant->kind == RUDA_CHECKED_ENUM_VARIANT_NAMED
                    && !payload->has_name)) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            payloads[payload_cursor].type = payload->type;
            payload_cursor += 1;
        }
    }
    if (payload_cursor != payload_count || !ruda_mir_add_enum(
            &function->mir,
            declaration->type,
            variants,
            declaration->variants.length,
            payloads,
            payload_count,
            result
        )) {
        ruda_checker_fail(
            function->checker,
            layout_exists
                ? RUDA_CHECK_INTERNAL_ERROR
                : RUDA_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    accepted = true;

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        payloads,
        payload_bytes
    );
    ruda_deallocate(
        &function->checker->result->allocator,
        variants,
        variant_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_function_emit_record_access(
    RudaFunctionChecker *function,
    RudaMirInstructionKind kind,
    RudaTypeId result_type,
    RudaMirRegister receiver,
    RudaMirRegister value,
    RudaMirRecordId record,
    size_t field,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    if ((kind != RUDA_MIR_RECORD_GET && kind != RUDA_MIR_RECORD_SET)
        || result == NULL
        || !ruda_function_add_register(function, result_type, &target)) {
        if (result == NULL || (kind != RUDA_MIR_RECORD_GET
                && kind != RUDA_MIR_RECORD_SET)) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
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
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

 bool ruda_checker_private_integer_magnitude(
    RudaChecker *checker,
    const RudaSyntaxToken *token,
    uint64_t limit,
    uint64_t *result
) {
    const unsigned char *bytes = NULL;
    size_t cursor = 0;
    size_t length = 0;
    unsigned int base = 10;
    uint64_t value = 0;
    if (token == NULL || result == NULL || token->kind != RUDA_TOKEN_INTEGER
        || token->synthetic
        || !ruda_checker_private_check_span_valid(checker, token->start, token->end)) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
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
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (digit >= base || value > (limit - digit) / base) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_INTEGER_LITERAL_RANGE,
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

bool ruda_compile_expression(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);

 bool ruda_checker_private_member_parts(
    const RudaAstNode *node,
    const RudaAstNode **receiver,
    const RudaSyntaxToken **member
);

 bool ruda_checker_private_compile_enum_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
);

 bool ruda_checker_private_compile_block(
    RudaFunctionChecker *function,
    const RudaAstNode *block
);

 bool ruda_checker_private_function_emit_copy(
    RudaFunctionChecker *function,
    RudaMirRegister target,
    RudaMirRegister source,
    size_t start,
    size_t end
);

 bool ruda_checker_private_function_set_goto(
    RudaFunctionChecker *function,
    RudaMirBlockId block,
    RudaMirBlockId target,
    size_t start,
    size_t end
);

 bool ruda_checker_private_function_set_branch(
    RudaFunctionChecker *function,
    RudaMirBlockId block,
    RudaMirRegister condition,
    RudaMirBlockId when_true,
    RudaMirBlockId when_false,
    size_t start,
    size_t end
);

 bool ruda_checker_private_function_set_panic(
    RudaFunctionChecker *function,
    RudaMirRegister message,
    size_t start,
    size_t end
);

 bool ruda_checker_private_function_set_never_call(
    RudaFunctionChecker *function,
    RudaMirCallId call,
    size_t start,
    size_t end
);

 bool ruda_checker_private_type_is_integer(RudaTypeId type) {
    return type >= RUDA_TYPE_I8 && type <= RUDA_TYPE_U64;
}

 bool ruda_checker_private_type_is_numeric(RudaTypeId type) {
    return ruda_checker_private_type_is_integer(type)
        || type == RUDA_TYPE_F32 || type == RUDA_TYPE_F64;
}
