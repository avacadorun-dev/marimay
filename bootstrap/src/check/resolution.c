#include "checker_internal.h"
#include "checker_package_context.h"
#include "package_imports_api.h"

#include <string.h>

#include "mmay/base/hash.h"

typedef struct MarimayCheckCallableKey {
    unsigned char bytes[32];
} MarimayCheckCallableKey;

static bool mmay_check_resolution_name_bytes(
    MarimayCheckName name,
    const unsigned char **bytes,
    size_t *length
) {
    const unsigned char *source = name.snapshot == NULL
        ? NULL
        : mmay_source_bytes(name.snapshot);
    if (bytes == NULL || length == NULL || source == NULL
        || name.start > name.end
        || name.end > mmay_source_length(name.snapshot)) {
        return false;
    }
    *bytes = source + name.start;
    *length = name.end - name.start;
    return true;
}

static bool mmay_check_callable_key(
    const void *name,
    size_t name_length,
    MarimaySymbolId receiver,
    MarimayCheckCallableKey *key
) {
    static const unsigned char domain[] = {
        'r', 'u', 'd', 'a', '-', 'd', 'o', 't', '-', 'c', 'a', 'l', 'l'
    };
    MarimaySha256 hash;
    unsigned char length_bytes[8];
    size_t index = 0;
    if (key == NULL || (name == NULL && name_length != 0)
        || (uintmax_t)name_length > UINT64_MAX) {
        return false;
    }
    for (index = 0; index < sizeof(length_bytes); index += 1) {
        length_bytes[index] = (unsigned char)(
            (uint64_t)name_length >> (index * 8)
        );
    }
    mmay_sha256_init(&hash);
    if (!mmay_sha256_update(&hash, domain, sizeof(domain))
        || !mmay_sha256_update(
            &hash,
            receiver.bytes,
            sizeof(receiver.bytes)
        ) || !mmay_sha256_update(&hash, length_bytes, sizeof(length_bytes))
        || !mmay_sha256_update(&hash, name, name_length)) {
        return false;
    }
    mmay_sha256_final(&hash, key->bytes);
    return true;
}

bool mmay_check_callable_index_init(MarimayChecker *checker) {
    static const uint64_t key0 = UINT64_C(0x727564612d63616c);
    static const uint64_t key1 = UINT64_C(0x6c61626c652d6964);
    return checker != NULL && checker->result != NULL && mmay_map_init(
        &checker->callable_index,
        checker->result->allocator,
        sizeof(size_t),
        key0,
        key1
    );
}

bool mmay_check_owned_receiver(
    const MarimayChecker *checker,
    MarimayTypeId type,
    MarimaySymbolId *receiver
) {
    const MarimayTypeDescriptor *descriptor = NULL;
    const MarimaySymbolId *owned = NULL;
    if (checker == NULL || checker->result == NULL || receiver == NULL) {
        return false;
    }
    descriptor = mmay_type_descriptor(&checker->result->types, type);
    owned = mmay_map_get_const(
        &checker->local_nominal_index,
        &type,
        sizeof(type)
    );
    if (descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_NOMINAL
        || owned == NULL || !mmay_symbol_id_equal(*owned, descriptor->symbol)) {
        return false;
    }
    *receiver = descriptor->symbol;
    return true;
}

static bool mmay_check_declaration_name_bytes(
    const MarimayCheckFunctionDeclaration *declaration,
    const unsigned char **bytes,
    size_t *length
) {
    return declaration != NULL
        && mmay_check_resolution_name_bytes(declaration->name, bytes, length);
}

static MarimayCheckCallResolution mmay_check_unresolved_receiver(
    const MarimayChecker *checker,
    const MarimayCheckTopLevelEntry *entry,
    bool receiver_owned
) {
    const MarimayCheckFunctionDeclaration *first = NULL;
    if (checker == NULL || entry == NULL || entry->function_count == 0) {
        return MMAY_CHECK_CALL_INTERNAL_ERROR;
    }
    first = mmay_vector_at_const(&checker->declarations, entry->index);
    if (first == NULL || !first->signature_valid) {
        return MMAY_CHECK_CALL_INTERNAL_ERROR;
    }
    if (entry->function_count == 1 && !first->dot_eligible) {
        return MMAY_CHECK_CALL_NOT_DOT_ELIGIBLE;
    }
    if (!receiver_owned) {
        return MMAY_CHECK_CALL_RECEIVER_NOT_OWNED;
    }
    return MMAY_CHECK_CALL_OTHER_NOMINAL;
}

bool mmay_check_callable_insert(
    MarimayChecker *checker,
    const MarimayCheckFunctionDeclaration *declaration,
    size_t declaration_index
) {
    const unsigned char *name_bytes = NULL;
    size_t name_length = 0;
    MarimayCheckCallableKey key;
    if (checker == NULL || declaration == NULL
        || !declaration->signature_valid || !declaration->dot_eligible
        || declaration_index == SIZE_MAX
        || !mmay_check_declaration_name_bytes(
            declaration,
            &name_bytes,
            &name_length
        ) || !mmay_check_callable_key(
            name_bytes,
            name_length,
            declaration->receiver_symbol,
            &key
        )) {
        if (checker != NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    if (mmay_map_get_const(
            &checker->callable_index,
            key.bytes,
            sizeof(key.bytes)
        ) != NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (!mmay_map_put(
            &checker->callable_index,
            key.bytes,
            sizeof(key.bytes),
            &declaration_index
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool mmay_check_callable_remove(
    MarimayChecker *checker,
    const MarimayCheckFunctionDeclaration *declaration
) {
    const unsigned char *name_bytes = NULL;
    size_t name_length = 0;
    MarimayCheckCallableKey key;
    return checker != NULL && declaration != NULL
        && declaration->dot_eligible
        && mmay_check_declaration_name_bytes(
            declaration,
            &name_bytes,
            &name_length
        ) && mmay_check_callable_key(
            name_bytes,
            name_length,
            declaration->receiver_symbol,
            &key
        ) && mmay_map_remove(
            &checker->callable_index,
            key.bytes,
            sizeof(key.bytes)
        );
}

static bool mmay_check_callable_from_declaration(
    const MarimayCheckFunctionDeclaration *declaration,
    MarimayCheckCallable *callable
) {
    if (declaration == NULL || callable == NULL
        || !declaration->signature_valid) {
        return false;
    }
    memset(callable, 0, sizeof(*callable));
    callable->parameter_types = declaration->parameter_types.data;
    callable->parameter_count = declaration->parameter_types.length;
    callable->symbol = declaration->symbol;
    callable->receiver_symbol = declaration->receiver_symbol;
    callable->receiver_type = declaration->receiver_type;
    callable->result_type = declaration->result_type;
    callable->dot_eligible = declaration->dot_eligible;
    callable->is_external = declaration->is_external;
    return true;
}

MarimayCheckCallResolution mmay_check_resolve_call(
    const MarimayChecker *checker,
    MarimayCheckName name,
    bool receiver_known,
    MarimayTypeId receiver_type,
    MarimayCheckCallable *callable
) {
    const unsigned char *name_bytes = NULL;
    size_t name_length = 0;
    const MarimayCheckTopLevelEntry *entry = NULL;
    const MarimayCheckFunctionDeclaration *candidate = NULL;
    const unsigned char *candidate_name = NULL;
    size_t candidate_name_length = 0;
    MarimaySymbolId receiver;
    MarimayCheckCallableKey key;
    const size_t *candidate_index = NULL;
    bool receiver_owned = false;
    MarimayCheckCallResolution imported = MMAY_CHECK_CALL_UNKNOWN_NAME;
    MarimayCheckName no_namespace;
    memset(&no_namespace, 0, sizeof(no_namespace));
    if (callable == NULL || checker == NULL) {
        return MMAY_CHECK_CALL_INTERNAL_ERROR;
    }
    memset(callable, 0, sizeof(*callable));
    if (!mmay_check_resolution_name_bytes(name, &name_bytes, &name_length)) {
        return MMAY_CHECK_CALL_INTERNAL_ERROR;
    }
    entry = mmay_map_get_const(
        &checker->top_level_index,
        name_bytes,
        name_length
    );
    if (entry == NULL) {
        return mmay_check_resolve_imported_call(
            checker, no_namespace, false, name, receiver_known,
            receiver_type, callable
        );
    }
    if (entry->kind != MMAY_CHECK_TOP_LEVEL_FUNCTION) {
        return MMAY_CHECK_CALL_NOT_FUNCTION;
    }
    if (entry->function_count == 0) {
        return MMAY_CHECK_CALL_INTERNAL_ERROR;
    }
    if (!receiver_known) {
        if (entry->function_count != 1) {
            return MMAY_CHECK_CALL_RECEIVER_REQUIRED;
        }
        candidate = mmay_vector_at_const(&checker->declarations, entry->index);
    } else {
        receiver_owned = mmay_check_owned_receiver(
            checker,
            receiver_type,
            &receiver
        );
        if (receiver_owned) {
            if (!mmay_check_callable_key(
                    name_bytes,
                    name_length,
                    receiver,
                    &key
                )) {
                return MMAY_CHECK_CALL_INTERNAL_ERROR;
            }
            candidate_index = mmay_map_get_const(
                &checker->callable_index,
                key.bytes,
                sizeof(key.bytes)
            );
            if (candidate_index != NULL) {
                candidate = mmay_vector_at_const(
                    &checker->declarations,
                    *candidate_index
                );
                if (candidate == NULL || !candidate->dot_eligible
                    || !mmay_symbol_id_equal(
                        candidate->receiver_symbol,
                        receiver
                    )) {
                    return MMAY_CHECK_CALL_INTERNAL_ERROR;
                }
            }
        }
        if (candidate == NULL) {
            imported = mmay_check_resolve_imported_call(
                checker, no_namespace, false, name, true,
                receiver_type, callable
            );
            if (imported == MMAY_CHECK_CALL_FOUND) {
                return imported;
            }
            return mmay_check_unresolved_receiver(checker, entry,
                receiver_owned || imported == MMAY_CHECK_CALL_OTHER_NOMINAL);
        }
    }
    if (candidate == NULL || !candidate->signature_valid
        || !mmay_check_declaration_name_bytes(
            candidate,
            &candidate_name,
            &candidate_name_length
        ) || candidate_name_length != name_length
        || memcmp(candidate_name, name_bytes, name_length) != 0) {
        return MMAY_CHECK_CALL_INTERNAL_ERROR;
    }
    return mmay_check_callable_from_declaration(candidate, callable)
        ? MMAY_CHECK_CALL_FOUND : MMAY_CHECK_CALL_INTERNAL_ERROR;
}

MarimayCheckCallResolution mmay_check_resolve_free_call(
    const MarimayChecker *checker,
    MarimayCheckName name,
    MarimayTypeId receiver_type,
    MarimayCheckCallable *callable
) {
    MarimayCheckCallResolution resolution = mmay_check_resolve_imported_named_call(
        checker,
        name,
        receiver_type,
        callable
    );
    if (resolution != MMAY_CHECK_CALL_UNKNOWN_NAME) {
        return resolution;
    }
    return mmay_check_resolve_call(
        checker,
        name,
        true,
        receiver_type,
        callable
    );
}

MarimayCheckCallResolution mmay_check_resolve_qualified_call(
    const MarimayChecker *checker,
    MarimayCheckName namespace_name,
    MarimayCheckName name,
    bool receiver_known,
    MarimayTypeId receiver_type,
    MarimayCheckCallable *callable
) {
    return mmay_check_resolve_imported_call(
        checker, namespace_name, true, name, receiver_known,
        receiver_type, callable
    );
}

static bool mmay_check_completion_surface_active(
    const MarimayChecker *checker,
    size_t surface_index
) {
    size_t index = 0;
    for (index = 0; checker != NULL && checker->imports != NULL
        && index < checker->imports->binding_count; index += 1) {
        const MarimayCheckImportBinding *binding =
            &checker->imports->bindings[index];
        if (binding->ast == checker->ast
            && binding->surface_index == surface_index) {
            return true;
        }
    }
    return false;
}

static bool mmay_check_completion_copy_free_spelling(
    MarimayChecker *checker,
    MarimayCheckedCompletion *completion,
    const MarimayCheckImportBinding *binding,
    const unsigned char *name,
    size_t name_length,
    bool local_free
) {
    size_t length = 0;
    size_t allocation_size = 0;
    if (local_free) {
        return mmay_owned_bytes_copy(
            &completion->free_spelling,
            checker->result->allocator,
            name,
            name_length
        );
    }
    if (binding == NULL) {
        return mmay_owned_bytes_copy(
            &completion->free_spelling,
            checker->result->allocator,
            NULL,
            0
        );
    }
    if (!binding->is_namespace) {
        return mmay_owned_bytes_copy(
            &completion->free_spelling,
            checker->result->allocator,
            binding->local,
            binding->local_length
        );
    }
    if (!mmay_size_add(binding->local_length, 1, &length)
        || !mmay_size_add(length, name_length, &length)
        || !mmay_size_add(length, 1, &allocation_size)) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    completion->free_spelling.allocator = checker->result->allocator;
    completion->free_spelling.data = mmay_allocate(
        &completion->free_spelling.allocator,
        allocation_size
    );
    if (completion->free_spelling.data == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    memcpy(
        completion->free_spelling.data,
        binding->local,
        binding->local_length
    );
    completion->free_spelling.data[binding->local_length] = '.';
    memcpy(
        completion->free_spelling.data + binding->local_length + 1,
        name,
        name_length
    );
    completion->free_spelling.data[length] = 0;
    completion->free_spelling.length = length;
    return true;
}

static const MarimayCheckImportBinding *mmay_check_completion_free_binding(
    const MarimayChecker *checker,
    size_t surface_index,
    const unsigned char *name,
    size_t name_length
) {
    const MarimayCheckImportBinding *best_named = NULL;
    const MarimayCheckImportBinding *best_namespace = NULL;
    size_t index = 0;
    for (index = 0; checker != NULL && checker->imports != NULL
        && index < checker->imports->binding_count; index += 1) {
        const MarimayCheckImportBinding *binding =
            &checker->imports->bindings[index];
        const MarimayCheckImportBinding **best = NULL;
        size_t shared = 0;
        int comparison = 0;
        if (binding->ast != checker->ast
            || binding->surface_index != surface_index
            || (!binding->is_namespace
                && (binding->original_length != name_length
                    || memcmp(binding->original, name, name_length) != 0))) {
            continue;
        }
        best = binding->is_namespace ? &best_namespace : &best_named;
        if (*best == NULL) {
            *best = binding;
            continue;
        }
        shared = binding->local_length < (*best)->local_length
            ? binding->local_length : (*best)->local_length;
        comparison = shared == 0 ? 0 : memcmp(
            binding->local,
            (*best)->local,
            shared
        );
        if (comparison < 0 || (comparison == 0
                && binding->local_length < (*best)->local_length)) {
            *best = binding;
        }
    }
    return best_named != NULL ? best_named : best_namespace;
}

static bool mmay_check_append_completion(
    MarimayChecker *checker,
    const unsigned char *name,
    size_t name_length,
    const unsigned char *package_name,
    size_t package_name_length,
    MarimaySymbolId symbol,
    MarimaySymbolId receiver_symbol,
    const MarimayCheckImportBinding *free_binding,
    bool local_free
) {
    MarimayCheckedCompletion completion;
    memset(&completion, 0, sizeof(completion));
    if (checker->result->completions.length
        >= checker->options.max_completions) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    completion.symbol = symbol;
    completion.receiver_symbol = receiver_symbol;
    completion.snapshot = checker->ast->snapshot;
    if (!mmay_owned_bytes_copy(
            &completion.name,
            checker->result->allocator,
            name,
            name_length
        ) || !mmay_owned_bytes_copy(
            &completion.package_name,
            checker->result->allocator,
            package_name,
            package_name_length
        ) || !mmay_check_completion_copy_free_spelling(
            checker,
            &completion,
            free_binding,
            name,
            name_length,
            local_free
        )) {
        mmay_owned_bytes_dispose(&completion.free_spelling);
        mmay_owned_bytes_dispose(&completion.package_name);
        mmay_owned_bytes_dispose(&completion.name);
        if (!mmay_checker_has_failed(checker)) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        }
        return false;
    }
    if (!mmay_vector_push(&checker->result->completions, &completion)) {
        mmay_owned_bytes_dispose(&completion.free_spelling);
        mmay_owned_bytes_dispose(&completion.package_name);
        mmay_owned_bytes_dispose(&completion.name);
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool mmay_check_publish_completions_for_ast(MarimayChecker *checker) {
    size_t index = 0;
    if (checker == NULL || checker->result == NULL || checker->ast == NULL) {
        return false;
    }
    for (index = 0; index < checker->declarations.length; index += 1) {
        const MarimayCheckFunctionDeclaration *declaration = mmay_vector_at_const(
            &checker->declarations,
            index
        );
        MarimayCheckCallable resolved;
        if (declaration == NULL || !declaration->signature_valid) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!declaration->dot_eligible) {
            continue;
        }
        if (mmay_check_resolve_call(
                checker,
                declaration->name,
                true,
                declaration->receiver_type,
                &resolved
            ) != MMAY_CHECK_CALL_FOUND
            || !mmay_symbol_id_equal(resolved.symbol, declaration->symbol)
            || !mmay_check_append_completion(
                checker,
                declaration->owned_name.data,
                declaration->owned_name.length,
                (const unsigned char *)checker->package_name,
                checker->package_name_length,
                declaration->symbol,
                declaration->receiver_symbol,
                NULL,
                true
            )) {
            if (!mmay_checker_has_failed(checker)) {
                mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    for (index = 0; checker->imports != NULL
        && index < checker->imports->surface_count; index += 1) {
        const MarimayCheckPackageSurface *surface = checker->imports->surfaces[index];
        size_t function_index = 0;
        if (!mmay_check_completion_surface_active(checker, index)) {
            continue;
        }
        for (function_index = 0; function_index < surface->function_count;
            function_index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                checker->result,
                surface->function_start + function_index
            );
            const MarimayCheckedSymbol *definition = function == NULL ? NULL
                : mmay_checked_symbol_by_id(
                    checker->result,
                    function->mir.symbol
                );
            MarimayCheckCallable resolved;
            MarimayCheckName name;
            const MarimayCheckImportBinding *free_binding = NULL;
            if (function == NULL || !function->is_public
                || !function->dot_eligible) {
                continue;
            }
            if (definition == NULL) {
                mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
                return false;
            }
            name.snapshot = definition->snapshot;
            name.start = definition->start;
            name.end = definition->end;
            if (mmay_check_resolve_call(
                    checker,
                    name,
                    true,
                    function->receiver_type,
                    &resolved
                ) != MMAY_CHECK_CALL_FOUND
                || !mmay_symbol_id_equal(resolved.symbol, function->mir.symbol)) {
                continue;
            }
            free_binding = mmay_check_completion_free_binding(
                checker,
                index,
                function->name.data,
                function->name.length
            );
            if (!mmay_check_append_completion(
                    checker,
                    function->name.data,
                    function->name.length,
                    surface->path,
                    surface->path_length,
                    function->mir.symbol,
                    function->receiver_symbol,
                    free_binding,
                    false
                )) {
                return false;
            }
        }
    }
    return true;
}
