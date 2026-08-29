#include "checker_generic_types_api.h"

#include "checker_core_api.h"
#include "checker_internal.h"
#include "checker_lookup_api.h"
#include "checker_declaration_lifecycle_api.h"

static bool mmay_checker_private_type_parameter_name(
    const MarimayChecker *checker,
    MarimayCheckName name,
    const unsigned char **bytes,
    size_t *length
) {
    return checker != NULL && bytes != NULL && length != NULL
        && mmay_checker_private_check_name_bytes(name, bytes, length);
}

static void mmay_checker_private_fail_type_intern(
    MarimayChecker *checker
) {
    const size_t max_types = checker->options.type_options.max_types;
    const size_t dynamic_types = checker->result->types.descriptors.length;
    const bool at_limit = max_types <= MMAY_TYPE_FIRST_DYNAMIC
        || dynamic_types >= max_types - MMAY_TYPE_FIRST_DYNAMIC;
    mmay_checker_fail(
        checker,
        at_limit ? MMAY_CHECK_LIMIT_REACHED : MMAY_CHECK_ALLOCATION_FAILED
    );
}

bool mmay_checker_private_collect_type_parameters(
    MarimayChecker *checker,
    const MarimayAstNode *declaration_node,
    MarimayVector *parameters
) {
    const MarimayAstNode *type_parameters = NULL;
    size_t index = 0;
    const MarimayVector *previous_parameters = checker != NULL
        ? checker->active_type_parameters : NULL;
    if (checker == NULL || declaration_node == NULL || parameters == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    type_parameters = mmay_checker_private_check_child_node(
        declaration_node, MMAY_SYNTAX_TYPE_PARAMETERS, 0
    );
    if (type_parameters == NULL) {
        return true;
    }
    checker->active_type_parameters = parameters;
    if (type_parameters->child_count > checker->options.type_options.max_type_arguments) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        checker->active_type_parameters = previous_parameters;
        return false;
    }
    for (index = 0; index < type_parameters->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(type_parameters, index);
        const MarimayAstNode *parameter_node = NULL;
        const MarimaySyntaxToken *token = NULL;
        const unsigned char *bytes = NULL;
        size_t length = 0;
        MarimayCheckTypeParameterDeclaration parameter;
        MarimaySymbolId symbol;
        size_t previous = 0;
        memset(&parameter, 0, sizeof(parameter));
        memset(&symbol, 0, sizeof(symbol));
        if (element == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        if (element->kind == MMAY_AST_ELEMENT_TOKEN) {
            if (element->as.token.synthetic) {
                mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
                checker->active_type_parameters = previous_parameters;
                return false;
            }
            continue;
        }
        if (element->kind != MMAY_AST_ELEMENT_NODE
            || (parameter_node = element->as.node) == NULL
            || parameter_node->kind != MMAY_SYNTAX_TYPE_PARAMETER) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        token = mmay_checker_private_check_child_token(
            parameter_node, MMAY_TOKEN_IDENTIFIER, 0
        );
        if (token == NULL || token->synthetic) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        parameter.name.snapshot = checker->ast->snapshot;
        parameter.name.start = token->start;
        parameter.name.end = token->end;
        if (!mmay_checker_private_type_parameter_name(
                checker, parameter.name, &bytes, &length
            )) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        for (previous = 0; previous < parameters->length; previous += 1) {
            const MarimayCheckTypeParameterDeclaration *other =
                mmay_vector_at_const(parameters, previous);
            if (other != NULL && mmay_checker_private_check_names_equal(
                    checker, other->name, parameter.name
                )) {
                (void)mmay_checker_add_diagnostic(
                    checker,
                    MMAY_DIAGNOSTIC_DUPLICATE_NAME,
                    "generic type parameter is declared more than once",
                    parameter.name.start,
                    parameter.name.end
                );
                checker->active_type_parameters = previous_parameters;
                return !mmay_checker_has_failed(checker);
            }
        }
        if (!mmay_owned_bytes_copy(
                &parameter.owned_name,
                checker->result->allocator,
                bytes,
                length
            ) || !mmay_symbol_id_from_bytes(
                checker->package_name,
                checker->package_name_length,
                mmay_source_path(checker->ast->snapshot),
                mmay_source_path_length(checker->ast->snapshot),
                bytes,
                length,
                UINT32_C(0x54500000) | (uint32_t)index,
                &symbol
            )) {
            mmay_owned_bytes_dispose(&parameter.owned_name);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        if (!mmay_type_intern_nominal(
                &checker->result->types,
                MMAY_TYPE_KIND_TYPE_PARAMETER,
                symbol,
                NULL,
                0,
                &parameter.type
            )) {
            mmay_owned_bytes_dispose(&parameter.owned_name);
            mmay_checker_private_fail_type_intern(checker);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        parameter.name_initialized = true;
        if (!mmay_vector_init(
                &parameter.constraints,
                checker->result->allocator,
                sizeof(MarimayTypeId)
            )) {
            mmay_owned_bytes_dispose(&parameter.owned_name);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        parameter.constraints_initialized = true;
        {
            const MarimayAstNode *constraint = mmay_checker_private_check_child_node(
                parameter_node, MMAY_SYNTAX_TYPE, 0
            );
            MarimayTypeId constraint_type = MMAY_TYPE_INVALID;
            if (constraint != NULL
                && (!mmay_checker_private_check_exact_type(
                        checker, constraint, &constraint_type
                    ) || !mmay_vector_push(
                        &parameter.constraints, &constraint_type
                    ))) {
                mmay_owned_bytes_dispose(&parameter.owned_name);
                mmay_vector_dispose(&parameter.constraints);
                mmay_checker_private_type_parameter_vector_dispose(parameters);
                mmay_checker_fail(
                    checker,
                    mmay_checker_has_failed(checker)
                        ? checker->failure : MMAY_CHECK_ALLOCATION_FAILED
                );
                checker->active_type_parameters = previous_parameters;
                return false;
            }
        }
        if (!mmay_vector_push(parameters, &parameter)) {
            mmay_owned_bytes_dispose(&parameter.owned_name);
            mmay_vector_dispose(&parameter.constraints);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            checker->active_type_parameters = previous_parameters;
            return false;
        }
        memset(&parameter, 0, sizeof(parameter));
    }
    checker->active_type_parameters = previous_parameters;
    return true;
}

const MarimayCheckTypeParameterDeclaration *mmay_checker_private_find_type_parameter(
    const MarimayChecker *checker,
    MarimayCheckName name
) {
    const MarimayVector *parameters = checker != NULL
        ? checker->active_type_parameters
        : NULL;
    size_t index = 0;
    if (parameters == NULL) {
        return NULL;
    }
    for (index = 0; index < parameters->length; index += 1) {
        const MarimayCheckTypeParameterDeclaration *parameter =
            mmay_vector_at_const(parameters, index);
        if (parameter != NULL && mmay_checker_private_check_names_equal(
                checker, parameter->name, name
            )) {
            return parameter;
        }
    }
    return NULL;
}

bool mmay_checker_private_resolve_type_parameter(
    const MarimayChecker *checker,
    MarimayCheckName name,
    MarimayTypeId *result
) {
    const MarimayVector *parameters = checker != NULL
        ? checker->active_type_parameters : NULL;
    const MarimayVector *arguments = checker != NULL
        ? checker->active_type_arguments : NULL;
    size_t index = 0;
    if (result == NULL || parameters == NULL) {
        return false;
    }
    for (index = 0; index < parameters->length; index += 1) {
        const MarimayCheckTypeParameterDeclaration *parameter =
            mmay_vector_at_const(parameters, index);
        if (parameter != NULL && mmay_checker_private_check_names_equal(
                checker, parameter->name, name
            )) {
            if (arguments != NULL && arguments->length == parameters->length) {
                const MarimayTypeId *argument = mmay_vector_at_const(arguments, index);
                if (argument != NULL && *argument != MMAY_TYPE_INVALID) {
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

static bool mmay_checker_private_substitution_lookup(
    MarimayTypeId type,
    const MarimayVector *parameters,
    const MarimayVector *arguments,
    MarimayTypeId *result
) {
    size_t index = 0;
    if (parameters == NULL || arguments == NULL
        || parameters->length != arguments->length || result == NULL) {
        return false;
    }
    for (index = 0; index < parameters->length; index += 1) {
        const void *parameter = mmay_vector_at_const(parameters, index);
        const MarimayTypeId *argument = mmay_vector_at_const(arguments, index);
        MarimayTypeId parameter_type = MMAY_TYPE_INVALID;
        if (parameter != NULL
            && parameters->element_size == sizeof(MarimayTypeId)) {
            parameter_type = *(const MarimayTypeId *)parameter;
        } else if (parameter != NULL
            && parameters->element_size
                == sizeof(MarimayCheckTypeParameterDeclaration)) {
            parameter_type =
                ((const MarimayCheckTypeParameterDeclaration *)parameter)->type;
        }
        if (argument != NULL && parameter_type == type) {
            *result = *argument;
            return true;
        }
    }
    return false;
}

bool mmay_checker_private_substitute_type(
    MarimayChecker *checker,
    MarimayTypeId type,
    const MarimayVector *parameters,
    const MarimayVector *arguments,
    MarimayTypeId *result
) {
    const MarimayTypeDescriptor *descriptor = NULL;
    MarimayTypeId *substituted = NULL;
    size_t bytes = 0;
    size_t index = 0;
    bool accepted = false;
    if (checker == NULL || result == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (mmay_checker_private_substitution_lookup(
            type, parameters, arguments, result
        )) {
        return true;
    }
    descriptor = mmay_type_descriptor(&checker->result->types, type);
    if (descriptor == NULL || descriptor->argument_count == 0) {
        *result = type;
        return true;
    }
    if (!mmay_size_multiply(
            descriptor->argument_count, sizeof(MarimayTypeId), &bytes
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    substituted = mmay_allocate(&checker->result->allocator, bytes);
    if (substituted == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < descriptor->argument_count; index += 1) {
        if (!mmay_checker_private_substitute_type(
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
        case MMAY_TYPE_KIND_ARRAY:
            accepted = mmay_type_intern_array(
                &checker->result->types, substituted[0], result
            );
            break;
        case MMAY_TYPE_KIND_MAP:
            accepted = descriptor->argument_count == 2
                && mmay_type_intern_map(
                    &checker->result->types,
                    substituted[0],
                    substituted[1],
                    result
                );
            break;
        case MMAY_TYPE_KIND_SET:
            accepted = descriptor->argument_count == 1
                && mmay_type_intern_set(
                    &checker->result->types, substituted[0], result
                );
            break;
        case MMAY_TYPE_KIND_FUNCTION:
            accepted = mmay_type_intern_function(
                &checker->result->types,
                substituted,
                descriptor->parameter_count,
                substituted[descriptor->parameter_count],
                result
            );
            break;
        case MMAY_TYPE_KIND_TUPLE:
            accepted = mmay_type_intern_tuple(
                &checker->result->types,
                substituted,
                descriptor->argument_count,
                result
            );
            break;
        default:
            accepted = mmay_type_intern_nominal(
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
    mmay_deallocate(&checker->result->allocator, substituted, bytes);
    if (!accepted) {
        mmay_checker_private_fail_type_intern(checker);
    }
    return accepted;
}

bool mmay_checker_private_substitute_function_type(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayTypeId *result
) {
    return function != NULL && mmay_checker_private_substitute_type(
        function->checker,
        type,
        function->type_parameters,
        function->type_arguments,
        result
    );
}

bool mmay_checker_private_substitute_nominal_member_type(
    MarimayFunctionChecker *function,
    MarimayTypeId nominal_type,
    const MarimayVector *parameters,
    MarimayTypeId member_type,
    MarimayTypeId *result
) {
    const MarimayTypeDescriptor *descriptor = NULL;
    MarimayVector arguments;
    MarimayTypeId substituted_member = MMAY_TYPE_INVALID;
    size_t index = 0;
    bool initialized = false;
    if (function == NULL || parameters == NULL || result == NULL) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    descriptor = mmay_type_descriptor(
        &function->checker->result->types,
        nominal_type
    );
    if (descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_NOMINAL
        || descriptor->argument_count != parameters->length) {
        return mmay_checker_private_substitute_function_type(
            function, member_type, result
        );
    }
    if (!mmay_vector_init(
            &arguments,
            function->checker->result->allocator,
            sizeof(MarimayTypeId)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    initialized = true;
    for (index = 0; index < descriptor->argument_count; index += 1) {
        MarimayTypeId argument = descriptor->arguments[index];
        if (function->type_arguments != NULL
            && !mmay_checker_private_substitute_function_type(
                function, argument, &argument
            )) {
            mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
        if (!mmay_vector_push(&arguments, &argument)) {
            mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
    }
    if (!mmay_checker_private_substitute_type(
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
        mmay_vector_dispose(&arguments);
    }
    return !mmay_checker_has_failed(function->checker)
        && substituted_member != MMAY_TYPE_INVALID;
}

bool mmay_checker_private_parse_type_arguments(
    MarimayChecker *checker,
    const MarimayAstNode *type_arguments,
    MarimayVector *arguments
) {
    size_t index = 0;
    if (checker == NULL || type_arguments == NULL || arguments == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < type_arguments->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(type_arguments, index);
        MarimayTypeId type = MMAY_TYPE_INVALID;
        if (element == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind != MMAY_AST_ELEMENT_NODE) {
            if (element->as.token.synthetic) {
                mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
                return false;
            }
            continue;
        }
        if (element->as.node == NULL || element->as.node->kind != MMAY_SYNTAX_TYPE
            || !mmay_checker_private_check_exact_type(
                checker, element->as.node, &type
            )) {
            return false;
        }
        if (!mmay_vector_push(arguments, &type)) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    return true;
}

bool mmay_checker_private_intern_generic_nominal(
    MarimayChecker *checker,
    MarimayTypeKind kind,
    MarimaySymbolId symbol,
    const MarimayVector *arguments,
    MarimayTypeId *result
) {
    if (checker == NULL || arguments == NULL || result == NULL) {
        return false;
    }
    if (mmay_type_intern_nominal(
            &checker->result->types,
            kind,
            symbol,
            arguments->data,
            arguments->length,
            result
        )) {
        return true;
    }
    mmay_checker_private_fail_type_intern(checker);
    return false;
}

bool mmay_checker_private_type_matches_nominal(
    const MarimayChecker *checker,
    MarimayTypeId type,
    MarimaySymbolId symbol,
    size_t argument_count
) {
    const MarimayTypeDescriptor *descriptor = checker == NULL || checker->result == NULL
        ? NULL : mmay_type_descriptor(&checker->result->types, type);
    return descriptor != NULL
        && descriptor->kind == MMAY_TYPE_KIND_NOMINAL
        && mmay_symbol_id_equal(descriptor->symbol, symbol)
        && descriptor->argument_count == argument_count;
}
