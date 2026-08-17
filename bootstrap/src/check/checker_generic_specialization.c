#include "checker_generic_specialization_api.h"

#include "checker_core_api.h"
#include "checker_declaration_lifecycle_api.h"
#include "checker_internal.h"
#include "checker_lookup_api.h"

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

const RudaCheckFunctionDeclaration *ruda_checker_private_find_function_by_symbol(
    const RudaChecker *checker,
    RudaSymbolId symbol,
    size_t *index
) {
    size_t current = 0;
    if (checker == NULL) {
        return NULL;
    }
    for (current = 0; current < checker->declarations.length; current += 1) {
        const RudaCheckFunctionDeclaration *declaration = ruda_vector_at_const(
            &checker->declarations, current
        );
        if (declaration != NULL && declaration->signature_valid
            && ruda_symbol_id_equal(declaration->symbol, symbol)) {
            if (index != NULL) {
                *index = current;
            }
            return declaration;
        }
    }
    return NULL;
}

static bool ruda_checker_private_generic_parameter_index(
    const RudaCheckFunctionDeclaration *declaration,
    RudaTypeId type,
    size_t *index
) {
    size_t current = 0;
    if (declaration == NULL || index == NULL) {
        return false;
    }
    for (current = 0; current < declaration->type_parameters.length; current += 1) {
        const RudaCheckTypeParameterDeclaration *parameter =
            ruda_vector_at_const(&declaration->type_parameters, current);
        if (parameter != NULL && parameter->type == type) {
            *index = current;
            return true;
        }
    }
    return false;
}

static bool ruda_checker_private_infer_generic_type(
    const RudaChecker *checker,
    const RudaCheckFunctionDeclaration *declaration,
    RudaTypeId formal,
    RudaTypeId actual,
    RudaTypeId *arguments
) {
    const RudaTypeDescriptor *formal_descriptor = NULL;
    const RudaTypeDescriptor *actual_descriptor = NULL;
    size_t parameter_index = 0;
    size_t index = 0;
    if (checker == NULL || declaration == NULL || arguments == NULL) {
        return false;
    }
    if (ruda_checker_private_generic_parameter_index(
            declaration, formal, &parameter_index
        )) {
        if (arguments[parameter_index] == RUDA_TYPE_INVALID) {
            arguments[parameter_index] = actual;
            return true;
        }
        return arguments[parameter_index] == actual;
    }
    if (formal == actual) {
        return true;
    }
    formal_descriptor = ruda_type_descriptor(&checker->result->types, formal);
    actual_descriptor = ruda_type_descriptor(&checker->result->types, actual);
    if (formal_descriptor == NULL || actual_descriptor == NULL
        || formal_descriptor->kind != actual_descriptor->kind
        || formal_descriptor->argument_count != actual_descriptor->argument_count
        || !ruda_symbol_id_equal(
            formal_descriptor->symbol, actual_descriptor->symbol
        )) {
        return false;
    }
    for (index = 0; index < formal_descriptor->argument_count; index += 1) {
        if (!ruda_checker_private_infer_generic_type(
                checker,
                declaration,
                formal_descriptor->arguments[index],
                actual_descriptor->arguments[index],
                arguments
            )) {
            return false;
        }
    }
    return true;
}

static bool ruda_checker_private_validate_generic_constraints(
    RudaChecker *checker,
    const RudaCheckFunctionDeclaration *declaration,
    const RudaVector *arguments
) {
    size_t index = 0;
    if (checker == NULL || declaration == NULL || arguments == NULL) {
        return false;
    }
    for (index = 0; index < declaration->type_parameters.length; index += 1) {
        const RudaCheckTypeParameterDeclaration *parameter =
            ruda_vector_at_const(&declaration->type_parameters, index);
        const RudaTypeId *argument = ruda_vector_at_const(arguments, index);
        size_t constraint_index = 0;
        if (parameter == NULL || argument == NULL) {
            return false;
        }
        for (constraint_index = 0;
            constraint_index < parameter->constraints.length;
            constraint_index += 1) {
            const RudaTypeId *constraint = ruda_vector_at_const(
                &parameter->constraints, constraint_index
            );
            RudaCheckInterfaceDeclaration *interface_declaration = NULL;
            size_t method_index = 0;
            if (constraint == NULL
                || (interface_declaration =
                    ruda_checker_private_checker_find_interface_by_type(
                        checker, *constraint
                    )) == NULL) {
                (void)ruda_checker_add_diagnostic(
                    checker,
                    RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                    "generic constraint is not a known interface contract",
                    declaration->name.start,
                    declaration->name.end
                );
                return false;
            }
            for (method_index = 0; method_index < interface_declaration->methods.length;
                method_index += 1) {
                const RudaCheckInterfaceMethodDeclaration *method =
                    ruda_vector_at_const(
                        &interface_declaration->methods, method_index
                    );
                RudaCheckCallable callable;
                RudaCheckCallResolution resolution;
                size_t parameter_index = 0;
                if (method == NULL) {
                    return false;
                }
                resolution = ruda_check_resolve_call(
                    checker, method->name, true, *argument, &callable
                );
                if (resolution != RUDA_CHECK_CALL_FOUND
                    || callable.parameter_count != method->parameter_types.length + 1
                    || callable.result_type != method->result_type) {
                    (void)ruda_checker_add_diagnostic(
                        checker,
                        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                        "generic type argument does not satisfy its interface constraint",
                        declaration->name.start,
                        declaration->name.end
                    );
                    return false;
                }
                for (parameter_index = 0;
                    parameter_index < method->parameter_types.length;
                    parameter_index += 1) {
                    if (callable.parameter_types[parameter_index + 1]
                            != *(const RudaTypeId *)ruda_vector_at_const(
                                &method->parameter_types, parameter_index
                            )) {
                        (void)ruda_checker_add_diagnostic(
                            checker,
                            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                            "generic type argument does not satisfy its interface method signature",
                            declaration->name.start,
                            declaration->name.end
                        );
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

bool ruda_checker_private_prepare_generic_arguments(
    RudaChecker *checker,
    const RudaCheckFunctionDeclaration *declaration,
    const RudaTypeId *actual_types,
    size_t actual_count,
    const RudaVector *explicit_arguments,
    RudaVector *arguments
) {
    size_t index = 0;
    if (checker == NULL || declaration == NULL || arguments == NULL
        || !declaration->is_generic
        || (actual_types == NULL && actual_count != 0)) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (explicit_arguments != NULL
        && explicit_arguments->length != declaration->type_parameters.length) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "explicit generic argument count does not match the function declaration",
            declaration->name.start,
            declaration->name.end
        );
        return false;
    }
    for (index = 0; index < declaration->type_parameters.length; index += 1) {
        RudaTypeId type = explicit_arguments == NULL
            ? RUDA_TYPE_INVALID
            : *(const RudaTypeId *)ruda_vector_at_const(
                explicit_arguments, index
            );
        if (!ruda_vector_push(arguments, &type)) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    if (explicit_arguments == NULL) {
        for (index = 0; index < actual_count; index += 1) {
            const RudaTypeId *formal = ruda_vector_at_const(
                &declaration->parameter_types, index
            );
            if (formal == NULL || !ruda_checker_private_infer_generic_type(
                    checker,
                    declaration,
                    *formal,
                    actual_types[index],
                    (RudaTypeId *)arguments->data
                )) {
                (void)ruda_checker_add_diagnostic(
                    checker,
                    RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                    "generic argument inference cannot match the call arguments",
                    declaration->name.start,
                    declaration->name.end
                );
                return false;
            }
        }
    }
    for (index = 0; index < arguments->length; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(arguments, index);
        if (type == NULL || *type == RUDA_TYPE_INVALID) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "generic type parameter cannot be inferred; provide explicit type arguments",
                declaration->name.start,
                declaration->name.end
            );
            return false;
        }
    }
    return ruda_checker_private_validate_generic_constraints(
        checker, declaration, arguments
    );
}

static bool ruda_checker_private_copy_type_parameters(
    RudaChecker *checker,
    const RudaVector *source,
    RudaVector *target
) {
    size_t index = 0;
    if (!ruda_vector_init(
            target,
            checker->result->allocator,
            sizeof(RudaCheckTypeParameterDeclaration)
        )) {
        return false;
    }
    for (index = 0; index < source->length; index += 1) {
        const RudaCheckTypeParameterDeclaration *source_parameter =
            ruda_vector_at_const(source, index);
        RudaCheckTypeParameterDeclaration parameter;
        memset(&parameter, 0, sizeof(parameter));
        if (source_parameter == NULL
            || !ruda_owned_bytes_copy(
                &parameter.owned_name,
                checker->result->allocator,
                source_parameter->owned_name.data,
                source_parameter->owned_name.length
            )) {
            ruda_checker_private_type_parameter_vector_dispose(target);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        parameter.name = source_parameter->name;
        parameter.type = source_parameter->type;
        parameter.name_initialized = true;
        if (!ruda_vector_init(
                &parameter.constraints,
                checker->result->allocator,
                sizeof(RudaTypeId)
            ) || !ruda_vector_append(
                &parameter.constraints,
                source_parameter->constraints.data,
                source_parameter->constraints.length
            )) {
            ruda_owned_bytes_dispose(&parameter.owned_name);
            ruda_vector_dispose(&parameter.constraints);
            ruda_checker_private_type_parameter_vector_dispose(target);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        parameter.constraints_initialized = true;
        if (!ruda_vector_push(target, &parameter)) {
            ruda_owned_bytes_dispose(&parameter.owned_name);
            ruda_vector_dispose(&parameter.constraints);
            ruda_checker_private_type_parameter_vector_dispose(target);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        memset(&parameter, 0, sizeof(parameter));
    }
    return true;
}

static bool ruda_checker_private_derive_instance_symbol(
    RudaChecker *checker,
    const RudaCheckFunctionDeclaration *base,
    const RudaVector *arguments,
    RudaSymbolId *result
) {
    unsigned char *bytes = NULL;
    size_t type_bytes = 0;
    size_t total = 0;
    size_t index = 0;
    bool accepted = false;
    if (!ruda_size_multiply(arguments->length, sizeof(RudaTypeId), &type_bytes)
        || !ruda_size_add(base->owned_name.length, type_bytes, &total)) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    bytes = ruda_allocate(&checker->result->allocator, total == 0 ? 1 : total);
    if (bytes == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    memcpy(bytes, base->owned_name.data, base->owned_name.length);
    for (index = 0; index < arguments->length; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(arguments, index);
        if (type == NULL) {
            goto cleanup;
        }
        memcpy(bytes + base->owned_name.length + index * sizeof(RudaTypeId),
            type, sizeof(RudaTypeId));
    }
    accepted = ruda_symbol_id_from_bytes(
        checker->package_name,
        checker->package_name_length,
        ruda_source_path(base->ast->snapshot),
        ruda_source_path_length(base->ast->snapshot),
        bytes,
        total,
        UINT32_C(0x47500001),
        result
    );
cleanup:
    ruda_deallocate(&checker->result->allocator, bytes, total == 0 ? 1 : total);
    if (!accepted) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
    }
    return accepted;
}

bool ruda_checker_private_instantiate_function(
    RudaChecker *checker,
    size_t base_index,
    const RudaVector *arguments,
    RudaCheckFunctionDeclaration **result
) {
    const RudaCheckFunctionDeclaration *base = NULL;
    RudaCheckFunctionDeclaration instance;
    size_t index = 0;
    bool accepted = false;
    memset(&instance, 0, sizeof(instance));
    if (checker == NULL || arguments == NULL || result == NULL
        || base_index >= checker->declarations.length) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    base = ruda_vector_at_const(&checker->declarations, base_index);
    if (base == NULL || !base->signature_valid || !base->is_generic
        || arguments->length != base->type_parameters.length) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < checker->declarations.length; index += 1) {
        const RudaCheckFunctionDeclaration *existing = ruda_vector_at_const(
            &checker->declarations, index
        );
        size_t argument_index = 0;
        bool same = existing != NULL && existing->is_instance
            && existing->generic_base_index == base_index
            && existing->generic_arguments.length == arguments->length;
        for (argument_index = 0; same && argument_index < arguments->length;
            argument_index += 1) {
            const RudaTypeId *left = ruda_vector_at_const(
                &existing->generic_arguments, argument_index
            );
            const RudaTypeId *right = ruda_vector_at_const(arguments, argument_index);
            same = left != NULL && right != NULL && *left == *right;
        }
        if (same) {
            *result = ruda_vector_at(&checker->declarations, index);
            return true;
        }
    }
    if (checker->declarations.length >= checker->options.max_functions) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    instance.ast = base->ast;
    instance.node = base->node;
    instance.block = base->block;
    instance.name = base->name;
    instance.symbol = base->symbol;
    instance.receiver_symbol = base->receiver_symbol;
    instance.receiver_type = base->receiver_type;
    instance.signature_valid = base->signature_valid;
    instance.dot_eligible = base->dot_eligible;
    instance.is_public = base->is_public;
    instance.is_unsafe = base->is_unsafe;
    instance.is_external = base->is_external;
    instance.is_test = false;
    instance.is_fuzz = false;
    instance.is_instance = true;
    instance.generic_base_index = base_index;
    if (!ruda_owned_bytes_copy(
            &instance.owned_name,
            checker->result->allocator,
            base->owned_name.data,
            base->owned_name.length
        )) {
        ruda_checker_private_function_declaration_dispose(&instance);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    instance.name_initialized = true;
    if (!ruda_vector_init(
            &instance.parameter_names,
            checker->result->allocator,
            sizeof(RudaCheckName)
        )) {
        ruda_checker_private_function_declaration_dispose(&instance);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    instance.parameter_names_initialized = true;
    if (!ruda_vector_init(
            &instance.parameter_types,
            checker->result->allocator,
            sizeof(RudaTypeId)
        )) {
        ruda_checker_private_function_declaration_dispose(&instance);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    instance.parameter_types_initialized = true;
    if (!ruda_vector_init(
            &instance.generic_arguments,
            checker->result->allocator,
            sizeof(RudaTypeId)
        )) {
        ruda_checker_private_function_declaration_dispose(&instance);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    instance.generic_arguments_initialized = true;
    if (!ruda_checker_private_copy_type_parameters(
            checker, &base->type_parameters, &instance.type_parameters
        )) {
        ruda_checker_private_function_declaration_dispose(&instance);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    instance.type_parameters_initialized = true;
    for (index = 0; index < arguments->length; index += 1) {
        const RudaTypeId *argument = ruda_vector_at_const(arguments, index);
        if (argument == NULL || !ruda_vector_push(&instance.generic_arguments, argument)) {
            ruda_checker_private_function_declaration_dispose(&instance);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    for (index = 0; index < base->parameter_names.length; index += 1) {
        const RudaCheckName *name = ruda_vector_at_const(&base->parameter_names, index);
        const RudaTypeId *type = ruda_vector_at_const(&base->parameter_types, index);
        RudaTypeId substituted = RUDA_TYPE_INVALID;
        if (name == NULL || type == NULL
            || !ruda_vector_push(&instance.parameter_names, name)
            || !ruda_checker_private_substitute_type(
                checker,
                *type,
                &base->type_parameters,
                arguments,
                &substituted
            ) || !ruda_vector_push(&instance.parameter_types, &substituted)) {
            ruda_checker_private_function_declaration_dispose(&instance);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    if (!ruda_checker_private_substitute_type(
            checker,
            base->result_type,
            &base->type_parameters,
            arguments,
            &instance.result_type
        ) || !ruda_type_intern_function(
            &checker->result->types,
            instance.parameter_types.data,
            instance.parameter_types.length,
            instance.result_type,
            &instance.signature_type
        ) || !ruda_checker_private_derive_instance_symbol(
            checker, base, arguments, &instance.symbol
    )) {
        ruda_checker_private_function_declaration_dispose(&instance);
        if (checker->failure == RUDA_CHECK_OK) {
            ruda_checker_private_fail_type_intern(checker);
        }
        return false;
    }
    if (!ruda_vector_push(&checker->declarations, &instance)) {
        ruda_checker_private_function_declaration_dispose(&instance);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    memset(&instance, 0, sizeof(instance));
    *result = ruda_vector_at(&checker->declarations, checker->declarations.length - 1);
    accepted = *result != NULL;
    if (!accepted) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
    }
    return accepted;
}
