#include "checker_visible_api.h"
#include "checker_package_context.h"
#include "checker_core_api.h"
#include "package_imports_api.h"

#include <string.h>

static bool mmay_visible_equal(const void *left, size_t left_length,
    const void *right, size_t right_length) {
    return left_length == right_length
        && (left_length == 0 || memcmp(left, right, left_length) == 0);
}

static bool mmay_visible_append(MarimayChecker *checker,
    const MarimayCheckedSymbol *symbol, const void *name, size_t length,
    const MarimayCheckImportBinding *namespace_binding) {
    MarimayCheckedVisibleSymbol item = {0};
    MarimayVector spelling = {0};
    size_t index = 0;
    if (checker->result->visible_symbols.length >= checker->options.max_symbols) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_vector_init(&spelling, checker->result->allocator, 1)
        || (namespace_binding != NULL && (!mmay_vector_append(&spelling,
                namespace_binding->local, namespace_binding->local_length)
            || !mmay_vector_append(&spelling, ".", 1)))
        || !mmay_vector_append(&spelling, name, length)
        || !mmay_owned_bytes_copy(&item.spelling, checker->result->allocator,
            spelling.data, spelling.length)) goto allocation_failed;
    mmay_vector_dispose(&spelling);
    item.snapshot = checker->ast->snapshot;
    item.symbol = symbol->symbol;
    for (index = 0; index < checker->result->visible_symbols.length; index += 1) {
        const MarimayCheckedVisibleSymbol *existing = mmay_vector_at_const(
            &checker->result->visible_symbols, index);
        if (existing->snapshot == item.snapshot
            && mmay_symbol_id_equal(existing->symbol, item.symbol)
            && mmay_visible_equal(existing->spelling.data, existing->spelling.length,
                item.spelling.data, item.spelling.length)) {
            mmay_owned_bytes_dispose(&item.spelling);
            return true;
        }
    }
    if (mmay_vector_push(&checker->result->visible_symbols, &item)) return true;
allocation_failed:
    mmay_vector_dispose(&spelling);
    mmay_owned_bytes_dispose(&item.spelling);
    mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
    return false;
}

bool mmay_check_publish_visible_symbols(MarimayChecker *checker) {
    size_t index = 0;
    if (checker == NULL || checker->ast == NULL || checker->result == NULL) return false;
    for (index = 0; index < checker->result->symbols.length; index += 1) {
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol(checker->result, index);
        if (symbol->kind > MMAY_CHECKED_SYMBOL_INTERFACE) continue;
        if (mmay_visible_equal(symbol->package_name.data, symbol->package_name.length,
                checker->package_name, checker->package_name_length)
            && !mmay_visible_append(checker, symbol, symbol->name.data,
                symbol->name.length, NULL)) return false;
    }
    if (checker->imports == NULL) return true;
    for (index = 0; index < checker->imports->binding_count; index += 1) {
        const MarimayCheckImportBinding *binding = &checker->imports->bindings[index];
        const MarimayCheckPackageSurface *surface;
        size_t symbol_index = 0;
        if (binding->ast != checker->ast) continue;
        if (binding->surface_index >= checker->imports->surface_count) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        /* Ordinary package declarations shadow an imported spelling. */
        if (mmay_map_get_const(&checker->top_level_index,
                binding->local, binding->local_length) != NULL) continue;
        surface = checker->imports->surfaces[binding->surface_index];
        for (symbol_index = 0; symbol_index < checker->result->symbols.length;
            symbol_index += 1) {
            const MarimayCheckedSymbol *symbol = mmay_checked_symbol(checker->result, symbol_index);
            if (symbol->kind > MMAY_CHECKED_SYMBOL_INTERFACE || !symbol->is_public
                || !mmay_visible_equal(symbol->package_name.data, symbol->package_name.length,
                    surface->path, surface->path_length)
                || (!binding->is_namespace && !mmay_visible_equal(symbol->name.data,
                    symbol->name.length, binding->original, binding->original_length))) continue;
            if (!mmay_visible_append(checker, symbol,
                    binding->is_namespace ? symbol->name.data : binding->local,
                    binding->is_namespace ? symbol->name.length : binding->local_length,
                    binding->is_namespace ? binding : NULL)) return false;
        }
    }
    return true;
}

void mmay_check_dispose_visible_symbols(MarimayCheckResult *result) {
    size_t index = 0;
    for (index = 0; index < result->visible_symbols.length; index += 1) {
        MarimayCheckedVisibleSymbol *item = mmay_vector_at(&result->visible_symbols, index);
        mmay_owned_bytes_dispose(&item->spelling);
    }
    result->visible_symbols.length = 0;
}

static bool mmay_visible_matches(const MarimayCheckedVisibleSymbol *item,
    const MarimaySourceSnapshot *snapshot, const void *prefix, size_t length) {
    return item->snapshot == snapshot && length <= item->spelling.length
        && (length == 0 || memcmp(item->spelling.data, prefix, length) == 0);
}

size_t mmay_checked_visible_symbol_count(const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot, const void *prefix, size_t length) {
    size_t index = 0, count = 0;
    if (result == NULL || !result->initialized || snapshot == NULL
        || (prefix == NULL && length != 0)) return 0;
    for (index = 0; index < result->visible_symbols.length; index += 1)
        if (mmay_visible_matches(mmay_vector_at_const(&result->visible_symbols, index),
                snapshot, prefix, length)) count += 1;
    return count;
}

const MarimayCheckedVisibleSymbol *mmay_checked_visible_symbol(
    const MarimayCheckResult *result, const MarimaySourceSnapshot *snapshot,
    const void *prefix, size_t length, size_t match) {
    size_t index = 0, count = 0;
    if (result == NULL || !result->initialized || snapshot == NULL
        || (prefix == NULL && length != 0)) return NULL;
    for (index = 0; index < result->visible_symbols.length; index += 1) {
        const MarimayCheckedVisibleSymbol *item = mmay_vector_at_const(&result->visible_symbols, index);
        if (!mmay_visible_matches(item, snapshot, prefix, length)) continue;
        if (count == match) return item;
        count += 1;
    }
    return NULL;
}
