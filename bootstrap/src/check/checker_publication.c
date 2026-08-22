#include "checker_publication_api.h"
#include "checker_declaration_lifecycle_api.h"
#include "checker_imported_enums_api.h"






 void mmay_checker_private_checker_dispose_declarations(MarimayChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->declarations.length; index += 1) {
        MarimayCheckFunctionDeclaration *declaration = mmay_vector_at(
            &checker->declarations,
            index
        );
        mmay_checker_private_function_declaration_dispose(declaration);
    }
    for (index = 0; index < checker->constants.length; index += 1) {
        MarimayCheckConstantDeclaration *constant = mmay_vector_at(
            &checker->constants,
            index
        );
        mmay_checker_private_constant_declaration_dispose(constant);
    }
    for (index = 0; index < checker->type_aliases.length; index += 1) {
        MarimayCheckTypeAliasDeclaration *alias = mmay_vector_at(
            &checker->type_aliases,
            index
        );
        mmay_checker_private_type_alias_declaration_dispose(alias);
    }
    for (index = 0; index < checker->records.length; index += 1) {
        MarimayCheckRecordDeclaration *record = mmay_vector_at(
            &checker->records,
            index
        );
        mmay_checker_private_record_declaration_dispose(record);
    }
    for (index = 0; index < checker->enums.length; index += 1) {
        MarimayCheckEnumDeclaration *enum_declaration = mmay_vector_at(
            &checker->enums,
            index
        );
        mmay_checker_private_enum_declaration_dispose(enum_declaration);
    }
    for (index = 0; index < checker->interfaces.length; index += 1) {
        mmay_checker_private_interface_declaration_dispose(
            mmay_vector_at(&checker->interfaces, index)
        );
    }
    if (checker->prelude_enums_initialized) {
        for (index = 0; index < checker->prelude_enums.length; index += 1) {
            mmay_checker_private_enum_declaration_dispose(
                mmay_vector_at(&checker->prelude_enums, index)
            );
        }
        mmay_vector_dispose(&checker->prelude_enums);
        checker->prelude_enums_initialized = false;
    }
    mmay_checker_private_imported_enums_dispose(checker);
    mmay_map_dispose(&checker->top_level_index);
    mmay_map_dispose(&checker->local_nominal_index);
    mmay_map_dispose(&checker->callable_index);
    mmay_vector_dispose(&checker->constants);
    mmay_vector_dispose(&checker->type_aliases);
    mmay_vector_dispose(&checker->records);
    mmay_vector_dispose(&checker->enums);
    mmay_vector_dispose(&checker->interfaces);
    mmay_vector_dispose(&checker->declarations);
}

 bool mmay_checker_private_checker_top_level_index_init(MarimayChecker *checker) {
    static const uint64_t key0 = UINT64_C(0x727564612d6e616d);
    static const uint64_t key1 = UINT64_C(0x652d696e64657821);
    return mmay_map_init(
        &checker->top_level_index,
        checker->result->allocator,
        sizeof(MarimayCheckTopLevelEntry),
        key0,
        key1
    );
}

 bool mmay_checker_private_checker_local_nominal_index_init(MarimayChecker *checker) {
    static const uint64_t key0 = UINT64_C(0x727564612d6c6f63);
    static const uint64_t key1 = UINT64_C(0x616c2d6e6f6d696e);
    return mmay_map_init(
        &checker->local_nominal_index,
        checker->result->allocator,
        sizeof(MarimaySymbolId),
        key0,
        key1
    );
}

bool mmay_checker_private_check_symbol_fact_total(
    const MarimayChecker *checker,
    size_t *result
) {
    size_t total = 0;
    if (checker == NULL || result == NULL
        || !mmay_size_add(total, checker->declarations.length, &total)
        || !mmay_size_add(total, checker->constants.length, &total)
        || !mmay_size_add(total, checker->type_aliases.length, &total)
        || !mmay_size_add(total, checker->records.length, &total)
        || !mmay_size_add(total, checker->enums.length, &total)
        || !mmay_size_add(total, checker->interfaces.length, &total)) {
        return false;
    }
    *result = total;
    return true;
}

 bool mmay_checker_private_check_push_symbol_seed(
    MarimayChecker *checker,
    MarimayVector *seeds,
    MarimayCheckName name,
    MarimaySymbolId symbol,
    MarimayTypeId type,
    MarimayCheckedSymbolKind kind,
    bool is_public
) {
    MarimayCheckSymbolSeed seed;
    if (checker == NULL || seeds == NULL) {
        return false;
    }
    memset(&seed, 0, sizeof(seed));
    seed.name = name;
    seed.symbol = symbol;
    seed.type = type;
    seed.kind = kind;
    seed.is_public = is_public;
    if (!mmay_vector_push(seeds, &seed)) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

 bool mmay_checker_private_publish_symbol_facts(MarimayChecker *checker) {
    MarimayVector seeds;
    size_t total = 0;
    size_t index = 0;
    bool seeds_initialized = false;
    bool published = false;
    memset(&seeds, 0, sizeof(seeds));
    if (!mmay_checker_private_check_symbol_fact_total(checker, &total)
        || total > checker->options.max_symbols) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_vector_init(
            &seeds,
            checker->result->allocator,
            sizeof(MarimayCheckSymbolSeed)
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    seeds_initialized = true;
    if (!mmay_vector_reserve(&seeds, total)) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    for (index = 0; index < checker->declarations.length; index += 1) {
        const MarimayCheckFunctionDeclaration *declaration = mmay_vector_at_const(
            &checker->declarations,
            index
        );
        if (declaration == NULL || !declaration->signature_valid) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!mmay_checker_private_check_push_symbol_seed(
                checker,
                &seeds,
                declaration->name,
                declaration->symbol,
                declaration->signature_type,
                MMAY_CHECKED_SYMBOL_FUNCTION,
                declaration->is_public
            )) {
            goto cleanup;
        }
    }
    for (index = 0; index < checker->constants.length; index += 1) {
        const MarimayCheckConstantDeclaration *declaration = mmay_vector_at_const(
            &checker->constants,
            index
        );
        if (declaration == NULL
            || declaration->state != MMAY_CHECK_CONSTANT_EVALUATED) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!mmay_checker_private_check_push_symbol_seed(
                checker,
                &seeds,
                declaration->name,
                declaration->symbol,
                declaration->type,
                MMAY_CHECKED_SYMBOL_CONSTANT,
                declaration->is_public
            )) {
            goto cleanup;
        }
    }
    for (index = 0; index < checker->type_aliases.length; index += 1) {
        const MarimayCheckTypeAliasDeclaration *declaration = mmay_vector_at_const(
            &checker->type_aliases,
            index
        );
        if (declaration == NULL
            || declaration->state != MMAY_CHECK_TYPE_ALIAS_RESOLVED) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!mmay_checker_private_check_push_symbol_seed(
                checker,
                &seeds,
                declaration->name,
                declaration->symbol,
                declaration->type,
                MMAY_CHECKED_SYMBOL_TYPE_ALIAS,
                declaration->is_public
            )) {
            goto cleanup;
        }
    }
    for (index = 0; index < checker->records.length; index += 1) {
        const MarimayCheckRecordDeclaration *declaration = mmay_vector_at_const(
            &checker->records,
            index
        );
        if (declaration == NULL || !declaration->fields_resolved) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!mmay_checker_private_check_push_symbol_seed(
                checker,
                &seeds,
                declaration->name,
                declaration->symbol,
                declaration->type,
                MMAY_CHECKED_SYMBOL_RECORD,
                declaration->is_public
            )) {
            goto cleanup;
        }
    }
    for (index = 0; index < checker->enums.length; index += 1) {
        const MarimayCheckEnumDeclaration *declaration = mmay_vector_at_const(
            &checker->enums,
            index
        );
        if (declaration == NULL || !declaration->variants_resolved) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!mmay_checker_private_check_push_symbol_seed(
                checker,
                &seeds,
                declaration->name,
                declaration->symbol,
                declaration->type,
                MMAY_CHECKED_SYMBOL_ENUM,
                declaration->is_public
            )) {
            goto cleanup;
        }
    }
    for (index = 0; index < checker->interfaces.length; index += 1) {
        const MarimayCheckInterfaceDeclaration *declaration = mmay_vector_at_const(
            &checker->interfaces, index
        );
        if (declaration == NULL || !declaration->methods_resolved) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!mmay_checker_private_check_push_symbol_seed(
                checker, &seeds, declaration->name, declaration->symbol,
                declaration->type, MMAY_CHECKED_SYMBOL_INTERFACE,
                declaration->is_public
            )) {
            goto cleanup;
        }
    }
    published = mmay_check_publish_symbol_seeds(
        checker,
        seeds.data,
        seeds.length
    );

cleanup:
    if (seeds_initialized) {
        mmay_vector_dispose(&seeds);
    }
    return published && !mmay_checker_has_failed(checker);
}
