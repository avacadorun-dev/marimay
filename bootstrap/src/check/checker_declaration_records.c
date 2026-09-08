#include "checker_declaration_records_api.h"
#include "checker_core_api.h"
#include "checker_declaration_lifecycle_api.h"
#include "checker_lookup_api.h"
#include "checker_generic_types_api.h"
#include "checker_derive_api.h"






 bool mmay_checker_private_collect_record_declaration(
    MarimayChecker *checker,
    const MarimayAstNode *node
) {
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    MarimayCheckRecordDeclaration declaration;
    MarimayCheckTopLevelEntry top_level_entry;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool local_nominal_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    if (name_token == NULL || name_token->synthetic) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (checker->records.length >= checker->options.max_records) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.type = MMAY_TYPE_INVALID;
    declaration.is_public = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
    declaration.declaration_valid = true;
    if (!mmay_checker_private_collect_derives(
            checker,
            node,
            &declaration.derives_debug,
            &declaration.derives_eq,
            &declaration.derives_eq_hash,
            &declaration.declaration_valid
        )) goto cleanup;
    duplicate_name = mmay_checker_private_declaration_name_is_duplicate(
        checker,
        declaration.name
    ) || mmay_checker_private_check_name_is_primitive_type(checker, declaration.name);
    if (duplicate_name) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            declaration.name.start,
            declaration.name.end
        );
        declaration.declaration_valid = false;
        if (mmay_checker_has_failed(checker)) {
            goto cleanup;
        }
    }
    if (!mmay_owned_bytes_copy(
            &declaration.owned_name,
            checker->result->allocator,
            checker->source + declaration.name.start,
            declaration.name.end - declaration.name.start
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.name_initialized = true;
    if (!mmay_vector_init(
            &declaration.fields,
            checker->result->allocator,
            sizeof(MarimayCheckRecordFieldDeclaration)
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.fields_initialized = true;
    declaration.type_parameters_initialized = mmay_vector_init(
        &declaration.type_parameters,
        checker->result->allocator,
        sizeof(MarimayCheckTypeParameterDeclaration)
    );
    if (!declaration.type_parameters_initialized
        || !mmay_checker_private_collect_type_parameters(
            checker, node, &declaration.type_parameters
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.is_generic = declaration.type_parameters.length != 0;
    if (!mmay_symbol_id_from_bytes(
            checker->package_name,
            checker->package_name_length,
            mmay_source_path(checker->ast->snapshot),
            mmay_source_path_length(checker->ast->snapshot),
            declaration.owned_name.data,
            declaration.owned_name.length,
            0,
            &declaration.symbol
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!duplicate_name && !mmay_type_intern_nominal(
            &checker->result->types,
            MMAY_TYPE_KIND_NOMINAL,
            declaration.symbol,
            NULL,
            0,
            &declaration.type
        )) {
        mmay_checker_fail(
            checker,
            checker->result->types.descriptors.length
                    >= checker->options.type_options.max_types
                        - MMAY_TYPE_FIRST_DYNAMIC
                ? MMAY_CHECK_LIMIT_REACHED
                : MMAY_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    top_level_entry.kind = MMAY_CHECK_TOP_LEVEL_RECORD;
    top_level_entry.index = checker->records.length;
    top_level_entry.function_count = 0;
    if (top_level_entry.index == SIZE_MAX || !mmay_vector_reserve(
            &checker->records,
            top_level_entry.index + 1
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!duplicate_name) {
        if (!mmay_map_put(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length,
                &top_level_entry
            )) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        top_level_index_inserted = true;
        if (mmay_map_get_const(
                &checker->local_nominal_index,
                &declaration.type,
                sizeof(declaration.type)
            ) != NULL) {
            (void)mmay_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!mmay_map_put(
                &checker->local_nominal_index,
                &declaration.type,
                sizeof(declaration.type),
                &declaration.symbol
            )) {
            (void)mmay_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        local_nominal_inserted = true;
    }
    if (!mmay_vector_push(&checker->records, &declaration)) {
        if (local_nominal_inserted) {
            (void)mmay_map_remove(
                &checker->local_nominal_index,
                &declaration.type,
                sizeof(declaration.type)
            );
        }
        if (top_level_index_inserted) {
            (void)mmay_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
        }
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&declaration, 0, sizeof(declaration));
    accepted = true;

cleanup:
    mmay_checker_private_record_declaration_dispose(&declaration);
    return accepted && !mmay_checker_has_failed(checker);
}

 bool mmay_checker_private_collect_enum_declaration(
    MarimayChecker *checker,
    const MarimayAstNode *node
) {
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    MarimayCheckEnumDeclaration declaration;
    MarimayCheckTopLevelEntry top_level_entry;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool local_nominal_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    if (name_token == NULL || name_token->synthetic) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (checker->enums.length >= checker->options.max_enums) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.type = MMAY_TYPE_INVALID;
    declaration.is_public = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
    declaration.declaration_valid = true;
    if (!mmay_checker_private_collect_derives(
            checker,
            node,
            &declaration.derives_debug,
            &declaration.derives_eq,
            &declaration.derives_eq_hash,
            &declaration.declaration_valid
        )) goto cleanup;
    duplicate_name = mmay_checker_private_declaration_name_is_duplicate(
        checker,
        declaration.name
    ) || mmay_checker_private_check_name_is_primitive_type(checker, declaration.name);
    if (duplicate_name) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            declaration.name.start,
            declaration.name.end
        );
        declaration.declaration_valid = false;
        if (mmay_checker_has_failed(checker)) {
            goto cleanup;
        }
    }
    if (!mmay_owned_bytes_copy(
            &declaration.owned_name,
            checker->result->allocator,
            checker->source + declaration.name.start,
            declaration.name.end - declaration.name.start
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.name_initialized = true;
    if (!mmay_vector_init(
            &declaration.variants,
            checker->result->allocator,
            sizeof(MarimayCheckEnumVariantDeclaration)
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.variants_initialized = true;
    declaration.type_parameters_initialized = mmay_vector_init(
        &declaration.type_parameters,
        checker->result->allocator,
        sizeof(MarimayCheckTypeParameterDeclaration)
    );
    if (!declaration.type_parameters_initialized
        || !mmay_checker_private_collect_type_parameters(
            checker, node, &declaration.type_parameters
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.is_generic = declaration.type_parameters.length != 0;
    if (!mmay_symbol_id_from_bytes(
            checker->package_name,
            checker->package_name_length,
            mmay_source_path(checker->ast->snapshot),
            mmay_source_path_length(checker->ast->snapshot),
            declaration.owned_name.data,
            declaration.owned_name.length,
            0,
            &declaration.symbol
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!duplicate_name && !mmay_type_intern_nominal(
            &checker->result->types,
            MMAY_TYPE_KIND_NOMINAL,
            declaration.symbol,
            NULL,
            0,
            &declaration.type
        )) {
        mmay_checker_fail(
            checker,
            checker->result->types.descriptors.length
                    >= checker->options.type_options.max_types
                        - MMAY_TYPE_FIRST_DYNAMIC
                ? MMAY_CHECK_LIMIT_REACHED
                : MMAY_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    top_level_entry.kind = MMAY_CHECK_TOP_LEVEL_ENUM;
    top_level_entry.index = checker->enums.length;
    top_level_entry.function_count = 0;
    if (top_level_entry.index == SIZE_MAX || !mmay_vector_reserve(
            &checker->enums,
            top_level_entry.index + 1
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!duplicate_name) {
        if (!mmay_map_put(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length,
                &top_level_entry
            )) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        top_level_index_inserted = true;
        if (mmay_map_get_const(
                &checker->local_nominal_index,
                &declaration.type,
                sizeof(declaration.type)
            ) != NULL) {
            (void)mmay_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!mmay_map_put(
                &checker->local_nominal_index,
                &declaration.type,
                sizeof(declaration.type),
                &declaration.symbol
            )) {
            (void)mmay_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        local_nominal_inserted = true;
    }
    if (!mmay_vector_push(&checker->enums, &declaration)) {
        if (local_nominal_inserted) {
            (void)mmay_map_remove(
                &checker->local_nominal_index,
                &declaration.type,
                sizeof(declaration.type)
            );
        }
        if (top_level_index_inserted) {
            (void)mmay_map_remove(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
        }
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&declaration, 0, sizeof(declaration));
    accepted = true;

cleanup:
    mmay_checker_private_enum_declaration_dispose(&declaration);
    return accepted && !mmay_checker_has_failed(checker);
}

 bool mmay_checker_private_record_field_name_is_duplicate(
    const MarimayChecker *checker,
    const MarimayCheckRecordDeclaration *declaration,
    MarimayCheckName name
) {
    size_t index = 0;
    for (index = 0; index < declaration->fields.length; index += 1) {
        const MarimayCheckRecordFieldDeclaration *field = mmay_vector_at_const(
            &declaration->fields,
            index
        );
        if (field != NULL && mmay_checker_private_check_names_equal(checker, field->name, name)) {
            return true;
        }
    }
    return false;
}

 bool mmay_checker_private_resolve_record_fields(
    MarimayChecker *checker,
    MarimayCheckRecordDeclaration *declaration
) {
    size_t child_index = 0;
    size_t field_count = 0;
    bool valid = true;
    if (declaration == NULL || !declaration->declaration_valid
        || !declaration->fields_initialized
        || !mmay_checker_private_checker_set_ast(checker, declaration->ast)) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    checker->active_type_parameters = &declaration->type_parameters;
    for (child_index = 0; child_index < declaration->node->child_count;
        child_index += 1) {
        const MarimayAstElement *element = mmay_ast_child(
            declaration->node,
            child_index
        );
        MarimayCheckRecordFieldDeclaration field;
        const MarimaySyntaxToken *name_token = NULL;
        const MarimayAstNode *type_node = NULL;
        bool duplicate = false;
        if (element == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind != MMAY_AST_ELEMENT_NODE
            || element->as.node == NULL
            || element->as.node->kind != MMAY_SYNTAX_FIELD_DECL) {
            continue;
        }
        if (field_count >= checker->options.max_fields_per_record) {
            mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
            return false;
        }
        field_count += 1;
        memset(&field, 0, sizeof(field));
        field.node = element->as.node;
        name_token = mmay_checker_private_check_child_token(
            field.node,
            MMAY_TOKEN_IDENTIFIER,
            0
        );
        type_node = mmay_checker_private_check_child_node(field.node, MMAY_SYNTAX_TYPE, 0);
        if (name_token == NULL || name_token->synthetic || type_node == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            mmay_checker_private_record_field_declaration_dispose(&field);
            return false;
        }
        field.name.snapshot = checker->ast->snapshot;
        field.name.start = name_token->start;
        field.name.end = name_token->end;
        field.type = MMAY_TYPE_INVALID;
        field.is_public = mmay_checker_private_check_child_token(
            field.node,
            MMAY_TOKEN_KW_PUBLIC,
            0
        ) != NULL;
        field.is_mutable = mmay_checker_private_check_child_token(
            field.node,
            MMAY_TOKEN_KW_MUTABLE,
            0
        ) != NULL;
        duplicate = mmay_checker_private_record_field_name_is_duplicate(
            checker,
            declaration,
            field.name
        );
        if (duplicate) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_DUPLICATE_NAME,
                "field name is already declared in this record",
                field.name.start,
                field.name.end
            );
            valid = false;
            if (mmay_checker_has_failed(checker)) {
                mmay_checker_private_record_field_declaration_dispose(&field);
                return false;
            }
            continue;
        }
        if (!mmay_owned_bytes_copy(
                &field.owned_name,
                checker->result->allocator,
                checker->source + field.name.start,
                field.name.end - field.name.start
            )) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            mmay_checker_private_record_field_declaration_dispose(&field);
            return false;
        }
        field.name_initialized = true;
        if (!mmay_checker_private_check_exact_type(checker, type_node, &field.type)) {
            if (mmay_checker_has_failed(checker)) {
                mmay_checker_private_record_field_declaration_dispose(&field);
                return false;
            }
            valid = false;
        } else if (declaration->is_public && field.is_public
            && !mmay_checker_private_check_public_type_reference(checker, type_node)) {
            if (mmay_checker_has_failed(checker)) {
                mmay_checker_private_record_field_declaration_dispose(&field);
                return false;
            }
            valid = false;
        }
        if (field.is_mutable
            && (declaration->derives_eq
                || declaration->derives_eq_hash)) {
            (void)mmay_checker_add_diagnostic(
                checker, MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "Eq and Hash cannot be derived for a record with mutable fields",
                field.node->start, field.node->end
            );
            if (mmay_checker_has_failed(checker)) {
                mmay_checker_private_record_field_declaration_dispose(&field);
                return false;
            }
            valid = false;
        }
        if (!mmay_vector_push(&declaration->fields, &field)) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            mmay_checker_private_record_field_declaration_dispose(&field);
            return false;
        }
        memset(&field, 0, sizeof(field));
    }
    if (checker->active_type_parameters == &declaration->type_parameters) {
        checker->active_type_parameters = NULL;
    }
    declaration->fields_resolved = valid;
    return valid && !mmay_checker_has_failed(checker);
}

 bool mmay_checker_private_resolve_all_record_fields(MarimayChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->records.length
        && !mmay_checker_has_failed(checker)
        && checker->result->error_count == 0; index += 1) {
        MarimayCheckRecordDeclaration *declaration = mmay_vector_at(
            &checker->records,
            index
        );
        if (declaration == NULL || !mmay_checker_private_resolve_record_fields(
                checker,
                declaration
            )) {
            if (!mmay_checker_has_failed(checker)
                && checker->result->error_count == 0) {
                mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    return !mmay_checker_has_failed(checker)
        && checker->result->error_count == 0;
}
