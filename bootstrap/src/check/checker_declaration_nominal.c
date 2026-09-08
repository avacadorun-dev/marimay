#include "checker_declaration_nominal_api.h"
#include "checker_core_api.h"
#include "checker_declaration_lifecycle_api.h"
#include "checker_lookup_api.h"





#include "checker_internal.h"

 bool mmay_checker_private_collect_type_alias_declaration(
    MarimayChecker *checker,
    const MarimayAstNode *node
) {
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    const MarimayAstNode *target = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_TYPE,
        0
    );
    MarimayCheckTypeAliasDeclaration declaration;
    MarimayCheckTopLevelEntry top_level_entry;
    size_t index = 0;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    if (name_token == NULL || name_token->synthetic || target == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node != NULL
            && (element->as.node->kind == MMAY_SYNTAX_ATTRIBUTE
                || element->as.node->kind == MMAY_SYNTAX_TYPE_PARAMETERS)) {
            mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
    }
    if (checker->type_aliases.length >= checker->options.max_type_aliases) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.target = target;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.type = MMAY_TYPE_INVALID;
    declaration.state = MMAY_CHECK_TYPE_ALIAS_UNRESOLVED;
    declaration.declaration_valid = true;
    declaration.is_public = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
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
        declaration.state = MMAY_CHECK_TYPE_ALIAS_INVALID;
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
    top_level_entry.kind = MMAY_CHECK_TOP_LEVEL_TYPE_ALIAS;
    top_level_entry.index = checker->type_aliases.length;
    top_level_entry.function_count = 0;
    if (top_level_entry.index == SIZE_MAX || !mmay_vector_reserve(
            &checker->type_aliases,
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
    }
    if (!mmay_vector_push(&checker->type_aliases, &declaration)) {
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
    mmay_checker_private_type_alias_declaration_dispose(&declaration);
    return accepted && !mmay_checker_has_failed(checker);
}

 bool mmay_checker_private_evaluate_type_alias(
    MarimayChecker *checker,
    MarimayCheckTypeAliasDeclaration *declaration,
    size_t use_start,
    size_t use_end
) {
    const MarimayAstTree *previous_ast = NULL;
    bool accepted = false;
    if (checker == NULL || declaration == NULL
        || !declaration->declaration_valid) {
        if (checker != NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    if (declaration->state == MMAY_CHECK_TYPE_ALIAS_RESOLVED) {
        return true;
    }
    if (declaration->state == MMAY_CHECK_TYPE_ALIAS_INVALID) {
        return false;
    }
    if (declaration->state == MMAY_CHECK_TYPE_ALIAS_RESOLVING) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_TYPE_ALIAS_CYCLE,
            "type alias dependency cycle",
            use_start,
            use_end
        );
        return false;
    }
    if (checker->type_alias_depth >= checker->options.max_type_alias_depth) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    previous_ast = checker->ast;
    declaration->state = MMAY_CHECK_TYPE_ALIAS_RESOLVING;
    checker->type_alias_depth += 1;
    if (!mmay_checker_private_checker_set_ast(checker, declaration->ast)
        || !mmay_checker_private_check_exact_type(
            checker,
            declaration->target,
            &declaration->type
        )) {
        goto cleanup;
    }
    if (declaration->type == MMAY_TYPE_INVALID) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    declaration->state = MMAY_CHECK_TYPE_ALIAS_RESOLVED;
    accepted = true;

cleanup:
    if (!accepted) {
        declaration->state = MMAY_CHECK_TYPE_ALIAS_INVALID;
    }
    checker->type_alias_depth -= 1;
    if (previous_ast != NULL && !mmay_checker_private_checker_set_ast(checker, previous_ast)) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    return accepted && !mmay_checker_has_failed(checker);
}

 bool mmay_checker_private_evaluate_all_type_aliases(MarimayChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->type_aliases.length
        && !mmay_checker_has_failed(checker)
        && checker->result->error_count == 0; index += 1) {
        MarimayCheckTypeAliasDeclaration *declaration = mmay_vector_at(
            &checker->type_aliases,
            index
        );
        if (declaration == NULL || !mmay_checker_private_checker_set_ast(
                checker,
                declaration->ast
            ) || !mmay_checker_private_evaluate_type_alias(
                checker,
                declaration,
                declaration->name.start,
                declaration->name.end
            )) {
            if (!mmay_checker_has_failed(checker)
                && checker->result->error_count == 0) {
                mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
        if (declaration->is_public && !mmay_checker_private_check_public_type_reference(
                checker,
                declaration->target
            )) {
            if (mmay_checker_has_failed(checker)) {
                return false;
            }
            return false;
        }
    }
    return !mmay_checker_has_failed(checker)
        && checker->result->error_count == 0;
}

 bool mmay_checker_private_publish_type_aliases(MarimayChecker *checker) {
    size_t index = 0;
    if (!mmay_vector_reserve(
            &checker->result->type_aliases,
            checker->type_aliases.length
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < checker->type_aliases.length; index += 1) {
        MarimayCheckTypeAliasDeclaration *declaration = mmay_vector_at(
            &checker->type_aliases,
            index
        );
        MarimayCheckedTypeAlias alias;
        if (declaration == NULL
            || declaration->state != MMAY_CHECK_TYPE_ALIAS_RESOLVED) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        memset(&alias, 0, sizeof(alias));
        alias.name = declaration->owned_name;
        memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
        declaration->name_initialized = false;
        alias.symbol = declaration->symbol;
        alias.target_type = declaration->type;
        alias.is_public = declaration->is_public;
        if (!mmay_vector_push(&checker->result->type_aliases, &alias)) {
            mmay_owned_bytes_dispose(&alias.name);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    return true;
}
