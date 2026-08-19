#include "checker_generic_types_api.h"

#include "checker_core_api.h"
#include "checker_internal.h"
#include "checker_lookup_api.h"
#include "checker_declaration_lifecycle_api.h"

static bool ruda_checker_private_type_parameter_name(
    const RudaChecker *checker,
    RudaCheckName name,
    const unsigned char **bytes,
    size_t *length
) {
    return checker != NULL && bytes != NULL && length != NULL
        && ruda_checker_private_check_name_bytes(name, bytes, length);
}

static void ruda_checker_private_fail_type_intern(
    RudaChecker *checker
) {
    const size_t max_types = checker->options.type_options.max_types;
    const size_t dynamic_types = checker->result->types.descriptors.length;
    const bool at_limit = max_types <= RUDA_TYPE_FIRST_DYNAMIC
        || dynamic_types >= max_types - RUDA_TYPE_FIRST_DYNAMIC;
    ruda_checker_fail(
        checker,
        at_limit ? RUDA_CHECK_LIMIT_REACHED : RUDA_CHECK_ALLOCATION_FAILED
    );
}

bool ruda_checker_private_collect_type_parameters(
    RudaChecker *checker,
    const RudaAstNode *declaration_node,
    RudaVector *parameters
) {
    const RudaAstNode *type_parameters = NULL;
    size_t index = 0;
    const RudaVector *previous_parameters = checker != NULL
        ? checker->active_type_parameters : NULL;
    if (checker == NULL || declaration_node == NULL || parameters == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    type_parameters = ruda_checker_private_check_child_node(
        declaration_node, RUDA_SYNTAX_TYPE_PARAMETERS, 0
    );
    if (type_parameters == NULL) {
        return true;
    }
    checker->active_type_parameters = parameters;
    if (type_parameters->child_count > checker->options.type_options.max_type_arguments) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        checker->active_type_parameters = previous_parameters;
        return false;
    }
    for (index = 0; index < type_parameters->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(type_parameters, index);
        const RudaAstNode *parameter_node = NULL;
        const RudaSyntaxToken *token = NULL;
        const unsigned char *bytes = NULL;
        size_t length = 0;
        RudaCheckTypeParameterDeclaration parameter;
        RudaSymbolId symbol;
        size_t previous = 0;
        memset(&parameter, 0, sizeof(parameter));
        memset(&symbol, 0, sizeof(symbol));
        if (element == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        if (element->kind == RUDA_AST_ELEMENT_TOKEN) {
            if (element->as.token.synthetic) {
                ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
                checker->active_type_parameters = previous_parameters;
                return false;
            }
            continue;
        }
        if (element->kind != RUDA_AST_ELEMENT_NODE
            || (parameter_node = element->as.node) == NULL
            || parameter_node->kind != RUDA_SYNTAX_TYPE_PARAMETER) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        token = ruda_checker_private_check_child_token(
            parameter_node, RUDA_TOKEN_IDENTIFIER, 0
        );
        if (token == NULL || token->synthetic) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        parameter.name.snapshot = checker->ast->snapshot;
        parameter.name.start = token->start;
        parameter.name.end = token->end;
        if (!ruda_checker_private_type_parameter_name(
                checker, parameter.name, &bytes, &length
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        for (previous = 0; previous < parameters->length; previous += 1) {
            const RudaCheckTypeParameterDeclaration *other =
                ruda_vector_at_const(parameters, previous);
            if (other != NULL && ruda_checker_private_check_names_equal(
                    checker, other->name, parameter.name
                )) {
                (void)ruda_checker_add_diagnostic(
                    checker,
                    RUDA_DIAGNOSTIC_DUPLICATE_NAME,
                    "generic type parameter is declared more than once",
                    parameter.name.start,
                    parameter.name.end
                );
                checker->active_type_parameters = previous_parameters;
                return !ruda_checker_has_failed(checker);
            }
        }
        if (!ruda_owned_bytes_copy(
                &parameter.owned_name,
                checker->result->allocator,
                bytes,
                length
            ) || !ruda_symbol_id_from_bytes(
                checker->package_name,
                checker->package_name_length,
                ruda_source_path(checker->ast->snapshot),
                ruda_source_path_length(checker->ast->snapshot),
                bytes,
                length,
                UINT32_C(0x54500000) | (uint32_t)index,
                &symbol
            )) {
            ruda_owned_bytes_dispose(&parameter.owned_name);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        if (!ruda_type_intern_nominal(
                &checker->result->types,
                RUDA_TYPE_KIND_TYPE_PARAMETER,
                symbol,
                NULL,
                0,
                &parameter.type
            )) {
            ruda_owned_bytes_dispose(&parameter.owned_name);
            ruda_checker_private_fail_type_intern(checker);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        parameter.name_initialized = true;
        if (!ruda_vector_init(
                &parameter.constraints,
                checker->result->allocator,
                sizeof(RudaTypeId)
            )) {
            ruda_owned_bytes_dispose(&parameter.owned_name);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        parameter.constraints_initialized = true;
        {
            const RudaAstNode *constraint = ruda_checker_private_check_child_node(
                parameter_node, RUDA_SYNTAX_TYPE, 0
            );
            RudaTypeId constraint_type = RUDA_TYPE_INVALID;
            if (constraint != NULL
                && (!ruda_checker_private_check_exact_type(
                        checker, constraint, &constraint_type
                    ) || !ruda_vector_push(
                        &parameter.constraints, &constraint_type
                    ))) {
                ruda_owned_bytes_dispose(&parameter.owned_name);
                ruda_vector_dispose(&parameter.constraints);
                ruda_checker_private_type_parameter_vector_dispose(parameters);
                ruda_checker_fail(
                    checker,
                    ruda_checker_has_failed(checker)
                        ? checker->failure : RUDA_CHECK_ALLOCATION_FAILED
                );
                checker->active_type_parameters = previous_parameters;
                return false;
            }
        }
        if (!ruda_vector_push(parameters, &parameter)) {
            ruda_owned_bytes_dispose(&parameter.owned_name);
            ruda_vector_dispose(&parameter.constraints);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        memset(&parameter, 0, sizeof(parameter));
    }
    checker->active_type_parameters = previous_parameters;
    return true;
}

const RudaCheckTypeParameterDeclaration *ruda_checker_private_find_type_parameter(
    const RudaChecker *checker,
    RudaCheckName name
) {
    const RudaVector *parameters = checker != NULL
        ? checker->active_type_parameters
        : NULL;
    size_t index = 0;
    if (parameters == NULL) {
        return NULL;
    }
    for (index = 0; index < parameters->length; index += 1) {
        const RudaCheckTypeParameterDeclaration *parameter =
            ruda_vector_at_const(parameters, index);
        if (parameter != NULL && ruda_checker_private_check_names_equal(
                checker, parameter->name, name
            )) {
            return parameter;
        }
    }
    return NULL;
}

bool ruda_checker_private_resolve_type_parameter(
    const RudaChecker *checker,
    RudaCheckName name,
    RudaTypeId *result
) {
    const RudaVector *parameters = checker != NULL
        ? checker->active_type_parameters : NULL;
    const RudaVector *arguments = checker != NULL
        ? checker->active_type_arguments : NULL;
    size_t index = 0;
    if (result == NULL || parameters == NULL) {
        return false;
    }
    for (index = 0; index < parameters->length; index += 1) {
        const RudaCheckTypeParameterDeclaration *parameter =
            ruda_vector_at_const(parameters, index);
        if (parameter != NULL && ruda_checker_private_check_names_equal(
                checker, parameter->name, name
            )) {
            if (arguments != NULL && arguments->length == parameters->length) {
                const RudaTypeId *argument = ruda_vector_at_const(arguments, index);
                if (argument != NULL && *argument != RUDA_TYPE_INVALID) {
                    *result = *argument;
                    return true;
                }
            }
            *result = parameter->type;
            return true;
        }
    }
    return false;
}

static bool ruda_checker_private_substitution_lookup(
    RudaTypeId type,
    const RudaVector *parameters,
    const RudaVector *arguments,
    RudaTypeId *result
) {
    size_t index = 0;
    if (parameters == NULL || arguments == NULL
        || parameters->length != arguments->length || result == NULL) {
        return false;
    }
    for (index = 0; index < parameters->length; index += 1) {
        const void *parameter = ruda_vector_at_const(parameters, index);
        const RudaTypeId *argument = ruda_vector_at_const(arguments, index);
        RudaTypeId parameter_type = RUDA_TYPE_INVALID;
        if (parameter != NULL
            && parameters->element_size == sizeof(RudaTypeId)) {
            parameter_type = *(const RudaTypeId *)parameter;
        } else if (parameter != NULL
            && parameters->element_size
                == sizeof(RudaCheckTypeParameterDeclaration)) {
            parameter_type =
                ((const RudaCheckTypeParameterDeclaration *)parameter)->type;
        }
        if (argument != NULL && parameter_type == type) {
            *result = *argument;
            return true;
        }
    }
    return false;
}

bool ruda_checker_private_substitute_type(
    RudaChecker *checker,
    RudaTypeId type,
    const RudaVector *parameters,
    const RudaVector *arguments,
    RudaTypeId *result
) {
    const RudaTypeDescriptor *descriptor = NULL;
    RudaTypeId *substituted = NULL;
    size_t bytes = 0;
    size_t index = 0;
    bool accepted = false;
    if (checker == NULL || result == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (ruda_checker_private_substitution_lookup(
            type, parameters, arguments, result
        )) {
        return true;
    }
    descriptor = ruda_type_descriptor(&checker->result->types, type);
    if (descriptor == NULL || descriptor->argument_count == 0) {
        *result = type;
        return true;
    }
    if (!ruda_size_multiply(
            descriptor->argument_count, sizeof(RudaTypeId), &bytes
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    substituted = ruda_allocate(&checker->result->allocator, bytes);
    if (substituted == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < descriptor->argument_count; index += 1) {
        if (!ruda_checker_private_substitute_type(
                checker,
                descriptor->arguments[index],
                parameters,
                arguments,
                &substituted[index]
            )) {
            goto cleanup;
        }
    }
    switch (descriptor->kind) {
        case RUDA_TYPE_KIND_ARRAY:
            accepted = ruda_type_intern_array(
                &checker->result->types, substituted[0], result
            );
            break;
        case RUDA_TYPE_KIND_FUNCTION:
            accepted = ruda_type_intern_function(
                &checker->result->types,
                substituted,
                descriptor->parameter_count,
                substituted[descriptor->parameter_count],
                result
            );
            break;
        case RUDA_TYPE_KIND_TUPLE:
            accepted = ruda_type_intern_tuple(
                &checker->result->types,
                substituted,
                descriptor->argument_count,
                result
            );
            break;
        default:
            accepted = ruda_type_intern_nominal(
                &checker->result->types,
                descriptor->kind,
                descriptor->symbol,
                substituted,
                descriptor->argument_count,
                result
            );
            break;
    }
cleanup:
    ruda_deallocate(&checker->result->allocator, substituted, bytes);
    if (!accepted) {
        ruda_checker_private_fail_type_intern(checker);
    }
    return accepted;
}

bool ruda_checker_private_substitute_function_type(
    RudaFunctionChecker *function,
    RudaTypeId type,
    RudaTypeId *result
) {
    return function != NULL && ruda_checker_private_substitute_type(
        function->checker,
        type,
        function->type_parameters,
        function->type_arguments,
        result
    );
}

bool ruda_checker_private_substitute_nominal_member_type(
    RudaFunctionChecker *function,
    RudaTypeId nominal_type,
    const RudaVector *parameters,
    RudaTypeId member_type,
    RudaTypeId *result
) {
    const RudaTypeDescriptor *descriptor = NULL;
    RudaVector arguments;
    RudaTypeId substituted_member = RUDA_TYPE_INVALID;
    size_t index = 0;
    bool initialized = false;
    if (function == NULL || parameters == NULL || result == NULL) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    descriptor = ruda_type_descriptor(
        &function->checker->result->types,
        nominal_type
    );
    if (descriptor == NULL || descriptor->kind != RUDA_TYPE_KIND_NOMINAL
        || descriptor->argument_count != parameters->length) {
        return ruda_checker_private_substitute_function_type(
            function, member_type, result
        );
    }
    if (!ruda_vector_init(
            &arguments,
            function->checker->result->allocator,
            sizeof(RudaTypeId)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    initialized = true;
    for (index = 0; index < descriptor->argument_count; index += 1) {
        RudaTypeId argument = descriptor->arguments[index];
        if (function->type_arguments != NULL
            && !ruda_checker_private_substitute_function_type(
                function, argument, &argument
            )) {
            ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        if (!ruda_vector_push(&arguments, &argument)) {
            ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
    }
    if (!ruda_checker_private_substitute_type(
            function->checker,
            member_type,
            parameters,
            &arguments,
            &substituted_member
        )) {
        goto cleanup;
    }
    *result = substituted_member;

cleanup:
    if (initialized) {
        ruda_vector_dispose(&arguments);
    }
    return !ruda_checker_has_failed(function->checker)
        && substituted_member != RUDA_TYPE_INVALID;
}

bool ruda_checker_private_parse_type_arguments(
    RudaChecker *checker,
    const RudaAstNode *type_arguments,
    RudaVector *arguments
) {
    size_t index = 0;
    if (checker == NULL || type_arguments == NULL || arguments == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < type_arguments->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(type_arguments, index);
        RudaTypeId type = RUDA_TYPE_INVALID;
        if (element == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind != RUDA_AST_ELEMENT_NODE) {
            if (element->as.token.synthetic) {
                ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
                return false;
            }
            continue;
        }
        if (element->as.node == NULL || element->as.node->kind != RUDA_SYNTAX_TYPE
            || !ruda_checker_private_check_exact_type(
                checker, element->as.node, &type
            )) {
            return false;
        }
        if (!ruda_vector_push(arguments, &type)) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    return true;
}

bool ruda_checker_private_intern_generic_nominal(
    RudaChecker *checker,
    RudaTypeKind kind,
    RudaSymbolId symbol,
    const RudaVector *arguments,
    RudaTypeId *result
) {
    if (checker == NULL || arguments == NULL || result == NULL) {
        return false;
    }
    if (ruda_type_intern_nominal(
            &checker->result->types,
            kind,
            symbol,
            arguments->data,
            arguments->length,
            result
        )) {
        return true;
    }
    ruda_checker_private_fail_type_intern(checker);
    return false;
}

bool ruda_checker_private_type_matches_nominal(
    const RudaChecker *checker,
    RudaTypeId type,
    RudaSymbolId symbol,
    size_t argument_count
) {
    const RudaTypeDescriptor *descriptor = checker == NULL || checker->result == NULL
        ? NULL : ruda_type_descriptor(&checker->result->types, type);
    return descriptor != NULL
        && descriptor->kind == RUDA_TYPE_KIND_NOMINAL
        && ruda_symbol_id_equal(descriptor->symbol, symbol)
        && descriptor->argument_count == argument_count;
}
