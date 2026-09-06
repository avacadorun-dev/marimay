#include "checker_intrinsics_api.h"
#include "checker_internal.h"
#include "checker_package_context.h"
#include "package_imports_api.h"

bool mmay_check_completion_budget_available(
    const MarimayCheckResult *result, size_t maximum
) {
    return result->completions.length < maximum
        && result->intrinsic_completions.length < maximum - result->completions.length;
}

void mmay_check_dispose_intrinsic_values(MarimayCheckResult *result) {
    size_t index;
    for (index = 0; index < result->intrinsic_completions.length; index += 1) {
        MarimayCheckedIntrinsicCompletion *item = mmay_vector_at(
            &result->intrinsic_completions, index);
        mmay_owned_bytes_dispose(&item->spelling);
        memset(item, 0, sizeof(*item));
    }
    result->intrinsic_completions.length = 0;
}

static bool mmay_intrinsic_append(MarimayChecker *checker,
    const MarimayCheckImportBinding *binding, const MarimayIntrinsicSignature *intrinsic
) {
    MarimayCheckedIntrinsicCompletion item;
    size_t length = binding->local_length, allocation_size;
    memset(&item, 0, sizeof(item));
    if (!mmay_check_completion_budget_available(checker->result, checker->options.max_completions)
        || (binding->is_namespace && (!mmay_size_add(length, 1, &length)
            || !mmay_size_add(length, strlen(intrinsic->name), &length)))
        || !mmay_size_add(length, 1, &allocation_size)) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    item.intrinsic = intrinsic;
    item.snapshot = checker->ast->snapshot;
    item.spelling.allocator = checker->result->allocator;
    item.spelling.data = mmay_allocate(&item.spelling.allocator, allocation_size);
    if (item.spelling.data == NULL) goto allocation_failed;
    item.spelling.length = length;
    memcpy(item.spelling.data, binding->local, binding->local_length);
    if (binding->is_namespace) {
        item.spelling.data[binding->local_length] = '.';
        memcpy(item.spelling.data + binding->local_length + 1,
            intrinsic->name, strlen(intrinsic->name));
    }
    item.spelling.data[length] = 0;
    if (mmay_vector_push(&checker->result->intrinsic_completions, &item)) return true;
allocation_failed:
    mmay_owned_bytes_dispose(&item.spelling);
    mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
    return false;
}

static int mmay_intrinsic_compare(const void *left, const void *right) {
    const MarimayCheckedIntrinsicCompletion *a = left, *b = right;
    size_t shared = a->spelling.length < b->spelling.length ? a->spelling.length : b->spelling.length;
    int order = shared == 0 ? 0 : memcmp(a->spelling.data, b->spelling.data, shared);
    return order != 0 ? order : (a->spelling.length > b->spelling.length)
        - (a->spelling.length < b->spelling.length);
}

bool mmay_check_publish_intrinsics_for_ast(MarimayChecker *checker) {
    size_t index, start = checker->result->intrinsic_completions.length;
    if (checker->imports == NULL) return true;
    for (index = 0; index < checker->imports->binding_count; index += 1) {
        const MarimayCheckImportBinding *binding = &checker->imports->bindings[index];
        const MarimayCheckPackageSurface *surface;
        const MarimayIntrinsicSignature *intrinsic;
        size_t intrinsic_index;
        if (binding->ast != checker->ast) continue;
        if (binding->surface_index >= checker->imports->surface_count) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (mmay_map_get_const(&checker->top_level_index, binding->local, binding->local_length) != NULL) continue;
        surface = checker->imports->surfaces[binding->surface_index];
        for (intrinsic_index = 0; (intrinsic = mmay_json_intrinsic_at(intrinsic_index)) != NULL;
            intrinsic_index += 1) {
            const unsigned char *name = binding->is_namespace
                ? (const unsigned char *)intrinsic->name : binding->original;
            size_t length = binding->is_namespace ? strlen(intrinsic->name) : binding->original_length;
            if (mmay_json_intrinsic_find(surface->path, surface->path_length, name, length) != intrinsic) continue;
            if (!mmay_intrinsic_append(checker, binding, intrinsic)) return false;
        }
    }
    if (checker->result->intrinsic_completions.length - start > 1) {
        qsort(mmay_vector_at(&checker->result->intrinsic_completions, start),
            checker->result->intrinsic_completions.length - start,
            sizeof(MarimayCheckedIntrinsicCompletion), mmay_intrinsic_compare);
    }
    return true;
}

static bool mmay_intrinsic_matches(const MarimayCheckedIntrinsicCompletion *item,
    const MarimaySourceSnapshot *snapshot, const void *prefix, size_t prefix_length
) {
    return item->snapshot == snapshot && prefix_length <= item->spelling.length
        && (prefix_length == 0 || memcmp(item->spelling.data, prefix, prefix_length) == 0);
}

size_t mmay_checked_intrinsic_completion_count(
    const MarimayCheckResult *result, const MarimaySourceSnapshot *snapshot,
    const void *prefix, size_t prefix_length
) {
    size_t index, count = 0;
    if (result == NULL || !result->initialized || snapshot == NULL
        || (prefix == NULL && prefix_length != 0)) return 0;
    for (index = 0; index < result->intrinsic_completions.length; index += 1) {
        if (mmay_intrinsic_matches(mmay_vector_at_const(&result->intrinsic_completions, index),
                snapshot, prefix, prefix_length)) count += 1;
    }
    return count;
}

const MarimayCheckedIntrinsicCompletion *mmay_checked_intrinsic_completion(
    const MarimayCheckResult *result, const MarimaySourceSnapshot *snapshot,
    const void *prefix, size_t prefix_length, size_t match_index
) {
    size_t index, count = 0;
    if (result == NULL || !result->initialized || snapshot == NULL
        || (prefix == NULL && prefix_length != 0)) return NULL;
    for (index = 0; index < result->intrinsic_completions.length; index += 1) {
        const MarimayCheckedIntrinsicCompletion *item = mmay_vector_at_const(&result->intrinsic_completions, index);
        if (!mmay_intrinsic_matches(item, snapshot, prefix, prefix_length)) continue;
        if (count == match_index) return item;
        count += 1;
    }
    return NULL;
}
