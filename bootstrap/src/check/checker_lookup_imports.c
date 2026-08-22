#include "package_imports_api.h"

#include <string.h>

#include "checker_core_api.h"
#include "checker_generic_types_api.h"
#include "checker_package_context.h"
#include "mmay/generated/diagnostic_codes.h"

static bool mmay_import_name(
    MarimayCheckName name,
    const unsigned char **bytes,
    size_t *length
) {
    return mmay_checker_private_check_name_bytes(name, bytes, length);
}

static bool mmay_import_equal(
    const unsigned char *left,
    size_t left_length,
    const unsigned char *right,
    size_t right_length
) {
    return left_length == right_length
        && (left_length == 0 || memcmp(left, right, left_length) == 0);
}

static const MarimayCheckImportBinding *mmay_import_binding(
    const MarimayChecker *checker,
    const unsigned char *name,
    size_t name_length,
    bool namespace_only
) {
    size_t index = 0;
    if (checker == NULL || checker->imports == NULL || checker->ast == NULL) {
        return NULL;
    }
    for (index = 0; index < checker->imports->binding_count; index += 1) {
        const MarimayCheckImportBinding *binding = &checker->imports->bindings[index];
        if (binding->ast == checker->ast
            && (!namespace_only || binding->is_namespace)
            && mmay_import_equal(
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

static const MarimayCheckPackageSurface *mmay_import_surface(
    const MarimayChecker *checker,
    const MarimayCheckImportBinding *binding
) {
    if (checker == NULL || checker->imports == NULL || binding == NULL
        || binding->surface_index >= checker->imports->surface_count) {
        return NULL;
    }
    return checker->imports->surfaces[binding->surface_index];
}

bool mmay_check_import_binding_exists(
    const MarimayChecker *checker,
    const unsigned char *name,
    size_t name_length
) {
    return mmay_import_binding(checker, name, name_length, false) != NULL;
}

bool mmay_check_import_namespace_exists(
    const MarimayChecker *checker,
    const unsigned char *name,
    size_t name_length
) {
    return mmay_import_binding(checker, name, name_length, true) != NULL;
}

bool mmay_check_import_namespace_owns_enum(
    const MarimayChecker *checker,
    MarimayCheckName namespace_name,
    MarimayTypeId type
) {
    const unsigned char *namespace_bytes = NULL;
    size_t namespace_length = 0;
    const MarimayCheckImportBinding *binding = NULL;
    const MarimayCheckPackageSurface *surface = NULL;
    size_t index = 0;
    if (checker == NULL || type == MMAY_TYPE_INVALID
        || !mmay_import_name(
            namespace_name, &namespace_bytes, &namespace_length
        )) {
        return false;
    }
    binding = mmay_import_binding(
        checker, namespace_bytes, namespace_length, true
    );
    surface = mmay_import_surface(checker, binding);
    if (surface == NULL) {
        return false;
    }
    for (index = 0; index < surface->enum_count; index += 1) {
        const MarimayCheckedEnum *enum_value = mmay_checked_enum(
            checker->result, surface->enum_start + index
        );
        if (enum_value != NULL && enum_value->is_public
            && mmay_checker_private_type_matches_nominal(
                checker,
                type,
                enum_value->symbol,
                enum_value->type_parameters.length
            )) {
            return true;
        }
    }
    for (index = 0; index < surface->type_alias_count; index += 1) {
        const MarimayCheckedTypeAlias *alias = mmay_checked_type_alias(
            checker->result, surface->type_alias_start + index
        );
        if (alias != NULL && alias->is_public
            && alias->target_type == type) {
            return true;
        }
    }
    return false;
}

static bool mmay_import_surface_type(
    const MarimayChecker *checker,
    const MarimayCheckPackageSurface *surface,
    const unsigned char *name,
    size_t name_length,
    MarimayTypeId *type,
    MarimaySymbolId *symbol,
    size_t *generic_arity,
    bool *exists_private
) {
    size_t index = 0;
    *exists_private = false;
    for (index = 0; index < surface->record_count; index += 1) {
        const MarimayCheckedRecord *item = mmay_checked_record(
            checker->result, surface->record_start + index
        );
        if (item != NULL && mmay_import_equal(
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
        const MarimayCheckedEnum *item = mmay_checked_enum(
            checker->result, surface->enum_start + index
        );
        if (item != NULL && mmay_import_equal(
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
        const MarimayCheckedInterface *item = mmay_checked_interface(
            checker->result, surface->interface_start + index
        );
        if (item != NULL && mmay_import_equal(
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
        const MarimayCheckedTypeAlias *item = mmay_checked_type_alias(
            checker->result, surface->type_alias_start + index
        );
        if (item != NULL && mmay_import_equal(
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

static bool mmay_import_specialize_type(
    MarimayChecker *checker,
    const MarimayAstNode *type_node,
    MarimaySymbolId symbol,
    size_t generic_arity,
    MarimayTypeId *type
) {
    const MarimayAstNode *type_arguments =
        mmay_checker_private_check_child_node(
            type_node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
        );
    MarimayVector arguments;
    bool initialized = false;
    memset(&arguments, 0, sizeof(arguments));
    if (type_arguments == NULL) {
        if (generic_arity == 0) return true;
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "generic record or enum type requires explicit type arguments",
            type_node->start,
            type_node->end
        );
        *type = MMAY_TYPE_INVALID;
        return !mmay_checker_has_failed(checker);
    }
    *type = MMAY_TYPE_INVALID;
    if (!mmay_vector_init(
            &arguments, checker->result->allocator, sizeof(MarimayTypeId)
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    initialized = true;
    if (!mmay_checker_private_parse_type_arguments(
            checker, type_arguments, &arguments
        )) goto cleanup;
    if (generic_arity == 0 || arguments.length != generic_arity) {
        (void)mmay_checker_add_diagnostic(
            checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            generic_arity == 0
                ? "type does not accept generic arguments"
                : "generic type argument count does not match its declaration",
            type_node->start,
            type_node->end
        );
        *type = MMAY_TYPE_INVALID;
        goto cleanup;
    }
    if (!mmay_checker_private_intern_generic_nominal(
            checker,
            MMAY_TYPE_KIND_NOMINAL,
            symbol,
            &arguments,
            type
        )) goto cleanup;
cleanup:
    if (initialized) mmay_vector_dispose(&arguments);
    return !mmay_checker_has_failed(checker) && *type != MMAY_TYPE_INVALID;
}

bool mmay_check_import_type(
    MarimayChecker *checker,
    const MarimayAstNode *type_node,
    MarimayTypeId *type,
    bool *resolved
) {
    const MarimaySyntaxToken *first = NULL;
    const MarimaySyntaxToken *second = NULL;
    const unsigned char *first_bytes = NULL;
    const unsigned char *second_bytes = NULL;
    size_t first_length = 0;
    size_t second_length = 0;
    const MarimayCheckImportBinding *binding = NULL;
    const MarimayCheckPackageSurface *surface = NULL;
    MarimaySymbolId symbol;
    size_t generic_arity = 0;
    bool exists_private = false;
    if (checker == NULL || type_node == NULL || type == NULL
        || resolved == NULL || checker->ast == NULL) {
        return false;
    }
    *resolved = false;
    memset(&symbol, 0, sizeof(symbol));
    first = mmay_checker_private_check_child_token(
        type_node, MMAY_TOKEN_IDENTIFIER, 0
    );
    second = mmay_checker_private_check_child_token(
        type_node, MMAY_TOKEN_IDENTIFIER, 1
    );
    if (first == NULL || first->synthetic) {
        return true;
    }
    first_bytes = checker->source + first->start;
    first_length = first->end - first->start;
    if (second == NULL) {
        binding = mmay_import_binding(
            checker, first_bytes, first_length, false
        );
        if (binding == NULL || binding->is_namespace) {
            return true;
        }
        surface = mmay_import_surface(checker, binding);
        if (surface != NULL && mmay_import_surface_type(
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
            return mmay_import_specialize_type(
                    checker, type_node, symbol, generic_arity, type
                ) && mmay_check_record_occurrence(
                checker,
                (MarimayCheckName){checker->ast->snapshot, first->start, first->end},
                symbol
            );
        }
        return true;
    }
    second_bytes = checker->source + second->start;
    second_length = second->end - second->start;
    binding = mmay_import_binding(checker, first_bytes, first_length, true);
    if (binding == NULL) {
        return true;
    }
    *resolved = true;
    surface = mmay_import_surface(checker, binding);
    if (surface != NULL && mmay_import_surface_type(
            checker,
            surface,
            second_bytes,
            second_length,
            type,
            &symbol,
            &generic_arity,
            &exists_private
        )) {
        return mmay_import_specialize_type(
                checker, type_node, symbol, generic_arity, type
            ) && mmay_check_record_occurrence(
            checker,
            (MarimayCheckName){checker->ast->snapshot, second->start, second->end},
            symbol
        );
    }
    *type = MMAY_TYPE_INVALID;
    (void)mmay_checker_add_diagnostic(
        checker,
        exists_private ? MMAY_DIAGNOSTIC_PRIVATE_IMPORT
            : MMAY_DIAGNOSTIC_UNKNOWN_IMPORT,
        exists_private ? "imported type is package-private"
            : "unknown public type in imported package",
        second->start,
        second->end
    );
    return !mmay_checker_has_failed(checker);
}

static bool mmay_import_surface_active(
    const MarimayChecker *checker,
    size_t surface_index
) {
    size_t index = 0;
    for (index = 0; checker != NULL && checker->imports != NULL
        && index < checker->imports->binding_count; index += 1) {
        const MarimayCheckImportBinding *binding = &checker->imports->bindings[index];
        if (binding->ast == checker->ast
            && binding->surface_index == surface_index) {
            return true;
        }
    }
    return false;
}

bool mmay_check_imported_nominal(
    const MarimayChecker *checker,
    MarimayTypeId type,
    MarimaySymbolId *symbol
) {
    const MarimayTypeDescriptor *descriptor = checker == NULL ? NULL
        : mmay_type_descriptor(&checker->result->types, type);
    size_t surface_index = 0;
    if (descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_NOMINAL
        || symbol == NULL || checker->imports == NULL) {
        return false;
    }
    for (surface_index = 0; surface_index < checker->imports->surface_count;
        surface_index += 1) {
        if (mmay_import_surface_active(checker, surface_index)) {
            const MarimayCheckPackageSurface *surface =
                checker->imports->surfaces[surface_index];
            size_t index = 0;
            for (index = 0; index < surface->record_count; index += 1) {
                const MarimayCheckedRecord *item = mmay_checked_record(
                    checker->result, surface->record_start + index
                );
                if (item != NULL && item->is_public
                    && mmay_checker_private_type_matches_nominal(
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
                const MarimayCheckedEnum *item = mmay_checked_enum(
                    checker->result, surface->enum_start + index
                );
                if (item != NULL && item->is_public
                    && mmay_checker_private_type_matches_nominal(
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

static bool mmay_import_callable_from_checked(
    const MarimayCheckedFunction *function,
    MarimayCheckCallable *callable
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

static MarimayCheckCallResolution mmay_import_resolve_surface_function(
    const MarimayChecker *checker,
    const MarimayCheckPackageSurface *surface,
    const unsigned char *name,
    size_t name_length,
    bool receiver_known,
    MarimayTypeId receiver_type,
    MarimayCheckCallable *callable
) {
    const MarimayCheckedFunction *only = NULL;
    size_t match_count = 0;
    size_t index = 0;
    bool name_exists = false;
    bool private_name_exists = false;
    for (index = 0; index < surface->function_count; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checker->result, surface->function_start + index
        );
        if (function == NULL || !mmay_import_equal(
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
    if (match_count == 1 && mmay_import_callable_from_checked(only, callable)) {
        return MMAY_CHECK_CALL_FOUND;
    }
    if (!receiver_known && match_count > 1) {
        return MMAY_CHECK_CALL_RECEIVER_REQUIRED;
    }
    if (!name_exists && private_name_exists) {
        return MMAY_CHECK_CALL_PRIVATE_IMPORT;
    }
    return name_exists ? MMAY_CHECK_CALL_OTHER_NOMINAL
        : MMAY_CHECK_CALL_UNKNOWN_NAME;
}

MarimayCheckCallResolution mmay_check_resolve_imported_named_call(
    const MarimayChecker *checker,
    MarimayCheckName name,
    MarimayTypeId receiver_type,
    MarimayCheckCallable *callable
) {
    const unsigned char *name_bytes = NULL;
    size_t name_length = 0;
    const MarimayCheckImportBinding *binding = NULL;
    const MarimayCheckPackageSurface *surface = NULL;
    if (checker == NULL || callable == NULL
        || !mmay_import_name(name, &name_bytes, &name_length)) {
        return MMAY_CHECK_CALL_INTERNAL_ERROR;
    }
    binding = mmay_import_binding(
        checker,
        name_bytes,
        name_length,
        false
    );
    surface = mmay_import_surface(checker, binding);
    if (binding == NULL || binding->is_namespace || surface == NULL) {
        return MMAY_CHECK_CALL_UNKNOWN_NAME;
    }
    return mmay_import_resolve_surface_function(
        checker,
        surface,
        binding->original,
        binding->original_length,
        true,
        receiver_type,
        callable
    );
}

MarimayCheckCallResolution mmay_check_resolve_imported_call(
    const MarimayChecker *checker,
    MarimayCheckName namespace_name,
    bool has_namespace,
    MarimayCheckName name,
    bool receiver_known,
    MarimayTypeId receiver_type,
    MarimayCheckCallable *callable
) {
    const unsigned char *name_bytes = NULL;
    const unsigned char *namespace_bytes = NULL;
    size_t name_length = 0;
    size_t namespace_length = 0;
    size_t index = 0;
    bool name_seen = false;
    bool private_name_seen = false;
    if (checker == NULL || callable == NULL
        || !mmay_import_name(name, &name_bytes, &name_length)
        || (has_namespace && !mmay_import_name(
            namespace_name, &namespace_bytes, &namespace_length))) {
        return MMAY_CHECK_CALL_INTERNAL_ERROR;
    }
    memset(callable, 0, sizeof(*callable));
    if (has_namespace || !receiver_known) {
        const MarimayCheckImportBinding *binding = mmay_import_binding(
            checker,
            has_namespace ? namespace_bytes : name_bytes,
            has_namespace ? namespace_length : name_length,
            has_namespace
        );
        const MarimayCheckPackageSurface *surface = mmay_import_surface(
            checker, binding
        );
        if (binding == NULL || surface == NULL
            || (!has_namespace && binding->is_namespace)) {
            return MMAY_CHECK_CALL_UNKNOWN_NAME;
        }
        {
            MarimayCheckCallResolution resolution =
                mmay_import_resolve_surface_function(
            checker,
            surface,
            has_namespace ? name_bytes : binding->original,
            has_namespace ? name_length : binding->original_length,
            receiver_known,
            receiver_type,
            callable
        );
            if (has_namespace
                && resolution == MMAY_CHECK_CALL_UNKNOWN_NAME) {
                return MMAY_CHECK_CALL_UNKNOWN_IMPORT;
            }
            return resolution;
        }
    }
    for (index = 0; checker->imports != NULL
        && index < checker->imports->surface_count; index += 1) {
        MarimayCheckCallResolution resolution;
        if (!mmay_import_surface_active(checker, index)) {
            continue;
        }
        resolution = mmay_import_resolve_surface_function(
            checker,
            checker->imports->surfaces[index],
            name_bytes,
            name_length,
            true,
            receiver_type,
            callable
        );
        if (resolution == MMAY_CHECK_CALL_FOUND) {
            return resolution;
        }
        if (resolution == MMAY_CHECK_CALL_PRIVATE_IMPORT) {
            private_name_seen = true;
        } else if (resolution != MMAY_CHECK_CALL_UNKNOWN_NAME) {
            name_seen = true;
        }
    }
    if (name_seen) {
        return MMAY_CHECK_CALL_OTHER_NOMINAL;
    }
    return private_name_seen ? MMAY_CHECK_CALL_PRIVATE_IMPORT
        : MMAY_CHECK_CALL_UNKNOWN_NAME;
}

const MarimayCheckedRecord *mmay_check_imported_record_by_type(
    const MarimayChecker *checker,
    MarimayTypeId type
) {
    size_t surface_index = 0;
    for (surface_index = 0; checker != NULL && checker->imports != NULL
        && surface_index < checker->imports->surface_count; surface_index += 1) {
        const MarimayCheckPackageSurface *surface =
            checker->imports->surfaces[surface_index];
        size_t index = 0;
        if (!mmay_import_surface_active(checker, surface_index)) {
            continue;
        }
        for (index = 0; index < surface->record_count; index += 1) {
            const MarimayCheckedRecord *record = mmay_checked_record(
                checker->result, surface->record_start + index
            );
            if (record != NULL && record->is_public
                && mmay_checker_private_type_matches_nominal(
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

const MarimayCheckedEnum *mmay_check_imported_enum_by_type(
    const MarimayChecker *checker,
    MarimayTypeId type
) {
    size_t surface_index = 0;
    for (surface_index = 0; checker != NULL && checker->imports != NULL
        && surface_index < checker->imports->surface_count; surface_index += 1) {
        const MarimayCheckPackageSurface *surface =
            checker->imports->surfaces[surface_index];
        size_t index = 0;
        if (!mmay_import_surface_active(checker, surface_index)) {
            continue;
        }
        for (index = 0; index < surface->enum_count; index += 1) {
            const MarimayCheckedEnum *enum_value = mmay_checked_enum(
                checker->result, surface->enum_start + index
            );
            if (enum_value != NULL && enum_value->is_public
                && mmay_checker_private_type_matches_nominal(
                    checker,
                    type,
                    enum_value->symbol,
                    enum_value->type_parameters.length
                )) {
                return enum_value;
            }
        }
        for (index = 0; index < surface->type_alias_count; index += 1) {
            const MarimayCheckedTypeAlias *alias = mmay_checked_type_alias(
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
                const MarimayCheckedEnum *enum_value = mmay_checked_enum(
                    checker->result, enum_index
                );
                if (enum_value != NULL
                    && mmay_checker_private_type_matches_nominal(
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

const MarimayCheckedEnum *mmay_check_imported_enum_by_name(
    const MarimayChecker *checker,
    MarimayCheckName namespace_name,
    bool has_namespace,
    MarimayCheckName enum_name,
    bool *resolved,
    bool *exists_private
) {
    const unsigned char *namespace_bytes = NULL;
    const unsigned char *enum_bytes = NULL;
    size_t namespace_length = 0;
    size_t enum_length = 0;
    const MarimayCheckImportBinding *binding = NULL;
    const MarimayCheckPackageSurface *surface = NULL;
    size_t index = 0;
    if (checker == NULL || resolved == NULL || exists_private == NULL
        || !mmay_import_name(enum_name, &enum_bytes, &enum_length)
        || (has_namespace && !mmay_import_name(
            namespace_name, &namespace_bytes, &namespace_length
        ))) {
        return NULL;
    }
    *resolved = false;
    *exists_private = false;
    binding = mmay_import_binding(
        checker,
        has_namespace ? namespace_bytes : enum_bytes,
        has_namespace ? namespace_length : enum_length,
        has_namespace
    );
    if (binding == NULL || (!has_namespace && binding->is_namespace)) {
        return NULL;
    }
    *resolved = true;
    surface = mmay_import_surface(checker, binding);
    if (surface == NULL) {
        return NULL;
    }
    if (!has_namespace) {
        enum_bytes = binding->original;
        enum_length = binding->original_length;
    }
    for (index = 0; index < surface->enum_count; index += 1) {
        const MarimayCheckedEnum *enum_value = mmay_checked_enum(
            checker->result, surface->enum_start + index
        );
        if (enum_value != NULL && mmay_import_equal(
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
        const MarimayCheckedTypeAlias *alias = mmay_checked_type_alias(
            checker->result, surface->type_alias_start + index
        );
        size_t enum_index = 0;
        if (alias == NULL || !mmay_import_equal(
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
            const MarimayCheckedEnum *enum_value = mmay_checked_enum(
                checker->result, enum_index
            );
            if (enum_value != NULL
                && mmay_checker_private_type_matches_nominal(
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
