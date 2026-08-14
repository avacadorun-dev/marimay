#include "checker_declaration_records_api.h"
#include "checker_core_api.h"
#include "checker_declaration_lifecycle_api.h"
#include "checker_lookup_api.h"






 bool ruda_checker_private_collect_record_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    RudaCheckRecordDeclaration declaration;
    RudaCheckTopLevelEntry top_level_entry;
    size_t index = 0;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool local_nominal_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    if (name_token == NULL || name_token->synthetic) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL
            && (element->as.node->kind == RUDA_SYNTAX_ATTRIBUTE
                || element->as.node->kind == RUDA_SYNTAX_TYPE_PARAMETERS)) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
    }
    if (checker->records.length >= checker->options.max_records) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.type = RUDA_TYPE_INVALID;
    declaration.is_public = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
    declaration.declaration_valid = true;
    duplicate_name = ruda_checker_private_declaration_name_is_duplicate(
        checker,
        declaration.name
    ) || ruda_checker_private_check_name_is_primitive_type(checker, declaration.name);
    if (duplicate_name) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            declaration.name.start,
            declaration.name.end
        );
        declaration.declaration_valid = false;
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
    }
    if (!ruda_owned_bytes_copy(
            &declaration.owned_name,
            checker->result->allocator,
            checker->source + declaration.name.start,
            declaration.name.end - declaration.name.start
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.name_initialized = true;
    if (!ruda_vector_init(
            &declaration.fields,
            checker->result->allocator,
            sizeof(RudaCheckRecordFieldDeclaration)
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.fields_initialized = true;
    if (!ruda_symbol_id_from_bytes(
            checker->result->package_name.data,
            checker->result->package_name.length,
            ruda_source_path(checker->ast->snapshot),
            ruda_source_path_length(checker->ast->snapshot),
            declaration.owned_name.data,
            declaration.owned_name.length,
            0,
            &declaration.symbol
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!duplicate_name && !ruda_type_intern_nominal(
            &checker->result->types,
            RUDA_TYPE_KIND_NOMINAL,
            declaration.symbol,
            NULL,
            0,
            &declaration.type
        )) {
        ruda_checker_fail(
            checker,
            checker->result->types.descriptors.length
                    >= checker->options.type_options.max_types
                        - RUDA_TYPE_FIRST_DYNAMIC
                ? RUDA_CHECK_LIMIT_REACHED
                : RUDA_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    top_level_entry.kind = RUDA_CHECK_TOP_LEVEL_RECORD;
    top_level_entry.index = checker->records.length;
    top_level_entry.function_count = 0;
    if (top_level_entry.index == SIZE_MAX || !ruda_vector_reserve(
            &checker->records,
            top_level_entry.index + 1
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!duplicate_name) {
        if (!ruda_map_put(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length,
                &top_level_entry
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        top_level_index_inserted = true;
        if (ruda_map_get_const(
                &checker->local_nominal_index,
                &declaration.type,
                sizeof(declaration.type)
            ) != NULL) {
            (void)ruda_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!ruda_map_put(
                &checker->local_nominal_index,
                &declaration.type,
                sizeof(declaration.type),
                &declaration.symbol
            )) {
            (void)ruda_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        local_nominal_inserted = true;
    }
    if (!ruda_vector_push(&checker->records, &declaration)) {
        if (local_nominal_inserted) {
            (void)ruda_map_remove(
                &checker->local_nominal_index,
                &declaration.type,
                sizeof(declaration.type)
            );
        }
        if (top_level_index_inserted) {
            (void)ruda_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
        }
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&declaration, 0, sizeof(declaration));
    accepted = true;

cleanup:
    ruda_checker_private_record_declaration_dispose(&declaration);
    return accepted && !ruda_checker_has_failed(checker);
}

 bool ruda_checker_private_collect_enum_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    RudaCheckEnumDeclaration declaration;
    RudaCheckTopLevelEntry top_level_entry;
    size_t index = 0;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool local_nominal_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    if (name_token == NULL || name_token->synthetic) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL
            && (element->as.node->kind == RUDA_SYNTAX_ATTRIBUTE
                || element->as.node->kind == RUDA_SYNTAX_TYPE_PARAMETERS)) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
    }
    if (checker->enums.length >= checker->options.max_enums) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.type = RUDA_TYPE_INVALID;
    declaration.is_public = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
    declaration.declaration_valid = true;
    duplicate_name = ruda_checker_private_declaration_name_is_duplicate(
        checker,
        declaration.name
    ) || ruda_checker_private_check_name_is_primitive_type(checker, declaration.name);
    if (duplicate_name) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            declaration.name.start,
            declaration.name.end
        );
        declaration.declaration_valid = false;
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
    }
    if (!ruda_owned_bytes_copy(
            &declaration.owned_name,
            checker->result->allocator,
            checker->source + declaration.name.start,
            declaration.name.end - declaration.name.start
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.name_initialized = true;
    if (!ruda_vector_init(
            &declaration.variants,
            checker->result->allocator,
            sizeof(RudaCheckEnumVariantDeclaration)
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.variants_initialized = true;
    if (!ruda_symbol_id_from_bytes(
            checker->result->package_name.data,
            checker->result->package_name.length,
            ruda_source_path(checker->ast->snapshot),
            ruda_source_path_length(checker->ast->snapshot),
            declaration.owned_name.data,
            declaration.owned_name.length,
            0,
            &declaration.symbol
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!duplicate_name && !ruda_type_intern_nominal(
            &checker->result->types,
            RUDA_TYPE_KIND_NOMINAL,
            declaration.symbol,
            NULL,
            0,
            &declaration.type
        )) {
        ruda_checker_fail(
            checker,
            checker->result->types.descriptors.length
                    >= checker->options.type_options.max_types
                        - RUDA_TYPE_FIRST_DYNAMIC
                ? RUDA_CHECK_LIMIT_REACHED
                : RUDA_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    top_level_entry.kind = RUDA_CHECK_TOP_LEVEL_ENUM;
    top_level_entry.index = checker->enums.length;
    top_level_entry.function_count = 0;
    if (top_level_entry.index == SIZE_MAX || !ruda_vector_reserve(
            &checker->enums,
            top_level_entry.index + 1
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!duplicate_name) {
        if (!ruda_map_put(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length,
                &top_level_entry
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        top_level_index_inserted = true;
        if (ruda_map_get_const(
                &checker->local_nominal_index,
                &declaration.type,
                sizeof(declaration.type)
            ) != NULL) {
            (void)ruda_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!ruda_map_put(
                &checker->local_nominal_index,
                &declaration.type,
                sizeof(declaration.type),
                &declaration.symbol
            )) {
            (void)ruda_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        local_nominal_inserted = true;
    }
    if (!ruda_vector_push(&checker->enums, &declaration)) {
        if (local_nominal_inserted) {
            (void)ruda_map_remove(
                &checker->local_nominal_index,
                &declaration.type,
                sizeof(declaration.type)
            );
        }
        if (top_level_index_inserted) {
            (void)ruda_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
        }
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&declaration, 0, sizeof(declaration));
    accepted = true;

cleanup:
    ruda_checker_private_enum_declaration_dispose(&declaration);
    return accepted && !ruda_checker_has_failed(checker);
}

 bool ruda_checker_private_record_field_name_is_duplicate(
    const RudaChecker *checker,
    const RudaCheckRecordDeclaration *declaration,
    RudaCheckName name
) {
    size_t index = 0;
    for (index = 0; index < declaration->fields.length; index += 1) {
        const RudaCheckRecordFieldDeclaration *field = ruda_vector_at_const(
            &declaration->fields,
            index
        );
        if (field != NULL && ruda_checker_private_check_names_equal(checker, field->name, name)) {
            return true;
        }
    }
    return false;
}

 bool ruda_checker_private_resolve_record_fields(
    RudaChecker *checker,
    RudaCheckRecordDeclaration *declaration
) {
    size_t child_index = 0;
    size_t field_count = 0;
    bool valid = true;
    if (declaration == NULL || !declaration->declaration_valid
        || !declaration->fields_initialized
        || !ruda_checker_private_checker_set_ast(checker, declaration->ast)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (child_index = 0; child_index < declaration->node->child_count;
        child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(
            declaration->node,
            child_index
        );
        RudaCheckRecordFieldDeclaration field;
        const RudaSyntaxToken *name_token = NULL;
        const RudaAstNode *type_node = NULL;
        bool duplicate = false;
        if (element == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind != RUDA_AST_ELEMENT_NODE
            || element->as.node == NULL
            || element->as.node->kind != RUDA_SYNTAX_FIELD_DECL) {
            continue;
        }
        if (field_count >= checker->options.max_fields_per_record) {
            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
            return false;
        }
        field_count += 1;
        memset(&field, 0, sizeof(field));
        field.node = element->as.node;
        name_token = ruda_checker_private_check_child_token(
            field.node,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        type_node = ruda_checker_private_check_child_node(field.node, RUDA_SYNTAX_TYPE, 0);
        if (name_token == NULL || name_token->synthetic || type_node == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            ruda_checker_private_record_field_declaration_dispose(&field);
            return false;
        }
        field.name.snapshot = checker->ast->snapshot;
        field.name.start = name_token->start;
        field.name.end = name_token->end;
        field.type = RUDA_TYPE_INVALID;
        field.is_public = ruda_checker_private_check_child_token(
            field.node,
            RUDA_TOKEN_KW_PUBLIC,
            0
        ) != NULL;
        field.is_mutable = ruda_checker_private_check_child_token(
            field.node,
            RUDA_TOKEN_KW_MUTABLE,
            0
        ) != NULL;
        duplicate = ruda_checker_private_record_field_name_is_duplicate(
            checker,
            declaration,
            field.name
        );
        if (duplicate) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME,
                "field name is already declared in this record",
                field.name.start,
                field.name.end
            );
            valid = false;
            if (ruda_checker_has_failed(checker)) {
                ruda_checker_private_record_field_declaration_dispose(&field);
                return false;
            }
            continue;
        }
        if (!ruda_owned_bytes_copy(
                &field.owned_name,
                checker->result->allocator,
                checker->source + field.name.start,
                field.name.end - field.name.start
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            ruda_checker_private_record_field_declaration_dispose(&field);
            return false;
        }
        field.name_initialized = true;
        if (!ruda_checker_private_check_exact_type(checker, type_node, &field.type)) {
            if (ruda_checker_has_failed(checker)) {
                ruda_checker_private_record_field_declaration_dispose(&field);
                return false;
            }
            valid = false;
        } else if (declaration->is_public && field.is_public
            && !ruda_checker_private_check_public_type_reference(checker, type_node)) {
            if (ruda_checker_has_failed(checker)) {
                ruda_checker_private_record_field_declaration_dispose(&field);
                return false;
            }
            valid = false;
        }
        if (!ruda_vector_push(&declaration->fields, &field)) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            ruda_checker_private_record_field_declaration_dispose(&field);
            return false;
        }
        memset(&field, 0, sizeof(field));
    }
    declaration->fields_resolved = valid;
    return valid && !ruda_checker_has_failed(checker);
}

 bool ruda_checker_private_resolve_all_record_fields(RudaChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->records.length
        && !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0; index += 1) {
        RudaCheckRecordDeclaration *declaration = ruda_vector_at(
            &checker->records,
            index
        );
        if (declaration == NULL || !ruda_checker_private_resolve_record_fields(
                checker,
                declaration
            )) {
            if (!ruda_checker_has_failed(checker)
                && checker->result->diagnostics.length == 0) {
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    return !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0;
}
