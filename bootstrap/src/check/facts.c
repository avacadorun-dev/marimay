#include "checker_internal.h"
#include "checker_package_context.h"

#include <stdlib.h>
#include <string.h>

static int ruda_check_compare_symbol_facts(
    const void *left_pointer,
    const void *right_pointer
) {
    const RudaCheckedSymbol *left = left_pointer;
    const RudaCheckedSymbol *right = right_pointer;
    const unsigned char *left_path = (const unsigned char *)ruda_source_path(
        left->snapshot
    );
    const unsigned char *right_path = (const unsigned char *)ruda_source_path(
        right->snapshot
    );
    size_t left_length = ruda_source_path_length(left->snapshot);
    size_t right_length = ruda_source_path_length(right->snapshot);
    size_t shared = left_length < right_length ? left_length : right_length;
    int comparison = shared == 0 ? 0 : memcmp(left_path, right_path, shared);
    if (comparison != 0) {
        return comparison;
    }
    if (left_length != right_length) {
        return left_length < right_length ? -1 : 1;
    }
    if (left->start != right->start) {
        return left->start < right->start ? -1 : 1;
    }
    if (left->end != right->end) {
        return left->end < right->end ? -1 : 1;
    }
    if (left->kind != right->kind) {
        return left->kind < right->kind ? -1 : 1;
    }
    shared = left->name.length < right->name.length
        ? left->name.length
        : right->name.length;
    comparison = shared == 0 ? 0 : memcmp(
        left->name.data,
        right->name.data,
        shared
    );
    if (comparison != 0) {
        return comparison;
    }
    if (left->name.length != right->name.length) {
        return left->name.length < right->name.length ? -1 : 1;
    }
    return memcmp(
        left->symbol.bytes,
        right->symbol.bytes,
        sizeof(left->symbol.bytes)
    );
}

static bool ruda_check_publish_symbol(
    RudaChecker *checker,
    const RudaCheckSymbolSeed *seed
) {
    RudaCheckedSymbol fact;
    const unsigned char *source = NULL;
    size_t source_length = 0;
    memset(&fact, 0, sizeof(fact));
    if (seed == NULL || seed->name.snapshot == NULL
        || seed->type == RUDA_TYPE_INVALID
        || seed->name.start >= seed->name.end) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    source = ruda_source_bytes(seed->name.snapshot);
    source_length = ruda_source_length(seed->name.snapshot);
    if (seed->name.end > source_length || source == NULL) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (!ruda_owned_bytes_copy(
            &fact.name,
            checker->result->allocator,
            source + seed->name.start,
            seed->name.end - seed->name.start
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    fact.symbol = seed->symbol;
    fact.type = seed->type;
    fact.snapshot = seed->name.snapshot;
    fact.start = seed->name.start;
    fact.end = seed->name.end;
    fact.kind = seed->kind;
    fact.is_public = seed->is_public;
    if (!ruda_vector_push(&checker->result->symbols, &fact)) {
        ruda_owned_bytes_dispose(&fact.name);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool ruda_check_publish_symbol_seeds(
    RudaChecker *checker,
    const RudaCheckSymbolSeed *seeds,
    size_t seed_count
) {
    size_t index = 0;
    size_t total = 0;
    if (checker == NULL) {
        return false;
    }
    if (checker->result == NULL || (seeds == NULL && seed_count != 0)
        || !ruda_size_add(
            checker->result->symbols.length, seed_count, &total
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (total > checker->options.max_symbols) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_vector_reserve(&checker->result->symbols, total)) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < seed_count; index += 1) {
        if (!ruda_check_publish_symbol(checker, &seeds[index])) {
            return false;
        }
    }
    if (checker->result->symbols.length > 1) {
        qsort(
            checker->result->symbols.data,
            checker->result->symbols.length,
            checker->result->symbols.element_size,
            ruda_check_compare_symbol_facts
        );
    }
    ruda_map_dispose(&checker->result->symbol_index);
    if (!ruda_check_symbol_index_init(checker->result)) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < checker->result->symbols.length; index += 1) {
        const RudaCheckedSymbol *fact = ruda_vector_at_const(
            &checker->result->symbols,
            index
        );
        if (fact == NULL || ruda_map_get_const(
                &checker->result->symbol_index,
                fact->symbol.bytes,
                sizeof(fact->symbol.bytes)
            ) != NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!ruda_map_put(
                &checker->result->symbol_index,
                fact->symbol.bytes,
                sizeof(fact->symbol.bytes),
                &index
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    return true;
}
