#include "checker_generic_specialization_api.h"

#include "checker_core_api.h"
#include "checker_declaration_lifecycle_api.h"
#include "checker_internal.h"
#include "checker_lookup_api.h"

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

const MarimayCheckFunctionDeclaration *mmay_checker_private_find_function_by_symbol(
    const MarimayChecker *checker,
    MarimaySymbolId symbol,
    size_t *index
) {
    size_t current = 0;
    if (checker == NULL) {
        return NULL;
    }
    for (current = 0; current < checker->declarations.length; current += 1) {
        const MarimayCheckFunctionDeclaration *declaration = mmay_vector_at_const(
            &checker->declarations, current
        );
        if (declaration != NULL && declaration->signature_valid
            && mmay_symbol_id_equal(declaration->symbol, symbol)) {
            if (index != NULL) {
                *index = current;
            }
            return declaration;
        }
    }
    return NULL;
}

static bool mmay_checker_private_generic_parameter_index(
    const MarimayCheckFunctionDeclaration *declaration,
    MarimayTypeId type,
    size_t *index
) {
    size_t current = 0;
    if (declaration == NULL || index == NULL) {
        return false;
    }
    for (current = 0; current < declaration->type_parameters.length; current += 1) {
        const MarimayCheckTypeParameterDeclaration *parameter =
            mmay_vector_at_const(&declaration->type_parameters, current);
        if (parameter != NULL && parameter->type == type) {
            *index = current;
            return true;
        }
    }
    return false;
}

static bool mmay_checker_private_infer_generic_type(
    const MarimayChecker *checker,
    const MarimayCheckFunctionDeclaration *declaration,
    MarimayTypeId formal,
    MarimayTypeId actual,
    MarimayTypeId *arguments
) {
    const MarimayTypeDescriptor *formal_descriptor = NULL;
    const MarimayTypeDescriptor *actual_descriptor = NULL;
    size_t parameter_index = 0;
    size_t index = 0;
    if (checker == NULL || declaration == NULL || arguments == NULL) {
        return false;
    }
    if (mmay_checker_private_generic_parameter_index(
            declaration, formal, &parameter_index
        )) {
        if (arguments[parameter_index] == MMAY_TYPE_INVALID) {
            arguments[parameter_index] = actual;
            return true;
        }
        return arguments[parameter_index] == actual;
    }
    if (formal == actual) {
        return true;
    }
    formal_descriptor = mmay_type_descriptor(&checker->result->types, formal);
    actual_descriptor = mmay_type_descriptor(&checker->result->types, actual);
    if (formal_descriptor == NULL || actual_descriptor == NULL
        || formal_descriptor->kind != actual_descriptor->kind
        || formal_descriptor->argument_count != actual_descriptor->argument_count
        || !mmay_symbol_id_equal(
            formal_descriptor->symbol, actual_descriptor->symbol
        )) {
        return false;
    }
    for (index = 0; index < formal_descriptor->argument_count; index += 1) {
        if (!mmay_checker_private_infer_generic_type(
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

static bool mmay_checker_private_validate_generic_constraints(
    MarimayChecker *checker,
    const MarimayCheckFunctionDeclaration *declaration,
    const MarimayVector *arguments
) {
    size_t index = 0;
    if (checker == NULL || declaration == NULL || arguments == NULL) {
        return false;
    }
    for (index = 0; index < declaration->type_parameters.length; index += 1) {
        const MarimayCheckTypeParameterDeclaration *parameter =
            mmay_vector_at_const(&declaration->type_parameters, index);
        const MarimayTypeId *argument = mmay_vector_at_const(arguments, index);
        size_t constraint_index = 0;
        if (parameter == NULL || argument == NULL) {
            return false;
        }
        for (constraint_index = 0;
            constraint_index < parameter->constraints.length;
            constraint_index += 1) {
            const MarimayTypeId *constraint = mmay_vector_at_const(
                &parameter->constraints, constraint_index
            );
            MarimayCheckInterfaceDeclaration *interface_declaration = NULL;
            size_t method_index = 0;
            if (constraint == NULL
                || (interface_declaration =
                    mmay_checker_private_checker_find_interface_by_type(
                        checker, *constraint
                    )) == NULL) {
                (void)mmay_checker_add_diagnostic(
                    checker,
                    MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                    "generic constraint is not a known interface contract",
                    declaration->name.start,
                    declaration->name.end
                );
                return false;
            }
            for (method_index = 0; method_index < interface_declaration->methods.length;
                method_index += 1) {
                const MarimayCheckInterfaceMethodDeclaration *method =
                    mmay_vector_at_const(
                        &interface_declaration->methods, method_index
                    );
                MarimayCheckCallable callable;
                MarimayCheckCallResolution resolution;
                size_t parameter_index = 0;
                if (method == NULL) {
                    return false;
                }
                resolution = mmay_check_resolve_call(
                    checker, method->name, true, *argument, &callable
                );
                if (resolution != MMAY_CHECK_CALL_FOUND
                    || callable.parameter_count != method->parameter_types.length + 1
                    || callable.result_type != method->result_type) {
                    (void)mmay_checker_add_diagnostic(
                        checker,
                        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
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
                            != *(const MarimayTypeId *)mmay_vector_at_const(
                                &method->parameter_types, parameter_index
                            )) {
                        (void)mmay_checker_add_diagnostic(
                            checker,
                            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
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

bool mmay_checker_private_prepare_generic_arguments(
    MarimayChecker *checker,
    const MarimayCheckFunctionDeclaration *declaration,
    const MarimayTypeId *actual_types,
    size_t actual_count,
    const MarimayVector *explicit_arguments,
    MarimayVector *arguments
) {
    size_t index = 0;
    if (checker == NULL || declaration == NULL || arguments == NULL
        || !declaration->is_generic
        || (actual_types == NULL && actual_count != 0)) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (explicit_arguments != NULL
        && explicit_arguments->length != declaration->type_parameters.length) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "explicit generic argument count does not match the function declaration",
            declaration->name.start,
            declaration->name.end
        );
        return false;
    }
    for (index = 0; index < declaration->type_parameters.length; index += 1) {
        MarimayTypeId type = explicit_arguments == NULL
            ? MMAY_TYPE_INVALID
            : *(const MarimayTypeId *)mmay_vector_at_const(
                explicit_arguments, index
            );
        if (!mmay_vector_push(arguments, &type)) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    if (explicit_arguments == NULL) {
        for (index = 0; index < actual_count; index += 1) {
            const MarimayTypeId *formal = mmay_vector_at_const(
                &declaration->parameter_types, index
            );
            if (formal == NULL || !mmay_checker_private_infer_generic_type(
                    checker,
                    declaration,
                    *formal,
                    actual_types[index],
                    (MarimayTypeId *)arguments->data
                )) {
                (void)mmay_checker_add_diagnostic(
                    checker,
                    MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                    "generic argument inference cannot match the call arguments",
                    declaration->name.start,
                    declaration->name.end
                );
                return false;
            }
        }
    }
    for (index = 0; index < arguments->length; index += 1) {
        const MarimayTypeId *type = mmay_vector_at_const(arguments, index);
        if (type == NULL || *type == MMAY_TYPE_INVALID) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "generic type parameter cannot be inferred; provide explicit type arguments",
                declaration->name.start,
                declaration->name.end
            );
            return false;
        }
    }
    return mmay_checker_private_validate_generic_constraints(
        checker, declaration, arguments
    );
}

static bool mmay_checker_private_copy_type_parameters(
    MarimayChecker *checker,
    const MarimayVector *source,
    MarimayVector *target
) {
    size_t index = 0;
    if (!mmay_vector_init(
            target,
            checker->result->allocator,
            sizeof(MarimayCheckTypeParameterDeclaration)
        )) {
        return false;
    }
    for (index = 0; index < source->length; index += 1) {
        const MarimayCheckTypeParameterDeclaration *source_parameter =
            mmay_vector_at_const(source, index);
        MarimayCheckTypeParameterDeclaration parameter;
        memset(&parameter, 0, sizeof(parameter));
        if (source_parameter == NULL
            || !mmay_owned_bytes_copy(
                &parameter.owned_name,
                checker->result->allocator,
                source_parameter->owned_name.data,
                source_parameter->owned_name.length
            )) {
            mmay_checker_private_type_parameter_vector_dispose(target);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        parameter.name = source_parameter->name;
        parameter.type = source_parameter->type;
        parameter.name_initialized = true;
        if (!mmay_vector_init(
                &parameter.constraints,
                checker->result->allocator,
                sizeof(MarimayTypeId)
            ) || !mmay_vector_append(
                &parameter.constraints,
                source_parameter->constraints.data,
                source_parameter->constraints.length
            )) {
            mmay_owned_bytes_dispose(&parameter.owned_name);
            mmay_vector_dispose(&parameter.constraints);
            mmay_checker_private_type_parameter_vector_dispose(target);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        parameter.constraints_initialized = true;
        if (!mmay_vector_push(target, &parameter)) {
            mmay_owned_bytes_dispose(&parameter.owned_name);
            mmay_vector_dispose(&parameter.constraints);
            mmay_checker_private_type_parameter_vector_dispose(target);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        memset(&parameter, 0, sizeof(parameter));
    }
    return true;
}

static bool mmay_checker_private_derive_instance_symbol(
    MarimayChecker *checker,
    const MarimayCheckFunctionDeclaration *base,
    const MarimayVector *arguments,
    MarimaySymbolId *result
) {
    unsigned char *bytes = NULL;
    size_t type_bytes = 0;
    size_t total = 0;
    size_t index = 0;
    bool accepted = false;
    if (!mmay_size_multiply(arguments->length, sizeof(MarimayTypeId), &type_bytes)
        || !mmay_size_add(base->owned_name.length, type_bytes, &total)) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    bytes = mmay_allocate(&checker->result->allocator, total == 0 ? 1 : total);
    if (bytes == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    memcpy(bytes, base->owned_name.data, base->owned_name.length);
    for (index = 0; index < arguments->length; index += 1) {
        const MarimayTypeId *type = mmay_vector_at_const(arguments, index);
        if (type == NULL) {
            goto cleanup;
        }
        memcpy(bytes + base->owned_name.length + index * sizeof(MarimayTypeId),
            type, sizeof(MarimayTypeId));
    }
    accepted = mmay_symbol_id_from_bytes(
        checker->package_name,
        checker->package_name_length,
        mmay_source_path(base->ast->snapshot),
        mmay_source_path_length(base->ast->snapshot),
        bytes,
        total,
        UINT32_C(0x47500001),
        result
    );
cleanup:
    mmay_deallocate(&checker->result->allocator, bytes, total == 0 ? 1 : total);
    if (!accepted) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
    }
    return accepted;
}

bool mmay_checker_private_instantiate_function(
    MarimayChecker *checker,
    size_t base_index,
    const MarimayVector *arguments,
    MarimayCheckFunctionDeclaration **result
) {
    const MarimayCheckFunctionDeclaration *base = NULL;
    MarimayCheckFunctionDeclaration instance;
    size_t index = 0;
    bool accepted = false;
    memset(&instance, 0, sizeof(instance));
    if (checker == NULL || arguments == NULL || result == NULL
        || base_index >= checker->declarations.length) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    base = mmay_vector_at_const(&checker->declarations, base_index);
    if (base == NULL || !base->signature_valid || !base->is_generic
        || arguments->length != base->type_parameters.length) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < checker->declarations.length; index += 1) {
        const MarimayCheckFunctionDeclaration *existing = mmay_vector_at_const(
            &checker->declarations, index
        );
        size_t argument_index = 0;
        bool same = existing != NULL && existing->is_instance
            && existing->generic_base_index == base_index
            && existing->generic_arguments.length == arguments->length;
        for (argument_index = 0; same && argument_index < arguments->length;
            argument_index += 1) {
            const MarimayTypeId *left = mmay_vector_at_const(
                &existing->generic_arguments, argument_index
            );
            const MarimayTypeId *right = mmay_vector_at_const(arguments, argument_index);
            same = left != NULL && right != NULL && *left == *right;
        }
        if (same) {
            *result = mmay_vector_at(&checker->declarations, index);
            return true;
        }
    }
    if (checker->declarations.length >= checker->options.max_functions) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
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
    if (!mmay_owned_bytes_copy(
            &instance.owned_name,
            checker->result->allocator,
            base->owned_name.data,
            base->owned_name.length
        )) {
        mmay_checker_private_function_declaration_dispose(&instance);
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    instance.name_initialized = true;
    if (!mmay_vector_init(
            &instance.parameter_names,
            checker->result->allocator,
            sizeof(MarimayCheckName)
        )) {
        mmay_checker_private_function_declaration_dispose(&instance);
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    instance.parameter_names_initialized = true;
    if (!mmay_vector_init(
            &instance.parameter_types,
            checker->result->allocator,
            sizeof(MarimayTypeId)
        )) {
        mmay_checker_private_function_declaration_dispose(&instance);
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    instance.parameter_types_initialized = true;
    if (!mmay_vector_init(
            &instance.generic_arguments,
            checker->result->allocator,
            sizeof(MarimayTypeId)
        )) {
        mmay_checker_private_function_declaration_dispose(&instance);
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    instance.generic_arguments_initialized = true;
    if (!mmay_checker_private_copy_type_parameters(
            checker, &base->type_parameters, &instance.type_parameters
        )) {
        mmay_checker_private_function_declaration_dispose(&instance);
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    instance.type_parameters_initialized = true;
    for (index = 0; index < arguments->length; index += 1) {
        const MarimayTypeId *argument = mmay_vector_at_const(arguments, index);
        if (argument == NULL || !mmay_vector_push(&instance.generic_arguments, argument)) {
            mmay_checker_private_function_declaration_dispose(&instance);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    for (index = 0; index < base->parameter_names.length; index += 1) {
        const MarimayCheckName *name = mmay_vector_at_const(&base->parameter_names, index);
        const MarimayTypeId *type = mmay_vector_at_const(&base->parameter_types, index);
        MarimayTypeId substituted = MMAY_TYPE_INVALID;
        if (name == NULL || type == NULL
            || !mmay_vector_push(&instance.parameter_names, name)
            || !mmay_checker_private_substitute_type(
                checker,
                *type,
                &base->type_parameters,
                arguments,
                &substituted
            ) || !mmay_vector_push(&instance.parameter_types, &substituted)) {
            mmay_checker_private_function_declaration_dispose(&instance);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    if (!mmay_checker_private_substitute_type(
            checker,
            base->result_type,
            &base->type_parameters,
            arguments,
            &instance.result_type
        ) || !mmay_type_intern_function(
            &checker->result->types,
            instance.parameter_types.data,
            instance.parameter_types.length,
            instance.result_type,
            &instance.signature_type
        ) || !mmay_checker_private_derive_instance_symbol(
            checker, base, arguments, &instance.symbol
    )) {
        mmay_checker_private_function_declaration_dispose(&instance);
        if (checker->failure == MMAY_CHECK_OK) {
            mmay_checker_private_fail_type_intern(checker);
        }
        return false;
    }
    if (!mmay_vector_push(&checker->declarations, &instance)) {
        mmay_checker_private_function_declaration_dispose(&instance);
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    memset(&instance, 0, sizeof(instance));
    *result = mmay_vector_at(&checker->declarations, checker->declarations.length - 1);
    accepted = *result != NULL;
    if (!accepted) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
    }
    return accepted;
}
