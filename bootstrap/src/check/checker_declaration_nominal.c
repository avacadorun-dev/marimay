#include "checker_declaration_nominal_api.h"
#include "checker_core_api.h"
#include "checker_declaration_lifecycle_api.h"
#include "checker_lookup_api.h"





#include "checker_internal.h"

 bool ruda_checker_private_collect_type_alias_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    const RudaAstNode *target = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_TYPE,
        0
    );
    RudaCheckTypeAliasDeclaration declaration;
    RudaCheckTopLevelEntry top_level_entry;
    size_t index = 0;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    if (name_token == NULL || name_token->synthetic || target == NULL) {
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
    if (checker->type_aliases.length >= checker->options.max_type_aliases) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.target = target;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.type = RUDA_TYPE_INVALID;
    declaration.state = RUDA_CHECK_TYPE_ALIAS_UNRESOLVED;
    declaration.declaration_valid = true;
    declaration.is_public = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
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
        declaration.state = RUDA_CHECK_TYPE_ALIAS_INVALID;
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
    top_level_entry.kind = RUDA_CHECK_TOP_LEVEL_TYPE_ALIAS;
    top_level_entry.index = checker->type_aliases.length;
    top_level_entry.function_count = 0;
    if (top_level_entry.index == SIZE_MAX || !ruda_vector_reserve(
            &checker->type_aliases,
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
    }
    if (!ruda_vector_push(&checker->type_aliases, &declaration)) {
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
    ruda_checker_private_type_alias_declaration_dispose(&declaration);
    return accepted && !ruda_checker_has_failed(checker);
}

 bool ruda_checker_private_evaluate_type_alias(
    RudaChecker *checker,
    RudaCheckTypeAliasDeclaration *declaration,
    size_t use_start,
    size_t use_end
) {
    const RudaAstTree *previous_ast = NULL;
    bool accepted = false;
    if (checker == NULL || declaration == NULL
        || !declaration->declaration_valid) {
        if (checker != NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    if (declaration->state == RUDA_CHECK_TYPE_ALIAS_RESOLVED) {
        return true;
    }
    if (declaration->state == RUDA_CHECK_TYPE_ALIAS_INVALID) {
        return false;
    }
    if (declaration->state == RUDA_CHECK_TYPE_ALIAS_RESOLVING) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_TYPE_ALIAS_CYCLE,
            "type alias dependency cycle",
            use_start,
            use_end
        );
        return false;
    }
    if (checker->type_alias_depth >= checker->options.max_type_alias_depth) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    previous_ast = checker->ast;
    declaration->state = RUDA_CHECK_TYPE_ALIAS_RESOLVING;
    checker->type_alias_depth += 1;
    if (!ruda_checker_private_checker_set_ast(checker, declaration->ast)
        || !ruda_checker_private_check_exact_type(
            checker,
            declaration->target,
            &declaration->type
        )) {
        goto cleanup;
    }
    if (declaration->type == RUDA_TYPE_INVALID) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    declaration->state = RUDA_CHECK_TYPE_ALIAS_RESOLVED;
    accepted = true;

cleanup:
    if (!accepted) {
        declaration->state = RUDA_CHECK_TYPE_ALIAS_INVALID;
    }
    checker->type_alias_depth -= 1;
    if (previous_ast != NULL && !ruda_checker_private_checker_set_ast(checker, previous_ast)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    return accepted && !ruda_checker_has_failed(checker);
}

 bool ruda_checker_private_evaluate_all_type_aliases(RudaChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->type_aliases.length
        && !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0; index += 1) {
        RudaCheckTypeAliasDeclaration *declaration = ruda_vector_at(
            &checker->type_aliases,
            index
        );
        if (declaration == NULL || !ruda_checker_private_checker_set_ast(
                checker,
                declaration->ast
            ) || !ruda_checker_private_evaluate_type_alias(
                checker,
                declaration,
                declaration->name.start,
                declaration->name.end
            )) {
            if (!ruda_checker_has_failed(checker)
                && checker->result->diagnostics.length == 0) {
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
        if (declaration->is_public && !ruda_checker_private_check_public_type_reference(
                checker,
                declaration->target
            )) {
            if (ruda_checker_has_failed(checker)) {
                return false;
            }
            return false;
        }
    }
    return !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0;
}

 bool ruda_checker_private_publish_type_aliases(RudaChecker *checker) {
    size_t index = 0;
    if (!ruda_vector_reserve(
            &checker->result->type_aliases,
            checker->type_aliases.length
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < checker->type_aliases.length; index += 1) {
        RudaCheckTypeAliasDeclaration *declaration = ruda_vector_at(
            &checker->type_aliases,
            index
        );
        RudaCheckedTypeAlias alias;
        if (declaration == NULL
            || declaration->state != RUDA_CHECK_TYPE_ALIAS_RESOLVED) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        memset(&alias, 0, sizeof(alias));
        alias.name = declaration->owned_name;
        memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
        declaration->name_initialized = false;
        alias.symbol = declaration->symbol;
        alias.target_type = declaration->type;
        alias.is_public = declaration->is_public;
        if (!ruda_vector_push(&checker->result->type_aliases, &alias)) {
            ruda_owned_bytes_dispose(&alias.name);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    return true;
}
