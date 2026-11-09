#include "checker_internal.h"
#include "checker_package_context.h"

#include <stdlib.h>
#include <string.h>

static int mmay_check_compare_symbol_facts(
    const void *left_pointer,
    const void *right_pointer
) {
    const MarimayCheckedSymbol *left = left_pointer;
    const MarimayCheckedSymbol *right = right_pointer;
    const unsigned char *left_path = (const unsigned char *)mmay_source_path(
        left->snapshot
    );
    const unsigned char *right_path = (const unsigned char *)mmay_source_path(
        right->snapshot
    );
    size_t left_length = mmay_source_path_length(left->snapshot);
    size_t right_length = mmay_source_path_length(right->snapshot);
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

static bool mmay_check_publish_symbol(
    MarimayChecker *checker,
    const MarimayCheckSymbolSeed *seed
) {
    MarimayCheckedSymbol fact;
    const unsigned char *source = NULL;
    size_t source_length = 0;
    memset(&fact, 0, sizeof(fact));
    if (seed == NULL || seed->name.snapshot == NULL
        || seed->type == MMAY_TYPE_INVALID
        || seed->name.start >= seed->name.end) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    source = mmay_source_bytes(seed->name.snapshot);
    source_length = mmay_source_length(seed->name.snapshot);
    if (seed->name.end > source_length || source == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (!mmay_owned_bytes_copy(
            &fact.name,
            checker->result->allocator,
            source + seed->name.start,
            seed->name.end - seed->name.start
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    if (!mmay_owned_bytes_copy(
            &fact.package_name,
            checker->result->allocator,
            checker->package_name,
            checker->package_name_length
        )) {
        mmay_owned_bytes_dispose(&fact.name);
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    fact.symbol = seed->symbol;
    fact.type = seed->type;
    fact.snapshot = seed->name.snapshot;
    fact.start = seed->name.start;
    fact.end = seed->name.end;
    fact.kind = seed->kind;
    fact.is_public = seed->is_public;
    fact.owner_symbol = seed->owner_symbol;
    fact.scope_start = seed->scope_start;
    fact.scope_end = seed->scope_end;
    fact.visible_from = seed->visible_from;
    fact.scope_depth = seed->scope_depth;
    if (!mmay_vector_push(&checker->result->symbols, &fact)) {
        mmay_owned_bytes_dispose(&fact.package_name);
        mmay_owned_bytes_dispose(&fact.name);
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool mmay_check_append_symbol_seed(MarimayChecker *checker,
    const MarimayCheckSymbolSeed *seed) {
    if (checker->result->symbols.length >= checker->options.max_symbols) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    return mmay_check_publish_symbol(checker, seed);
}

static int mmay_check_compare_occurrences(
    const void *left_pointer,
    const void *right_pointer
) {
    const MarimayCheckedOccurrence *left = left_pointer;
    const MarimayCheckedOccurrence *right = right_pointer;
    const unsigned char *left_path = (const unsigned char *)mmay_source_path(
        left->snapshot
    );
    const unsigned char *right_path = (const unsigned char *)mmay_source_path(
        right->snapshot
    );
    size_t left_length = mmay_source_path_length(left->snapshot);
    size_t right_length = mmay_source_path_length(right->snapshot);
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

static int mmay_check_compare_owned_bytes(
    const MarimayOwnedBytes *left,
    const MarimayOwnedBytes *right
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

static int mmay_check_compare_completions(
    const void *left_pointer,
    const void *right_pointer
) {
    const MarimayCheckedCompletion *left = left_pointer;
    const MarimayCheckedCompletion *right = right_pointer;
    MarimayCheckedOccurrence left_location;
    MarimayCheckedOccurrence right_location;
    int comparison = 0;
    memset(&left_location, 0, sizeof(left_location));
    memset(&right_location, 0, sizeof(right_location));
    left_location.snapshot = left->snapshot;
    right_location.snapshot = right->snapshot;
    comparison = mmay_check_compare_occurrences(
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
    comparison = mmay_check_compare_owned_bytes(&left->name, &right->name);
    if (comparison != 0) {
        return comparison;
    }
    comparison = mmay_check_compare_owned_bytes(
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

bool mmay_check_record_occurrence(
    MarimayChecker *checker,
    MarimayCheckName name,
    MarimaySymbolId symbol
) {
    MarimayCheckedOccurrence occurrence;
    if (checker == NULL || checker->result == NULL || name.snapshot == NULL
        || name.start >= name.end
        || name.end > mmay_source_length(name.snapshot)) {
        if (checker != NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    if (checker->result->occurrences.length
        >= checker->options.max_occurrences) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    memset(&occurrence, 0, sizeof(occurrence));
    occurrence.symbol = symbol;
    occurrence.snapshot = name.snapshot;
    occurrence.start = name.start;
    occurrence.end = name.end;
    if (!mmay_vector_push(&checker->result->occurrences, &occurrence)) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

bool mmay_check_finalize_semantic_facts(MarimayChecker *checker) {
    MarimayVector *occurrences = NULL;
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
            mmay_check_compare_occurrences
        );
        for (read_index = 0; read_index < occurrences->length;
            read_index += 1) {
            MarimayCheckedOccurrence *read = mmay_vector_at(
                occurrences,
                read_index
            );
            MarimayCheckedOccurrence *previous = write_index == 0 ? NULL
                : mmay_vector_at(occurrences, write_index - 1);
            if (read == NULL) {
                mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
                return false;
            }
            if (previous != NULL
                && mmay_check_compare_occurrences(previous, read) == 0) {
                continue;
            }
            if (write_index != read_index) {
                MarimayCheckedOccurrence *write = mmay_vector_at(
                    occurrences,
                    write_index
                );
                if (write == NULL) {
                    mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
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
            mmay_check_compare_completions
        );
    }
    for (index = 0; index < occurrences->length; index += 1) {
        const MarimayCheckedOccurrence *occurrence = mmay_vector_at_const(
            occurrences,
            index
        );
        if (occurrence == NULL || mmay_checked_symbol_by_id(
                checker->result,
                occurrence->symbol
            ) == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
    }
    for (index = 0; index < checker->result->completions.length;
        index += 1) {
        const MarimayCheckedCompletion *completion = mmay_vector_at_const(
            &checker->result->completions,
            index
        );
        if (completion == NULL || mmay_checked_symbol_by_id(
                checker->result,
                completion->symbol
            ) == NULL || mmay_checked_symbol_by_id(
                checker->result,
                completion->receiver_symbol
            ) == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
    }
    return true;
}

bool mmay_check_publish_symbol_seeds(
    MarimayChecker *checker,
    const MarimayCheckSymbolSeed *seeds,
    size_t seed_count
) {
    size_t index = 0;
    size_t total = 0;
    if (checker == NULL) {
        return false;
    }
    if (checker->result == NULL || (seeds == NULL && seed_count != 0)
        || !mmay_size_add(
            checker->result->symbols.length, seed_count, &total
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (total > checker->options.max_symbols) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_vector_reserve(&checker->result->symbols, total)) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < seed_count; index += 1) {
        if (!mmay_check_publish_symbol(checker, &seeds[index])) {
            return false;
        }
    }
    if (checker->result->symbols.length > 1) {
        qsort(
            checker->result->symbols.data,
            checker->result->symbols.length,
            checker->result->symbols.element_size,
            mmay_check_compare_symbol_facts
        );
    }
    mmay_map_dispose(&checker->result->symbol_index);
    if (!mmay_check_symbol_index_init(checker->result)) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    for (index = 0; index < checker->result->symbols.length; index += 1) {
        const MarimayCheckedSymbol *fact = mmay_vector_at_const(
            &checker->result->symbols,
            index
        );
        if (fact == NULL || mmay_map_get_const(
                &checker->result->symbol_index,
                fact->symbol.bytes,
                sizeof(fact->symbol.bytes)
            ) != NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!mmay_map_put(
                &checker->result->symbol_index,
                fact->symbol.bytes,
                sizeof(fact->symbol.bytes),
                &index
            )) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
    }
    return true;
}
