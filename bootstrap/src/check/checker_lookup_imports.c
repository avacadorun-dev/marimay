#include "package_imports_api.h"

#include <string.h>

#include "checker_core_api.h"
#include "checker_generic_types_api.h"
#include "checker_package_context.h"
#include "ruda/generated/diagnostic_codes.h"

static bool ruda_import_name(
    RudaCheckName name,
    const unsigned char **bytes,
    size_t *length
) {
    return ruda_checker_private_check_name_bytes(name, bytes, length);
}

static bool ruda_import_equal(
    const unsigned char *left,
    size_t left_length,
    const unsigned char *right,
    size_t right_length
) {
    return left_length == right_length
        && (left_length == 0 || memcmp(left, right, left_length) == 0);
}

static const RudaCheckImportBinding *ruda_import_binding(
    const RudaChecker *checker,
    const unsigned char *name,
    size_t name_length,
    bool namespace_only
) {
    size_t index = 0;
    if (checker == NULL || checker->imports == NULL || checker->ast == NULL) {
        return NULL;
    }
    for (index = 0; index < checker->imports->binding_count; index += 1) {
        const RudaCheckImportBinding *binding = &checker->imports->bindings[index];
        if (binding->ast == checker->ast
            && (!namespace_only || binding->is_namespace)
            && ruda_import_equal(
                binding->local,
                binding->local_length,
                name,
                name_length
            )) {
            return binding;
        }
    }
    return NULL;
}

static const RudaCheckPackageSurface *ruda_import_surface(
    const RudaChecker *checker,
    const RudaCheckImportBinding *binding
) {
    if (checker == NULL || checker->imports == NULL || binding == NULL
        || binding->surface_index >= checker->imports->surface_count) {
        return NULL;
    }
    return checker->imports->surfaces[binding->surface_index];
}

bool ruda_check_import_binding_exists(
    const RudaChecker *checker,
    const unsigned char *name,
    size_t name_length
) {
    return ruda_import_binding(checker, name, name_length, false) != NULL;
}

bool ruda_check_import_namespace_exists(
    const RudaChecker *checker,
    const unsigned char *name,
    size_t name_length
) {
    return ruda_import_binding(checker, name, name_length, true) != NULL;
}

bool ruda_check_import_namespace_owns_enum(
    const RudaChecker *checker,
    RudaCheckName namespace_name,
    RudaTypeId type
) {
    const unsigned char *namespace_bytes = NULL;
    size_t namespace_length = 0;
    const RudaCheckImportBinding *binding = NULL;
    const RudaCheckPackageSurface *surface = NULL;
    size_t index = 0;
    if (checker == NULL || type == RUDA_TYPE_INVALID
        || !ruda_import_name(
            namespace_name, &namespace_bytes, &namespace_length
        )) {
        return false;
    }
    binding = ruda_import_binding(
        checker, namespace_bytes, namespace_length, true
    );
    surface = ruda_import_surface(checker, binding);
    if (surface == NULL) {
        return false;
    }
    for (index = 0; index < surface->enum_count; index += 1) {
        const RudaCheckedEnum *enum_value = ruda_checked_enum(
            checker->result, surface->enum_start + index
        );
        if (enum_value != NULL && enum_value->is_public
            && ruda_checker_private_type_matches_nominal(
                checker,
                type,
                enum_value->symbol,
                enum_value->type_parameters.length
            )) {
            return true;
        }
    }
    for (index = 0; index < surface->type_alias_count; index += 1) {
        const RudaCheckedTypeAlias *alias = ruda_checked_type_alias(
            checker->result, surface->type_alias_start + index
        );
        if (alias != NULL && alias->is_public
            && alias->target_type == type) {
            return true;
        }
    }
    return false;
}

static bool ruda_import_surface_type(
    const RudaChecker *checker,
    const RudaCheckPackageSurface *surface,
    const unsigned char *name,
    size_t name_length,
    RudaTypeId *type,
    RudaSymbolId *symbol,
    size_t *generic_arity,
    bool *exists_private
) {
    size_t index = 0;
    *exists_private = false;
    for (index = 0; index < surface->record_count; index += 1) {
        const RudaCheckedRecord *item = ruda_checked_record(
            checker->result, surface->record_start + index
        );
        if (item != NULL && ruda_import_equal(
                item->name.data, item->name.length, name, name_length)) {
            *exists_private = !item->is_public;
            if (item->is_public) {
                *type = item->type;
                *symbol = item->symbol;
                *generic_arity = item->type_parameters.length;
                return true;
            }
            return false;
        }
    }
    for (index = 0; index < surface->enum_count; index += 1) {
        const RudaCheckedEnum *item = ruda_checked_enum(
            checker->result, surface->enum_start + index
        );
        if (item != NULL && ruda_import_equal(
                item->name.data, item->name.length, name, name_length)) {
            *exists_private = !item->is_public;
            if (item->is_public) {
                *type = item->type;
                *symbol = item->symbol;
                *generic_arity = item->type_parameters.length;
                return true;
            }
            return false;
        }
    }
    for (index = 0; index < surface->interface_count; index += 1) {
        const RudaCheckedInterface *item = ruda_checked_interface(
            checker->result, surface->interface_start + index
        );
        if (item != NULL && ruda_import_equal(
                item->name.data, item->name.length, name, name_length)) {
            *exists_private = !item->is_public;
            if (item->is_public) {
                *type = item->type;
                *symbol = item->symbol;
                *generic_arity = 0;
                return true;
            }
            return false;
        }
    }
    for (index = 0; index < surface->type_alias_count; index += 1) {
        const RudaCheckedTypeAlias *item = ruda_checked_type_alias(
            checker->result, surface->type_alias_start + index
        );
        if (item != NULL && ruda_import_equal(
                item->name.data, item->name.length, name, name_length)) {
            *exists_private = !item->is_public;
            if (item->is_public) {
                *type = item->target_type;
                *symbol = item->symbol;
                *generic_arity = 0;
                return true;
            }
            return false;
        }
    }
    return false;
}

static bool ruda_import_specialize_type(
    RudaChecker *checker,
    const RudaAstNode *type_node,
    RudaSymbolId symbol,
    size_t generic_arity,
    RudaTypeId *type
) {
    const RudaAstNode *type_arguments =
        ruda_checker_private_check_child_node(
            type_node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0
        );
    RudaVector arguments;
    bool initialized = false;
    memset(&arguments, 0, sizeof(arguments));
    if (type_arguments == NULL) {
        if (generic_arity == 0) return true;
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "generic record or enum type requires explicit type arguments",
            type_node->start,
            type_node->end
        );
        *type = RUDA_TYPE_INVALID;
        return !ruda_checker_has_failed(checker);
    }
    *type = RUDA_TYPE_INVALID;
    if (!ruda_vector_init(
            &arguments, checker->result->allocator, sizeof(RudaTypeId)
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    initialized = true;
    if (!ruda_checker_private_parse_type_arguments(
            checker, type_arguments, &arguments
        )) goto cleanup;
    if (generic_arity == 0 || arguments.length != generic_arity) {
        (void)ruda_checker_add_diagnostic(
            checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            generic_arity == 0
                ? "type does not accept generic arguments"
                : "generic type argument count does not match its declaration",
            type_node->start,
            type_node->end
        );
        *type = RUDA_TYPE_INVALID;
        goto cleanup;
    }
    if (!ruda_checker_private_intern_generic_nominal(
            checker,
            RUDA_TYPE_KIND_NOMINAL,
            symbol,
            &arguments,
            type
        )) goto cleanup;
cleanup:
    if (initialized) ruda_vector_dispose(&arguments);
    return !ruda_checker_has_failed(checker) && *type != RUDA_TYPE_INVALID;
}

bool ruda_check_import_type(
    RudaChecker *checker,
    const RudaAstNode *type_node,
    RudaTypeId *type,
    bool *resolved
) {
    const RudaSyntaxToken *first = NULL;
    const RudaSyntaxToken *second = NULL;
    const unsigned char *first_bytes = NULL;
    const unsigned char *second_bytes = NULL;
    size_t first_length = 0;
    size_t second_length = 0;
    const RudaCheckImportBinding *binding = NULL;
    const RudaCheckPackageSurface *surface = NULL;
    RudaSymbolId symbol;
    size_t generic_arity = 0;
    bool exists_private = false;
    if (checker == NULL || type_node == NULL || type == NULL
        || resolved == NULL || checker->ast == NULL) {
        return false;
    }
    *resolved = false;
    memset(&symbol, 0, sizeof(symbol));
    first = ruda_checker_private_check_child_token(
        type_node, RUDA_TOKEN_IDENTIFIER, 0
    );
    second = ruda_checker_private_check_child_token(
        type_node, RUDA_TOKEN_IDENTIFIER, 1
    );
    if (first == NULL || first->synthetic) {
        return true;
    }
    first_bytes = checker->source + first->start;
    first_length = first->end - first->start;
    if (second == NULL) {
        binding = ruda_import_binding(
            checker, first_bytes, first_length, false
        );
        if (binding == NULL || binding->is_namespace) {
            return true;
        }
        surface = ruda_import_surface(checker, binding);
        if (surface != NULL && ruda_import_surface_type(
                checker,
                surface,
                binding->original,
                binding->original_length,
                type,
                &symbol,
                &generic_arity,
                &exists_private
            )) {
            *resolved = true;
            return ruda_import_specialize_type(
                    checker, type_node, symbol, generic_arity, type
                ) && ruda_check_record_occurrence(
                checker,
                (RudaCheckName){checker->ast->snapshot, first->start, first->end},
                symbol
            );
        }
        return true;
    }
    second_bytes = checker->source + second->start;
    second_length = second->end - second->start;
    binding = ruda_import_binding(checker, first_bytes, first_length, true);
    if (binding == NULL) {
        return true;
    }
    *resolved = true;
    surface = ruda_import_surface(checker, binding);
    if (surface != NULL && ruda_import_surface_type(
            checker,
            surface,
            second_bytes,
            second_length,
            type,
            &symbol,
            &generic_arity,
            &exists_private
        )) {
        return ruda_import_specialize_type(
                checker, type_node, symbol, generic_arity, type
            ) && ruda_check_record_occurrence(
            checker,
            (RudaCheckName){checker->ast->snapshot, second->start, second->end},
            symbol
        );
    }
    *type = RUDA_TYPE_INVALID;
    (void)ruda_checker_add_diagnostic(
        checker,
        exists_private ? RUDA_DIAGNOSTIC_PRIVATE_IMPORT
            : RUDA_DIAGNOSTIC_UNKNOWN_IMPORT,
        exists_private ? "imported type is package-private"
            : "unknown public type in imported package",
        second->start,
        second->end
    );
    return !ruda_checker_has_failed(checker);
}

static bool ruda_import_surface_active(
    const RudaChecker *checker,
    size_t surface_index
) {
    size_t index = 0;
    for (index = 0; checker != NULL && checker->imports != NULL
        && index < checker->imports->binding_count; index += 1) {
        const RudaCheckImportBinding *binding = &checker->imports->bindings[index];
        if (binding->ast == checker->ast
            && binding->surface_index == surface_index) {
            return true;
        }
    }
    return false;
}

bool ruda_check_imported_nominal(
    const RudaChecker *checker,
    RudaTypeId type,
    RudaSymbolId *symbol
) {
    const RudaTypeDescriptor *descriptor = checker == NULL ? NULL
        : ruda_type_descriptor(&checker->result->types, type);
    size_t surface_index = 0;
    if (descriptor == NULL || descriptor->kind != RUDA_TYPE_KIND_NOMINAL
        || symbol == NULL || checker->imports == NULL) {
        return false;
    }
    for (surface_index = 0; surface_index < checker->imports->surface_count;
        surface_index += 1) {
        if (ruda_import_surface_active(checker, surface_index)) {
            const RudaCheckPackageSurface *surface =
                checker->imports->surfaces[surface_index];
            size_t index = 0;
            for (index = 0; index < surface->record_count; index += 1) {
                const RudaCheckedRecord *item = ruda_checked_record(
                    checker->result, surface->record_start + index
                );
                if (item != NULL && item->is_public
                    && ruda_checker_private_type_matches_nominal(
                        checker,
                        type,
                        item->symbol,
                        item->type_parameters.length
                    )) {
                    *symbol = descriptor->symbol;
                    return true;
                }
            }
            for (index = 0; index < surface->enum_count; index += 1) {
                const RudaCheckedEnum *item = ruda_checked_enum(
                    checker->result, surface->enum_start + index
                );
                if (item != NULL && item->is_public
                    && ruda_checker_private_type_matches_nominal(
                        checker,
                        type,
                        item->symbol,
                        item->type_parameters.length
                    )) {
                    *symbol = descriptor->symbol;
                    return true;
                }
            }
        }
    }
    return false;
}

static bool ruda_import_callable_from_checked(
    const RudaCheckedFunction *function,
    RudaCheckCallable *callable
) {
    if (function == NULL || callable == NULL || !function->mir.initialized
        || function->mir.parameter_count > function->mir.register_types.length) {
        return false;
    }
    memset(callable, 0, sizeof(*callable));
    callable->parameter_types = function->mir.register_types.data;
    callable->parameter_count = function->mir.parameter_count;
    callable->symbol = function->mir.symbol;
    callable->receiver_symbol = function->receiver_symbol;
    callable->receiver_type = function->receiver_type;
    callable->result_type = function->mir.result_type;
    callable->dot_eligible = function->dot_eligible;
    callable->is_external = function->is_external;
    return true;
}

static RudaCheckCallResolution ruda_import_resolve_surface_function(
    const RudaChecker *checker,
    const RudaCheckPackageSurface *surface,
    const unsigned char *name,
    size_t name_length,
    bool receiver_known,
    RudaTypeId receiver_type,
    RudaCheckCallable *callable
) {
    const RudaCheckedFunction *only = NULL;
    size_t match_count = 0;
    size_t index = 0;
    bool name_exists = false;
    bool private_name_exists = false;
    for (index = 0; index < surface->function_count; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            checker->result, surface->function_start + index
        );
        if (function == NULL || !ruda_import_equal(
                function->name.data, function->name.length, name, name_length
            )) {
            continue;
        }
        if (!function->is_public) {
            private_name_exists = true;
            continue;
        }
        name_exists = true;
        if (!receiver_known || (function->dot_eligible
                && function->receiver_type == receiver_type)) {
            only = function;
            match_count += 1;
        }
    }
    if (match_count == 1 && ruda_import_callable_from_checked(only, callable)) {
        return RUDA_CHECK_CALL_FOUND;
    }
    if (!receiver_known && match_count > 1) {
        return RUDA_CHECK_CALL_RECEIVER_REQUIRED;
    }
    if (!name_exists && private_name_exists) {
        return RUDA_CHECK_CALL_PRIVATE_IMPORT;
    }
    return name_exists ? RUDA_CHECK_CALL_OTHER_NOMINAL
        : RUDA_CHECK_CALL_UNKNOWN_NAME;
}

RudaCheckCallResolution ruda_check_resolve_imported_named_call(
    const RudaChecker *checker,
    RudaCheckName name,
    RudaTypeId receiver_type,
    RudaCheckCallable *callable
) {
    const unsigned char *name_bytes = NULL;
    size_t name_length = 0;
    const RudaCheckImportBinding *binding = NULL;
    const RudaCheckPackageSurface *surface = NULL;
    if (checker == NULL || callable == NULL
        || !ruda_import_name(name, &name_bytes, &name_length)) {
        return RUDA_CHECK_CALL_INTERNAL_ERROR;
    }
    binding = ruda_import_binding(
        checker,
        name_bytes,
        name_length,
        false
    );
    surface = ruda_import_surface(checker, binding);
    if (binding == NULL || binding->is_namespace || surface == NULL) {
        return RUDA_CHECK_CALL_UNKNOWN_NAME;
    }
    return ruda_import_resolve_surface_function(
        checker,
        surface,
        binding->original,
        binding->original_length,
        true,
        receiver_type,
        callable
    );
}

RudaCheckCallResolution ruda_check_resolve_imported_call(
    const RudaChecker *checker,
    RudaCheckName namespace_name,
    bool has_namespace,
    RudaCheckName name,
    bool receiver_known,
    RudaTypeId receiver_type,
    RudaCheckCallable *callable
) {
    const unsigned char *name_bytes = NULL;
    const unsigned char *namespace_bytes = NULL;
    size_t name_length = 0;
    size_t namespace_length = 0;
    size_t index = 0;
    bool name_seen = false;
    if (checker == NULL || callable == NULL
        || !ruda_import_name(name, &name_bytes, &name_length)
        || (has_namespace && !ruda_import_name(
            namespace_name, &namespace_bytes, &namespace_length))) {
        return RUDA_CHECK_CALL_INTERNAL_ERROR;
    }
    memset(callable, 0, sizeof(*callable));
    if (has_namespace || !receiver_known) {
        const RudaCheckImportBinding *binding = ruda_import_binding(
            checker,
            has_namespace ? namespace_bytes : name_bytes,
            has_namespace ? namespace_length : name_length,
            has_namespace
        );
        const RudaCheckPackageSurface *surface = ruda_import_surface(
            checker, binding
        );
        if (binding == NULL || surface == NULL
            || (!has_namespace && binding->is_namespace)) {
            return RUDA_CHECK_CALL_UNKNOWN_NAME;
        }
        {
            RudaCheckCallResolution resolution =
                ruda_import_resolve_surface_function(
            checker,
            surface,
            has_namespace ? name_bytes : binding->original,
            has_namespace ? name_length : binding->original_length,
            receiver_known,
            receiver_type,
            callable
        );
            if (has_namespace
                && resolution == RUDA_CHECK_CALL_UNKNOWN_NAME) {
                return RUDA_CHECK_CALL_UNKNOWN_IMPORT;
            }
            return resolution;
        }
    }
    for (index = 0; checker->imports != NULL
        && index < checker->imports->surface_count; index += 1) {
        RudaCheckCallResolution resolution;
        if (!ruda_import_surface_active(checker, index)) {
            continue;
        }
        resolution = ruda_import_resolve_surface_function(
            checker,
            checker->imports->surfaces[index],
            name_bytes,
            name_length,
            true,
            receiver_type,
            callable
        );
        if (resolution == RUDA_CHECK_CALL_FOUND) {
            return resolution;
        }
        if (resolution != RUDA_CHECK_CALL_UNKNOWN_NAME
            && resolution != RUDA_CHECK_CALL_PRIVATE_IMPORT) {
            name_seen = true;
        }
    }
    return name_seen ? RUDA_CHECK_CALL_OTHER_NOMINAL
        : RUDA_CHECK_CALL_UNKNOWN_NAME;
}

const RudaCheckedRecord *ruda_check_imported_record_by_type(
    const RudaChecker *checker,
    RudaTypeId type
) {
    size_t surface_index = 0;
    for (surface_index = 0; checker != NULL && checker->imports != NULL
        && surface_index < checker->imports->surface_count; surface_index += 1) {
        const RudaCheckPackageSurface *surface =
            checker->imports->surfaces[surface_index];
        size_t index = 0;
        if (!ruda_import_surface_active(checker, surface_index)) {
            continue;
        }
        for (index = 0; index < surface->record_count; index += 1) {
            const RudaCheckedRecord *record = ruda_checked_record(
                checker->result, surface->record_start + index
            );
            if (record != NULL && record->is_public
                && ruda_checker_private_type_matches_nominal(
                    checker,
                    type,
                    record->symbol,
                    record->type_parameters.length
                )) {
                return record;
            }
        }
    }
    return NULL;
}

const RudaCheckedEnum *ruda_check_imported_enum_by_type(
    const RudaChecker *checker,
    RudaTypeId type
) {
    size_t surface_index = 0;
    for (surface_index = 0; checker != NULL && checker->imports != NULL
        && surface_index < checker->imports->surface_count; surface_index += 1) {
        const RudaCheckPackageSurface *surface =
            checker->imports->surfaces[surface_index];
        size_t index = 0;
        if (!ruda_import_surface_active(checker, surface_index)) {
            continue;
        }
        for (index = 0; index < surface->enum_count; index += 1) {
            const RudaCheckedEnum *enum_value = ruda_checked_enum(
                checker->result, surface->enum_start + index
            );
            if (enum_value != NULL && enum_value->is_public
                && ruda_checker_private_type_matches_nominal(
                    checker,
                    type,
                    enum_value->symbol,
                    enum_value->type_parameters.length
                )) {
                return enum_value;
            }
        }
        for (index = 0; index < surface->type_alias_count; index += 1) {
            const RudaCheckedTypeAlias *alias = ruda_checked_type_alias(
                checker->result, surface->type_alias_start + index
            );
            size_t enum_index = 0;
            if (alias == NULL || !alias->is_public
                || alias->target_type != type) {
                continue;
            }
            for (enum_index = 0;
                enum_index < checker->result->enums.length;
                enum_index += 1) {
                const RudaCheckedEnum *enum_value = ruda_checked_enum(
                    checker->result, enum_index
                );
                if (enum_value != NULL
                    && ruda_checker_private_type_matches_nominal(
                        checker,
                        type,
                        enum_value->symbol,
                        enum_value->type_parameters.length
                    )) {
                    return enum_value;
                }
            }
        }
    }
    return NULL;
}

const RudaCheckedEnum *ruda_check_imported_enum_by_name(
    const RudaChecker *checker,
    RudaCheckName namespace_name,
    bool has_namespace,
    RudaCheckName enum_name,
    bool *resolved,
    bool *exists_private
) {
    const unsigned char *namespace_bytes = NULL;
    const unsigned char *enum_bytes = NULL;
    size_t namespace_length = 0;
    size_t enum_length = 0;
    const RudaCheckImportBinding *binding = NULL;
    const RudaCheckPackageSurface *surface = NULL;
    size_t index = 0;
    if (checker == NULL || resolved == NULL || exists_private == NULL
        || !ruda_import_name(enum_name, &enum_bytes, &enum_length)
        || (has_namespace && !ruda_import_name(
            namespace_name, &namespace_bytes, &namespace_length
        ))) {
        return NULL;
    }
    *resolved = false;
    *exists_private = false;
    binding = ruda_import_binding(
        checker,
        has_namespace ? namespace_bytes : enum_bytes,
        has_namespace ? namespace_length : enum_length,
        has_namespace
    );
    if (binding == NULL || (!has_namespace && binding->is_namespace)) {
        return NULL;
    }
    *resolved = true;
    surface = ruda_import_surface(checker, binding);
    if (surface == NULL) {
        return NULL;
    }
    if (!has_namespace) {
        enum_bytes = binding->original;
        enum_length = binding->original_length;
    }
    for (index = 0; index < surface->enum_count; index += 1) {
        const RudaCheckedEnum *enum_value = ruda_checked_enum(
            checker->result, surface->enum_start + index
        );
        if (enum_value != NULL && ruda_import_equal(
                enum_value->name.data,
                enum_value->name.length,
                enum_bytes,
                enum_length
            )) {
            *exists_private = !enum_value->is_public;
            return enum_value->is_public ? enum_value : NULL;
        }
    }
    for (index = 0; index < surface->type_alias_count; index += 1) {
        const RudaCheckedTypeAlias *alias = ruda_checked_type_alias(
            checker->result, surface->type_alias_start + index
        );
        size_t enum_index = 0;
        if (alias == NULL || !ruda_import_equal(
                alias->name.data,
                alias->name.length,
                enum_bytes,
                enum_length
            )) {
            continue;
        }
        *exists_private = !alias->is_public;
        if (!alias->is_public) {
            return NULL;
        }
        for (enum_index = 0;
            enum_index < checker->result->enums.length;
            enum_index += 1) {
            const RudaCheckedEnum *enum_value = ruda_checked_enum(
                checker->result, enum_index
            );
            if (enum_value != NULL
                && ruda_checker_private_type_matches_nominal(
                    checker,
                    alias->target_type,
                    enum_value->symbol,
                    enum_value->type_parameters.length
                )) {
                return enum_value;
            }
        }
        return NULL;
    }
    return NULL;
}
