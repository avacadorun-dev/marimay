#include "checker_declaration_functions_api.h"
#include "package_imports_api.h"
#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_declaration_lifecycle_api.h"
#include "checker_expr_interfaces_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_lookup_api.h"
#include "checker_statement_api.h"
#include "checker_generic_types_api.h"

static bool mmay_checker_test_arguments_type(
    const MarimayChecker *checker,
    MarimayTypeId type
) {
    const MarimayTypeDescriptor *descriptor = checker != NULL
        ? mmay_type_descriptor(&checker->result->types, type) : NULL;
    return descriptor != NULL && descriptor->kind == MMAY_TYPE_KIND_ARRAY
        && descriptor->argument_count == 1
        && descriptor->arguments[0] == MMAY_TYPE_STRING;
}

 bool mmay_checker_private_collect_function_signature(
    MarimayChecker *checker,
    const MarimayAstNode *node
) {
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    const MarimayAstNode *parameters = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_PARAMETERS,
        0
    );
    const MarimayAstNode *return_type_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_TYPE,
        0
    );
    MarimayCheckFunctionDeclaration declaration;
    MarimayCheckTopLevelEntry top_level_entry;
    size_t diagnostics_before = checker->result->diagnostics.length;
    size_t index = 0;
    bool attribute_valid = true;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool callable_index_inserted = false;
    bool overload_group = false;
    bool overload_count_incremented = false;
    bool accepted = false;
    bool external = node != NULL && node->kind == MMAY_SYNTAX_EXTERN_DECL;
    memset(&declaration, 0, sizeof(declaration));
    declaration.block = mmay_checker_private_check_child_node(node, MMAY_SYNTAX_BLOCK, 0);
    if (name_token == NULL || name_token->synthetic || parameters == NULL
        || return_type_node == NULL
        || (!external && declaration.block == NULL)
        || (external && declaration.block != NULL)) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node->kind == MMAY_SYNTAX_ATTRIBUTE) {
            if (mmay_checker_private_check_attribute_is(checker, element->as.node, "test")) {
                if (declaration.is_test || declaration.is_fuzz
                    || mmay_checker_private_check_child_node(
                        element->as.node,
                        MMAY_SYNTAX_ARGUMENTS,
                        0
                    ) != NULL) {
                    (void)mmay_checker_add_diagnostic(
                        checker,
                        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
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
            if (mmay_checker_private_check_attribute_is(checker, element->as.node, "fuzz")) {
                if (declaration.is_test || declaration.is_fuzz
                    || mmay_checker_private_check_child_node(
                        element->as.node,
                        MMAY_SYNTAX_ARGUMENTS,
                        0
                    ) != NULL) {
                    (void)mmay_checker_add_diagnostic(
                        checker,
                        MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
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
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "attribute is not valid on an ordinary function",
                element->as.node->start,
                element->as.node->end
            );
            attribute_valid = false;
            continue;
        }
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_TOKEN
            && element->as.token.kind == MMAY_TOKEN_KW_UNSAFE) {
            declaration.is_unsafe = true;
        }
    }
    if (checker->declarations.length >= checker->options.max_functions) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.signature_valid = attribute_valid;
    declaration.is_external = external;
    declaration.is_public = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
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
    declaration.parameter_names_initialized = mmay_vector_init(
        &declaration.parameter_names,
        checker->result->allocator,
        sizeof(MarimayCheckName)
    );
    declaration.parameter_types_initialized = mmay_vector_init(
        &declaration.parameter_types,
        checker->result->allocator,
        sizeof(MarimayTypeId)
    );
    if (!declaration.parameter_names_initialized
        || !declaration.parameter_types_initialized) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
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
    checker->active_type_parameters = &declaration.type_parameters;
    for (index = 0; index < parameters->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(parameters, index);
        const MarimaySyntaxToken *parameter_name = NULL;
        const MarimayAstNode *type_node = NULL;
        MarimayCheckName name;
        MarimayTypeId type = MMAY_TYPE_INVALID;
        if (element == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind == MMAY_AST_ELEMENT_TOKEN) {
            if (element->as.token.synthetic) {
                mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
                goto cleanup;
            }
            continue;
        }
        if (element->as.node->kind != MMAY_SYNTAX_PARAMETER) {
            mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
        parameter_name = mmay_checker_private_check_child_token(
            element->as.node,
            MMAY_TOKEN_IDENTIFIER,
            0
        );
        type_node = mmay_checker_private_check_child_node(
            element->as.node,
            MMAY_SYNTAX_TYPE,
            0
        );
        if (parameter_name == NULL || parameter_name->synthetic
            || type_node == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (declaration.parameter_names.length
            >= checker->options.max_locals_per_function
            || declaration.parameter_types.length
                >= checker->options.mir_options.max_registers) {
            mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        name.snapshot = checker->ast->snapshot;
        name.start = parameter_name->start;
        name.end = parameter_name->end;
        if (mmay_checker_private_parameter_name_is_duplicate(checker, &declaration, name)) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_DUPLICATE_NAME,
                "name is already declared in this scope",
                name.start,
                name.end
            );
            declaration.signature_valid = false;
            if (mmay_checker_has_failed(checker)) {
                goto cleanup;
            }
        }
        if (!mmay_checker_private_check_exact_type(checker, type_node, &type)) {
            if (mmay_checker_has_failed(checker)) {
                goto cleanup;
            }
            declaration.signature_valid = false;
        } else if (declaration.is_public
            && !mmay_checker_private_check_public_type_reference(checker, type_node)) {
            if (mmay_checker_has_failed(checker)) {
                goto cleanup;
            }
            declaration.signature_valid = false;
        }
        if (!mmay_vector_push(&declaration.parameter_names, &name)
            || !mmay_vector_push(&declaration.parameter_types, &type)) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
    }
    if (!mmay_checker_private_check_exact_type(
            checker,
            return_type_node,
            &declaration.result_type
        )) {
        if (mmay_checker_has_failed(checker)) {
            goto cleanup;
        }
        declaration.signature_valid = false;
    } else if (declaration.is_public
        && !mmay_checker_private_check_public_type_reference(checker, return_type_node)) {
        if (mmay_checker_has_failed(checker)) {
            goto cleanup;
        }
        declaration.signature_valid = false;
    }
    if (checker->result->diagnostics.length != diagnostics_before) {
        declaration.signature_valid = false;
    }
    if (declaration.signature_valid) {
        if ((declaration.is_test || declaration.is_fuzz)
            && !mmay_checker_private_check_source_is_test_file(checker)) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "@test and @fuzz require a file ending _test.my",
                declaration.name.start,
                declaration.name.end
            );
            declaration.signature_valid = false;
        } else if ((declaration.is_test || declaration.is_fuzz)
            && declaration.is_public) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "test and fuzz functions cannot be public",
                declaration.name.start,
                declaration.name.end
            );
            declaration.signature_valid = false;
        } else if (declaration.is_test
            && (declaration.parameter_types.length > 1
                || (declaration.parameter_types.length == 1
                    && !mmay_checker_test_arguments_type(
                        checker,
                        *(const MarimayTypeId *)declaration.parameter_types.data
                    ))
                || declaration.result_type != MMAY_TYPE_VOID)) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
                "@test requires fn(): void or fn([]string): void",
                declaration.name.start,
                declaration.name.end
            );
            declaration.signature_valid = false;
        } else if (declaration.is_fuzz
            && (declaration.parameter_types.length != 1
                || declaration.result_type != MMAY_TYPE_VOID)) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
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
            mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        if (!mmay_type_intern_function(
                &checker->result->types,
                declaration.parameter_types.data,
                declaration.parameter_types.length,
                declaration.result_type,
                &declaration.signature_type
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
        if (declaration.parameter_types.length != 0) {
            const MarimayTypeId *first_type = mmay_vector_at_const(
                &declaration.parameter_types,
                0
            );
            if (first_type != NULL && mmay_check_owned_receiver(
                    checker,
                    *first_type,
                    &declaration.receiver_symbol
                )) {
                declaration.dot_eligible = true;
                declaration.receiver_type = *first_type;
                if (!mmay_symbol_id_from_bytes_with_receiver(
                        checker->package_name,
                        checker->package_name_length,
                        mmay_source_path(checker->ast->snapshot),
                        mmay_source_path_length(checker->ast->snapshot),
                        declaration.owned_name.data,
                        declaration.owned_name.length,
                        declaration.receiver_symbol,
                        &declaration.symbol
                    )) {
                    mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
                    goto cleanup;
                }
            } else if (!mmay_symbol_id_from_bytes(
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
        } else if (!mmay_symbol_id_from_bytes(
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
    }
    if (declaration.signature_valid) {
        const MarimayCheckTopLevelEntry *existing = mmay_map_get_const(
            &checker->top_level_index,
            declaration.owned_name.data,
            declaration.owned_name.length
        );
        if (existing != NULL) {
            MarimayCheckCallable resolved;
            MarimayCheckCallResolution resolution = MMAY_CHECK_CALL_INTERNAL_ERROR;
            const MarimayCheckFunctionDeclaration *first = NULL;
            if (existing->kind == MMAY_CHECK_TOP_LEVEL_FUNCTION) {
                first = mmay_vector_at_const(
                    &checker->declarations,
                    existing->index
                );
            }
            if (existing->kind != MMAY_CHECK_TOP_LEVEL_FUNCTION
                || first == NULL || !first->signature_valid
                || !first->dot_eligible || !declaration.dot_eligible) {
                duplicate_name = true;
            } else {
                resolution = mmay_check_resolve_call(
                    checker,
                    declaration.name,
                    true,
                    declaration.receiver_type,
                    &resolved
                );
                if (resolution == MMAY_CHECK_CALL_FOUND) {
                    duplicate_name = true;
                } else if (resolution == MMAY_CHECK_CALL_OTHER_NOMINAL) {
                    overload_group = true;
                } else {
                    mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
                    goto cleanup;
                }
            }
            if (duplicate_name) {
                (void)mmay_checker_add_diagnostic(
                    checker,
                    MMAY_DIAGNOSTIC_DUPLICATE_NAME,
                    "name is already declared in this scope",
                    declaration.name.start,
                    declaration.name.end
                );
                declaration.signature_valid = false;
                if (mmay_checker_has_failed(checker)) {
                    goto cleanup;
                }
            }
        }
    }
    top_level_entry.kind = MMAY_CHECK_TOP_LEVEL_FUNCTION;
    top_level_entry.index = checker->declarations.length;
    top_level_entry.function_count = 1;
    if (top_level_entry.index == SIZE_MAX || !mmay_vector_reserve(
            &checker->declarations,
            top_level_entry.index + 1
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (declaration.signature_valid && !overload_group) {
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
    if (declaration.signature_valid && declaration.dot_eligible) {
        if (!mmay_check_callable_insert(
                checker,
                &declaration,
                top_level_entry.index
            )) {
            if (top_level_index_inserted) {
                (void)mmay_map_remove(
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
        MarimayCheckTopLevelEntry *group = mmay_map_get(
            &checker->top_level_index,
            declaration.owned_name.data,
            declaration.owned_name.length
        );
        if (group == NULL || group->kind != MMAY_CHECK_TOP_LEVEL_FUNCTION
            || group->function_count == SIZE_MAX) {
            if (callable_index_inserted) {
                (void)mmay_check_callable_remove(checker, &declaration);
            }
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        group->function_count += 1;
        overload_count_incremented = true;
    }
    if (!mmay_vector_push(&checker->declarations, &declaration)) {
        if (overload_count_incremented) {
            MarimayCheckTopLevelEntry *group = mmay_map_get(
                &checker->top_level_index,
                declaration.owned_name.data,
                declaration.owned_name.length
            );
            if (group != NULL && group->function_count != 0) {
                group->function_count -= 1;
            }
        }
        if (callable_index_inserted) {
            (void)mmay_check_callable_remove(checker, &declaration);
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
    if (checker->active_type_parameters == &declaration.type_parameters) {
        checker->active_type_parameters = NULL;
    }
    mmay_checker_private_function_declaration_dispose(&declaration);
    return accepted && !mmay_checker_has_failed(checker);
}

 bool mmay_checker_private_collect_constant_declaration(
    MarimayChecker *checker,
    const MarimayAstNode *node
) {
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    const MarimayAstNode *type_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_TYPE,
        0
    );
    const MarimayAstNode *initializer = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_EXPRESSION,
        0
    );
    MarimayCheckConstantDeclaration declaration;
    MarimayCheckTopLevelEntry top_level_entry;
    size_t diagnostics_before = checker->result->diagnostics.length;
    size_t index = 0;
    bool duplicate_name = false;
    bool top_level_index_inserted = false;
    bool accepted = false;
    memset(&declaration, 0, sizeof(declaration));
    if (name_token == NULL || name_token->synthetic || initializer == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node != NULL
            && element->as.node->kind == MMAY_SYNTAX_ATTRIBUTE) {
            mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
            goto cleanup;
        }
    }
    if (checker->constants.length >= checker->options.max_constants) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.initializer = initializer;
    declaration.name.snapshot = checker->ast->snapshot;
    declaration.name.start = name_token->start;
    declaration.name.end = name_token->end;
    declaration.type = MMAY_TYPE_INVALID;
    declaration.state = MMAY_CHECK_CONSTANT_UNEVALUATED;
    declaration.declaration_valid = true;
    declaration.is_public = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_KW_PUBLIC,
        0
    ) != NULL;
    duplicate_name = mmay_checker_private_declaration_name_is_duplicate(
        checker,
        declaration.name
    );
    if (duplicate_name) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            declaration.name.start,
            declaration.name.end
        );
        declaration.declaration_valid = false;
        declaration.state = MMAY_CHECK_CONSTANT_INVALID;
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
    if (type_node != NULL
        && !mmay_checker_private_check_exact_type(checker, type_node, &declaration.type)) {
        if (mmay_checker_has_failed(checker)) {
            goto cleanup;
        }
        declaration.declaration_valid = false;
        declaration.state = MMAY_CHECK_CONSTANT_INVALID;
    } else if (type_node != NULL && declaration.is_public
        && !mmay_checker_private_check_public_type_reference(checker, type_node)) {
        if (mmay_checker_has_failed(checker)) {
            goto cleanup;
        }
        declaration.declaration_valid = false;
        declaration.state = MMAY_CHECK_CONSTANT_INVALID;
    }
    if (declaration.type != MMAY_TYPE_INVALID
        && declaration.type != MMAY_TYPE_I64
        && !mmay_checker_private_type_is_integer(declaration.type)
        && declaration.type != MMAY_TYPE_F64
        && declaration.type != MMAY_TYPE_BOOL) {
        mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (checker->result->diagnostics.length != diagnostics_before) {
        declaration.declaration_valid = false;
        declaration.state = MMAY_CHECK_CONSTANT_INVALID;
    }
    top_level_entry.kind = MMAY_CHECK_TOP_LEVEL_CONSTANT;
    top_level_entry.index = checker->constants.length;
    top_level_entry.function_count = 0;
    if (top_level_entry.index == SIZE_MAX || !mmay_vector_reserve(
            &checker->constants,
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
    if (!mmay_vector_push(&checker->constants, &declaration)) {
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
    mmay_checker_private_constant_declaration_dispose(&declaration);
    return accepted && !mmay_checker_has_failed(checker);
}

 bool mmay_checker_private_bind_function_parameters(
    MarimayFunctionChecker *function,
    const MarimayCheckFunctionDeclaration *declaration
) {
    size_t index = 0;
    if (declaration->parameter_names.length
        != declaration->parameter_types.length) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < declaration->parameter_names.length; index += 1) {
        const MarimayCheckName *name = mmay_vector_at_const(
            &declaration->parameter_names,
            index
        );
        const MarimayTypeId *type = mmay_vector_at_const(
            &declaration->parameter_types,
            index
        );
        MarimayCheckLocal local;
        if (name == NULL || type == NULL || *type == MMAY_TYPE_INVALID
            || index > UINT32_MAX) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        local.name = *name;
        local.type = *type;
        local.register_id = (MarimayMirRegister)index;
        local.scope_depth = function->scope_depth;
        local.mutable_binding = false;
        local.initialized = true;
        if (!mmay_vector_push(&function->locals, &local)) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_ALLOCATION_FAILED
            );
            return false;
        }
    }
    return true;
}

static bool mmay_checker_private_publish_external_function(
    MarimayChecker *checker,
    MarimayCheckFunctionDeclaration *declaration
) {
    MarimayMirFunction mir;
    MarimayCheckedFunction checked;
    size_t function_index = 0;
    bool accepted = false;
    memset(&mir, 0, sizeof(mir));
    memset(&checked, 0, sizeof(checked));
    if (checker == NULL || declaration == NULL || !declaration->is_external
        || !declaration->signature_valid || declaration->block != NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (!mmay_mir_function_init(
            &mir,
            checker->result->allocator,
            declaration->symbol,
            declaration->parameter_types.data,
            declaration->parameter_types.length,
            declaration->result_type,
            checker->options.mir_options
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    function_index = checker->result->functions.length;
    if (mmay_map_get_const(
            &checker->result->function_index,
            mir.symbol.bytes,
            sizeof(mir.symbol.bytes)
        ) != NULL
        || !mmay_vector_reserve(
            &checker->result->functions,
            function_index + 1
        ) || !mmay_map_put(
            &checker->result->function_index,
            mir.symbol.bytes,
            sizeof(mir.symbol.bytes),
            &function_index
        )) {
        mmay_checker_fail(
            checker,
            mmay_map_get_const(
                &checker->result->function_index,
                mir.symbol.bytes,
                sizeof(mir.symbol.bytes)
            ) != NULL
                ? MMAY_CHECK_INTERNAL_ERROR
                : MMAY_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    checked.name = declaration->owned_name;
    memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
    declaration->name_initialized = false;
    checked.signature_type = declaration->signature_type;
    checked.receiver_symbol = declaration->receiver_symbol;
    checked.receiver_type = declaration->receiver_type;
    checked.dot_eligible = declaration->dot_eligible;
    checked.is_public = declaration->is_public;
    checked.is_external = true;
    checked.is_test = declaration->is_test;
    checked.is_fuzz = declaration->is_fuzz;
    checked.mir = mir;
    memset(&mir, 0, sizeof(mir));
    if (!mmay_vector_push(&checker->result->functions, &checked)) {
        (void)mmay_map_remove(
            &checker->result->function_index,
            checked.mir.symbol.bytes,
            sizeof(checked.mir.symbol.bytes)
        );
        mmay_owned_bytes_dispose(&checked.name);
        mmay_mir_function_dispose(&checked.mir);
        memset(&checked, 0, sizeof(checked));
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&checked, 0, sizeof(checked));
    accepted = true;

cleanup:
    if (mir.initialized) {
        mmay_mir_function_dispose(&mir);
    }
    return accepted && !mmay_checker_has_failed(checker);
}

 bool mmay_checker_private_check_function_body(
    MarimayChecker *checker,
    MarimayCheckFunctionDeclaration *declaration
) {
    MarimayFunctionChecker function;
    MarimayCheckedFunction checked;
    MarimayMirVerifyStatus verify_status;
    size_t function_index = 0;
    size_t layout_index = 0;
    bool locals_initialized = false;
    bool loops_initialized = false;
    bool defers_initialized = false;
    bool accepted = false;
    memset(&function, 0, sizeof(function));
    memset(&checked, 0, sizeof(checked));
    if (declaration != NULL && declaration->is_external) {
        return mmay_checker_private_publish_external_function(checker, declaration);
    }
    if (declaration == NULL || declaration->ast == NULL
        || !declaration->signature_valid
        || declaration->signature_type == MMAY_TYPE_INVALID
        || declaration->block == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (!mmay_checker_private_checker_set_ast(checker, declaration->ast)) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    function.checker = checker;
    function.block = MMAY_MIR_NO_BLOCK;
    function.result_type = declaration->result_type;
    function.unsafe_context = declaration->is_unsafe;
    function.test_context = declaration->is_test || declaration->is_fuzz;
    function.type_parameters = &declaration->type_parameters;
    function.type_arguments = declaration->is_instance
        ? &declaration->generic_arguments : NULL;
    checker->active_type_parameters = function.type_parameters;
    checker->active_type_arguments = function.type_arguments;
    function.diagnostic_start = checker->result->diagnostics.length;
    locals_initialized = mmay_vector_init(
        &function.locals,
        checker->result->allocator,
        sizeof(MarimayCheckLocal)
    );
    loops_initialized = mmay_vector_init(
        &function.loops,
        checker->result->allocator,
        sizeof(MarimayLoopContext)
    );
    defers_initialized = mmay_vector_init(
        &function.defers,
        checker->result->allocator,
        sizeof(MarimayDeferredCall)
    );
    if (!locals_initialized || !loops_initialized || !defers_initialized) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (!mmay_checker_private_bind_function_parameters(&function, declaration)) {
        goto cleanup;
    }
    if (!mmay_mir_function_init(
            &function.mir,
            checker->result->allocator,
            declaration->symbol,
            declaration->parameter_types.data,
            declaration->parameter_types.length,
            function.result_type,
            checker->options.mir_options
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    function.mir_initialized = true;
    for (layout_index = 0;
        layout_index < declaration->parameter_types.length;
        layout_index += 1) {
        const MarimayTypeId *parameter_type = mmay_vector_at_const(
            &declaration->parameter_types,
            layout_index
        );
        if (parameter_type == NULL || !mmay_function_ensure_array_layout(
                &function,
                *parameter_type
            )) {
            if (parameter_type == NULL) {
                mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            goto cleanup;
        }
    }
    if (!mmay_function_ensure_array_layout(
            &function,
            function.result_type
        )) {
        goto cleanup;
    }
    if (!mmay_checker_private_function_add_block(&function, &function.block)) {
        goto cleanup;
    }
    (void)mmay_checker_private_compile_block(&function, declaration->block);
    if (mmay_checker_has_failed(checker)) {
        goto cleanup;
    }
    if (checker->result->diagnostics.length != function.diagnostic_start) {
        accepted = true;
        goto cleanup;
    }
    if (!function.terminated) {
        if (function.result_type == MMAY_TYPE_VOID) {
            if (!mmay_checker_private_function_set_return(
                    &function,
                    MMAY_MIR_NO_REGISTER,
                    declaration->block->end,
                    declaration->block->end
                )) {
                goto cleanup;
            }
        } else {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_MISSING_RETURN,
                function.result_type == MMAY_TYPE_NEVER
                    ? "not every reachable path terminates"
                    : "not every reachable path returns a value",
                declaration->block->end,
                declaration->block->end
            );
            accepted = true;
            goto cleanup;
        }
    }
    if (!mmay_checker_private_function_ensure_nominal_layouts(&function)
        || !mmay_checker_private_function_ensure_interface_layouts(&function)) {
        goto cleanup;
    }
    verify_status = mmay_mir_verify(&function.mir);
    if (verify_status == MMAY_MIR_VERIFY_ALLOCATION_FAILED) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (verify_status == MMAY_MIR_VERIFY_LIMIT_REACHED) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (verify_status != MMAY_MIR_VERIFY_OK) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    function_index = checker->result->functions.length;
    if (mmay_map_get_const(
            &checker->result->function_index,
            function.mir.symbol.bytes,
            sizeof(function.mir.symbol.bytes)
        ) != NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    if (!mmay_vector_reserve(
            &checker->result->functions,
            function_index + 1
        ) || !mmay_map_put(
            &checker->result->function_index,
            function.mir.symbol.bytes,
            sizeof(function.mir.symbol.bytes),
            &function_index
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    checked.name = declaration->owned_name;
    memset(&declaration->owned_name, 0, sizeof(declaration->owned_name));
    declaration->name_initialized = false;
    checked.signature_type = declaration->signature_type;
    checked.receiver_symbol = declaration->receiver_symbol;
    checked.receiver_type = declaration->receiver_type;
    checked.dot_eligible = declaration->dot_eligible;
    checked.is_public = declaration->is_public;
    checked.is_test = declaration->is_test;
    checked.is_fuzz = declaration->is_fuzz;
    checked.mir = function.mir;
    memset(&function.mir, 0, sizeof(function.mir));
    function.mir_initialized = false;
    if (!mmay_vector_push(&checker->result->functions, &checked)) {
        (void)mmay_map_remove(
            &checker->result->function_index,
            checked.mir.symbol.bytes,
            sizeof(checked.mir.symbol.bytes)
        );
        mmay_owned_bytes_dispose(&checked.name);
        mmay_mir_function_dispose(&checked.mir);
        memset(&checked, 0, sizeof(checked));
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&checked, 0, sizeof(checked));
    accepted = true;

cleanup:
    if (checker->active_type_parameters == function.type_parameters) {
        checker->active_type_parameters = NULL;
        checker->active_type_arguments = NULL;
    }
    if (function.mir_initialized) {
        mmay_mir_function_dispose(&function.mir);
    }
    if (locals_initialized) {
        mmay_vector_dispose(&function.locals);
    }
    if (loops_initialized) {
        mmay_vector_dispose(&function.loops);
    }
    if (defers_initialized) {
        mmay_vector_dispose(&function.defers);
    }
    return accepted && !mmay_checker_has_failed(checker);
}

 bool mmay_checker_private_infer_constant_expression_type(
    MarimayChecker *checker,
    const MarimayAstNode *node,
    MarimayTypeId *result
) {
    const MarimayAstNode *inner = NULL;
    const MarimaySyntaxToken *token = NULL;
    MarimayCheckName name;
    MarimayCheckConstantDeclaration *constant = NULL;
    if (checker == NULL || node == NULL || result == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (node->kind == MMAY_SYNTAX_EXPRESSION
        || node->kind == MMAY_SYNTAX_PAREN_GROUP) {
        inner = mmay_checker_private_expression_only_node(node);
        if (inner == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        return mmay_checker_private_infer_constant_expression_type(checker, inner, result);
    }
    if (node->kind == MMAY_SYNTAX_LITERAL_EXPRESSION) {
        token = mmay_checker_private_check_first_token(node);
        if (token == NULL || token->synthetic) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (token->kind == MMAY_TOKEN_INTEGER) {
            *result = MMAY_TYPE_I64;
            return true;
        }
        if (token->kind == MMAY_TOKEN_FLOAT) {
            *result = MMAY_TYPE_F64;
            return true;
        }
        if (token->kind == MMAY_TOKEN_KW_TRUE
            || token->kind == MMAY_TOKEN_KW_FALSE) {
            *result = MMAY_TYPE_BOOL;
            return true;
        }
        mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (node->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
        token = mmay_checker_private_check_child_token(node, MMAY_TOKEN_IDENTIFIER, 0);
        if (token == NULL || token->synthetic) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        name.snapshot = checker->ast->snapshot;
        name.start = token->start;
        name.end = token->end;
        constant = mmay_checker_private_checker_find_constant(checker, name);
        if (constant != NULL) {
            if (!mmay_checker_private_evaluate_constant(
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
        if (mmay_checker_private_checker_find_top_level(checker, name) != NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
            return false;
        }
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_UNKNOWN_NAME,
            "unknown name",
            token->start,
            token->end
        );
        return false;
    }
    if (node->kind == MMAY_SYNTAX_UNARY_EXPRESSION) {
        token = mmay_checker_private_check_first_token(node);
        if (token == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (token->kind == MMAY_TOKEN_BANG) {
            *result = MMAY_TYPE_BOOL;
            return true;
        }
        if (token->kind == MMAY_TOKEN_MINUS
            || token->kind == MMAY_TOKEN_TILDE) {
            if (token->kind == MMAY_TOKEN_MINUS) {
                const MarimayAstNode *operand = mmay_checker_private_expression_only_node(node);
                if (operand != NULL && mmay_checker_private_infer_constant_expression_type(
                        checker,
                        operand,
                        result
                    )) {
                    return true;
                }
                return false;
            }
            *result = MMAY_TYPE_I64;
            return true;
        }
        mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (node->kind == MMAY_SYNTAX_BINARY_EXPRESSION) {
        const MarimayAstNode *left = NULL;
        const MarimayAstNode *right = NULL;
        MarimayTypeId left_type = MMAY_TYPE_INVALID;
        MarimayTypeId right_type = MMAY_TYPE_INVALID;
        if (!mmay_checker_private_binary_parts(node, &left, &token, &right)) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        switch (token->kind) {
            case MMAY_TOKEN_PLUS:
            case MMAY_TOKEN_MINUS:
            case MMAY_TOKEN_STAR:
            case MMAY_TOKEN_SLASH:
                if (!mmay_checker_private_infer_constant_expression_type(
                        checker,
                        left,
                        &left_type
                    ) || !mmay_checker_private_infer_constant_expression_type(
                        checker,
                        right,
                        &right_type
                    )) {
                    return false;
                }
                if (left_type == right_type
                    && (left_type == MMAY_TYPE_I64
                        || left_type == MMAY_TYPE_F64)) {
                    *result = left_type;
                    return true;
                }
                *result = MMAY_TYPE_I64;
                return true;
            case MMAY_TOKEN_PERCENT:
            case MMAY_TOKEN_AMPERSAND:
            case MMAY_TOKEN_CARET:
            case MMAY_TOKEN_PIPE:
            case MMAY_TOKEN_SHIFT_LEFT:
            case MMAY_TOKEN_SHIFT_RIGHT:
                *result = MMAY_TYPE_I64;
                return true;
            case MMAY_TOKEN_LESS:
            case MMAY_TOKEN_LESS_EQUAL:
            case MMAY_TOKEN_GREATER:
            case MMAY_TOKEN_GREATER_EQUAL:
            case MMAY_TOKEN_EQUAL_EQUAL:
            case MMAY_TOKEN_BANG_EQUAL:
            case MMAY_TOKEN_AMPERSAND_AMPERSAND:
            case MMAY_TOKEN_PIPE_PIPE:
                *result = MMAY_TYPE_BOOL;
                return true;
            default:
                mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
                return false;
        }
    }
    if (node->kind == MMAY_SYNTAX_CALL_EXPRESSION) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
            "function calls are not permitted in this constant subset",
            node->start,
            node->end
        );
        return false;
    }
    mmay_checker_fail(checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}
