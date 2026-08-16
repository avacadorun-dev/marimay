#include "checker_internal.h"
#include "checker_package_context.h"
#include "package_imports_api.h"

#include <string.h>

#include "ruda/base/hash.h"

typedef struct RudaCheckCallableKey {
    unsigned char bytes[32];
} RudaCheckCallableKey;

static bool ruda_check_resolution_name_bytes(
    RudaCheckName name,
    const unsigned char **bytes,
    size_t *length
) {
    const unsigned char *source = name.snapshot == NULL
        ? NULL
        : ruda_source_bytes(name.snapshot);
    if (bytes == NULL || length == NULL || source == NULL
        || name.start > name.end
        || name.end > ruda_source_length(name.snapshot)) {
        return false;
    }
    *bytes = source + name.start;
    *length = name.end - name.start;
    return true;
}

static bool ruda_check_callable_key(
    const void *name,
    size_t name_length,
    RudaSymbolId receiver,
    RudaCheckCallableKey *key
) {
    static const unsigned char domain[] = {
        'r', 'u', 'd', 'a', '-', 'd', 'o', 't', '-', 'c', 'a', 'l', 'l'
    };
    RudaSha256 hash;
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
    ruda_sha256_init(&hash);
    if (!ruda_sha256_update(&hash, domain, sizeof(domain))
        || !ruda_sha256_update(
            &hash,
            receiver.bytes,
            sizeof(receiver.bytes)
        ) || !ruda_sha256_update(&hash, length_bytes, sizeof(length_bytes))
        || !ruda_sha256_update(&hash, name, name_length)) {
        return false;
    }
    ruda_sha256_final(&hash, key->bytes);
    return true;
}

bool ruda_check_callable_index_init(RudaChecker *checker) {
    static const uint64_t key0 = UINT64_C(0x727564612d63616c);
    static const uint64_t key1 = UINT64_C(0x6c61626c652d6964);
    return checker != NULL && checker->result != NULL && ruda_map_init(
        &checker->callable_index,
        checker->result->allocator,
        sizeof(size_t),
        key0,
        key1
    );
}

bool ruda_check_owned_receiver(
    const RudaChecker *checker,
    RudaTypeId type,
    RudaSymbolId *receiver
) {
    const RudaTypeDescriptor *descriptor = NULL;
    const RudaSymbolId *owned = NULL;
    if (checker == NULL || checker->result == NULL || receiver == NULL) {
        return false;
    }
    descriptor = ruda_type_descriptor(&checker->result->types, type);
    owned = ruda_map_get_const(
        &checker->local_nominal_index,
        &type,
        sizeof(type)
    );
    if (descriptor == NULL || descriptor->kind != RUDA_TYPE_KIND_NOMINAL
        || owned == NULL || !ruda_symbol_id_equal(*owned, descriptor->symbol)) {
        return false;
    }
    *receiver = descriptor->symbol;
    return true;
}

static bool ruda_check_declaration_name_bytes(
    const RudaCheckFunctionDeclaration *declaration,
    const unsigned char **bytes,
    size_t *length
) {
    return declaration != NULL
        && ruda_check_resolution_name_bytes(declaration->name, bytes, length);
}

static RudaCheckCallResolution ruda_check_unresolved_receiver(
    const RudaChecker *checker,
    const RudaCheckTopLevelEntry *entry,
    bool receiver_owned
) {
    const RudaCheckFunctionDeclaration *first = NULL;
    if (checker == NULL || entry == NULL || entry->function_count == 0) {
        return RUDA_CHECK_CALL_INTERNAL_ERROR;
    }
    first = ruda_vector_at_const(&checker->declarations, entry->index);
    if (first == NULL || !first->signature_valid) {
        return RUDA_CHECK_CALL_INTERNAL_ERROR;
    }
    if (entry->function_count == 1 && !first->dot_eligible) {
        return RUDA_CHECK_CALL_NOT_DOT_ELIGIBLE;
    }
    if (!receiver_owned) {
        return RUDA_CHECK_CALL_RECEIVER_NOT_OWNED;
    }
    return RUDA_CHECK_CALL_OTHER_NOMINAL;
}

bool ruda_check_callable_insert(
    RudaChecker *checker,
    const RudaCheckFunctionDeclaration *declaration,
    size_t declaration_index
) {
    const unsigned char *name_bytes = NULL;
    size_t name_length = 0;
    RudaCheckCallableKey key;
    if (checker == NULL || declaration == NULL
        || !declaration->signature_valid || !declaration->dot_eligible
        || declaration_index == SIZE_MAX
        || !ruda_check_declaration_name_bytes(
            declaration,
            &name_bytes,
            &name_length
        ) || !ruda_check_callable_key(
            name_bytes,
            name_length,
            declaration->receiver_symbol,
            &key
        )) {
        if (checker != NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    if (ruda_map_get_const(
            &checker->callable_index,
            key.bytes,
            sizeof(key.bytes)
        ) != NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (!ruda_map_put(
            &checker->callable_index,
            key.bytes,
            sizeof(key.bytes),
            &declaration_index
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool ruda_check_callable_remove(
    RudaChecker *checker,
    const RudaCheckFunctionDeclaration *declaration
) {
    const unsigned char *name_bytes = NULL;
    size_t name_length = 0;
    RudaCheckCallableKey key;
    return checker != NULL && declaration != NULL
        && declaration->dot_eligible
        && ruda_check_declaration_name_bytes(
            declaration,
            &name_bytes,
            &name_length
        ) && ruda_check_callable_key(
            name_bytes,
            name_length,
            declaration->receiver_symbol,
            &key
        ) && ruda_map_remove(
            &checker->callable_index,
            key.bytes,
            sizeof(key.bytes)
        );
}

static bool ruda_check_callable_from_declaration(
    const RudaCheckFunctionDeclaration *declaration,
    RudaCheckCallable *callable
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
    return true;
}

RudaCheckCallResolution ruda_check_resolve_call(
    const RudaChecker *checker,
    RudaCheckName name,
    bool receiver_known,
    RudaTypeId receiver_type,
    RudaCheckCallable *callable
) {
    const unsigned char *name_bytes = NULL;
    size_t name_length = 0;
    const RudaCheckTopLevelEntry *entry = NULL;
    const RudaCheckFunctionDeclaration *candidate = NULL;
    const unsigned char *candidate_name = NULL;
    size_t candidate_name_length = 0;
    RudaSymbolId receiver;
    RudaCheckCallableKey key;
    const size_t *candidate_index = NULL;
    bool receiver_owned = false;
    RudaCheckCallResolution imported = RUDA_CHECK_CALL_UNKNOWN_NAME;
    RudaCheckName no_namespace;
    memset(&no_namespace, 0, sizeof(no_namespace));
    if (callable == NULL || checker == NULL) {
        return RUDA_CHECK_CALL_INTERNAL_ERROR;
    }
    memset(callable, 0, sizeof(*callable));
    if (!ruda_check_resolution_name_bytes(name, &name_bytes, &name_length)) {
        return RUDA_CHECK_CALL_INTERNAL_ERROR;
    }
    entry = ruda_map_get_const(
        &checker->top_level_index,
        name_bytes,
        name_length
    );
    if (entry == NULL) {
        return ruda_check_resolve_imported_call(
            checker, no_namespace, false, name, receiver_known,
            receiver_type, callable
        );
    }
    if (entry->kind != RUDA_CHECK_TOP_LEVEL_FUNCTION) {
        return RUDA_CHECK_CALL_NOT_FUNCTION;
    }
    if (entry->function_count == 0) {
        return RUDA_CHECK_CALL_INTERNAL_ERROR;
    }
    if (!receiver_known) {
        if (entry->function_count != 1) {
            return RUDA_CHECK_CALL_RECEIVER_REQUIRED;
        }
        candidate = ruda_vector_at_const(&checker->declarations, entry->index);
    } else {
        receiver_owned = ruda_check_owned_receiver(
            checker,
            receiver_type,
            &receiver
        );
        if (receiver_owned) {
            if (!ruda_check_callable_key(
                    name_bytes,
                    name_length,
                    receiver,
                    &key
                )) {
                return RUDA_CHECK_CALL_INTERNAL_ERROR;
            }
            candidate_index = ruda_map_get_const(
                &checker->callable_index,
                key.bytes,
                sizeof(key.bytes)
            );
            if (candidate_index != NULL) {
                candidate = ruda_vector_at_const(
                    &checker->declarations,
                    *candidate_index
                );
                if (candidate == NULL || !candidate->dot_eligible
                    || !ruda_symbol_id_equal(
                        candidate->receiver_symbol,
                        receiver
                    )) {
                    return RUDA_CHECK_CALL_INTERNAL_ERROR;
                }
            }
        }
        if (candidate == NULL) {
            imported = ruda_check_resolve_imported_call(
                checker, no_namespace, false, name, true,
                receiver_type, callable
            );
            if (imported == RUDA_CHECK_CALL_FOUND) {
                return imported;
            }
            return ruda_check_unresolved_receiver(checker, entry,
                receiver_owned || imported == RUDA_CHECK_CALL_OTHER_NOMINAL);
        }
    }
    if (candidate == NULL || !candidate->signature_valid
        || !ruda_check_declaration_name_bytes(
            candidate,
            &candidate_name,
            &candidate_name_length
        ) || candidate_name_length != name_length
        || memcmp(candidate_name, name_bytes, name_length) != 0) {
        return RUDA_CHECK_CALL_INTERNAL_ERROR;
    }
    return ruda_check_callable_from_declaration(candidate, callable)
        ? RUDA_CHECK_CALL_FOUND : RUDA_CHECK_CALL_INTERNAL_ERROR;
}

RudaCheckCallResolution ruda_check_resolve_qualified_call(
    const RudaChecker *checker,
    RudaCheckName namespace_name,
    RudaCheckName name,
    bool receiver_known,
    RudaTypeId receiver_type,
    RudaCheckCallable *callable
) {
    return ruda_check_resolve_imported_call(
        checker, namespace_name, true, name, receiver_known,
        receiver_type, callable
    );
}
