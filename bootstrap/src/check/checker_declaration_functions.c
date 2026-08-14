#include "checker_declaration_functions_api.h"
#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_declaration_lifecycle_api.h"
#include "checker_expr_interfaces_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_lookup_api.h"
#include "checker_statement_api.h"






 bool ruda_checker_private_collect_function_signature(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    const RudaAstNode *parameters = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_PARAMETERS,
        0
    );
    const RudaAstNode *return_type_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_TYPE,
        0
    );
    RudaCheckFunctionDeclaration declaration;
    RudaCheckTopLevelEntry top_level_entry;
    size_t diagnostics_before = checker->result->diagnostics.length;
    size_t index = 0;
    bool attribute_valid = true;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool callable_index_inserted = false;
    bool overload_group = false;
    bool overload_count_incremented = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    declaration.block = ruda_checker_private_check_child_node(node, RUDA_SYNTAX_BLOCK, 0);
    if (name_token == NULL || name_token->synthetic || parameters == NULL
        || return_type_node == NULL || declaration.block == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node->kind == RUDA_SYNTAX_ATTRIBUTE) {
            if (ruda_checker_private_check_attribute_is(checker, element->as.node, "test")) {
                if (declaration.is_test || declaration.is_fuzz
                    || ruda_checker_private_check_child_node(
                        element->as.node,
                        RUDA_SYNTAX_ARGUMENTS,
                        0
                    ) != NULL) {
                    (void)ruda_checker_add_diagnostic(
                        checker,
                        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                        "@test takes no arguments and cannot be combined with @fuzz",
                        element->as.node->start,
                        element->as.node->end
                    );
                    attribute_valid = false;
                } else {
                    declaration.is_test = true;
                }
                continue;
            }
            if (ruda_checker_private_check_attribute_is(checker, element->as.node, "fuzz")) {
                if (declaration.is_test || declaration.is_fuzz
                    || ruda_checker_private_check_child_node(
                        element->as.node,
                        RUDA_SYNTAX_ARGUMENTS,
                        0
                    ) != NULL) {
                    (void)ruda_checker_add_diagnostic(
                        checker,
                        RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                        "@fuzz takes no arguments and cannot be combined with @test",
                        element->as.node->start,
                        element->as.node->end
                    );
                    attribute_valid = false;
                } else {
                    declaration.is_fuzz = true;
                }
                continue;
            }
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "attribute is not valid on an ordinary function",
                element->as.node->start,
                element->as.node->end
            );
            attribute_valid = false;
            continue;
        }
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node->kind == RUDA_SYNTAX_TYPE_PARAMETERS) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_TOKEN
            && element->as.token.kind == RUDA_TOKEN_KW_UNSAFE) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
    }
    if (checker->declarations.length >= checker->options.max_functions) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.signature_valid = attribute_valid;
    declaration.is_public = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
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
    declaration.parameter_names_initialized = ruda_vector_init(
        &declaration.parameter_names,
        checker->result->allocator,
        sizeof(RudaCheckName)
    );
    declaration.parameter_types_initialized = ruda_vector_init(
        &declaration.parameter_types,
        checker->result->allocator,
        sizeof(RudaTypeId)
    );
    if (!declaration.parameter_names_initialized
        || !declaration.parameter_types_initialized) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    for (index = 0; index < parameters->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(parameters, index);
        const RudaSyntaxToken *parameter_name = NULL;
        const RudaAstNode *type_node = NULL;
        RudaCheckName name;
        RudaTypeId type = RUDA_TYPE_INVALID;
        if (element == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind == RUDA_AST_ELEMENT_TOKEN) {
            if (element->as.token.synthetic) {
                ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
                goto cleanup;
            }
            continue;
        }
        if (element->as.node->kind != RUDA_SYNTAX_PARAMETER) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
        parameter_name = ruda_checker_private_check_child_token(
            element->as.node,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        type_node = ruda_checker_private_check_child_node(
            element->as.node,
            RUDA_SYNTAX_TYPE,
            0
        );
        if (parameter_name == NULL || parameter_name->synthetic
            || type_node == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (declaration.parameter_names.length
            >= checker->options.max_locals_per_function
            || declaration.parameter_types.length
                >= checker->options.mir_options.max_registers) {
            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        name.snapshot = checker->ast->snapshot;
        name.start = parameter_name->start;
        name.end = parameter_name->end;
        if (ruda_checker_private_parameter_name_is_duplicate(checker, &declaration, name)) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME,
                "name is already declared in this scope",
                name.start,
                name.end
            );
            declaration.signature_valid = false;
            if (ruda_checker_has_failed(checker)) {
                goto cleanup;
            }
        }
        if (!ruda_checker_private_check_exact_type(checker, type_node, &type)) {
            if (ruda_checker_has_failed(checker)) {
                goto cleanup;
            }
            declaration.signature_valid = false;
        } else if (declaration.is_public
            && !ruda_checker_private_check_public_type_reference(checker, type_node)) {
            if (ruda_checker_has_failed(checker)) {
                goto cleanup;
            }
            declaration.signature_valid = false;
        }
        if (!ruda_vector_push(&declaration.parameter_names, &name)
            || !ruda_vector_push(&declaration.parameter_types, &type)) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
    }
    if (!ruda_checker_private_check_exact_type(
            checker,
            return_type_node,
            &declaration.result_type
        )) {
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
        declaration.signature_valid = false;
    } else if (declaration.is_public
        && !ruda_checker_private_check_public_type_reference(checker, return_type_node)) {
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
        declaration.signature_valid = false;
    }
    if (checker->result->diagnostics.length != diagnostics_before) {
        declaration.signature_valid = false;
    }
    if (declaration.signature_valid) {
        if ((declaration.is_test || declaration.is_fuzz)
            && !ruda_checker_private_check_source_is_test_file(checker)) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "@test and @fuzz require a file ending _test.ruda",
                declaration.name.start,
                declaration.name.end
            );
            declaration.signature_valid = false;
        } else if ((declaration.is_test || declaration.is_fuzz)
            && declaration.is_public) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "test and fuzz functions cannot be public",
                declaration.name.start,
                declaration.name.end
            );
            declaration.signature_valid = false;
        } else if (declaration.is_test
            && (declaration.parameter_types.length != 0
                || declaration.result_type != RUDA_TYPE_VOID)) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "@test requires a non-generic fn(): void declaration",
                declaration.name.start,
                declaration.name.end
            );
            declaration.signature_valid = false;
        } else if (declaration.is_fuzz
            && (declaration.parameter_types.length != 1
                || declaration.result_type != RUDA_TYPE_VOID)) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "@fuzz requires exactly one parameter and a void result",
                declaration.name.start,
                declaration.name.end
            );
            declaration.signature_valid = false;
        }
    }
    if (declaration.signature_valid) {
        if (declaration.parameter_types.length == SIZE_MAX
            || declaration.parameter_types.length + 1
                > checker->options.type_options.max_type_arguments) {
            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        if (!ruda_type_intern_function(
                &checker->result->types,
                declaration.parameter_types.data,
                declaration.parameter_types.length,
                declaration.result_type,
                &declaration.signature_type
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
        if (declaration.parameter_types.length != 0) {
            const RudaTypeId *first_type = ruda_vector_at_const(
                &declaration.parameter_types,
                0
            );
            if (first_type != NULL && ruda_check_owned_receiver(
                    checker,
                    *first_type,
                    &declaration.receiver_symbol
                )) {
                declaration.dot_eligible = true;
                declaration.receiver_type = *first_type;
                if (!ruda_symbol_id_from_bytes_with_receiver(
                        checker->result->package_name.data,
                        checker->result->package_name.length,
                        ruda_source_path(checker->ast->snapshot),
                        ruda_source_path_length(checker->ast->snapshot),
                        declaration.owned_name.data,
                        declaration.owned_name.length,
                        declaration.receiver_symbol,
                        &declaration.symbol
                    )) {
                    ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
                    goto cleanup;
                }
            } else if (!ruda_symbol_id_from_bytes(
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
        } else if (!ruda_symbol_id_from_bytes(
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
    }
    if (declaration.signature_valid) {
        const RudaCheckTopLevelEntry *existing = ruda_map_get_const(
            &checker->top_level_index,
            declaration.owned_name.data,
            declaration.owned_name.length
        );
        if (existing != NULL) {
            const RudaCheckFunctionDeclaration *resolved = NULL;
            RudaCheckCallResolution resolution = RUDA_CHECK_CALL_INTERNAL_ERROR;
            const RudaCheckFunctionDeclaration *first = NULL;
            if (existing->kind == RUDA_CHECK_TOP_LEVEL_FUNCTION) {
                first = ruda_vector_at_const(
                    &checker->declarations,
                    existing->index
                );
            }
            if (existing->kind != RUDA_CHECK_TOP_LEVEL_FUNCTION
                || first == NULL || !first->signature_valid
                || !first->dot_eligible || !declaration.dot_eligible) {
                duplicate_name = true;
            } else {
                resolution = ruda_check_resolve_call(
                    checker,
                    declaration.name,
                    true,
                    declaration.receiver_type,
                    &resolved
                );
                if (resolution == RUDA_CHECK_CALL_FOUND) {
                    duplicate_name = true;
                } else if (resolution == RUDA_CHECK_CALL_OTHER_NOMINAL) {
                    overload_group = true;
                } else {
                    ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
                    goto cleanup;
                }
            }
            if (duplicate_name) {
                (void)ruda_checker_add_diagnostic(
                    checker,
                    RUDA_DIAGNOSTIC_DUPLICATE_NAME,
                    "name is already declared in this scope",
                    declaration.name.start,
                    declaration.name.end
                );
                declaration.signature_valid = false;
                if (ruda_checker_has_failed(checker)) {
                    goto cleanup;
                }
            }
        }
    }
    top_level_entry.kind = RUDA_CHECK_TOP_LEVEL_FUNCTION;
    top_level_entry.index = checker->declarations.length;
    top_level_entry.function_count = 1;
    if (top_level_entry.index == SIZE_MAX || !ruda_vector_reserve(
            &checker->declarations,
            top_level_entry.index + 1
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (declaration.signature_valid && !overload_group) {
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
    if (declaration.signature_valid && declaration.dot_eligible) {
        if (!ruda_check_callable_insert(
                checker,
                &declaration,
                top_level_entry.index
            )) {
            if (top_level_index_inserted) {
                (void)ruda_map_remove(
                    &checker->top_level_index,
                    declaration.owned_name.data,
                    declaration.owned_name.length
                );
            }
            goto cleanup;
        }
        callable_index_inserted = true;
    }
    if (overload_group) {
        RudaCheckTopLevelEntry *group = ruda_map_get(
            &checker->top_level_index,
            declaration.owned_name.data,
            declaration.owned_name.length
        );
        if (group == NULL || group->kind != RUDA_CHECK_TOP_LEVEL_FUNCTION
            || group->function_count == SIZE_MAX) {
            if (callable_index_inserted) {
                (void)ruda_check_callable_remove(checker, &declaration);
            }
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        group->function_count += 1;
        overload_count_incremented = true;
    }
    if (!ruda_vector_push(&checker->declarations, &declaration)) {
        if (overload_count_incremented) {
            RudaCheckTopLevelEntry *group = ruda_map_get(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
            if (group != NULL && group->function_count != 0) {
                group->function_count -= 1;
            }
        }
        if (callable_index_inserted) {
            (void)ruda_check_callable_remove(checker, &declaration);
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
    ruda_checker_private_function_declaration_dispose(&declaration);
    return accepted && !ruda_checker_has_failed(checker);
}

 bool ruda_checker_private_collect_constant_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    const RudaAstNode *type_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_TYPE,
        0
    );
    const RudaAstNode *initializer = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    RudaCheckConstantDeclaration declaration;
    RudaCheckTopLevelEntry top_level_entry;
    size_t diagnostics_before = checker->result->diagnostics.length;
    size_t index = 0;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    if (name_token == NULL || name_token->synthetic || initializer == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL
            && element->as.node->kind == RUDA_SYNTAX_ATTRIBUTE) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
    }
    if (checker->constants.length >= checker->options.max_constants) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.initializer = initializer;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.type = RUDA_TYPE_INVALID;
    declaration.state = RUDA_CHECK_CONSTANT_UNEVALUATED;
    declaration.declaration_valid = true;
    declaration.is_public = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
    duplicate_name = ruda_checker_private_declaration_name_is_duplicate(
        checker,
        declaration.name
    );
    if (duplicate_name) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            declaration.name.start,
            declaration.name.end
        );
        declaration.declaration_valid = false;
        declaration.state = RUDA_CHECK_CONSTANT_INVALID;
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
    if (type_node != NULL
        && !ruda_checker_private_check_exact_type(checker, type_node, &declaration.type)) {
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
        declaration.declaration_valid = false;
        declaration.state = RUDA_CHECK_CONSTANT_INVALID;
    } else if (type_node != NULL && declaration.is_public
        && !ruda_checker_private_check_public_type_reference(checker, type_node)) {
        if (ruda_checker_has_failed(checker)) {
            goto cleanup;
        }
        declaration.declaration_valid = false;
        declaration.state = RUDA_CHECK_CONSTANT_INVALID;
    }
    if (declaration.type != RUDA_TYPE_INVALID
        && declaration.type != RUDA_TYPE_I64
        && declaration.type != RUDA_TYPE_F64
        && declaration.type != RUDA_TYPE_BOOL) {
        ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (checker->result->diagnostics.length != diagnostics_before) {
        declaration.declaration_valid = false;
        declaration.state = RUDA_CHECK_CONSTANT_INVALID;
    }
    top_level_entry.kind = RUDA_CHECK_TOP_LEVEL_CONSTANT;
    top_level_entry.index = checker->constants.length;
    top_level_entry.function_count = 0;
    if (top_level_entry.index == SIZE_MAX || !ruda_vector_reserve(
            &checker->constants,
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
    if (!ruda_vector_push(&checker->constants, &declaration)) {
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
    ruda_checker_private_constant_declaration_dispose(&declaration);
    return accepted && !ruda_checker_has_failed(checker);
}

 bool ruda_checker_private_bind_function_parameters(
    RudaFunctionChecker *function,
    const RudaCheckFunctionDeclaration *declaration
) {
    size_t index = 0;
    if (declaration->parameter_names.length
        != declaration->parameter_types.length) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < declaration->parameter_names.length; index += 1) {
        const RudaCheckName *name = ruda_vector_at_const(
            &declaration->parameter_names,
            index
        );
        const RudaTypeId *type = ruda_vector_at_const(
            &declaration->parameter_types,
            index
        );
        RudaCheckLocal local;
        if (name == NULL || type == NULL || *type == RUDA_TYPE_INVALID
            || index > UINT32_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        local.name = *name;
        local.type = *type;
        local.register_id = (RudaMirRegister)index;
        local.scope_depth = function->scope_depth;
        local.mutable_binding = false;
        local.initialized = true;
        if (!ruda_vector_push(&function->locals, &local)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            return false;
        }
    }
    return true;
}

 bool ruda_checker_private_check_function_body(
    RudaChecker *checker,
    RudaCheckFunctionDeclaration *declaration
) {
    RudaFunctionChecker function;
    RudaCheckedFunction checked;
    RudaMirVerifyStatus verify_status;
    size_t function_index = 0;
    size_t layout_index = 0;
    bool locals_initialized = false;
    bool loops_initialized = false;
    bool accepted = false;
    memset(&function, 0, sizeof(function));
    memset(&checked, 0, sizeof(checked));
    if (declaration == NULL || declaration->ast == NULL
        || !declaration->signature_valid
        || declaration->signature_type == RUDA_TYPE_INVALID
        || declaration->block == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (!ruda_checker_private_checker_set_ast(checker, declaration->ast)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    function.checker = checker;
    function.block = RUDA_MIR_NO_BLOCK;
    function.result_type = declaration->result_type;
    function.diagnostic_start = checker->result->diagnostics.length;
    locals_initialized = ruda_vector_init(
        &function.locals,
        checker->result->allocator,
        sizeof(RudaCheckLocal)
    );
    loops_initialized = ruda_vector_init(
        &function.loops,
        checker->result->allocator,
        sizeof(RudaLoopContext)
    );
    if (!locals_initialized || !loops_initialized) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!ruda_checker_private_bind_function_parameters(&function, declaration)) {
        goto cleanup;
    }
    if (!ruda_mir_function_init(
            &function.mir,
            checker->result->allocator,
            declaration->symbol,
            declaration->parameter_types.data,
            declaration->parameter_types.length,
            function.result_type,
            checker->options.mir_options
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    function.mir_initialized = true;
    for (layout_index = 0;
        layout_index < declaration->parameter_types.length;
        layout_index += 1) {
        const RudaTypeId *parameter_type = ruda_vector_at_const(
            &declaration->parameter_types,
            layout_index
        );
        if (parameter_type == NULL || !ruda_function_ensure_array_layout(
                &function,
                *parameter_type
            )) {
            if (parameter_type == NULL) {
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            goto cleanup;
        }
    }
    if (!ruda_function_ensure_array_layout(
            &function,
            function.result_type
        )) {
        goto cleanup;
    }
    if (!ruda_checker_private_function_add_block(&function, &function.block)) {
        goto cleanup;
    }
    (void)ruda_checker_private_compile_block(&function, declaration->block);
    if (ruda_checker_has_failed(checker)) {
        goto cleanup;
    }
    if (checker->result->diagnostics.length != function.diagnostic_start) {
        accepted = true;
        goto cleanup;
    }
    if (!function.terminated) {
        if (function.result_type == RUDA_TYPE_VOID) {
            if (!ruda_checker_private_function_set_return(
                    &function,
                    RUDA_MIR_NO_REGISTER,
                    declaration->block->end,
                    declaration->block->end
                )) {
                goto cleanup;
            }
        } else {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_MISSING_RETURN,
                function.result_type == RUDA_TYPE_NEVER
                    ? "not every reachable path terminates"
                    : "not every reachable path returns a value",
                declaration->block->end,
                declaration->block->end
            );
            accepted = true;
            goto cleanup;
        }
    }
    if (!ruda_checker_private_function_ensure_interface_layouts(&function)) {
        goto cleanup;
    }
    verify_status = ruda_mir_verify(&function.mir);
    if (verify_status == RUDA_MIR_VERIFY_ALLOCATION_FAILED) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (verify_status == RUDA_MIR_VERIFY_LIMIT_REACHED) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (verify_status != RUDA_MIR_VERIFY_OK) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    function_index = checker->result->functions.length;
    if (ruda_map_get_const(
            &checker->result->function_index,
            function.mir.symbol.bytes,
            sizeof(function.mir.symbol.bytes)
        ) != NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!ruda_vector_reserve(
            &checker->result->functions,
            function_index + 1
        ) || !ruda_map_put(
            &checker->result->function_index,
            function.mir.symbol.bytes,
            sizeof(function.mir.symbol.bytes),
            &function_index
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    checked.name = declaration->owned_name;
    memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
    declaration->name_initialized = false;
    checked.signature_type = declaration->signature_type;
    checked.is_public = declaration->is_public;
    checked.is_test = declaration->is_test;
    checked.is_fuzz = declaration->is_fuzz;
    checked.mir = function.mir;
    memset(&function.mir, 0, sizeof(function.mir));
    function.mir_initialized = false;
    if (!ruda_vector_push(&checker->result->functions, &checked)) {
        (void)ruda_map_remove(
            &checker->result->function_index,
            checked.mir.symbol.bytes,
            sizeof(checked.mir.symbol.bytes)
        );
        ruda_owned_bytes_dispose(&checked.name);
        ruda_mir_function_dispose(&checked.mir);
        memset(&checked, 0, sizeof(checked));
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&checked, 0, sizeof(checked));
    accepted = true;

cleanup:
    if (function.mir_initialized) {
        ruda_mir_function_dispose(&function.mir);
    }
    if (locals_initialized) {
        ruda_vector_dispose(&function.locals);
    }
    if (loops_initialized) {
        ruda_vector_dispose(&function.loops);
    }
    return accepted && !ruda_checker_has_failed(checker);
}

 bool ruda_checker_private_infer_constant_expression_type(
    RudaChecker *checker,
    const RudaAstNode *node,
    RudaTypeId *result
) {
    const RudaAstNode *inner = NULL;
    const RudaSyntaxToken *token = NULL;
    RudaCheckName name;
    RudaCheckConstantDeclaration *constant = NULL;
    if (checker == NULL || node == NULL || result == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (node->kind == RUDA_SYNTAX_EXPRESSION
        || node->kind == RUDA_SYNTAX_PAREN_GROUP) {
        inner = ruda_checker_private_expression_only_node(node);
        if (inner == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        return ruda_checker_private_infer_constant_expression_type(checker, inner, result);
    }
    if (node->kind == RUDA_SYNTAX_LITERAL_EXPRESSION) {
        token = ruda_checker_private_check_first_token(node);
        if (token == NULL || token->synthetic) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (token->kind == RUDA_TOKEN_INTEGER) {
            *result = RUDA_TYPE_I64;
            return true;
        }
        if (token->kind == RUDA_TOKEN_FLOAT) {
            *result = RUDA_TYPE_F64;
            return true;
        }
        if (token->kind == RUDA_TOKEN_KW_TRUE
            || token->kind == RUDA_TOKEN_KW_FALSE) {
            *result = RUDA_TYPE_BOOL;
            return true;
        }
        ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (node->kind == RUDA_SYNTAX_NAME_EXPRESSION) {
        token = ruda_checker_private_check_child_token(node, RUDA_TOKEN_IDENTIFIER, 0);
        if (token == NULL || token->synthetic) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        name.snapshot = checker->ast->snapshot;
        name.start = token->start;
        name.end = token->end;
        constant = ruda_checker_private_checker_find_constant(checker, name);
        if (constant != NULL) {
            if (!ruda_checker_private_evaluate_constant(
                    checker,
                    constant,
                    token->start,
                    token->end
                )) {
                return false;
            }
            *result = constant->value.type;
            return true;
        }
        if (ruda_checker_private_checker_find_top_level(checker, name) != NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
            return false;
        }
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_UNKNOWN_NAME,
            "unknown name",
            token->start,
            token->end
        );
        return false;
    }
    if (node->kind == RUDA_SYNTAX_UNARY_EXPRESSION) {
        token = ruda_checker_private_check_first_token(node);
        if (token == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (token->kind == RUDA_TOKEN_BANG) {
            *result = RUDA_TYPE_BOOL;
            return true;
        }
        if (token->kind == RUDA_TOKEN_MINUS
            || token->kind == RUDA_TOKEN_TILDE) {
            if (token->kind == RUDA_TOKEN_MINUS) {
                const RudaAstNode *operand = ruda_checker_private_expression_only_node(node);
                if (operand != NULL && ruda_checker_private_infer_constant_expression_type(
                        checker,
                        operand,
                        result
                    )) {
                    return true;
                }
                return false;
            }
            *result = RUDA_TYPE_I64;
            return true;
        }
        ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (node->kind == RUDA_SYNTAX_BINARY_EXPRESSION) {
        const RudaAstNode *left = NULL;
        const RudaAstNode *right = NULL;
        RudaTypeId left_type = RUDA_TYPE_INVALID;
        RudaTypeId right_type = RUDA_TYPE_INVALID;
        if (!ruda_checker_private_binary_parts(node, &left, &token, &right)) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        switch (token->kind) {
            case RUDA_TOKEN_PLUS:
            case RUDA_TOKEN_MINUS:
            case RUDA_TOKEN_STAR:
            case RUDA_TOKEN_SLASH:
                if (!ruda_checker_private_infer_constant_expression_type(
                        checker,
                        left,
                        &left_type
                    ) || !ruda_checker_private_infer_constant_expression_type(
                        checker,
                        right,
                        &right_type
                    )) {
                    return false;
                }
                if (left_type == right_type
                    && (left_type == RUDA_TYPE_I64
                        || left_type == RUDA_TYPE_F64)) {
                    *result = left_type;
                    return true;
                }
                *result = RUDA_TYPE_I64;
                return true;
            case RUDA_TOKEN_PERCENT:
            case RUDA_TOKEN_AMPERSAND:
            case RUDA_TOKEN_CARET:
            case RUDA_TOKEN_PIPE:
            case RUDA_TOKEN_SHIFT_LEFT:
            case RUDA_TOKEN_SHIFT_RIGHT:
                *result = RUDA_TYPE_I64;
                return true;
            case RUDA_TOKEN_LESS:
            case RUDA_TOKEN_LESS_EQUAL:
            case RUDA_TOKEN_GREATER:
            case RUDA_TOKEN_GREATER_EQUAL:
            case RUDA_TOKEN_EQUAL_EQUAL:
            case RUDA_TOKEN_BANG_EQUAL:
            case RUDA_TOKEN_AMPERSAND_AMPERSAND:
            case RUDA_TOKEN_PIPE_PIPE:
                *result = RUDA_TYPE_BOOL;
                return true;
            default:
                ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
                return false;
        }
    }
    if (node->kind == RUDA_SYNTAX_CALL_EXPRESSION) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_CONSTANT_EVALUATION,
            "function calls are not permitted in this constant subset",
            node->start,
            node->end
        );
        return false;
    }
    ruda_checker_fail(checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}
