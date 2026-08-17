#include "checker_publication_api.h"
#include "checker_declaration_lifecycle_api.h"






 void ruda_checker_private_checker_dispose_declarations(RudaChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->declarations.length; index += 1) {
        RudaCheckFunctionDeclaration *declaration = ruda_vector_at(
            &checker->declarations,
            index
        );
        ruda_checker_private_function_declaration_dispose(declaration);
    }
    for (index = 0; index < checker->constants.length; index += 1) {
        RudaCheckConstantDeclaration *constant = ruda_vector_at(
            &checker->constants,
            index
        );
        ruda_checker_private_constant_declaration_dispose(constant);
    }
    for (index = 0; index < checker->type_aliases.length; index += 1) {
        RudaCheckTypeAliasDeclaration *alias = ruda_vector_at(
            &checker->type_aliases,
            index
        );
        ruda_checker_private_type_alias_declaration_dispose(alias);
    }
    for (index = 0; index < checker->records.length; index += 1) {
        RudaCheckRecordDeclaration *record = ruda_vector_at(
            &checker->records,
            index
        );
        ruda_checker_private_record_declaration_dispose(record);
    }
    for (index = 0; index < checker->enums.length; index += 1) {
        RudaCheckEnumDeclaration *enum_declaration = ruda_vector_at(
            &checker->enums,
            index
        );
        ruda_checker_private_enum_declaration_dispose(enum_declaration);
    }
    for (index = 0; index < checker->interfaces.length; index += 1) {
        ruda_checker_private_interface_declaration_dispose(
            ruda_vector_at(&checker->interfaces, index)
        );
    }
    if (checker->prelude_enums_initialized) {
        for (index = 0; index < checker->prelude_enums.length; index += 1) {
            ruda_checker_private_enum_declaration_dispose(
                ruda_vector_at(&checker->prelude_enums, index)
            );
        }
        ruda_vector_dispose(&checker->prelude_enums);
        checker->prelude_enums_initialized = false;
    }
    ruda_map_dispose(&checker->top_level_index);
    ruda_map_dispose(&checker->local_nominal_index);
    ruda_map_dispose(&checker->callable_index);
    ruda_vector_dispose(&checker->constants);
    ruda_vector_dispose(&checker->type_aliases);
    ruda_vector_dispose(&checker->records);
    ruda_vector_dispose(&checker->enums);
    ruda_vector_dispose(&checker->interfaces);
    ruda_vector_dispose(&checker->declarations);
}

 bool ruda_checker_private_checker_top_level_index_init(RudaChecker *checker) {
    static const uint64_t key0 = UINT64_C(0x727564612d6e616d);
    static const uint64_t key1 = UINT64_C(0x652d696e64657821);
    return ruda_map_init(
        &checker->top_level_index,
        checker->result->allocator,
        sizeof(RudaCheckTopLevelEntry),
        key0,
        key1
    );
}

 bool ruda_checker_private_checker_local_nominal_index_init(RudaChecker *checker) {
    static const uint64_t key0 = UINT64_C(0x727564612d6c6f63);
    static const uint64_t key1 = UINT64_C(0x616c2d6e6f6d696e);
    return ruda_map_init(
        &checker->local_nominal_index,
        checker->result->allocator,
        sizeof(RudaSymbolId),
        key0,
        key1
    );
}

bool ruda_checker_private_check_symbol_fact_total(
    const RudaChecker *checker,
    size_t *result
) {
    size_t total = 0;
    if (checker == NULL || result == NULL
        || !ruda_size_add(total, checker->declarations.length, &total)
        || !ruda_size_add(total, checker->constants.length, &total)
        || !ruda_size_add(total, checker->type_aliases.length, &total)
        || !ruda_size_add(total, checker->records.length, &total)
        || !ruda_size_add(total, checker->enums.length, &total)
        || !ruda_size_add(total, checker->interfaces.length, &total)) {
        return false;
    }
    *result = total;
    return true;
}

 bool ruda_checker_private_check_push_symbol_seed(
    RudaChecker *checker,
    RudaVector *seeds,
    RudaCheckName name,
    RudaSymbolId symbol,
    RudaTypeId type,
    RudaCheckedSymbolKind kind,
    bool is_public
) {
    RudaCheckSymbolSeed seed;
    if (checker == NULL || seeds == NULL) {
        return false;
    }
    memset(&seed, 0, sizeof(seed));
    seed.name = name;
    seed.symbol = symbol;
    seed.type = type;
    seed.kind = kind;
    seed.is_public = is_public;
    if (!ruda_vector_push(seeds, &seed)) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

 bool ruda_checker_private_publish_symbol_facts(RudaChecker *checker) {
    RudaVector seeds;
    size_t total = 0;
    size_t index = 0;
    bool seeds_initialized = false;
    bool published = false;
    memset(&seeds, 0, sizeof(seeds));
    if (!ruda_checker_private_check_symbol_fact_total(checker, &total)
        || total > checker->options.max_symbols) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_vector_init(
            &seeds,
            checker->result->allocator,
            sizeof(RudaCheckSymbolSeed)
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    seeds_initialized = true;
    if (!ruda_vector_reserve(&seeds, total)) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    for (index = 0; index < checker->declarations.length; index += 1) {
        const RudaCheckFunctionDeclaration *declaration = ruda_vector_at_const(
            &checker->declarations,
            index
        );
        if (declaration == NULL || !declaration->signature_valid) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!ruda_checker_private_check_push_symbol_seed(
                checker,
                &seeds,
                declaration->name,
                declaration->symbol,
                declaration->signature_type,
                RUDA_CHECKED_SYMBOL_FUNCTION,
                declaration->is_public
            )) {
            goto cleanup;
        }
    }
    for (index = 0; index < checker->constants.length; index += 1) {
        const RudaCheckConstantDeclaration *declaration = ruda_vector_at_const(
            &checker->constants,
            index
        );
        if (declaration == NULL
            || declaration->state != RUDA_CHECK_CONSTANT_EVALUATED) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!ruda_checker_private_check_push_symbol_seed(
                checker,
                &seeds,
                declaration->name,
                declaration->symbol,
                declaration->type,
                RUDA_CHECKED_SYMBOL_CONSTANT,
                declaration->is_public
            )) {
            goto cleanup;
        }
    }
    for (index = 0; index < checker->type_aliases.length; index += 1) {
        const RudaCheckTypeAliasDeclaration *declaration = ruda_vector_at_const(
            &checker->type_aliases,
            index
        );
        if (declaration == NULL
            || declaration->state != RUDA_CHECK_TYPE_ALIAS_RESOLVED) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!ruda_checker_private_check_push_symbol_seed(
                checker,
                &seeds,
                declaration->name,
                declaration->symbol,
                declaration->type,
                RUDA_CHECKED_SYMBOL_TYPE_ALIAS,
                declaration->is_public
            )) {
            goto cleanup;
        }
    }
    for (index = 0; index < checker->records.length; index += 1) {
        const RudaCheckRecordDeclaration *declaration = ruda_vector_at_const(
            &checker->records,
            index
        );
        if (declaration == NULL || !declaration->fields_resolved) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!ruda_checker_private_check_push_symbol_seed(
                checker,
                &seeds,
                declaration->name,
                declaration->symbol,
                declaration->type,
                RUDA_CHECKED_SYMBOL_RECORD,
                declaration->is_public
            )) {
            goto cleanup;
        }
    }
    for (index = 0; index < checker->enums.length; index += 1) {
        const RudaCheckEnumDeclaration *declaration = ruda_vector_at_const(
            &checker->enums,
            index
        );
        if (declaration == NULL || !declaration->variants_resolved) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!ruda_checker_private_check_push_symbol_seed(
                checker,
                &seeds,
                declaration->name,
                declaration->symbol,
                declaration->type,
                RUDA_CHECKED_SYMBOL_ENUM,
                declaration->is_public
            )) {
            goto cleanup;
        }
    }
    for (index = 0; index < checker->interfaces.length; index += 1) {
        const RudaCheckInterfaceDeclaration *declaration = ruda_vector_at_const(
            &checker->interfaces, index
        );
        if (declaration == NULL || !declaration->methods_resolved) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!ruda_checker_private_check_push_symbol_seed(
                checker, &seeds, declaration->name, declaration->symbol,
                declaration->type, RUDA_CHECKED_SYMBOL_INTERFACE,
                declaration->is_public
            )) {
            goto cleanup;
        }
    }
    published = ruda_check_publish_symbol_seeds(
        checker,
        seeds.data,
        seeds.length
    );

cleanup:
    if (seeds_initialized) {
        ruda_vector_dispose(&seeds);
    }
    return published && !ruda_checker_has_failed(checker);
}
