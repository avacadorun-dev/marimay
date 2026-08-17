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
    if (!ruda_owned_bytes_copy(
            &fact.package_name,
            checker->result->allocator,
            checker->package_name,
            checker->package_name_length
        )) {
        ruda_owned_bytes_dispose(&fact.name);
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
        ruda_owned_bytes_dispose(&fact.package_name);
        ruda_owned_bytes_dispose(&fact.name);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

static int ruda_check_compare_occurrences(
    const void *left_pointer,
    const void *right_pointer
) {
    const RudaCheckedOccurrence *left = left_pointer;
    const RudaCheckedOccurrence *right = right_pointer;
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
    return memcmp(
        left->symbol.bytes,
        right->symbol.bytes,
        sizeof(left->symbol.bytes)
    );
}

static int ruda_check_compare_owned_bytes(
    const RudaOwnedBytes *left,
    const RudaOwnedBytes *right
) {
    size_t shared = left->length < right->length
        ? left->length : right->length;
    int comparison = shared == 0 ? 0 : memcmp(
        left->data,
        right->data,
        shared
    );
    if (comparison != 0) {
        return comparison;
    }
    if (left->length == right->length) {
        return 0;
    }
    return left->length < right->length ? -1 : 1;
}

static int ruda_check_compare_completions(
    const void *left_pointer,
    const void *right_pointer
) {
    const RudaCheckedCompletion *left = left_pointer;
    const RudaCheckedCompletion *right = right_pointer;
    RudaCheckedOccurrence left_location;
    RudaCheckedOccurrence right_location;
    int comparison = 0;
    memset(&left_location, 0, sizeof(left_location));
    memset(&right_location, 0, sizeof(right_location));
    left_location.snapshot = left->snapshot;
    right_location.snapshot = right->snapshot;
    comparison = ruda_check_compare_occurrences(
        &left_location,
        &right_location
    );
    if (comparison != 0) {
        return comparison;
    }
    comparison = memcmp(
        left->receiver_symbol.bytes,
        right->receiver_symbol.bytes,
        sizeof(left->receiver_symbol.bytes)
    );
    if (comparison != 0) {
        return comparison;
    }
    comparison = ruda_check_compare_owned_bytes(&left->name, &right->name);
    if (comparison != 0) {
        return comparison;
    }
    comparison = ruda_check_compare_owned_bytes(
        &left->package_name,
        &right->package_name
    );
    if (comparison != 0) {
        return comparison;
    }
    return memcmp(
        left->symbol.bytes,
        right->symbol.bytes,
        sizeof(left->symbol.bytes)
    );
}

bool ruda_check_record_occurrence(
    RudaChecker *checker,
    RudaCheckName name,
    RudaSymbolId symbol
) {
    RudaCheckedOccurrence occurrence;
    if (checker == NULL || checker->result == NULL || name.snapshot == NULL
        || name.start >= name.end
        || name.end > ruda_source_length(name.snapshot)) {
        if (checker != NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    if (checker->result->occurrences.length
        >= checker->options.max_occurrences) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    memset(&occurrence, 0, sizeof(occurrence));
    occurrence.symbol = symbol;
    occurrence.snapshot = name.snapshot;
    occurrence.start = name.start;
    occurrence.end = name.end;
    if (!ruda_vector_push(&checker->result->occurrences, &occurrence)) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool ruda_check_finalize_semantic_facts(RudaChecker *checker) {
    RudaVector *occurrences = NULL;
    size_t read_index = 0;
    size_t write_index = 0;
    size_t index = 0;
    if (checker == NULL || checker->result == NULL) {
        return false;
    }
    occurrences = &checker->result->occurrences;
    if (occurrences->length > 1) {
        qsort(
            occurrences->data,
            occurrences->length,
            occurrences->element_size,
            ruda_check_compare_occurrences
        );
        for (read_index = 0; read_index < occurrences->length;
            read_index += 1) {
            RudaCheckedOccurrence *read = ruda_vector_at(
                occurrences,
                read_index
            );
            RudaCheckedOccurrence *previous = write_index == 0 ? NULL
                : ruda_vector_at(occurrences, write_index - 1);
            if (read == NULL) {
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
                return false;
            }
            if (previous != NULL
                && ruda_check_compare_occurrences(previous, read) == 0) {
                continue;
            }
            if (write_index != read_index) {
                RudaCheckedOccurrence *write = ruda_vector_at(
                    occurrences,
                    write_index
                );
                if (write == NULL) {
                    ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
                    return false;
                }
                *write = *read;
            }
            write_index += 1;
        }
        occurrences->length = write_index;
    }
    if (checker->result->completions.length > 1) {
        qsort(
            checker->result->completions.data,
            checker->result->completions.length,
            checker->result->completions.element_size,
            ruda_check_compare_completions
        );
    }
    for (index = 0; index < occurrences->length; index += 1) {
        const RudaCheckedOccurrence *occurrence = ruda_vector_at_const(
            occurrences,
            index
        );
        if (occurrence == NULL || ruda_checked_symbol_by_id(
                checker->result,
                occurrence->symbol
            ) == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
    }
    for (index = 0; index < checker->result->completions.length;
        index += 1) {
        const RudaCheckedCompletion *completion = ruda_vector_at_const(
            &checker->result->completions,
            index
        );
        if (completion == NULL || ruda_checked_symbol_by_id(
                checker->result,
                completion->symbol
            ) == NULL || ruda_checked_symbol_by_id(
                checker->result,
                completion->receiver_symbol
            ) == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
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
