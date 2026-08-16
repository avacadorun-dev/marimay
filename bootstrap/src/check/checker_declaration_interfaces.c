#include "checker_declaration_interfaces_api.h"
#include "checker_core_api.h"
#include "checker_declaration_lifecycle_api.h"
#include "checker_lookup_api.h"

bool ruda_checker_private_collect_interface_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_checker_private_check_child_token(
        node, RUDA_TOKEN_IDENTIFIER, 0
    );
    RudaCheckInterfaceDeclaration declaration;
    RudaCheckTopLevelEntry entry;
    bool duplicate = false;
    bool inserted = false;
    size_t index = 0;
    memset(&declaration, 0, sizeof(declaration));
    if (checker == NULL || node == NULL || name_token == NULL
        || name_token->synthetic) {
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
    if (checker->interfaces.length >= checker->options.max_interfaces) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    declaration.ast = checker->ast;
    declaration.node = node;
    declaration.name = (RudaCheckName){
        checker->ast->snapshot, name_token->start, name_token->end
    };
    declaration.type = RUDA_TYPE_INVALID;
    declaration.is_public = ruda_checker_private_check_child_token(
        node, RUDA_TOKEN_KW_PUBLIC, 0
    ) != NULL;
    declaration.declaration_valid = true;
    duplicate = ruda_checker_private_declaration_name_is_duplicate(
        checker, declaration.name
    ) || ruda_checker_private_check_name_is_primitive_type(
        checker, declaration.name
    );
    if (duplicate) {
        (void)ruda_checker_add_diagnostic(
            checker, RUDA_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            declaration.name.start, declaration.name.end
        );
        declaration.declaration_valid = false;
    }
    if (!ruda_owned_bytes_copy(
            &declaration.owned_name, checker->result->allocator,
            checker->source + declaration.name.start,
            declaration.name.end - declaration.name.start
        ) || !ruda_vector_init(
            &declaration.methods, checker->result->allocator,
            sizeof(RudaCheckInterfaceMethodDeclaration)
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    declaration.name_initialized = true;
    declaration.methods_initialized = true;
    if (!ruda_symbol_id_from_bytes(
            checker->package_name, checker->package_name_length,
            ruda_source_path(checker->ast->snapshot),
            ruda_source_path_length(checker->ast->snapshot),
            declaration.owned_name.data, declaration.owned_name.length, 0,
            &declaration.symbol
        ) || (!duplicate && !ruda_type_intern_nominal(
            &checker->result->types, RUDA_TYPE_KIND_INTERFACE,
            declaration.symbol, NULL, 0, &declaration.type
        ))) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    entry.kind = RUDA_CHECK_TOP_LEVEL_INTERFACE;
    entry.index = checker->interfaces.length;
    entry.function_count = 0;
    if (!duplicate && !ruda_map_put(
            &checker->top_level_index, declaration.owned_name.data,
            declaration.owned_name.length, &entry
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    inserted = !duplicate;
    if (!ruda_vector_push(&checker->interfaces, &declaration)) {
        if (inserted) {
            (void)ruda_map_remove(
                &checker->top_level_index, declaration.owned_name.data,
                declaration.owned_name.length
            );
        }
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(&declaration, 0, sizeof(declaration));
cleanup:
    ruda_checker_private_interface_declaration_dispose(&declaration);
    return !ruda_checker_has_failed(checker);
}

static bool ruda_checker_resolve_interface_methods(
    RudaChecker *checker,
    RudaCheckInterfaceDeclaration *declaration
) {
    size_t index = 0;
    if (checker == NULL || declaration == NULL || !declaration->declaration_valid
        || declaration->methods_resolved) {
        return declaration != NULL && declaration->methods_resolved;
    }
    if (!ruda_checker_private_checker_set_ast(checker, declaration->ast)) {
        return false;
    }
    for (index = 0; index < declaration->node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(declaration->node, index);
        const RudaAstNode *item = element != NULL
            && element->kind == RUDA_AST_ELEMENT_NODE ? element->as.node : NULL;
        const RudaSyntaxToken *name_token = NULL;
        const RudaAstNode *parameters = NULL;
        const RudaAstNode *result_type = NULL;
        RudaCheckInterfaceMethodDeclaration method;
        size_t child = 0;
        memset(&method, 0, sizeof(method));
        if (item == NULL || item->kind != RUDA_SYNTAX_INTERFACE_ITEM) {
            continue;
        }
        if (ruda_checker_private_check_child_node(
                item, RUDA_SYNTAX_TYPE_PARAMETERS, 0
            ) != NULL || declaration->methods.length
                >= checker->options.max_methods_per_interface) {
            ruda_checker_fail(checker, declaration->methods.length
                    >= checker->options.max_methods_per_interface
                ? RUDA_CHECK_LIMIT_REACHED : RUDA_CHECK_INCOMPLETE_LANGUAGE);
            return false;
        }
        name_token = ruda_checker_private_check_child_token(
            item, RUDA_TOKEN_IDENTIFIER, 0
        );
        parameters = ruda_checker_private_check_child_node(
            item, RUDA_SYNTAX_PARAMETERS, 0
        );
        result_type = ruda_checker_private_check_child_node(
            item, RUDA_SYNTAX_TYPE, 0
        );
        if (name_token == NULL || name_token->synthetic || parameters == NULL
            || result_type == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        method.node = item;
        method.name = (RudaCheckName){
            checker->ast->snapshot, name_token->start, name_token->end
        };
        if (!ruda_owned_bytes_copy(
                &method.owned_name, checker->result->allocator,
                checker->source + method.name.start,
                method.name.end - method.name.start
            ) || !ruda_vector_init(
                &method.parameter_types, checker->result->allocator,
                sizeof(RudaTypeId)
            )) {
            ruda_checker_private_interface_method_declaration_dispose(&method);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        method.name_initialized = true;
        method.parameter_types_initialized = true;
        for (child = 0; child < parameters->child_count; child += 1) {
            const RudaAstElement *parameter = ruda_ast_child(parameters, child);
            const RudaAstNode *type = parameter != NULL
                && parameter->kind == RUDA_AST_ELEMENT_NODE
                && parameter->as.node != NULL
                && parameter->as.node->kind == RUDA_SYNTAX_PARAMETER
                    ? ruda_checker_private_check_child_node(
                        parameter->as.node, RUDA_SYNTAX_TYPE, 0
                    ) : NULL;
            RudaTypeId parameter_type = RUDA_TYPE_INVALID;
            if (parameter == NULL || parameter->kind != RUDA_AST_ELEMENT_NODE) {
                continue;
            }
            if (type == NULL || method.parameter_types.length
                    >= checker->options.max_parameters_per_interface_method
                || !ruda_checker_private_check_exact_type(
                    checker, type, &parameter_type
                ) || !ruda_vector_push(&method.parameter_types, &parameter_type)) {
                ruda_checker_private_interface_method_declaration_dispose(&method);
                if (!ruda_checker_has_failed(checker)) {
                    ruda_checker_fail(checker, type == NULL
                            ? RUDA_CHECK_INVALID_ARGUMENT
                            : RUDA_CHECK_ALLOCATION_FAILED);
                }
                return false;
            }
        }
        if (!ruda_checker_private_check_exact_type(
                checker, result_type, &method.result_type
            ) || !ruda_vector_push(&declaration->methods, &method)) {
            ruda_checker_private_interface_method_declaration_dispose(&method);
            if (!ruda_checker_has_failed(checker)) {
                ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            }
            return false;
        }
        memset(&method, 0, sizeof(method));
    }
    declaration->methods_resolved = true;
    return true;
}

bool ruda_checker_private_resolve_all_interface_methods(RudaChecker *checker) {
    size_t index = 0;
    for (index = 0; checker != NULL && index < checker->interfaces.length;
        index += 1) {
        RudaCheckInterfaceDeclaration *declaration = ruda_vector_at(
            &checker->interfaces, index
        );
        if (declaration != NULL && declaration->declaration_valid
            && !ruda_checker_resolve_interface_methods(checker, declaration)) {
            return false;
        }
    }
    return !ruda_checker_has_failed(checker);
}

static void ruda_checker_dispose_unpublished_interface(
    RudaCheckedInterface *interface_value
) {
    size_t index = 0;
    if (interface_value == NULL) {
        return;
    }
    for (index = 0; index < interface_value->methods.length; index += 1) {
        RudaCheckedInterfaceMethod *method = ruda_vector_at(
            &interface_value->methods,
            index
        );
        if (method != NULL) {
            ruda_vector_dispose(&method->parameter_types);
            ruda_owned_bytes_dispose(&method->name);
            memset(method, 0, sizeof(*method));
        }
    }
    ruda_vector_dispose(&interface_value->methods);
    ruda_owned_bytes_dispose(&interface_value->name);
    memset(interface_value, 0, sizeof(*interface_value));
}

bool ruda_checker_private_publish_interfaces(RudaChecker *checker) {
    size_t index = 0;
    for (index = 0; checker != NULL && index < checker->interfaces.length;
        index += 1) {
        RudaCheckInterfaceDeclaration *source = ruda_vector_at(
            &checker->interfaces, index
        );
        RudaCheckedInterface interface_value;
        size_t method_index = 0;
        memset(&interface_value, 0, sizeof(interface_value));
        if (source == NULL || !source->declaration_valid
            || !source->methods_resolved
            || !ruda_vector_init(&interface_value.methods,
                checker->result->allocator, sizeof(RudaCheckedInterfaceMethod))) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        interface_value.name = source->owned_name;
        memset(&source->owned_name, 0, sizeof(source->owned_name));
        source->name_initialized = false;
        interface_value.symbol = source->symbol;
        interface_value.type = source->type;
        interface_value.is_public = source->is_public;
        for (method_index = 0; method_index < source->methods.length;
            method_index += 1) {
            RudaCheckInterfaceMethodDeclaration *method = ruda_vector_at(
                &source->methods, method_index
            );
            RudaCheckedInterfaceMethod output;
            if (method == NULL) {
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
                goto cleanup_interface;
            }
            memset(&output, 0, sizeof(output));
            output.name = method->owned_name;
            memset(&method->owned_name, 0, sizeof(method->owned_name));
            method->name_initialized = false;
            output.parameter_types = method->parameter_types;
            memset(&method->parameter_types, 0, sizeof(method->parameter_types));
            method->parameter_types_initialized = false;
            output.result_type = method->result_type;
            if (!ruda_vector_push(&interface_value.methods, &output)) {
                ruda_owned_bytes_dispose(&output.name);
                ruda_vector_dispose(&output.parameter_types);
                ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
                goto cleanup_interface;
            }
        }
        if (!ruda_vector_push(&checker->result->interfaces, &interface_value)) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup_interface;
        }
        memset(&interface_value, 0, sizeof(interface_value));
        continue;

cleanup_interface:
        ruda_checker_dispose_unpublished_interface(&interface_value);
        return false;
    }
    return true;
}
